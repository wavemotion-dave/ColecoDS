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
#include "CRC32.h"
#include "cpu/z80/drz80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/sn76496/SN76496.h"
#include "cpu/ay38910/AY38910.h"
#include "cpu/sn76496/Fake_AY.h"
#define NORAM 0xFF

#define COLECODS_SAVE_VER 0x000F        // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

#include "cpu/z80/Z80.h"
Z80 CPU __attribute__((section(".dtcm")));

extern byte Loop9918(void);
extern void DrZ80_InitHandlers(void);
extern u8 lastBank;
s16 timingAdjustment = 0;

// Some sprite data arrays for the Mario character that walks around the upper screen..
extern const unsigned short sprPause_Palette[16];
extern const unsigned char sprPause_Bitmap[2560];
extern u32* lutTablehh;
extern int cycle_deficit;

// ----------------------------------------------------------------------
// Our "massive" ROM buffer - we support MegaCarts up to 512k but 
// we could bump this to 1MB as the MC standard supports up to 1MB
// but there are currently no games that take advantage of that 
// much... only Wizard of Wor is 512K as the largest I've seen...
// ----------------------------------------------------------------------
u8 romBuffer[512 * 1024] ALIGN(32);   // We support MegaCarts up to 512KB

u8 romBankMask    __attribute__((section(".dtcm"))) = 0x00;
u8 bBlendMode     __attribute__((section(".dtcm"))) = false;

u8 sgm_enable     __attribute__((section(".dtcm"))) = false;
u8 ay_reg_idx     __attribute__((section(".dtcm"))) = 0;
u8 ay_reg[256]    __attribute__((section(".dtcm"))) = {0};
u16 sgm_low_addr  __attribute__((section(".dtcm"))) = 0x2000;

static u8 Port53  __attribute__((section(".dtcm"))) = 0x00;
static u8 Port60  __attribute__((section(".dtcm"))) = 0x0F;

u8 bFirstSGMEnable __attribute__((section(".dtcm"))) = true;
u8 AY_Enable       __attribute__((section(".dtcm"))) = false;
u8 AY_NeverEnable  __attribute__((section(".dtcm"))) = false;
u8 SGM_NeverEnable __attribute__((section(".dtcm"))) = false;

u8  JoyMode        __attribute__((section(".dtcm"))) = 0;           // Joystick Mode (1=Keypad, 0=Joystick)
u32 JoyState       __attribute__((section(".dtcm"))) = 0;           // Joystick State for P1 and P2

// ---------------------------------------------------------------
// We provide 5 "Sensitivity" settings for the X/Y spinner
// ---------------------------------------------------------------
// Hand Tweaked Speeds:      Norm   Fast   Fastest  Slow   Slowest
const u16 SPINNER_SPEED[] = {120,   75,    50,      200,   300};    

// ------------------------------------------------------------
// Some global vars to track what kind of cart/rom we have...
// ------------------------------------------------------------
u8 bMagicMegaCart __attribute__((section(".dtcm"))) = 0;      // Mega Carts support > 32K 
u8 bActivisionPCB __attribute__((section(".dtcm"))) = 0;      // Activision PCB is 64K with EEPROM
u8 sRamAtE000_OK  __attribute__((section(".dtcm"))) = 0;      // Lord of the Dungeon is the only game that needs this

u32 file_crc = 0x00000000;  // Our global file CRC32 to uniquiely identify this game

u8 sgm_low_mem[8192] = {0}; // The 8K of SGM RAM that can be mapped into the BIOS area

// -----------------------------------------------------------
// The two master sound chips... both are mapped to SN sound.
// -----------------------------------------------------------
SN76496 sncol   __attribute__((section(".dtcm")));
SN76496 aycol   __attribute__((section(".dtcm")));

