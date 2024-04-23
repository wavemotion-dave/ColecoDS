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

#define COLECODS_SAVE_VER   0x001E        // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.


// -----------------------------------------------------------------------------------------------------
// Since the main MemoryMap[] can point to differt things (RAM, ROM, BIOS, etc) and since we can't rely
// on the memory being in the same spot on subsequent versions of the emulator... we need to save off
// the type and the offset so that we can patch it back together when we load back a saved state.
// -----------------------------------------------------------------------------------------------------
struct RomOffset
{
    u8   type;
    u32  offset;
};

struct RomOffset Offsets[8];

#define TYPE_ROM   0
#define TYPE_RAM   1
#define TYPE_BIOS  2
#define TYPE_EXP   3
#define TYPE_FDC   4
#define TYPE_OTHER 5

/*********************************************************************************
 * Save the current state - save everything we need to a single .sav file.
 ********************************************************************************/
u8  spare[512] = {0x00};            // We keep some spare bytes so we can use them in the future without changing the structure

static char szLoadFile[256];        // We build the filename out of the base filename and tack on .sav, .ee, etc.
static char tmpStr[32];

void colecoSaveState() 
{
  size_t retVal;
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
  strcpy(tmpStr,"SAVING...");
  DSPrint(6,0,0,tmpStr);
  
  FILE *handle = fopen(szLoadFile, "wb+");  
  if (handle != NULL) 
  {
    // Write Version
    u16 save_ver = COLECODS_SAVE_VER;
    retVal = fwrite(&save_ver, sizeof(u16), 1, handle);
      
    // Write CZ80 CPU
    retVal = fwrite(&CPU, sizeof(CPU), 1, handle);
      
    // Save Z80 Memory (yes, all of it!)
    if (retVal) retVal = fwrite(RAM_Memory, 0x10000,1, handle); 
      
    // And the Memory Map - we must only save offsets so that this is generic when we change code and memory shifts...
    for (u8 i=0; i<8; i++)
    {
        if ((MemoryMap[i] >= ROM_Memory) && (MemoryMap[i] < ROM_Memory+(sizeof(ROM_Memory))))
        {
            Offsets[i].type = TYPE_ROM;
            Offsets[i].offset = MemoryMap[i] - ROM_Memory;
        }
        else if ((MemoryMap[i] >= fastdrom_cdx2) && (MemoryMap[i] < fastdrom_cdx2+(sizeof(fastdrom_cdx2))))
        {
            Offsets[i].type = TYPE_FDC;
            Offsets[i].offset = MemoryMap[i] - fastdrom_cdx2;
        }        
        else if ((MemoryMap[i] >= RAM_Memory) && (MemoryMap[i] < RAM_Memory+(sizeof(RAM_Memory))))
        {
            Offsets[i].type = TYPE_RAM;
            Offsets[i].offset = MemoryMap[i] - RAM_Memory;
        }
        else if ((DSI_RAM_Buffer != 0) && (MemoryMap[i] >= DSI_RAM_Buffer) && (MemoryMap[i] < DSI_RAM_Buffer+(2*1024*1024)))
        {
            Offsets[i].type = TYPE_EXP;
            Offsets[i].offset = MemoryMap[i] - DSI_RAM_Buffer;
        }
        else if ((MemoryMap[i] >= BIOS_Memory) && (MemoryMap[i] < BIOS_Memory+(sizeof(BIOS_Memory))))
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
    if (retVal) retVal = fwrite(Offsets, sizeof(Offsets),1, handle);     
    
    // Write VDP
    if (retVal) retVal = fwrite(VDP, sizeof(VDP),1, handle); 
    if (retVal) retVal = fwrite(&VDPCtrlLatch, sizeof(VDPCtrlLatch),1, handle); 
    if (retVal) retVal = fwrite(&VDPStatus, sizeof(VDPStatus),1, handle); 
    if (retVal) retVal = fwrite(&FGColor, sizeof(FGColor),1, handle); 
    if (retVal) retVal = fwrite(&BGColor, sizeof(BGColor),1, handle); 
    if (retVal) retVal = fwrite(&OH, sizeof(OH),1, handle); 
    if (retVal) retVal = fwrite(&IH, sizeof(IH),1, handle);       
    if (retVal) retVal = fwrite(&ScrMode, sizeof(ScrMode),1, handle); 
    if (retVal) retVal = fwrite(&VDPDlatch, sizeof(VDPDlatch),1, handle); 
    if (retVal) retVal = fwrite(&VAddr, sizeof(VAddr),1, handle); 
    if (retVal) retVal = fwrite(&CurLine, sizeof(CurLine),1, handle); 
    if (retVal) retVal = fwrite(&ColTabM, sizeof(ColTabM),1, handle); 
    if (retVal) retVal = fwrite(&ChrGenM, sizeof(ChrGenM),1, handle); 
    if (retVal) retVal = fwrite(pVDPVidMem, 0x4000,1, handle); 
    pSvg = ChrGen-pVDPVidMem;
    if (retVal) retVal = fwrite(&pSvg, sizeof(pSvg),1, handle); 
    pSvg = ChrTab-pVDPVidMem;
    if (retVal) retVal = fwrite(&pSvg, sizeof(pSvg),1, handle); 
    pSvg = ColTab-pVDPVidMem;
    if (retVal) retVal = fwrite(&pSvg, sizeof(pSvg),1, handle); 
    pSvg = SprGen-pVDPVidMem;
    if (retVal) retVal = fwrite(&pSvg, sizeof(pSvg),1, handle); 
    pSvg = SprTab-pVDPVidMem;
    if (retVal) retVal = fwrite(&pSvg, sizeof(pSvg),1, handle); 

    // Write PSG SN and AY sound chips...
    if (retVal) retVal = fwrite(&mySN, sizeof(mySN),1, handle);
    if (retVal) retVal = fwrite(&myAY, sizeof(myAY),1, handle);    
      
    // Write the Super Game Module stuff
    if (retVal) retVal = fwrite(&sgm_enable, sizeof(sgm_enable), 1, handle); 
    if (retVal) retVal = fwrite(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
      
    // A few frame counters
    if (retVal) retVal = fwrite(&emuActFrames, sizeof(emuActFrames), 1, handle); 
    if (retVal) retVal = fwrite(&timingFrames, sizeof(timingFrames), 1, handle); 

    // Some Cassette Related stuff...    
    if (retVal) retVal = fwrite(&tape_pos, sizeof(tape_pos), 1, handle); 
    if (retVal) retVal = fwrite(&tape_len, sizeof(tape_len), 1, handle); 
    if (retVal) retVal = fwrite(&last_tape_pos, sizeof(last_tape_pos), 1, handle); 
      
    // Some Memotech MTX stuff...
    if (retVal) retVal = fwrite(&memotech_RAM_start, sizeof(memotech_RAM_start), 1, handle); 
    if (retVal) retVal = fwrite(&IOBYTE, sizeof(IOBYTE), 1, handle); 
    if (retVal) retVal = fwrite(&MTX_KBD_DRIVE, sizeof(MTX_KBD_DRIVE), 1, handle); 
    if (retVal) retVal = fwrite(&lastIOBYTE, sizeof(lastIOBYTE), 1, handle);      
    if (retVal) retVal = fwrite(&memotech_magrom_present, sizeof(memotech_magrom_present), 1, handle); 
    if (retVal) retVal = fwrite(&memotech_mtx_500_only, sizeof(memotech_mtx_500_only), 1, handle); 
    if (retVal) retVal = fwrite(&memotech_lastMagROMPage, sizeof(memotech_lastMagROMPage), 1, handle);       
      
    // Some SVI stuff...
    if (retVal) retVal = fwrite(&svi_RAM, sizeof(svi_RAM), 1, handle); 
      
    // Some SG-1000 / SC-3000 stuff...
    if (retVal) retVal = fwrite(&Port_PPI_CTRL, sizeof(Port_PPI_CTRL), 1, handle);       
    if (retVal) retVal = fwrite(&OldPortC, sizeof(OldPortC), 1, handle);                        

    // And a few things for the Super Game Cart
    if (retVal) retVal = fwrite(SGC_Bank, sizeof(SGC_Bank), 1, handle);
    if (retVal) retVal = fwrite(&SGC_EEPROM_State, sizeof(SGC_EEPROM_State), 1, handle);
    if (retVal) retVal = fwrite(&SGC_EEPROM_CmdPos, sizeof(SGC_EEPROM_CmdPos), 1, handle);
    
    // Write stuff for MSX, SordM5 and SG-1000
    if (retVal) retVal = fwrite(&Port_PPI_A, sizeof(Port_PPI_A),1, handle);
    if (retVal) retVal = fwrite(&Port_PPI_B, sizeof(Port_PPI_B),1, handle);
    if (retVal) retVal = fwrite(&Port_PPI_C, sizeof(Port_PPI_C),1, handle);
    
    if (retVal) retVal = fwrite(&mapperType, sizeof(mapperType),1, handle);
    if (retVal) retVal = fwrite(&mapperMask, sizeof(mapperMask),1, handle);
    if (retVal) retVal = fwrite(bROMInSegment, sizeof(bROMInSegment),1, handle);
    if (retVal) retVal = fwrite(bRAMInSegment, sizeof(bRAMInSegment),1, handle);
    
    // Some systems utilize the Z80 CTC 
    if (retVal) retVal = fwrite(CTC, sizeof(CTC),1, handle);
    if (retVal) retVal = fwrite(&vdp_int_source, sizeof(vdp_int_source),1, handle);
    
    // Various ports used in the system
    if (retVal) retVal = fwrite(&Port53, sizeof(Port53),1, handle);
    if (retVal) retVal = fwrite(&Port60, sizeof(Port60),1, handle);
    if (retVal) retVal = fwrite(&Port20, sizeof(Port20),1, handle);
    if (retVal) retVal = fwrite(&Port42, sizeof(Port42),1, handle);
    
    // Some spare memory we can eat into...
    if (retVal) retVal = fwrite(spare, 512, 1, handle);
      
    if (einstein_mode) // Big enough that we will not write this if we are not Einstein
    {
        if (retVal) retVal = fwrite(&keyboard_interrupt, sizeof(keyboard_interrupt),1, handle);      
        if (retVal) retVal = fwrite(&einstein_ram_start, sizeof(einstein_ram_start),1, handle);      
        if (retVal) retVal = fwrite(&keyboard_w, sizeof(keyboard_w),1, handle);      
        if (retVal) retVal = fwrite(&key_int_mask, sizeof(key_int_mask),1, handle);      
        if (retVal) retVal = fwrite(&myKeyData, sizeof(myKeyData),1, handle);      
        if (retVal) retVal = fwrite(&adc_mux, sizeof(adc_mux),1, handle);      
        if (retVal) retVal = fwrite(&FDC, sizeof(FDC), 1, handle);
    }
    else if (msx_mode)   // Big enough that we will not write this if we are not MSX 
    {
        if (retVal) retVal = fwrite(&msx_last_rom_size, sizeof(msx_last_rom_size),1, handle);
        
        // We need to save off the MSX Cart offsets so we can restore them properly...
        for (u8 i=0; i<8; i++)
        {
            if ((MSXCartPtr[i] >= ROM_Memory) && (MSXCartPtr[i] < ROM_Memory+(sizeof(ROM_Memory))))
            {
                Offsets[i].type = TYPE_ROM;
                Offsets[i].offset = MSXCartPtr[i] - ROM_Memory;
            }
            else
            {
                Offsets[i].type = TYPE_OTHER;
                Offsets[i].offset =  (u32)MSXCartPtr[i];
            }
        }
        if (retVal) retVal = fwrite(Offsets, sizeof(Offsets),1, handle);
        
        if (retVal) retVal = fwrite(&msx_sram_at_8000, sizeof(msx_sram_at_8000),1, handle);
        if (msx_sram_enabled) if (retVal) retVal = fwrite(SRAM_Memory, 0x4000,1, handle);    // No game uses more than 16K
        if (msx_scc_enable)   if (retVal) retVal = fwrite(&mySCC, sizeof(mySCC),1, handle);
        if (msx_mode == 3)    if (retVal) retVal = fwrite(&FDC, sizeof(FDC), 1, handle);
    }
    else if (adam_mode)  // Big enough that we will not write this if we are not ADAM
    {
        if (retVal) retVal = fwrite(PCBTable+0x8000, 0x8000, 1, handle);
        
        if (retVal) retVal = fwrite(&PCBAddr, sizeof(PCBAddr),1, handle);        
        if (retVal) retVal = fwrite(adam_ram_present, sizeof(adam_ram_present),1, handle);        
        if (retVal) retVal = fwrite(&KBDStatus, sizeof(KBDStatus),1, handle);
        if (retVal) retVal = fwrite(&LastKey, sizeof(LastKey),1, handle);
        if (retVal) retVal = fwrite(&adam_CapsLock, sizeof(adam_CapsLock),1, handle);        
        if (retVal) retVal = fwrite(&disk_unsaved_data, sizeof(disk_unsaved_data),1, handle);        
        if (retVal) retVal = fwrite(AdamDriveStatus, sizeof(AdamDriveStatus),1, handle);
        
        if (retVal) retVal = fwrite(spare, 32,1, handle);        
        if (retVal) retVal = fwrite(&adam_ext_ram_used, sizeof(adam_ext_ram_used),1, handle);
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
        if (retVal) retVal = fwrite(&EEPROM, sizeof(EEPROM),1, handle);      
    }
    else if (creativision_mode)
    {
        // Write some creativision stuff... mostly the non-Z80 CPU
        u16 cv_cpu_size=1;
        u8 *mem = creativision_get_cpu(&cv_cpu_size);
        if (retVal) retVal = fwrite(mem, cv_cpu_size,1, handle);
    }
    
    strcpy(tmpStr, (retVal ? "OK ":"ERR"));  
    DSPrint(15,0,0,tmpStr);
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    DSPrint(6,0,0,"             "); 
    DisplayStatusLine(true);
  }
  else {
    strcpy(tmpStr,"Error opening SAV file ...");
  }
  fclose(handle);
}


/*********************************************************************************
 * Load the current state - read everything back from the .sav file.
 ********************************************************************************/
void colecoLoadState() 
{
    size_t retVal;
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
         strcpy(tmpStr,"LOADING...");
         DSPrint(6,0,0,tmpStr);
       
        // Read Version
        u16 save_ver = 0xBEEF;
        retVal = fread(&save_ver, sizeof(u16), 1, handle);
        
        if (save_ver == COLECODS_SAVE_VER)
        {
            // Load CZ80 CPU
            retVal = fread(&CPU, sizeof(CPU), 1, handle);
       
            // Load Z80 Memory (yes, all of it!)
            if (retVal) retVal = fread(RAM_Memory, 0x10000,1, handle); 
            
            // Load back the Memory Map - these were saved as offsets so we must reconstruct actual pointers
            if (retVal) retVal = fread(Offsets, sizeof(Offsets),1, handle);     
            for (u8 i=0; i<8; i++)
            {
                if (Offsets[i].type == TYPE_ROM)
                {
                    MemoryMap[i] = (u8 *) (ROM_Memory + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_FDC)
                {
                    MemoryMap[i] = (u8 *) (fastdrom_cdx2 + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_RAM)
                {
                    MemoryMap[i] = (u8 *) (RAM_Memory + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_EXP)
                {
                    MemoryMap[i] = (u8 *) (DSI_RAM_Buffer + Offsets[i].offset);
                }
                else if (Offsets[i].type == TYPE_BIOS)
                {
                    MemoryMap[i] = (u8 *) (BIOS_Memory + Offsets[i].offset);
                }
                else // TYPE_OTHER - this is just a pointer to memory
                {
                    MemoryMap[i] = (u8 *) (Offsets[i].offset);
                }
            }
            
            // Load VDP
            if (retVal) retVal = fread(VDP, sizeof(VDP),1, handle); 
            if (retVal) retVal = fread(&VDPCtrlLatch, sizeof(VDPCtrlLatch),1, handle); 
            if (retVal) retVal = fread(&VDPStatus, sizeof(VDPStatus),1, handle); 
            if (retVal) retVal = fread(&FGColor, sizeof(FGColor),1, handle); 
            if (retVal) retVal = fread(&BGColor, sizeof(BGColor),1, handle); 
            if (retVal) retVal = fread(&OH, sizeof(OH),1, handle); 
            if (retVal) retVal = fread(&IH, sizeof(IH),1, handle); 
            if (retVal) retVal = fread(&ScrMode, sizeof(ScrMode),1, handle); 
            extern void (*RefreshLine)(u8 uY);  RefreshLine = SCR[ScrMode].Refresh;
            if (retVal) retVal = fread(&VDPDlatch, sizeof(VDPDlatch),1, handle); 
            if (retVal) retVal = fread(&VAddr, sizeof(VAddr),1, handle); 
            if (retVal) retVal = fread(&CurLine, sizeof(CurLine),1, handle); 
            if (retVal) retVal = fread(&ColTabM, sizeof(ColTabM),1, handle); 
            if (retVal) retVal = fread(&ChrGenM, sizeof(ChrGenM),1, handle); 
            
            if (retVal) retVal = fread(pVDPVidMem, 0x4000,1, handle); 
            if (retVal) retVal = fread(&pSvg, sizeof(pSvg),1, handle); 
            ChrGen = pSvg + pVDPVidMem;
            if (retVal) retVal = fread(&pSvg, sizeof(pSvg),1, handle); 
            ChrTab = pSvg + pVDPVidMem;
            if (retVal) retVal = fread(&pSvg, sizeof(pSvg),1, handle); 
            ColTab = pSvg + pVDPVidMem;
            if (retVal) retVal = fread(&pSvg, sizeof(pSvg),1, handle); 
            SprGen = pSvg + pVDPVidMem;
            if (retVal) retVal = fread(&pSvg, sizeof(pSvg),1, handle); 
            SprTab = pSvg + pVDPVidMem;
            
            // Read PSG SN and AY sound chips...
            if (retVal) retVal = fread(&mySN, sizeof(mySN),1, handle); 
            if (retVal) retVal = fread(&myAY, sizeof(myAY),1, handle); 
                       
            // Load the Super Game Module stuff
            if (retVal) retVal = fread(&sgm_enable, sizeof(sgm_enable), 1, handle); 
            if (retVal) retVal = fread(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
            
            // A few frame counters
            if (retVal) retVal = fread(&emuActFrames, sizeof(emuActFrames), 1, handle); 
            if (retVal) retVal = fread(&timingFrames, sizeof(timingFrames), 1, handle); 

            // Some Cassette Related stuff...
            if (retVal) retVal = fread(&tape_pos, sizeof(tape_pos), 1, handle); 
            if (retVal) retVal = fread(&tape_len, sizeof(tape_len), 1, handle); 
            if (retVal) retVal = fread(&last_tape_pos, sizeof(last_tape_pos), 1, handle); 
            
            // Some Memotech MTX stuff...
            if (retVal) retVal = fread(&memotech_RAM_start, sizeof(memotech_RAM_start), 1, handle); 
            if (retVal) retVal = fread(&IOBYTE, sizeof(IOBYTE), 1, handle); 
            if (retVal) retVal = fread(&MTX_KBD_DRIVE, sizeof(MTX_KBD_DRIVE), 1, handle); 
            if (retVal) retVal = fread(&lastIOBYTE, sizeof(lastIOBYTE), 1, handle); 
            if (retVal) retVal = fread(&memotech_magrom_present, sizeof(memotech_magrom_present), 1, handle); 
            if (retVal) retVal = fread(&memotech_mtx_500_only, sizeof(memotech_mtx_500_only), 1, handle); 
            if (retVal) retVal = fread(&memotech_lastMagROMPage, sizeof(memotech_lastMagROMPage), 1, handle); 
            
            // Some SVI stuff...
            if (retVal) retVal = fread(&svi_RAM, sizeof(svi_RAM), 1, handle); 
            
            // Some SG-1000 / SC-3000 stuff...
            if (retVal) retVal = fread(&Port_PPI_CTRL, sizeof(Port_PPI_CTRL), 1, handle);       
            if (retVal) retVal = fread(&OldPortC, sizeof(OldPortC), 1, handle);                  
            
            // And a few things for the Super Game Cart
            if (retVal) retVal = fread(SGC_Bank, sizeof(SGC_Bank), 1, handle);
            if (retVal) retVal = fread(&SGC_EEPROM_State, sizeof(SGC_EEPROM_State), 1, handle);
            if (retVal) retVal = fread(&SGC_EEPROM_CmdPos, sizeof(SGC_EEPROM_CmdPos), 1, handle);

            // Load stuff for MSX, SordM5 and SG-1000
            if (retVal) retVal = fread(&Port_PPI_A, sizeof(Port_PPI_A),1, handle);
            if (retVal) retVal = fread(&Port_PPI_B, sizeof(Port_PPI_B),1, handle);
            if (retVal) retVal = fread(&Port_PPI_C, sizeof(Port_PPI_C),1, handle);
            
            if (retVal) retVal = fread(&mapperType, sizeof(mapperType),1, handle);
            if (retVal) retVal = fread(&mapperMask, sizeof(mapperMask),1, handle);
            if (retVal) retVal = fread(bROMInSegment, sizeof(bROMInSegment),1, handle);
            if (retVal) retVal = fread(bRAMInSegment, sizeof(bRAMInSegment),1, handle);
            
            // Some systems utilize the Z80 CTC 
            if (retVal) retVal = fread(CTC, sizeof(CTC),1, handle);
            if (retVal) retVal = fread(&vdp_int_source, sizeof(vdp_int_source),1, handle);
            
            // Various ports used in the system
            if (retVal) retVal = fread(&Port53, sizeof(Port53),1, handle);
            if (retVal) retVal = fread(&Port60, sizeof(Port60),1, handle);
            if (retVal) retVal = fread(&Port20, sizeof(Port20),1, handle);
            if (retVal) retVal = fread(&Port42, sizeof(Port42),1, handle);

            // Some spare memory we can eat into...
            if (retVal) retVal = fread(spare, 512, 1, handle); 
            
		    if (einstein_mode) // Big enough that we will not write this if we are not Einstein
		    {
	            if (retVal) retVal = fread(&keyboard_interrupt, sizeof(keyboard_interrupt),1, handle);      
                if (retVal) retVal = fread(&einstein_ram_start, sizeof(einstein_ram_start),1, handle);      
                if (retVal) retVal = fread(&keyboard_w, sizeof(keyboard_w),1, handle);      
                if (retVal) retVal = fread(&key_int_mask, sizeof(key_int_mask),1, handle);      
                if (retVal) retVal = fread(&myKeyData, sizeof(myKeyData),1, handle);      
                if (retVal) retVal = fread(&adc_mux, sizeof(adc_mux),1, handle);      
                if (retVal) retVal = fread(&FDC, sizeof(FDC), 1, handle);
    		}
    		else if (msx_mode)   // Big enough that we will not write this if we are not MSX
            {
                if (retVal) retVal = fread(&msx_last_rom_size, sizeof(msx_last_rom_size),1, handle);
                
                if (retVal) retVal = fread(Offsets, sizeof(Offsets),1, handle);
                for (u8 i=0; i<8; i++)
                {
                    if (Offsets[i].type == TYPE_ROM)
                    {
                        MSXCartPtr[i] = (u8 *) (ROM_Memory + Offsets[i].offset);
                    }
                    else
                    {
                        MSXCartPtr[i] = (u8 *) (Offsets[i].offset);
                    }
                }
                
                if (retVal) retVal = fread(&msx_sram_at_8000, sizeof(msx_sram_at_8000),1, handle);
                if (msx_sram_enabled) if (retVal) retVal = fread(SRAM_Memory, 0x4000,1, handle);    // No game uses more than 16K
                if (msx_scc_enable)   if (retVal) retVal = fread(&mySCC, sizeof(mySCC),1, handle);
                if (msx_mode == 3)    if (retVal) retVal = fread(&FDC, sizeof(FDC), 1, handle);
                
                msx_caps_lock = ((Port_PPI_C & 0x40) ? 0:1); // Set the caps lock state back to what it should be based on Port C
                msx_last_block[0] = msx_last_block[1] = msx_last_block[2] = msx_last_block[3] = 199; // Ensure bank swaps always happen after a restore                
            }
            else if (adam_mode)  // Big enough that we will not read this if we are not ADAM
            {
                if (retVal) retVal = fread(PCBTable+0x8000, 0x8000, 1, handle);
                
                if (retVal) retVal = fread(&PCBAddr, sizeof(PCBAddr),1, handle);
                if (retVal) retVal = fread(adam_ram_present, sizeof(adam_ram_present),1, handle);                
                if (retVal) retVal = fread(&KBDStatus, sizeof(KBDStatus),1, handle);
                if (retVal) retVal = fread(&LastKey, sizeof(LastKey),1, handle);
                if (retVal) retVal = fread(&adam_CapsLock, sizeof(adam_CapsLock),1, handle);
                if (retVal) retVal = fread(&disk_unsaved_data, sizeof(disk_unsaved_data),1, handle);
                if (retVal) retVal = fread(AdamDriveStatus, sizeof(AdamDriveStatus),1, handle);
                
                if (retVal) retVal = fread(spare, 32,1, handle);                
                if (retVal) retVal = fread(&adam_ext_ram_used, sizeof(adam_ext_ram_used),1, handle);
                
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
                if (retVal) retVal = fread(&EEPROM, sizeof(EEPROM),1, handle);      
            }
            else if (creativision_mode)
            {
                // Read some Creativision stuff... mostly the non-Z80 CPU
                u16 cv_cpu_size=1;
                u8 *mem = creativision_get_cpu(&cv_cpu_size);
                if (retVal) retVal = fread(mem, cv_cpu_size,1, handle);
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
        else retVal = 0;
        
        strcpy(tmpStr, (retVal ? "OK ":"ERR"));
        DSPrint(15,0,0,tmpStr);
        
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

// End of file
