// =====================================================================================
// Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, it's source code and associated 
// readme files, with or without modification, are permitted in any medium without 
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty.
// =====================================================================================
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>

#include "colecoDS.h"
#include "AdamNet.h"
#include "FDIDisk.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "MTX_BIOS.h"
#define NORAM 0xFF

#define COLECODS_SAVE_VER 0x0011        // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.


/*********************************************************************************
 * Save the current state - save everything we need to a single .sav file.
 ********************************************************************************/
u8  spare[495] = {0x00};    // We keep some spare bytes so we can use them in the future without changing the structure
void colecoSaveState() 
{
  u32 uNbO;
  long pSvg;
  char szFile[128];
  char szCh1[32];
    
  // Init filename = romname and STA in place of ROM
  strcpy(szFile,gpFic[ucGameAct].szName);
  szFile[strlen(szFile)-3] = 's';
  szFile[strlen(szFile)-2] = 'a';
  szFile[strlen(szFile)-1] = 'v';
  strcpy(szCh1,"SAVING...");
  AffChaine(6,0,0,szCh1);
  
  FILE *handle = fopen(szFile, "wb+");  
  if (handle != NULL) 
  {
    // Write Version
    u16 save_ver = COLECODS_SAVE_VER;
    uNbO = fwrite(&save_ver, sizeof(u16), 1, handle);
      
    // Write DrZ80 CPU
    uNbO = fwrite(&drz80, sizeof(struct DrZ80), 1, handle);
      
    // Write CZ80 CPU
    uNbO = fwrite(&CPU, sizeof(CPU), 1, handle);
      
    // Need to save the DrZ80 SP/PC offsets as memory might shift on next load...
    u32 z80SPOffset = (u32) (drz80.Z80SP - drz80.Z80SP_BASE);
    if (uNbO) uNbO = fwrite(&z80SPOffset, sizeof(z80SPOffset),1, handle);

    u32 z80PCOffset = (u32) (drz80.Z80PC - drz80.Z80PC_BASE);
    if (uNbO) uNbO = fwrite(&z80PCOffset, sizeof(z80PCOffset),1, handle);

    // Save Coleco Memory (yes, all of it!)
    if (uNbO) uNbO = fwrite(pColecoMem, 0x10000,1, handle); 
      
    // Write XBuf Video Buffer (yes all of it!)
    if (uNbO) uNbO = fwrite(XBuf, sizeof(XBuf),1, handle); 
      
    // Write look-up-table
    if (uNbO) uNbO = fwrite(lutTablehh, 16*1024,1, handle);      

    // Write the Super Game Module and AY sound core 
    if (uNbO) uNbO = fwrite(ay_reg, 16, 1, handle);      
    if (uNbO) uNbO = fwrite(&sgm_enable, sizeof(sgm_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&ay_reg_idx, sizeof(ay_reg_idx), 1, handle); 
    if (uNbO) uNbO = fwrite(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
    if (uNbO) uNbO = fwrite(&AY_EnvelopeOn, sizeof(AY_EnvelopeOn), 1, handle); 
    if (uNbO) uNbO = fwrite(&envelope_period, sizeof(envelope_period), 1, handle); 
    if (uNbO) uNbO = fwrite(&envelope_counter, sizeof(envelope_counter), 1, handle); 
    if (uNbO) uNbO = fwrite(&noise_period, sizeof(noise_period), 1, handle); 
    if (uNbO) uNbO = fwrite(&a_idx, sizeof(a_idx), 1, handle); 
    if (uNbO) uNbO = fwrite(&b_idx, sizeof(b_idx), 1, handle); 
    if (uNbO) uNbO = fwrite(&c_idx, sizeof(c_idx), 1, handle); 
      
    // A few frame counters
    if (uNbO) uNbO = fwrite(&emuActFrames, sizeof(emuActFrames), 1, handle); 
    if (uNbO) uNbO = fwrite(&timingFrames, sizeof(timingFrames), 1, handle); 
      
    // Deficit Z80 CPU Cycle counter
    if (uNbO) uNbO = fwrite(&cycle_deficit, sizeof(cycle_deficit), 1, handle); 
      
    // Some Memotech MTX stuff...
    if (uNbO) uNbO = fwrite(&memotech_RAM_start, sizeof(memotech_RAM_start), 1, handle); 
    if (uNbO) uNbO = fwrite(&IOBYTE, sizeof(IOBYTE), 1, handle); 
    if (uNbO) uNbO = fwrite(&MTX_KBD_DRIVE, sizeof(MTX_KBD_DRIVE), 1, handle); 
    if (uNbO) uNbO = fwrite(&lastIOBYTE, sizeof(lastIOBYTE), 1, handle); 
    if (uNbO) uNbO = fwrite(&tape_pos, sizeof(tape_pos), 1, handle); 
    if (uNbO) uNbO = fwrite(&tape_len, sizeof(tape_len), 1, handle); 
    if (uNbO) uNbO = fwrite(&last_tape_pos, sizeof(last_tape_pos), 1, handle); 
      
    // Some SVI stuff...
    if (uNbO) uNbO = fwrite(&svi_RAM_start, sizeof(svi_RAM_start), 1, handle); 
      
    // Some spare memory we can eat into...
    if (uNbO) uNbO = fwrite(&spare, sizeof(spare),1, handle); 
      
    // Write VDP
    if (uNbO) uNbO = fwrite(VDP, sizeof(VDP),1, handle); 
    if (uNbO) uNbO = fwrite(&VDPCtrlLatch, sizeof(VDPCtrlLatch),1, handle); 
    if (uNbO) uNbO = fwrite(&VDPStatus, sizeof(VDPStatus),1, handle); 
    if (uNbO) uNbO = fwrite(&FGColor, sizeof(FGColor),1, handle); 
    if (uNbO) uNbO = fwrite(&BGColor, sizeof(BGColor),1, handle); 
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

    // Write PSG sound chips...
    if (uNbO) uNbO = fwrite(&sncol, sizeof(sncol),1, handle); 
    if (uNbO) uNbO = fwrite(&aycol, sizeof(aycol),1, handle);       
      
    // Write the SGM low memory
    if (uNbO) fwrite(sgm_low_mem, 0x2000,1, handle);      
      
    // Write stuff for MSX, SordM5 and SG-1000
    if (uNbO) fwrite(&mapperType, sizeof(mapperType),1, handle);
    if (uNbO) fwrite(&mapperMask, sizeof(mapperMask),1, handle);
    if (uNbO) fwrite(bROMInSlot, sizeof(bROMInSlot),1, handle);
    if (uNbO) fwrite(bRAMInSlot, sizeof(bRAMInSlot),1, handle);
    if (uNbO) fwrite(Slot1ROMPtr, sizeof(Slot1ROMPtr),1, handle);
    if (uNbO) fwrite(&PortA8, sizeof(PortA8),1, handle);
    if (uNbO) fwrite(&PortA9, sizeof(PortA9),1, handle);
    if (uNbO) fwrite(&PortAA, sizeof(PortAA),1, handle);
    if (uNbO) fwrite(&Port53, sizeof(Port53),1, handle);
    if (uNbO) fwrite(&Port60, sizeof(Port60),1, handle);
      
    if (uNbO) fwrite(ctc_control, sizeof(ctc_control),1, handle);
    if (uNbO) fwrite(ctc_time, sizeof(ctc_time),1, handle);
    if (uNbO) fwrite(ctc_timer, sizeof(ctc_timer),1, handle);
    if (uNbO) fwrite(ctc_vector, sizeof(ctc_vector),1, handle);
    if (uNbO) fwrite(ctc_latch, sizeof(ctc_latch),1, handle);
    if (uNbO) fwrite(&sordm5_irq, sizeof(sordm5_irq),1, handle);
      
    if (msx_mode)   // Big enough that we will not write this if we are not MSX
    {
        memcpy(Slot3RAM, Slot3RAMPtr, 0x10000);
        if (uNbO) fwrite(Slot3RAM, 0x10000,1, handle);
        if (uNbO) fwrite(msx_SRAM, 0x4000,1, handle);
        if (uNbO) fwrite(&msx_sram_at_8000, sizeof(msx_sram_at_8000),1, handle);
    }
      
    if (adam_mode)  // Big enough that we will not write this if we are not ADAM
    {
        if (uNbO) fwrite(AdamRAM, 0x20000,1, handle);
        if (uNbO) fwrite(PCBTable, 0x10000,1, handle);
        if (uNbO) fwrite(HoldingBuf, 0x4000,1, handle);
        if (uNbO) fwrite(Tapes, sizeof(Tapes),1, handle);
        if (uNbO) fwrite(Disks, sizeof(Disks),1, handle);
        if (uNbO) fwrite(&PCBAddr, sizeof(PCBAddr),1, handle);
        
        if (uNbO) fwrite(&Port20, sizeof(Port20),1, handle);
        if (uNbO) fwrite(&adam_ram_lo, sizeof(adam_ram_lo),1, handle);
        if (uNbO) fwrite(&adam_ram_hi, sizeof(adam_ram_hi),1, handle);
        if (uNbO) fwrite(&adam_ram_lo_exp, sizeof(adam_ram_lo_exp),1, handle);
        if (uNbO) fwrite(&adam_ram_hi_exp, sizeof(adam_ram_hi_exp),1, handle);
        if (uNbO) fwrite(&DiskID, sizeof(DiskID),1, handle);
        if (uNbO) fwrite(&KBDStatus, sizeof(KBDStatus),1, handle);
        if (uNbO) fwrite(&LastKey, sizeof(LastKey),1, handle);
        if (uNbO) fwrite(&io_busy, sizeof(io_busy),1, handle);
        if (uNbO) fwrite(&savedBUF, sizeof(savedBUF),1, handle);
        if (uNbO) fwrite(&savedLEN, sizeof(savedLEN),1, handle);
        if (uNbO) fwrite(&adam_CapsLock, sizeof(adam_CapsLock),1, handle);
        if (uNbO) fwrite(spare, 31,1, handle);        
    }
      
    if (uNbO) 
      strcpy(szCh1,"OK ");
    else
      strcpy(szCh1,"ERR");
     AffChaine(15,0,0,szCh1);
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    AffChaine(6,0,0,"             ");  
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
    char szFile[128];
    char szCh1[32];

    // Init filename = romname and .SAV in place of ROM
    strcpy(szFile,gpFic[ucGameAct].szName);
    szFile[strlen(szFile)-3] = 's';
    szFile[strlen(szFile)-2] = 'a';
    szFile[strlen(szFile)-1] = 'v';
    FILE* handle = fopen(szFile, "rb"); 
    if (handle != NULL) 
    {    
         strcpy(szCh1,"LOADING...");
         AffChaine(6,0,0,szCh1);
       
        // Read Version
        u16 save_ver = 0xBEEF;
        uNbO = fread(&save_ver, sizeof(u16), 1, handle);
        
        if (save_ver == COLECODS_SAVE_VER)
        {
            // Load DrZ80 CPU
            uNbO = fread(&drz80, sizeof(struct DrZ80), 1, handle);
            DrZ80_InitHandlers(); //DRZ80 saves a lot of binary code dependent stuff, reset the handlers
            
            // Load CZ80 CPU
            uNbO = fread(&CPU, sizeof(CPU), 1, handle);

            // Need to load and restore the DrZ80 SP/PC offsets as memory might have shifted ...
            u32 z80Offset = 0;
            if (uNbO) uNbO = fread(&z80Offset, sizeof(z80Offset),1, handle);
            z80_rebaseSP(z80Offset);
            if (uNbO) uNbO = fread(&z80Offset, sizeof(z80Offset),1, handle);
            z80_rebasePC(z80Offset);                  

            // Load Coleco Memory (yes, all of it!)
            if (uNbO) uNbO = fread(pColecoMem, 0x10000,1, handle); 

            // Load XBuf video buffer (yes, all of it!)
            if (uNbO) uNbO = fread(XBuf, sizeof(XBuf),1, handle); 

            // Load look-up-table
            if (uNbO) uNbO = fread(lutTablehh, 16*1024,1, handle);         
            
            // Load the Super Game Module stuff
            if (uNbO) uNbO = fread(ay_reg, 16, 1, handle);      
            if (uNbO) uNbO = fread(&sgm_enable, sizeof(sgm_enable), 1, handle); 
            if (uNbO) uNbO = fread(&ay_reg_idx, sizeof(ay_reg_idx), 1, handle); 
            if (uNbO) uNbO = fread(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
            if (uNbO) uNbO = fread(&AY_EnvelopeOn, sizeof(AY_EnvelopeOn), 1, handle); 
            if (uNbO) uNbO = fread(&envelope_period, sizeof(envelope_period), 1, handle); 
            if (uNbO) uNbO = fread(&envelope_counter, sizeof(envelope_counter), 1, handle); 
            if (uNbO) uNbO = fread(&noise_period, sizeof(noise_period), 1, handle); 
            if (uNbO) uNbO = fread(&a_idx, sizeof(a_idx), 1, handle); 
            if (uNbO) uNbO = fread(&b_idx, sizeof(b_idx), 1, handle); 
            if (uNbO) uNbO = fread(&c_idx, sizeof(c_idx), 1, handle); 
            
            // A few frame counters
            if (uNbO) uNbO = fread(&emuActFrames, sizeof(emuActFrames), 1, handle); 
            if (uNbO) uNbO = fread(&timingFrames, sizeof(timingFrames), 1, handle); 
            
            // Deficit Z80 CPU Cycle counter
            if (uNbO) uNbO = fread(&cycle_deficit, sizeof(cycle_deficit), 1, handle); 
            
            // Some Memotech MTX stuff...
            if (uNbO) uNbO = fread(&memotech_RAM_start, sizeof(memotech_RAM_start), 1, handle); 
            if (uNbO) uNbO = fread(&IOBYTE, sizeof(IOBYTE), 1, handle); 
            if (uNbO) uNbO = fread(&MTX_KBD_DRIVE, sizeof(MTX_KBD_DRIVE), 1, handle); 
            if (uNbO) uNbO = fread(&lastIOBYTE, sizeof(lastIOBYTE), 1, handle); 
            if (uNbO) uNbO = fread(&tape_pos, sizeof(tape_pos), 1, handle); 
            if (uNbO) uNbO = fread(&tape_len, sizeof(tape_len), 1, handle); 
            if (uNbO) uNbO = fread(&last_tape_pos, sizeof(last_tape_pos), 1, handle); 
            
            // Some SVI stuff...
            if (uNbO) uNbO = fread(&svi_RAM_start, sizeof(svi_RAM_start), 1, handle); 
            
            // Load spare memory for future use
            if (uNbO) uNbO = fread(&spare, sizeof(spare),1, handle); 

            // Load VDP
            if (uNbO) uNbO = fread(VDP, sizeof(VDP),1, handle); 
            if (uNbO) uNbO = fread(&VDPCtrlLatch, sizeof(VDPCtrlLatch),1, handle); 
            if (uNbO) uNbO = fread(&VDPStatus, sizeof(VDPStatus),1, handle); 
            if (uNbO) uNbO = fread(&FGColor, sizeof(FGColor),1, handle); 
            if (uNbO) uNbO = fread(&BGColor, sizeof(BGColor),1, handle); 
            if (uNbO) uNbO = fread(&ScrMode, sizeof(ScrMode),1, handle); 
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
            
            // Load PSG Sound Stuff
            if (uNbO) uNbO = fread(&sncol, sizeof(sncol),1, handle); 
            if (uNbO) uNbO = fread(&aycol, sizeof(aycol),1, handle);
            
            // Load the SGM low memory
            if (uNbO) uNbO = fread(sgm_low_mem, 0x2000,1, handle);
            
            // Load stuff for MSX, SordM5 and SG-1000
            if (uNbO) fread(&mapperType, sizeof(mapperType),1, handle);
            if (uNbO) fread(&mapperMask, sizeof(mapperMask),1, handle);
            if (uNbO) fread(bROMInSlot, sizeof(bROMInSlot),1, handle);
            if (uNbO) fread(bRAMInSlot, sizeof(bRAMInSlot),1, handle);
            if (uNbO) fread(Slot1ROMPtr, sizeof(Slot1ROMPtr),1, handle);
            if (uNbO) fread(&PortA8, sizeof(PortA8),1, handle);
            if (uNbO) fread(&PortA9, sizeof(PortA9),1, handle);
            if (uNbO) fread(&PortAA, sizeof(PortAA),1, handle);
            if (uNbO) fread(&Port53, sizeof(Port53),1, handle);
            if (uNbO) fread(&Port60, sizeof(Port60),1, handle);

            if (uNbO) fread(ctc_control, sizeof(ctc_control),1, handle);
            if (uNbO) fread(ctc_time, sizeof(ctc_time),1, handle);
            if (uNbO) fread(ctc_timer, sizeof(ctc_timer),1, handle);
            if (uNbO) fread(ctc_vector, sizeof(ctc_vector),1, handle);
            if (uNbO) fread(ctc_latch, sizeof(ctc_latch),1, handle);
            if (uNbO) fread(&sordm5_irq, sizeof(sordm5_irq),1, handle);
            
            if (msx_mode)   // Big enough that we will not read this if we are not MSX
            {
                if (uNbO) fread(Slot3RAM, 0x10000,1, handle);
                memcpy(Slot3RAMPtr, Slot3RAM, 0x10000);
                if (uNbO) fread(msx_SRAM, 0x4000,1, handle);
                if (uNbO) fread(&msx_sram_at_8000, sizeof(msx_sram_at_8000),1, handle);
            }
            
            if (adam_mode)  // Big enough that we will not write this if we are not ADAM
            {
                if (uNbO) fread(AdamRAM, 0x20000,1, handle);
                if (uNbO) fread(PCBTable, 0x10000,1, handle);
                if (uNbO) fread(HoldingBuf, 0x4000,1, handle);
                if (uNbO) fread(Tapes, sizeof(Tapes),1, handle);
                if (uNbO) fread(Disks, sizeof(Disks),1, handle);
                if (uNbO) fread(&PCBAddr, sizeof(PCBAddr),1, handle);

                if (uNbO) fread(&Port20, sizeof(Port20),1, handle);
                if (uNbO) fread(&adam_ram_lo, sizeof(adam_ram_lo),1, handle);
                if (uNbO) fread(&adam_ram_hi, sizeof(adam_ram_hi),1, handle);
                if (uNbO) fread(&adam_ram_lo_exp, sizeof(adam_ram_lo_exp),1, handle);
                if (uNbO) fread(&adam_ram_hi_exp, sizeof(adam_ram_hi_exp),1, handle);
                if (uNbO) fread(&DiskID, sizeof(DiskID),1, handle);
                if (uNbO) fread(&KBDStatus, sizeof(KBDStatus),1, handle);
                if (uNbO) fread(&LastKey, sizeof(LastKey),1, handle);
                if (uNbO) fread(&io_busy, sizeof(io_busy),1, handle);
                if (uNbO) fread(&savedBUF, sizeof(savedBUF),1, handle);
                if (uNbO) fread(&savedLEN, sizeof(savedLEN),1, handle);
                if (uNbO) fread(&adam_CapsLock, sizeof(adam_CapsLock),1, handle);
                if (uNbO) fread(spare, 31,1, handle);                
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
            
            // Restore the screen as it was...
            dmaCopyWords(2, (u32*)XBuf, (u32*)pVidFlipBuf, 256*192);
            
            lastBank = 199;  // Force load of bank if needed
            last_tape_pos = 9999;   // Force tape position to show
        }
        else uNbO = 0;
        
        if (uNbO) 
          strcpy(szCh1,"OK ");
        else
          strcpy(szCh1,"ERR");
         AffChaine(15,0,0,szCh1);
        
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        AffChaine(6,0,0,"             ");  
      }

    fclose(handle);
}

// End of file