// ---------------------------------------------------------
// Reset the Super Game Module vars... we reset back to 
// SGM disabled and no AY sound chip use
// ---------------------------------------------------------
void sgm_reset(void)
{
    // Make sure Super Game Module registers for AY chip are clear...
    memset(ay_reg, 0x00, 256);   // Clear the AY registers...
    ay_reg[0x07] = 0xFF;         // Everything turned off to start...
    ay_reg[0x0E] = 0xFF;         // These are "max attenuation" volumes
    ay_reg[0x0F] = 0xFF;         // to keep the volume disabled
    channel_a_enable = 0;        // All "AY" channels off
    channel_b_enable = 0;        // ..
    channel_c_enable = 0;        // ..
    noise_enable = 0;            // "AY" noise generator off
    
    sgm_enable = false;          // Default to no SGM until enabled
    sgm_low_addr = 0x2000;       // And the first 8K is BIOS
    AY_Enable = false;           // Default to no AY use until accessed
    bFirstSGMEnable = true;      // First time SGM enable we clear ram
    
    Port53 = 0x00;               // Init the SGM Port 53
    Port60 = 0x0F;               // And the Adam/Memory Port 60
}


/*********************************************************************************
 * Wipe main RAM with random patterns...
 ********************************************************************************/
void colecoWipeRAM(void)
{
  for (int i=0; i<0x400; i++)
  {
      u8 randbyte = rand() & 0xFF;
      pColecoMem[0x6000 + i] = randbyte;
      pColecoMem[0x6400 + i] = randbyte;
      pColecoMem[0x6800 + i] = randbyte;
      pColecoMem[0x6C00 + i] = randbyte;
      pColecoMem[0x7000 + i] = randbyte;
      pColecoMem[0x7400 + i] = randbyte;
      pColecoMem[0x7800 + i] = randbyte;
      pColecoMem[0x7C00 + i] = randbyte;
  }
}
    


/*********************************************************************************
 * Init coleco Engine for that game
 ********************************************************************************/
u8 colecoInit(char *szGame) {
  u8 RetFct,uBcl;
  u16 uVide;
  
  // Wipe area between BIOS and RAM (often SGM RAM mapped here but until then we are 0xFF)
  memset(pColecoMem+0x2000, 0xFF, 0x4000);
    
  // Wipe RAM
  colecoWipeRAM();
  
  // Set upper 32K ROM area to 0xFF before load
  memset(pColecoMem+0x8000, 0xFF, 0x8000);

  // Load coleco cartridge
  RetFct = loadrom(szGame,pColecoMem+0x8000,0x8000);
  if (RetFct) 
  {
    RetFct = colecoCartVerify(pColecoMem+0x8000);
    
    // If no error, change graphic mode to initiate emulation
    if (RetFct == IMAGE_VERIFY_PASS) {
      videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
      vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
      vramSetBankB(VRAM_B_LCD);
      REG_BG3CNT = BG_BMP8_256x256;
      REG_BG3PA = (1<<8); 
      REG_BG3PB = 0;
      REG_BG3PC = 0;
      REG_BG3PD = (1<<8);
      REG_BG3X = 0;
      REG_BG3Y = 0;
    }
    // Init var
    for (uBcl=0;uBcl<192;uBcl++) {
      uVide=(uBcl/12);
      dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
    }
    
    sgm_reset();                       // Make sure the super game module is disabled to start

    JoyMode=JOYMODE_JOYSTICK;          // Joystick mode key
    JoyState = 0x0000;                 // Nothing pressed to start

    sn76496Reset(1, &sncol);           // Reset the SN sound chip
    sn76496W(0x90 | 0x0F ,&sncol);     // Write new Volume for Channel A  
    sn76496W(0xB0 | 0x0F ,&sncol);     // Write new Volume for Channel B
    sn76496W(0xD0 | 0x0F ,&sncol);     // Write new Volume for Channel C  
    u16 tmp_samples[32];
    sn76496Mixer(32, tmp_samples, &sncol);

    sn76496Reset(1, &aycol);           // Reset the SN sound chip
    sn76496W(0x90 | 0x0F ,&aycol);     // Write new Volume for Channel A  
    sn76496W(0xB0 | 0x0F ,&aycol);     // Write new Volume for Channel B
    sn76496W(0xD0 | 0x0F ,&aycol);     // Write new Volume for Channel C  
    sn76496Mixer(32, tmp_samples, &aycol);
      
    DrZ80_Reset();                      // Reset the DrZ80 core CPU
    ResetZ80(&CPU);                     // Reset the CZ80 core CPU
    Reset9918();                        // Reset the VDP
      
    XBuf = XBuf_A;                      // Set the initial screen ping-pong buffer to A
  }
  
  // Return with result
  return (RetFct);
}

