// =====================================================================================
// Copyright (c) 2021-2024 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <dirent.h>

#include "colecoDS.h"
#include "Adam.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/z80/ctc.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "MTX_BIOS.h"
#include "fdc.h"
#include "printf.h"

#define COLECODS_SAVE_VER 0x001D        // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

struct RomOffset
{
    u8   type;
    u32  offset;
};

struct RomOffset Offsets[8];
#define TYPE_ROM   0
#define TYPE_RAM   1
#define TYPE_BIOS  2
#define TYPE_OTHER 3

/*********************************************************************************
 * Save the current state - save everything we need to a single .sav file.
 ********************************************************************************/
u8  spare[512] = {0x00};            // We keep some spare bytes so we can use them in the future without changing the structure

static char szLoadFile[256];        // We build the filename out of the base filename and tack on .sav, .ee, etc.
static char szCh1[32];

void colecoSaveState() 
{
  u32 uNbO;
  long pSvg;

  // Return to the original path
  chdir(initial_path);    
  
  // Init filename = romname and SAV in place of ROM
  DIR* dir = opendir("sav");
  if (dir) closedir(dir);    // Directory exists... close it out and move on.
  else mkdir("sav", 0777);   // Otherwise create the directory...
  sprintf(szLoadFile,"sav/%s", initial_file);

  int len = strlen(szLoadFile);
  if (szLoadFile[len-3] == '.') // In case of .sg or .sc
  {
      szLoadFile[len-2] = 's';
      szLoadFile[len-1] = 'a';
      szLoadFile[len-0] = 'v';
      szLoadFile[len+1] = 0;
  }
  else
  {
      szLoadFile[len-3] = 's';
      szLoadFile[len-2] = 'a';
      szLoadFile[len-1] = 'v';
  }
  strcpy(szCh1,"SAVING...");
  DSPrint(6,0,0,szCh1);
  
  FILE *handle = fopen(szLoadFile, "wb+");  
  if (handle != NULL) 
  {
    // Write Version
    u16 save_ver = COLECODS_SAVE_VER;
    uNbO = fwrite(&save_ver, sizeof(u16), 1, handle);
      
    // Write CZ80 CPU
    uNbO = fwrite(&CPU, sizeof(CPU), 1, handle);
      
    // Deficit Z80 CPU Cycle counter
    if (uNbO) uNbO = fwrite(&cycle_deficit, sizeof(cycle_deficit), 1, handle); 

    // Save Coleco Memory (yes, all of it!)
    if (uNbO) uNbO = fwrite(RAM_Memory, 0x10000,1, handle); 
      
    // And the Memory Map - we must only save offsets so that this is generic when we change code and memory shifts...
    for (u8 i=0; i<8; i++)
    {
        if ((MemoryMap[i] >= ROM_Memory) && (MemoryMap[i] <= ROM_Memory+(sizeof(ROM_Memory))))
        {
            Offsets[i].type = TYPE_ROM;
            Offsets[i].offset = MemoryMap[i] - ROM_Memory;
        }
        else if ((MemoryMap[i] >= RAM_Memory) && (MemoryMap[i] <= RAM_Memory+(sizeof(RAM_Memory))))
        {
            Offsets[i].type = TYPE_RAM;
            Offsets[i].offset = MemoryMap[i] - RAM_Memory;
        }
        else if ((MemoryMap[i] >= BIOS_Memory) && (MemoryMap[i] <= BIOS_Memory+(sizeof(BIOS_Memory))))
        {
            Offsets[i].type = TYPE_BIOS;
            Offsets[i].offset = MemoryMap[i] - BIOS_Memory;
        }
        else
        {
            Offsets[i].type = TYPE_OTHER;
            Offsets[i].offset =  (u32)MemoryMap[i];
        }
    }
    if (uNbO) uNbO = fwrite(Offsets, sizeof(Offsets),1, handle);     
      
    // Write the Super Game Module stuff
    if (uNbO) uNbO = fwrite(&sgm_enable, sizeof(sgm_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
      
    // A few frame counters
    if (uNbO) uNbO = fwrite(&emuActFrames, sizeof(emuActFrames), 1, handle); 
    if (uNbO) uNbO = fwrite(&timingFrames, sizeof(timingFrames), 1, handle); 
      
    // Some Memotech MTX stuff...
    if (uNbO) uNbO = fwrite(&memotech_RAM_start, sizeof(memotech_RAM_start), 1, handle); 
    if (uNbO) uNbO = fwrite(&IOBYTE, sizeof(IOBYTE), 1, handle); 
    if (uNbO) uNbO = fwrite(&MTX_KBD_DRIVE, sizeof(MTX_KBD_DRIVE), 1, handle); 
    if (uNbO) uNbO = fwrite(&lastIOBYTE, sizeof(lastIOBYTE), 1, handle); 
    if (uNbO) uNbO = fwrite(&tape_pos, sizeof(tape_pos), 1, handle); 
    if (uNbO) uNbO = fwrite(&tape_len, sizeof(tape_len), 1, handle); 
    if (uNbO) uNbO = fwrite(&last_tape_pos, sizeof(last_tape_pos), 1, handle); 
      
    if (uNbO) uNbO = fwrite(&memotech_magrom_present, sizeof(memotech_magrom_present), 1, handle); 
    if (uNbO) uNbO = fwrite(&memotech_mtx_500_only, sizeof(memotech_mtx_500_only), 1, handle); 
    if (uNbO) uNbO = fwrite(&memotech_lastMagROMPage, sizeof(memotech_lastMagROMPage), 1, handle);       
      
    // Some SVI stuff...
    if (uNbO) uNbO = fwrite(&svi_RAM, 2, 1, handle); 
      
    // Some SG-1000 / SC-3000 stuff...
    if (uNbO) uNbO = fwrite(&Port_PPI_CTRL, sizeof(Port_PPI_CTRL), 1, handle);       
    if (uNbO) uNbO = fwrite(&OldPortC, sizeof(OldPortC), 1, handle);                        

    // Some Tatung Einstein stuff...
    if (uNbO) uNbO = fwrite(&einstein_ram_start, sizeof(einstein_ram_start), 1, handle);                        
    if (uNbO) uNbO = fwrite(&keyboard_w, sizeof(keyboard_w), 1, handle);                        
    if (uNbO) uNbO = fwrite(&key_int_mask, sizeof(key_int_mask), 1, handle);
    if (einstein_mode == 2 || msx_mode == 3)
    {
        if (uNbO) uNbO = fwrite(&FDC, sizeof(FDC), 1, handle);        
    }

    if (uNbO) uNbO = fwrite(SGC_Bank, sizeof(SGC_Bank), 1, handle);
    if (uNbO) uNbO = fwrite(&SGC_EEPROM_State, sizeof(SGC_EEPROM_State), 1, handle);
    if (uNbO) uNbO = fwrite(&SGC_EEPROM_CmdPos, sizeof(SGC_EEPROM_CmdPos), 1, handle);
    
    // Some spare memory we can eat into...
    if (uNbO) uNbO = fwrite(spare, 506,1, handle);
      
    // Write VDP
    if (uNbO) uNbO = fwrite(VDP, sizeof(VDP),1, handle); 
    if (uNbO) uNbO = fwrite(&VDPCtrlLatch, sizeof(VDPCtrlLatch),1, handle); 
    if (uNbO) uNbO = fwrite(&VDPStatus, sizeof(VDPStatus),1, handle); 
    if (uNbO) uNbO = fwrite(&FGColor, sizeof(FGColor),1, handle); 
    if (uNbO) uNbO = fwrite(&BGColor, sizeof(BGColor),1, handle); 
    if (uNbO) uNbO = fwrite(&OH, sizeof(OH),1, handle); 
    if (uNbO) uNbO = fwrite(&IH, sizeof(IH),1, handle);       
    if (uNbO) uNbO = fwrite(&ScrMode, sizeof(ScrMode),1, handle); 
    if (uNbO) uNbO = fwrite(&VDPDlatch, sizeof(VDPDlatch),1, handle); 
    if (uNbO) uNbO = fwrite(&VAddr, sizeof(VAddr),1, handle); 
    if (uNbO) uNbO = fwrite(&CurLine, sizeof(CurLine),1, handle); 
    if (uNbO) uNbO = fwrite(&ColTabM, sizeof(ColTabM),1, handle); 
    if (uNbO) uNbO = fwrite(&ChrGenM, sizeof(ChrGenM),1, handle); 
    if (uNbO) uNbO = fwrite(pVDPVidMem, 0x4000,1, handle); 
    pSvg = ChrGen-pVDPVidMem;
    if (uNbO) uNbO = fwrite(&pSvg, sizeof(pSvg),1, handle); 
    pSvg = ChrTab-pVDPVidMem;
    if (uNbO) uNbO = fwrite(&pSvg, sizeof(pSvg),1, handle); 
    pSvg = ColTab-pVDPVidMem;
    if (uNbO) uNbO = fwrite(&pSvg, sizeof(pSvg),1, handle); 
    pSvg = SprGen-pVDPVidMem;
    if (uNbO) uNbO = fwrite(&pSvg, sizeof(pSvg),1, handle); 
    pSvg = SprTab-pVDPVidMem;
    if (uNbO) uNbO = fwrite(&pSvg, sizeof(pSvg),1, handle); 

    // Write PSG SN and AY sound chips...
    if (uNbO) uNbO = fwrite(&mySN, sizeof(mySN),1, handle);
    if (uNbO) uNbO = fwrite(&myAY, sizeof(myAY),1, handle);
      
    // Write stuff for MSX, SordM5 and SG-1000
    if (uNbO) fwrite(&Port_PPI_A, sizeof(Port_PPI_A),1, handle);
    if (uNbO) fwrite(&Port_PPI_B, sizeof(Port_PPI_B),1, handle);
    if (uNbO) fwrite(&Port_PPI_C, sizeof(Port_PPI_C),1, handle);
    
    if (uNbO) fwrite(&mapperType, sizeof(mapperType),1, handle);
    if (uNbO) fwrite(&mapperMask, sizeof(mapperMask),1, handle);
    if (uNbO) fwrite(bROMInSlot, sizeof(bROMInSlot),1, handle);
    if (uNbO) fwrite(bRAMInSlot, sizeof(bRAMInSlot),1, handle);
    
    if (uNbO) fwrite(CTC, sizeof(CTC),1, handle);
    if (uNbO) fwrite(&vdp_int_source, sizeof(vdp_int_source),1, handle);
    
    // Various ports used in the system
    if (uNbO) fwrite(&Port53, sizeof(Port53),1, handle);
    if (uNbO) fwrite(&Port60, sizeof(Port60),1, handle);
    if (uNbO) fwrite(&Port20, sizeof(Port20),1, handle);
    if (uNbO) fwrite(&Port42, sizeof(Port42),1, handle);    
      
    if (einstein_mode)
    {
        if (uNbO) fwrite(&keyboard_interrupt, sizeof(keyboard_interrupt),1, handle);      
        if (uNbO) fwrite(&einstein_ram_start, sizeof(einstein_ram_start),1, handle);      
        if (uNbO) fwrite(&keyboard_w, sizeof(keyboard_w),1, handle);      
        if (uNbO) fwrite(&key_int_mask, sizeof(key_int_mask),1, handle);      
        if (uNbO) fwrite(&myKeyData, sizeof(myKeyData),1, handle);      
        if (uNbO) fwrite(&adc_mux, sizeof(adc_mux),1, handle);      
    }
    else if (msx_mode)   // Big enough that we will not write this if we are not MSX 
    {
        for (u8 i=0; i<8; i++)
        {
            if ((Slot1ROMPtr[i] >= ROM_Memory) && (Slot1ROMPtr[i] <= ROM_Memory+(sizeof(ROM_Memory))))
            {
                Offsets[i].type = TYPE_ROM;
                Offsets[i].offset = Slot1ROMPtr[i] - ROM_Memory;
            }
            else
            {
                Offsets[i].type = TYPE_OTHER;
                Offsets[i].offset =  (u32)Slot1ROMPtr[i];
            }
        }
        if (uNbO) fwrite(Offsets, sizeof(Offsets),1, handle);
        
        if (uNbO) fwrite(&msx_sram_at_8000, sizeof(msx_sram_at_8000),1, handle);
        if (msx_sram_enabled) if (uNbO) fwrite(SRAM_Memory, 0x4000,1, handle);    // No game uses more than 16K
        if (msx_scc_enable)
        {
            if (uNbO) uNbO = fwrite(&mySCC, sizeof(mySCC),1, handle);
        }
    }
    else if (adam_mode)  // Big enough that we will not write this if we are not ADAM
    {
        if (uNbO) fwrite(PCBTable+0x8000, 0x8000, 1, handle);
        
        if (uNbO) fwrite(&PCBAddr, sizeof(PCBAddr),1, handle);        
        if (uNbO) fwrite(adam_ram_present, sizeof(adam_ram_present),1, handle);        
        if (uNbO) fwrite(&KBDStatus, sizeof(KBDStatus),1, handle);
        if (uNbO) fwrite(&LastKey, sizeof(LastKey),1, handle);
        if (uNbO) fwrite(&adam_CapsLock, sizeof(adam_CapsLock),1, handle);        
        if (uNbO) fwrite(&disk_unsaved_data, sizeof(disk_unsaved_data),1, handle);        
        if (uNbO) fwrite(AdamDriveStatus, sizeof(AdamDriveStatus),1, handle);
        
        if (uNbO) fwrite(spare, 32,1, handle);        
        if (uNbO) fwrite(&adam_ext_ram_used, sizeof(adam_ext_ram_used),1, handle);
        if (adam_ext_ram_used)
        {
            // This must be the last thing written so we can compress it...
            u32 len = (DSI_RAM_Buffer ? (2*1024*1024) : (64 * 1024)) >> 2;
            u32 *ptr = (DSI_RAM_Buffer ? ((u32*)DSI_RAM_Buffer) : ((u32 *)(EXP_Memory)));
            for (u32 i=0; i<len; i++)
            {
                fwrite(ptr, 1, sizeof(u32), handle); // Write 32-bits at a time...
                
                u32 zero_count = 0;
                while ((*ptr == 0x00000000) && (i<len)) // Compress
                {
                    zero_count++;
                    ptr++;
                    i++;
                }
                
                if (zero_count) fwrite(&zero_count, 1, sizeof(zero_count), handle); // Write the zero count
                else ptr++;
            }
        }
    }
    else if (bActivisionPCB)
    {
        // Write the EEPROM and memory
        if (uNbO) fwrite(&EEPROM, sizeof(EEPROM),1, handle);      
    }
    else if (creativision_mode)
    {
        // Write some creativision stuff...
        u16 cv_cpu_size=1;
        u8 *mem = creativision_get_cpu(&cv_cpu_size);
        if (uNbO) fwrite(mem, cv_cpu_size,1, handle);
    }
      
    if (uNbO) 
      strcpy(szCh1,"OK ");
    else
      strcpy(szCh1,"ERR");
     DSPrint(15,0,0,szCh1);
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    DSPrint(6,0,0,"             "); 
    DisplayStatusLine(true);
  }
  else {
    strcpy(szCh1,"Error opening SAV file ...");
  }
  fclose(handle);
}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void colecoLoadState() 
{
    u32 uNbO;
    long pSvg;

    // Return to the original path
    chdir(initial_path);    

    // Init filename = romname and .SAV in place of ROM
    sprintf(szLoadFile,"sav/%s", initial_file);
    int len = strlen(szLoadFile);
    if (szLoadFile[len-3] == '.') // In case of .sg or .sc
    {
      szLoadFile[len-2] = 's';
      szLoadFile[len-1] = 'a';
      szLoadFile[len-0] = 'v';
      szLoadFile[len+1] = 0;
    }
    else
    {
      szLoadFile[len-3] = 's';
      szLoadFile[len-2] = 'a';
      szLoadFile[len-1] = 'v';
    }
    FILE* handle = fopen(szLoadFile, "rb"); 
    if (handle != NULL) 
    {    
         strcpy(szCh1,"LOADING...");
         DSPrint(6,0,0,szCh1);
       
        // Read Version
        u16 save_ver = 0xBEEF;
        uNbO = fread(&save_ver, sizeof(u16), 1, handle);
        
        if (save_ver == COLECODS_SAVE_VER)
        {
            // Load CZ80 CPU
            uNbO = fread(&CPU, sizeof(CPU), 1, handle);
       
            // Deficit Z80 CPU Cycle counter
            if (uNbO) uNbO = fread(&cycle_deficit, sizeof(cycle_deficit), 1, handle); 
            
            // Load Coleco Memory (yes, all of it!)
            if (uNbO) uNbO = fread(RAM_Memory, 0x10000,1, handle); 
            
            // And the Memory Map - we must only save offsets so that this is generic when we change code and memory shifts...
            if (uNbO) uNbO = fread(Offsets, sizeof(Offsets),1, handle);     
            for (u8 i=0; i<8; i++)
            {
                if (Offsets[i].type == TYPE_ROM)
                {
                    MemoryMap[i] = (u8 *) (ROM_Memory + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_RAM)
                {
                    MemoryMap[i] = (u8 *) (RAM_Memory + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_BIOS)
                {
                    MemoryMap[i] = (u8 *) (BIOS_Memory + Offsets[i].offset);
                }
                else
                {
                    MemoryMap[i] = (u8 *) (Offsets[i].offset);
                }
            }
            
            // Load the Super Game Module stuff
            if (uNbO) uNbO = fread(&sgm_enable, sizeof(sgm_enable), 1, handle); 
            if (uNbO) uNbO = fread(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
            
            // A few frame counters
            if (uNbO) uNbO = fread(&emuActFrames, sizeof(emuActFrames), 1, handle); 
            if (uNbO) uNbO = fread(&timingFrames, sizeof(timingFrames), 1, handle); 
            
            // Some Memotech MTX stuff...
            if (uNbO) uNbO = fread(&memotech_RAM_start, sizeof(memotech_RAM_start), 1, handle); 
            if (uNbO) uNbO = fread(&IOBYTE, sizeof(IOBYTE), 1, handle); 
            if (uNbO) uNbO = fread(&MTX_KBD_DRIVE, sizeof(MTX_KBD_DRIVE), 1, handle); 
            if (uNbO) uNbO = fread(&lastIOBYTE, sizeof(lastIOBYTE), 1, handle); 
            if (uNbO) uNbO = fread(&tape_pos, sizeof(tape_pos), 1, handle); 
            if (uNbO) uNbO = fread(&tape_len, sizeof(tape_len), 1, handle); 
            if (uNbO) uNbO = fread(&last_tape_pos, sizeof(last_tape_pos), 1, handle); 
            
            if (uNbO) uNbO = fread(&memotech_magrom_present, sizeof(memotech_magrom_present), 1, handle); 
            if (uNbO) uNbO = fread(&memotech_mtx_500_only, sizeof(memotech_mtx_500_only), 1, handle); 
            if (uNbO) uNbO = fread(&memotech_lastMagROMPage, sizeof(memotech_lastMagROMPage), 1, handle); 
            
            // Some SVI stuff...
            if (uNbO) uNbO = fread(&svi_RAM, 2, 1, handle); 
            
            // Some SG-1000 / SC-3000 stuff...
            if (uNbO) uNbO = fread(&Port_PPI_CTRL, sizeof(Port_PPI_CTRL), 1, handle);       
            if (uNbO) uNbO = fread(&OldPortC, sizeof(OldPortC), 1, handle);                  
            
            // Some Tatung Einstein stuff...
            if (uNbO) uNbO = fread(&einstein_ram_start, sizeof(einstein_ram_start), 1, handle);                        
            if (uNbO) uNbO = fread(&keyboard_w, sizeof(keyboard_w), 1, handle);                        
            if (uNbO) uNbO = fread(&key_int_mask, sizeof(key_int_mask), 1, handle);
            if (einstein_mode == 2)
            {
                if (uNbO) uNbO = fread(&FDC, sizeof(FDC), 1, handle);
            }

            if (uNbO) uNbO = fread(SGC_Bank, sizeof(SGC_Bank), 1, handle);
            if (uNbO) uNbO = fread(&SGC_EEPROM_State, sizeof(SGC_EEPROM_State), 1, handle);
            if (uNbO) uNbO = fread(&SGC_EEPROM_CmdPos, sizeof(SGC_EEPROM_CmdPos), 1, handle);

            // Load spare memory for future use
            if (uNbO) uNbO = fread(spare, 506,1, handle); 

            // Load VDP
            if (uNbO) uNbO = fread(VDP, sizeof(VDP),1, handle); 
            if (uNbO) uNbO = fread(&VDPCtrlLatch, sizeof(VDPCtrlLatch),1, handle); 
            if (uNbO) uNbO = fread(&VDPStatus, sizeof(VDPStatus),1, handle); 
            if (uNbO) uNbO = fread(&FGColor, sizeof(FGColor),1, handle); 
            if (uNbO) uNbO = fread(&BGColor, sizeof(BGColor),1, handle); 
            if (uNbO) uNbO = fread(&OH, sizeof(OH),1, handle); 
            if (uNbO) uNbO = fread(&IH, sizeof(IH),1, handle); 
            if (uNbO) uNbO = fread(&ScrMode, sizeof(ScrMode),1, handle); 
            extern void (*RefreshLine)(u8 uY);  RefreshLine = SCR[ScrMode].Refresh;
            if (uNbO) uNbO = fread(&VDPDlatch, sizeof(VDPDlatch),1, handle); 
            if (uNbO) uNbO = fread(&VAddr, sizeof(VAddr),1, handle); 
            if (uNbO) uNbO = fread(&CurLine, sizeof(CurLine),1, handle); 
            if (uNbO) uNbO = fread(&ColTabM, sizeof(ColTabM),1, handle); 
            if (uNbO) uNbO = fread(&ChrGenM, sizeof(ChrGenM),1, handle); 
            
            if (uNbO) uNbO = fread(pVDPVidMem, 0x4000,1, handle); 
            if (uNbO) uNbO = fread(&pSvg, sizeof(pSvg),1, handle); 
            ChrGen = pSvg + pVDPVidMem;
            if (uNbO) uNbO = fread(&pSvg, sizeof(pSvg),1, handle); 
            ChrTab = pSvg + pVDPVidMem;
            if (uNbO) uNbO = fread(&pSvg, sizeof(pSvg),1, handle); 
            ColTab = pSvg + pVDPVidMem;
            if (uNbO) uNbO = fread(&pSvg, sizeof(pSvg),1, handle); 
            SprGen = pSvg + pVDPVidMem;
            if (uNbO) uNbO = fread(&pSvg, sizeof(pSvg),1, handle); 
            SprTab = pSvg + pVDPVidMem;
            
            // Read PSG SN and AY sound chips...
            if (uNbO) uNbO = fread(&mySN, sizeof(mySN),1, handle); 
            if (uNbO) uNbO = fread(&myAY, sizeof(myAY),1, handle);
            
            // Load stuff for MSX, SordM5 and SG-1000
            if (uNbO) fread(&Port_PPI_A, sizeof(Port_PPI_A),1, handle);
            if (uNbO) fread(&Port_PPI_B, sizeof(Port_PPI_B),1, handle);
            if (uNbO) fread(&Port_PPI_C, sizeof(Port_PPI_C),1, handle);
            
            if (uNbO) fread(&mapperType, sizeof(mapperType),1, handle);
            if (uNbO) fread(&mapperMask, sizeof(mapperMask),1, handle);
            if (uNbO) fread(bROMInSlot, sizeof(bROMInSlot),1, handle);
            if (uNbO) fread(bRAMInSlot, sizeof(bRAMInSlot),1, handle);
            
            if (uNbO) fread(CTC, sizeof(CTC),1, handle);
            if (uNbO) fread(&vdp_int_source, sizeof(vdp_int_source),1, handle);
            
            // Various ports used in the system
            if (uNbO) fread(&Port53, sizeof(Port53),1, handle);
            if (uNbO) fread(&Port60, sizeof(Port60),1, handle);
            if (uNbO) fread(&Port20, sizeof(Port20),1, handle);
            if (uNbO) fread(&Port42, sizeof(Port42),1, handle);
            
		    if (einstein_mode)
		    {
	            if (uNbO) fread(&keyboard_interrupt, sizeof(keyboard_interrupt),1, handle);      
                if (uNbO) fread(&einstein_ram_start, sizeof(einstein_ram_start),1, handle);      
                if (uNbO) fread(&keyboard_w, sizeof(keyboard_w),1, handle);      
                if (uNbO) fread(&key_int_mask, sizeof(key_int_mask),1, handle);      
                if (uNbO) fread(&myKeyData, sizeof(myKeyData),1, handle);      
                if (uNbO) fread(&adc_mux, sizeof(adc_mux),1, handle);      
    		}
    		else if (msx_mode)   // Big enough that we will not write this if we are not MSX
            {
                if (uNbO) fread(Offsets, sizeof(Offsets),1, handle);
                for (u8 i=0; i<8; i++)
                {
                    if (Offsets[i].type == TYPE_ROM)
                    {
                        Slot1ROMPtr[i] = (u8 *) (ROM_Memory + Offsets[i].offset);
                    }
                    else
                    {
                        Slot1ROMPtr[i] = (u8 *) (Offsets[i].offset);
                    }
                }

                if (uNbO) fread(&msx_sram_at_8000, sizeof(msx_sram_at_8000),1, handle);
                if (msx_sram_enabled) if (uNbO) fread(SRAM_Memory, 0x4000,1, handle);    // No game uses more than 16K
                if (msx_scc_enable)
                {
                    if (uNbO) uNbO = fread(&mySCC, sizeof(mySCC),1, handle);
                }
            }
            else if (adam_mode)  // Big enough that we will not read this if we are not ADAM
            {
                if (uNbO) fread(PCBTable+0x8000, 0x8000, 1, handle);
                
                if (uNbO) fread(&PCBAddr, sizeof(PCBAddr),1, handle);
                if (uNbO) fread(adam_ram_present, sizeof(adam_ram_present),1, handle);                
                if (uNbO) fread(&KBDStatus, sizeof(KBDStatus),1, handle);
                if (uNbO) fread(&LastKey, sizeof(LastKey),1, handle);
                if (uNbO) fread(&adam_CapsLock, sizeof(adam_CapsLock),1, handle);
                if (uNbO) fread(&disk_unsaved_data, sizeof(disk_unsaved_data),1, handle);
                if (uNbO) fread(AdamDriveStatus, sizeof(AdamDriveStatus),1, handle);
                
                if (uNbO) fread(spare, 32,1, handle);                
                if (uNbO) fread(&adam_ext_ram_used, sizeof(adam_ext_ram_used),1, handle);
                
                if (adam_ext_ram_used)
                {
                    // This must be the last thing written so we can compress it...
                    u32 len = (DSI_RAM_Buffer ? (2*1024*1024) : (64 * 1024)) >> 2;
                    u32 *ptr = (DSI_RAM_Buffer ? ((u32*)DSI_RAM_Buffer) : ((u32 *)(EXP_Memory)));
                    for (u32 i=0; i<len; i++)
                    {
                        fread(ptr, 1, sizeof(u32), handle); // Read 32-bits at a time...
                        
                        u32 zero_count = 0;
                        if (*ptr == 0x00000000) // UnCompress zeros...
                        {
                            fread(&zero_count, 1, sizeof(u32), handle);
                        }
                        
                        if (zero_count)
                        {
                            for (u32 j=0; j<zero_count; j++)
                            {
                                if (i < len)
                                {
                                    *ptr++ = 0x00000000;
                                    i++;
                                }
                            }
                        }
                        else ptr++;
                    }
                }
            }
            else if (bActivisionPCB)
            {
                // Read the EEPROM and memory
                if (uNbO) fread(&EEPROM, sizeof(EEPROM),1, handle);      
            }
            else if (creativision_mode)
            {
                // Read some Creativision stuff
                u16 cv_cpu_size=1;
                u8 *mem = creativision_get_cpu(&cv_cpu_size);
                if (uNbO) fread(mem, cv_cpu_size,1, handle);
                creativision_put_cpu(mem);
            }
            
            // Fix up transparency
            if (BGColor)
            {
              u8 r = (u8) ((float) TMS9918A_palette[BGColor*3+0]*0.121568f);
              u8 g = (u8) ((float) TMS9918A_palette[BGColor*3+1]*0.121568f);
              u8 b = (u8) ((float) TMS9918A_palette[BGColor*3+2]*0.121568f);
              BG_PALETTE[0] = RGB15(r,g,b);
            }
            else
            {
               BG_PALETTE[0] = RGB15(0x00,0x00,0x00);
            }
            
            last_mega_bank = 199;   // Force load of bank if needed
            last_tape_pos = 9999;   // Force tape position to show
        }
        else uNbO = 0;
        
        if (uNbO) 
          strcpy(szCh1,"OK ");
        else
          strcpy(szCh1,"ERR");
         DSPrint(15,0,0,szCh1);
        
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(6,0,0,"             ");  
        DisplayStatusLine(true);
      }
      else
      {
        DSPrint(6,0,0,"NO SAVED GAME");
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(6,0,0,"             ");  
      }

    fclose(handle);
}


void colecoSaveEEPROM(void) 
{
    // Init filename = romname and EE in place of ROM
    DIR* dir = opendir("sav");
    if (dir) closedir(dir);  // Directory exists... close it out and move on.
    else mkdir("sav", 0777);   // Otherwise create the directory...
    sprintf(szLoadFile,"sav/%s", initial_file);

    int len = strlen(szLoadFile);
    szLoadFile[len-3] = 'e';
    szLoadFile[len-2] = 'e';
    szLoadFile[len-1] = 0;

    FILE *handle = fopen(szLoadFile, "wb+");  
    if (handle != NULL) 
    {
      fwrite(EEPROM.Data, Size24XX(&EEPROM), 1, handle);
      fclose(handle);
    }
}

void colecoLoadEEPROM(void)
{
    // Return to the original path
    chdir(initial_path);    
    
    // Init filename = romname and EE in place of ROM
    DIR* dir = opendir("sav");
    if (dir) closedir(dir);  // Directory exists... close it out and move on.
    else mkdir("sav", 0777);   // Otherwise create the directory...
    sprintf(szLoadFile,"sav/%s", initial_file);

    int len = strlen(szLoadFile);
    szLoadFile[len-3] = 'e';
    szLoadFile[len-2] = 'e';
    szLoadFile[len-1] = 0;

    FILE *handle = fopen(szLoadFile, "rb+");
    if (handle != NULL) 
    {
      fread(EEPROM.Data, Size24XX(&EEPROM), 1, handle);
      fclose(handle);
    }
    else
    {
      memset(EEPROM.Data, 0xFF, 0x8000);
    }
}


void msxSaveEEPROM(void)
{
    // Return to the original path
    chdir(initial_path);    

    // Init filename = romname and SRM (SRAM) in place of ROM
    DIR* dir = opendir("sav");
    if (dir) closedir(dir);  // Directory exists... close it out and move on.
    else mkdir("sav", 0777);   // Otherwise create the directory...
    sprintf(szLoadFile,"sav/%s", initial_file);

    int len = strlen(szLoadFile);
    szLoadFile[len-3] = 's';
    szLoadFile[len-2] = 'r';
    szLoadFile[len-1] = 'm';
    szLoadFile[len-0] = 0;

    FILE *handle = fopen(szLoadFile, "wb+");  
    if (handle != NULL) 
    {
      fwrite(SRAM_Memory, 0x4000, 1, handle);
      fclose(handle);
    }
}

void msxLoadEEPROM(void)
{
    // Return to the original path
    chdir(initial_path);    

    // Init filename = romname and SRM (SRAM) in place of ROM
    DIR* dir = opendir("sav");
    if (dir) closedir(dir);  // Directory exists... close it out and move on.
    else mkdir("sav", 0777);   // Otherwise create the directory...
    sprintf(szLoadFile,"sav/%s", initial_file);

    int len = strlen(szLoadFile);
    szLoadFile[len-3] = 's';
    szLoadFile[len-2] = 'r';
    szLoadFile[len-1] = 'm';
    szLoadFile[len-0] = 0;

    FILE *handle = fopen(szLoadFile, "rb+");
    if (handle != NULL) 
    {
      fread(SRAM_Memory, 0x4000, 1, handle);
      fclose(handle);
    }
    else
    {
      memset(EEPROM.Data, 0xFF, 0x8000);
    }
}


// End of file