/*********************************************************************************
 * Run the emul
 ********************************************************************************/
void colecoRun(void) 
{
  DrZ80_Reset();                        // Reset the DrZ80 core CPU
  ResetZ80(&CPU);                       // Reset the CZ80 core CPU
  showMainMenu();                       // Show the game-related screen
}

/*********************************************************************************
 * Set coleco Palette
 ********************************************************************************/
void colecoSetPal(void) 
{
  u8 uBcl,r,g,b;
  
  // The Colecovision has a 16 color pallette... we set that up here.
  for (uBcl=0;uBcl<16;uBcl++) {
    r = (u8) ((float) TMS9918A_palette[uBcl*3+0]*0.121568f);
    g = (u8) ((float) TMS9918A_palette[uBcl*3+1]*0.121568f);
    b = (u8) ((float) TMS9918A_palette[uBcl*3+2]*0.121568f);

    SPRITE_PALETTE[uBcl] = RGB15(r,g,b);
    BG_PALETTE[uBcl] = RGB15(r,g,b);
  }
}


/*********************************************************************************
 * Save the current state - save everything we need to a single .sav file.
 ********************************************************************************/
u8  spare[508] = {0x00};    // We keep some spare bytes so we can use them in the future without changing the structure
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

    // Write the Super Game Module stuff
    if (uNbO) uNbO = fwrite(ay_reg, 256, 1, handle);      
    if (uNbO) uNbO = fwrite(&sgm_enable, sizeof(sgm_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&ay_reg_idx, sizeof(ay_reg_idx), 1, handle); 
    if (uNbO) uNbO = fwrite(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
    if (uNbO) uNbO = fwrite(&channel_a_enable, sizeof(channel_a_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&channel_b_enable, sizeof(channel_b_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&channel_c_enable, sizeof(channel_c_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&noise_enable, sizeof(noise_enable), 1, handle); 
      
    // A few frame counters
    if (uNbO) uNbO = fwrite(&emuActFrames, sizeof(emuActFrames), 1, handle); 
    if (uNbO) uNbO = fwrite(&timingFrames, sizeof(timingFrames), 1, handle); 
      
    // Deficit Z80 CPU Cycle counter
    if (uNbO) uNbO = fwrite(&cycle_deficit, sizeof(cycle_deficit), 1, handle); 
      
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
            if (uNbO) uNbO = fread(ay_reg, 256, 1, handle);      
            if (uNbO) uNbO = fread(&sgm_enable, sizeof(sgm_enable), 1, handle); 
            if (uNbO) uNbO = fread(&ay_reg_idx, sizeof(ay_reg_idx), 1, handle); 
            if (uNbO) uNbO = fread(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
            if (uNbO) uNbO = fread(&channel_a_enable, sizeof(channel_a_enable), 1, handle); 
            if (uNbO) uNbO = fread(&channel_b_enable, sizeof(channel_b_enable), 1, handle); 
            if (uNbO) uNbO = fread(&channel_c_enable, sizeof(channel_c_enable), 1, handle); 
            if (uNbO) uNbO = fread(&noise_enable, sizeof(noise_enable), 1, handle); 
            
            // A few frame counters
            if (uNbO) uNbO = fread(&emuActFrames, sizeof(emuActFrames), 1, handle); 
            if (uNbO) uNbO = fread(&timingFrames, sizeof(timingFrames), 1, handle); 
            
            // Deficit Z80 CPU Cycle counter
            if (uNbO) uNbO = fread(&cycle_deficit, sizeof(cycle_deficit), 1, handle); 
            
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

/*********************************************************************************
 * Update the screen for the current cycle. On the DSi this will generally
 * be called right after swiWaitForVBlank() in TMS9918a.c which will help
 * reduce visual tearing and other artifacts. It's not strictly necessary
 * and that does slow down the loop a bit... but DSi can handle it.
 ********************************************************************************/
ITCM_CODE void colecoUpdateScreen(void) 
{
    // ------------------------------------------------------------   
    // If we are in 'blendMode' we will OR the last two frames. 
    // This helps on some games where things are just 1 pixel 
    // wide and the non XL/LL DSi will just not hold onto the
    // image long enough to render it properly for the eye to 
    // pick up. This takes CPU speed, however, and will not be
    // supported for older DS-LITE/PHAT units with slower CPU.
    // ------------------------------------------------------------   
    if (myConfig.frameBlend)
    {
      if (XBuf == XBuf_A)
      {
          XBuf = XBuf_B;
      }
      else
      {
          XBuf = XBuf_A;
      }
      u32 *p1 = (u32*)XBuf_A;
      u32 *p2 = (u32*)XBuf_B;
      u32 *destP = (u32*)pVidFlipBuf;
        
      for (u16 i=0; i<(256*192)/4; i++)
      {
          *destP++ = (*p1++ | *p2++);       // Simple OR blending of 2 frames...
      }
    }
    else
    {
        // -----------------------------------------------------------------
        // Not blend mode... just blast it out via DMA as fast as we can...
        // -----------------------------------------------------------------
        dmaCopyWordsAsynch(2, (u32*)XBuf_A, (u32*)pVidFlipBuf, 256*192);
    }
}


/*********************************************************************************
 * Check if the cart is valid...
 ********************************************************************************/
u8 colecoCartVerify(const u8 *cartData) 
{
  // Who are we to argue? Some SGM roms shift the magic numbers (5AA5, A55A) up to 
  // bank 0 and it's not worth the hassle. The game either runs or it won't.
  return IMAGE_VERIFY_PASS;
}


/*******************************************************************************
 * Compute the file CRC - this will be our unique identifier for the game
 * for saving HI SCORES and Configuration / Key Mapping data.
 *******************************************************************************/
void getfile_crc(const char *path)
{
    file_crc = getFileCrc(path);        // The CRC is used as a unique ID to save out High Scores and Configuration...
    
    // --------------------------------------------------------------------------------------------------------
    // A few games need some timing adjustment tweaks to render correctly... due to DrZ80 inaccuracies.
    // These timing adjustments will only be applied to the lower-compatibilty DrZ80 core.
    // --------------------------------------------------------------------------------------------------------
    timingAdjustment = 0;                               // This timing adjustment is only used for DrZ80 (not CZ80 core)
    if (file_crc == 0xb3b767ae) timingAdjustment = -1;  // Fathom (Imagic) won't render right otherwise
    if (file_crc == 0x17edbfd4) timingAdjustment = -1;  // Centipede (Atari) has title screen glitches otherwise
    if (file_crc == 0x56c358a6) timingAdjustment =  2;  // Destructor (Coleco) requires more cycles
    if (file_crc == 0xb5be3448) timingAdjustment =  10; // Sudoku Homebrew requires more cycles
    
    // -----------------------------------------------------------------
    // Only Lord of the Dungeon allows SRAM writting in this area... 
    // -----------------------------------------------------------------
    sRamAtE000_OK = 0;  
    if (file_crc == 0xfee15196) sRamAtE000_OK = 1;      // 32K version of Lord of the Dungeon
    if (file_crc == 0x1053f610) sRamAtE000_OK = 1;      // 24K version of Lord of the Dungeon

    // --------------------------------------------------------------------------
    // There are a few games that don't want the SGM module... Check those now.
    // --------------------------------------------------------------------------
    AY_NeverEnable = false;                             // Default to allow AY sound
    SGM_NeverEnable = false;                            // And allow SGM by default unless Super DK or Super DK-Jr (directly below)
    if (file_crc == 0xef25af90) SGM_NeverEnable = true; // Super DK Prototype - ignore any SGM/Adam Writes
    if (file_crc == 0xc2e7f0e0) SGM_NeverEnable = true; // Super DK JR Prototype - ignore any SGM/Adam Writes
}

/** loadrom() ******************************************************************/
/* Open a rom file from file system                                            */
/*******************************************************************************/
u8 loadrom(const char *path,u8 * ptr, int nmemb) 
{
  u8 bOK = 0;

  FILE* handle = fopen(path, "r");  
  if (handle != NULL) 
  {
    fseek(handle, 0, SEEK_END);
    int iSSize = ftell(handle);
    fseek(handle, 0, SEEK_SET);
    if(iSSize <= (512 * 1024))  // Max size cart is 512KB - that's pretty huge...
    {
        fread((void*) romBuffer, iSSize, 1, handle); 
        
        // ----------------------------------------------------------------------
        // Do we fit within the standard 32K Colecovision Cart ROM memory space?
        // ----------------------------------------------------------------------
        if (iSSize <= (32*1024))
        {
            memcpy(ptr, romBuffer, nmemb);
            romBankMask = 0x00;
        }
        else    // No - must be Bankswitched Cart!!
        {
            // Copy 128K worth up to the VRAM for faster bank switching on the first 8 banks
            u32 copySize = ((iSSize <= 128*1024) ? iSSize : (128*1024));
            u32 *dest = (u32*)0x06880000;
            u32 *src  = (u32*)romBuffer;
            for (u32 i=0; i<copySize/4; i++)
            {
                *dest++ = *src++;
            }
                
            // --------------------------------------------------------------
            // Mega Carts have a special byte pattern in the upper block... 
            // but we need to distinguish between 64k Activision PCB and
            // possible 64K Megacart (theoretically MC should be 128K+ but
            // there are examples of 64K MegaCarts). This code does that...
            // --------------------------------------------------------------
            bMagicMegaCart = ((romBuffer[0xC000] == 0x55 && romBuffer[0xC001] == 0xAA) ? 1:0);
            lastBank = 199;                                 // Force load of the first bank when asked to bankswitch
            if ((iSSize == (64 * 1024)) && !bMagicMegaCart)
            {
                bActivisionPCB = 1;
                memcpy(ptr, romBuffer, 0x4000);                     // bank 0
                memcpy(ptr+0x4000, romBuffer+0x4000, 0x4000);       // bank 1
                romBankMask = 0x03;
                // TODO: Eventually handle EEPROM for these PCBs...
            }
            else    // We will assume Megacart then...
            {
                bMagicMegaCart = 1;
                memcpy(ptr, romBuffer+(iSSize-0x4000), 0x4000); // For MegaCart, we map highest 16K bank into fixed ROM
                BankSwitch(0);                                  // The initial 16K "switchable" bank is bank 0 (based on a post from Nanochess in AA forums)
                
                if      (iSSize == (64  * 1024)) romBankMask = 0x03;
                else if (iSSize == (128 * 1024)) romBankMask = 0x07;
                else if (iSSize == (256 * 1024)) romBankMask = 0x0F;
                else if (iSSize == (512 * 1024)) romBankMask = 0x1F;
                else romBankMask = 0x07;    // Not sure what to do... good enough
            }
        }
        bOK = 1;
    }
    fclose(handle);
  }
  return bOK;
}

// --------------------------------------------------------------------------
// Based on writes to Port53 and Port60 we configure the SGM handling of 
// memory... this includes 24K vs 32K of RAM (the latter is BIOS disabled).
// --------------------------------------------------------------------------
void SetupSGM(void)
{
    if (SGM_NeverEnable) return;        // There are a couple of games were we don't want to enable the SGM. Most notably Super DK won't play with SGM emulation.
    
    sgm_enable = (Port53 & 0x01) ? true:false;  // Port 53 lowest bit dictates SGM memory support enable.
    
    // ----------------------------------------------------------------
    // The first time we enable the SGM expansion RAM, we clear it out
    // ----------------------------------------------------------------
    if (sgm_enable && bFirstSGMEnable)
    {
        memset(pColecoMem+0x2000, 0x00, 0x6000);
        bFirstSGMEnable = false;
    }
    
    // ------------------------------------------------------
    // And Port 60 will tell us if we want to swap out the 
    // lower 8K bios for more RAM (total of 32K RAM for SGM)
    // Since this can swap back and forth (not sure if any
    // game really does this), we need to preserve that 8K
    // when we switch back and forth...
    // ------------------------------------------------------
    if (Port60 & 0x02)  
    {
      extern u8 ColecoBios[];       // Swap in the Coleco BIOS (save SRAM)
      if (sgm_low_addr != 0x2000)
      {
          memcpy(sgm_low_mem,pColecoMem,0x2000);
          sgm_low_addr = 0x2000;
          memcpy(pColecoMem,ColecoBios,0x2000);
      }
    }
    else 
    {
      sgm_enable = true;    // Force this if someone disabled the BIOS.... based on reading some comments in the AA forum...
      if (sgm_low_addr != 0x0000)
      {
          memcpy(pColecoMem,sgm_low_mem,0x2000);
          sgm_low_addr = 0x0000; 
      }
    }
}

/** InZ80() **************************************************/
/** Z80 emulation calls this function to read a byte from   **/
/** a given I/O port.                                       **/
/*************************************************************/
ITCM_CODE unsigned char cpu_readport16(register unsigned short Port) 
{
  // Colecovision ports are 8-bit
  Port &= 0x00FF; 
  
  // Port 52 is used for the AY sound chip for the Super Game Module
  if (Port == 0x52)
  {
      return FakeAY_ReadData();
  } 

  switch(Port&0xE0) 
  {
    case 0x40: // Printer Status - not used
      break;
   
    case 0x60:  // Adam/Memory Port
      return Port60;
      break;

    case 0xE0: // Joysticks Data
      Port = Port&0x02? (JoyState>>16):JoyState;
      Port = JoyMode?   (Port>>8):Port;
      return(~Port&0x7F);

    case 0xA0: /* VDP Status/Data */
      return(Port&0x01 ? RdCtrl9918():RdData9918());
  }

  // No such port
  return(NORAM);
}


/** OutZ80() *************************************************/
/** Z80 emulation calls this function to write a byte to a  **/
/** given I/O port.                                         **/
/*************************************************************/
ITCM_CODE void cpu_writeport16(register unsigned short Port,register unsigned char Value) 
{
  // Colecovision ports are 8-bit
  Port &= 0x00FF;

  // -----------------------------------------------------------------
  // Port 53 is used for the Super Game Module to enable SGM mode...
  // -----------------------------------------------------------------
  if (Port == 0x53) {Port53 = Value; SetupSGM(); return;}
  // -----------------------------------------------
  // Port 50 is the AY sound chip register index...
  // -----------------------------------------------
  else if (Port == 0x50)  
  {
      if ((Value & 0x0F) == 0x07) {AY_Enable = (AY_NeverEnable ? false:true);}
      FakeAY_WriteIndex(Value & 0x0F);
      return;
  }
  // -----------------------------------------------
  // Port 51 is the AY Sound chip register write...
  // -----------------------------------------------
  else if (Port == 0x51) 
  {
    FakeAY_WriteData(Value);
    return;
  }
  
  // ---------------------------------------------------------------------------
  // Now handle the rest of the CV ports - this handles the mirroring of
  // port writes - for example, a write to port 0x7F will hit 0x60 Memory Port
  // ---------------------------------------------------------------------------
  switch(Port&0xE0) 
  {
    case 0x80:  // Set Joystick Read Mode
      JoyMode=JOYMODE_JOYSTICK;
      return;
    case 0xC0:  // Set Keypad Read Mode 
      JoyMode=JOYMODE_KEYPAD;
      return;
    case 0xE0:  // The SN Sound port
      sn76496W(Value, &sncol);
      return;
    case 0xA0:  // The VDP graphics port
      if(!(Port&0x01)) WrData9918(Value);
      else if (WrCtrl9918(Value)) { CPU.IRequest=INT_NMI; cpuirequest=Z80_NMI_INT; }
      return;
    case 0x40:  // Printer status and ADAM related stuff...not used
    case 0x20:
      return;
    case 0x60:  // Adam/Memory port
      Port60 = Value;
      SetupSGM();
      return;   
  }
}



/** LoopZ80() *************************************************/
/** Z80 emulation calls this function periodically to run    **/
/** Z80 code for the loaded ROM. It runs code refreshing the **/
/** VDP and checking for interrupt requests.                 **/
/**************************************************************/
ITCM_CODE u32 LoopZ80() 
{
    static u16 spinnerDampen = 0;
    cpuirequest=0;
    
  // Just in case there are AY audio envelopes... this is very rough timing.
  if (AY_Enable) FakeAY_Loop();
    
  // ------------------------------------------------------------------
  // Before we execute Z80 or Loop the 9918 (both of which can cause 
  // NMI interrupt to occur), we check and adjust the spinners which 
  // can generate a lower priority interrupt to the running Z80 code.
  // ------------------------------------------------------------------
  if ((++spinnerDampen % SPINNER_SPEED[myConfig.spinSpeed]) == 0)
  {
      if (spinX_left)
      {
          cpuirequest = Z80_IRQ_INT;    // The DrZ80 way of requesting interrupt    
          CPU.IRequest=INT_RST38;       // The CZ80 way of requesting interrupt
          JoyState   &= 0xFFFFCFFF;
          JoyState   |= 0x00003000;
      }
      else if (spinX_right)
      {
          cpuirequest = Z80_IRQ_INT;    // The DrZ80 way of requesting interrupt    
          CPU.IRequest=INT_RST38;       // The CZ80 way of requesting interrupt
          JoyState   &= 0xFFFFCFFF;
          JoyState   |= 0x00001000;
      }
      
      if (spinY_left)
      {
          cpuirequest = Z80_IRQ_INT;    // The DrZ80 way of requesting interrupt    
          CPU.IRequest=INT_RST38;       // The CZ80 way of requesting interrupt
          JoyState   &= 0xCFFFFFFF;
          JoyState   |= 0x30000000;
      }
      else if (spinY_right)
      {
          cpuirequest = Z80_IRQ_INT;    // The DrZ80 way of requesting interrupt    
          CPU.IRequest=INT_RST38;       // The CZ80 way of requesting interrupt
          JoyState   &= 0xCFFFFFFF;
          JoyState   |= 0x10000000;
      }
  }

  // ---------------------------------------------------------------
  // We current support two different Z80 cores... the DrZ80 is
  // (relatively) blazingly fast on the DS ARM processor but
  // the compatibilty isn't 100%. The CZ80 core is slower but
  // has higher compatibilty. For now, the default core is 
  // DrZ80 for the DS-LITE/PHAT and CZ80 for the DSi and above.
  // The DSi has enough processing power to utilize this slower
  // but more accurate core. The user can switch cores as they like.
  // ---------------------------------------------------------------
  if (myConfig.cpuCore == 0) // DrZ80 Core ... faster but lower accuracy
  {
      // Execute 1 scanline worth of CPU instructions
      DrZ80_execute(TMS9918_LINE + timingAdjustment);
      
      // Refresh VDP 
      if(Loop9918()) cpuirequest=Z80_NMI_INT;
    
      // Generate interrupt if called for
      if (cpuirequest)
        Z80_Cause_Interrupt(cpuirequest);
      else
        Z80_Clear_Pending_Interrupts();
  }
  else  // CZ80 core from fMSX()... slower but higher accuracy
  {
      // Execute 1 scanline worth of CPU instructions
      cycle_deficit = ExecZ80(TMS9918_LINE + cycle_deficit);
      
      // Refresh VDP 
      if(Loop9918()) CPU.IRequest=INT_NMI;
      
      // Generate an interrupt if called for...
      if(CPU.IRequest!=INT_NONE) IntZ80(&CPU,CPU.IRequest);
  }
  
  // Drop out unless end of screen is reached 
  return (CurLine == TMS9918_END_LINE) ? 0:1;
}

// End of file
