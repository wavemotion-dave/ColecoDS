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

// ------------------------------------------------
// Adam RAM is 128K (64K Intrinsic, 64K Expanded)
// ------------------------------------------------
u8 AdamRAM[0x20000]   = {0x00};
u8 adam_128k_mode     = 0;
u8 sg1000_double_reset = false;

// -------------------------------------
// Some IO Port and Memory Map vars...
// -------------------------------------
u16 memotech_RAM_start  __attribute__((section(".dtcm"))) = 0x4000;
u8 svi_RAM[2]           __attribute__((section(".dtcm"))) = {0,1};
u8 IOBYTE               __attribute__((section(".dtcm"))) = 0x00;
u8 MTX_KBD_DRIVE        __attribute__((section(".dtcm"))) = 0x00;
u8 lastIOBYTE           __attribute__((section(".dtcm"))) = 99;
u32 tape_pos            __attribute__((section(".dtcm"))) = 0;
u32 tape_len            __attribute__((section(".dtcm"))) = 0;
u8 key_shift_hold       __attribute__((section(".dtcm"))) = 0;

u8 adam_ram_lo          __attribute__((section(".dtcm"))) = false;
u8 adam_ram_hi          __attribute__((section(".dtcm"))) = false;
u8 adam_ram_lo_exp      __attribute__((section(".dtcm"))) = false;
u8 adam_ram_hi_exp      __attribute__((section(".dtcm"))) = false;

Z80 CPU __attribute__((section(".dtcm")));

u8 bDontResetEnvelope __attribute__((section(".dtcm"))) = false;

// --------------------------------------------------
// Some special ports for the MSX machine emu
// --------------------------------------------------
u8 Port_PPI_A __attribute__((section(".dtcm"))) = 0x00;
u8 Port_PPI_B __attribute__((section(".dtcm"))) = 0x00;
u8 Port_PPI_C __attribute__((section(".dtcm"))) = 0x00;

u8 bIsComplicatedRAM __attribute__((section(".dtcm"))) = 0;   // Set to 1 if we have hotspots or other RAM needs

char lastAdamDataPath[256];

// --------------------------------------------------------------------------------------
// Some sprite data arrays for the Mario character that walks around the upper screen..
extern const unsigned short sprPause_Palette[16];
extern const unsigned char sprPause_Bitmap[2560];

// ----------------------------------------------------------------------------
// Some vars for the Z80-CTC timer/counter chip which is only partially 
// emulated - enough that we can do rough timing and generate VDP 
// interrupts. This chip is only used on the Sord M5 (not the Colecovision
// nor the SG-1000 which just ties interrupts directly between VDP and CPU).
// ----------------------------------------------------------------------------
u8 ctc_control[4]   __attribute__((section(".dtcm"))) = {0x02, 0x02, 0x02, 0x02};
u8 ctc_time[4]      __attribute__((section(".dtcm"))) = {0};
u32 ctc_timer[4]    __attribute__((section(".dtcm"))) = {0};
u8 ctc_vector[4]    __attribute__((section(".dtcm"))) = {0};
u8 ctc_latch[4]     __attribute__((section(".dtcm"))) = {0}; 

// ----------------------------------------------------------------------
// Our "massive" ROM buffer - we support MegaCarts up to 512k but 
// we could bump this to 1MB as the MC standard supports up to 1MB
// but there are currently no games that take advantage of that 
// much... only Wizard of Wor is 512K as the largest I've seen...
// ----------------------------------------------------------------------
u8 romBuffer[512 * 1024] ALIGN(32);   // We support MegaCarts up to 512KB

u8 romBankMask    __attribute__((section(".dtcm"))) = 0x00;
u8 sgm_enable     __attribute__((section(".dtcm"))) = false;
u8 ay_reg_idx     __attribute__((section(".dtcm"))) = 0;
u8 ay_reg[16]     __attribute__((section(".dtcm"))) = {0};
u16 sgm_low_addr  __attribute__((section(".dtcm"))) = 0x2000;

u8 Port53         __attribute__((section(".dtcm"))) = 0x00;
u8 Port60         __attribute__((section(".dtcm"))) = 0x0F;
u8 Port20         __attribute__((section(".dtcm"))) = 0x00;

u8 bFirstSGMEnable __attribute__((section(".dtcm"))) = true;
u8 AY_Enable       __attribute__((section(".dtcm"))) = false;
u8 AY_NeverEnable  __attribute__((section(".dtcm"))) = false;
u8 SGM_NeverEnable __attribute__((section(".dtcm"))) = false;
u8 AY_EnvelopeOn   __attribute__((section(".dtcm"))) = false;

u8  JoyMode        __attribute__((section(".dtcm"))) = 0;           // Joystick Mode (1=Keypad, 0=Joystick)
u32 JoyState       __attribute__((section(".dtcm"))) = 0;           // Joystick State for P1 and P2

s16 timingAdjustment __attribute__((section(".dtcm"))) = 0;

// ---------------------------------------------------------------
// We provide 5 "Sensitivity" settings for the X/Y spinner
// ---------------------------------------------------------------
// Hand Tweaked Speeds:      Norm   Fast   Fastest  Slow   Slowest
u16 SPINNER_SPEED[] __attribute__((section(".dtcm"))) = {120,   75,    50,      200,   300};    

// ------------------------------------------------------------
// Some global vars to track what kind of cart/rom we have...
// ------------------------------------------------------------
u8 bMagicMegaCart __attribute__((section(".dtcm"))) = 0;      // Mega Carts support > 32K 
u8 bActivisionPCB __attribute__((section(".dtcm"))) = 0;      // Activision PCB is 64K with EEPROM
u8 sRamAtE000_OK  __attribute__((section(".dtcm"))) = 0;      // Lord of the Dungeon is the only game that needs this

u32 file_crc __attribute__((section(".dtcm")))  = 0x00000000;  // Our global file CRC32 to uniquiely identify this game

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
    memset(ay_reg, 0x00, 16);    // Clear the AY registers...
    ay_reg[0x07] = 0xFF;         // Everything turned off to start...
    ay_reg[0x0E] = 0xFF;         // These are "max attenuation" volumes
    ay_reg[0x0F] = 0xFF;         // to keep the volume disabled
   
    sgm_enable = false;          // Default to no SGM until enabled
    sgm_low_addr = 0x2000;       // And the first 8K is BIOS
    if (!msx_mode && !svi_mode && !einstein_mode)
    {
        AY_Enable = false;       // Default to no AY use until accessed
    }
    AY_EnvelopeOn = false;       // No Envelope mode yet
    bFirstSGMEnable = true;      // First time SGM enable we clear ram
    
    Port53 = 0x00;               // Init the SGM Port 53
    Port60 = (adam_mode?0x00:0x0F);// And the Adam/Memory Port 60
    Port20 = 0x00;               // Adam Net 
}


/*********************************************************************************
 * Wipe main RAM with random patterns... or fill with 0x00 for some emulations.
 ********************************************************************************/
void colecoWipeRAM(void)
{
  if (sg1000_mode)
  {
      for (int i=0xC000; i<0x10000; i++) pColecoMem[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (pv2000_mode)
  {
      memset(pColecoMem+0x4000, 0xFF, 0x8000);
      for (int i=0x7000; i<0x8000; i++) pColecoMem[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (sordm5_mode)
  {
      for (int i=0x7000; i<0x10000; i++) pColecoMem[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (memotech_mode)
  {
    for (int i=0; i< 0xC000; i++) pColecoMem[0x4000+i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));   // This pattern tends to make most things start up properly...
  }
  else if (svi_mode)
  {
    for (int i=0; i< 0x8000; i++) pColecoMem[0x8000+i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));   // This pattern tends to make most things start up properly...
    memset(Slot3RAM,  0x00, 0x10000);
  }
  else if (msx_mode)
  {
    // Do nothing... MSX has all kinds of memory mapping that is handled elsewhere
  }
  else if (creativision_mode)
  {
     memset(pColecoMem, 0x00, 0x1000);  // Lower 1K is RAM mirrored four times (4K)
  }
  else if (adam_mode)
  {
    // ADAM has special handling...
    for (int i=0; i< 0x20000; i++) AdamRAM[i] = (myConfig.memWipe ? 0x02:  (rand() & 0xFF));   // This pattern tends to make most things start up properly...
    memset(pColecoMem, 0xFF, 0x10000);
  }
  else if (einstein_mode)
  {
      memset(pColecoMem+0x2000,0xFF, 0x6000);
      for (int i=0x8000; i<0x10000; i++) pColecoMem[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else
  {
      for (int i=0; i<0x400; i++)
      {
          u8 randbyte = rand() & 0xFF;
          pColecoMem[0x6000 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          pColecoMem[0x6400 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          pColecoMem[0x6800 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          pColecoMem[0x6C00 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          pColecoMem[0x7000 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          pColecoMem[0x7400 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          pColecoMem[0x7800 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          pColecoMem[0x7C00 + i] = (myConfig.memWipe ? 0x00 : randbyte);
      }
  }
}


/*********************************************************************************
 * Keybaord Key Buffering Engine...
 ********************************************************************************/
u8 BufferedKeys[32];
u8 BufferedKeysWriteIdx=0;
u8 BufferedKeysReadIdx=0;
void BufferKey(u8 key)
{
    BufferedKeys[BufferedKeysWriteIdx] = key;
    BufferedKeysWriteIdx = (BufferedKeysWriteIdx+1) % 32;
}

/*********************************************************************************
 * Init coleco Engine for that game
 ********************************************************************************/
u8 colecoInit(char *szGame) 
{
  extern u8 bForceMSXLoad;
  u8 RetFct,uBcl;
  u16 uVide;

  // ----------------------------------------------------------------------------------  
  // Clear the entire ROM buffer[] - fill with 0xFF to emulate non-responsive memory
  // ----------------------------------------------------------------------------------  
  memset(romBuffer, 0xFF, (512 * 1024));
  
  if (bForceMSXLoad) msx_mode = 1;
  if (msx_mode)      AY_Enable=true;
  if (svi_mode)      AY_Enable=true;
  if (einstein_mode) AY_Enable=true;
  if (msx_mode) InitBottomScreen();  // Could Need to ensure the MSX layout is shown
    
  // -----------------------------------------------------------------
  // Change graphic mode to initiate emulation.
  // Here we can claim back 128K of VRAM which is otherwise unused
  // but we can use it for fast memory swaps and look-up-tables.
  // -----------------------------------------------------------------
  videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);      // This is our top emulation screen (where the game is played)
  vramSetBankB(VRAM_B_LCD);                     // 128K of Video Memory mapped at 0x6820000
  REG_BG3CNT = BG_BMP8_256x256;
  REG_BG3PA = (1<<8); 
  REG_BG3PB = 0;
  REG_BG3PC = 0;
  REG_BG3PD = (1<<8);
  REG_BG3X = 0;
  REG_BG3Y = 0;
    
  // Unload any ADAM related stuff
  for(u8 J=0;J<MAX_DISKS;++J) ChangeDisk(J,0);
  for(u8 J=0;J<MAX_TAPES;++J) ChangeTape(J,0);

  // Init the page flipping buffer...
  for (uBcl=0;uBcl<192;uBcl++) 
  {
     uVide=(uBcl/12);
     dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
  }
    
  write_EE_counter=0;

  if (sg1000_mode)  // Load SG-1000 cartridge
  {
      colecoWipeRAM();                              // Wipe RAM
      RetFct = loadrom(szGame,pColecoMem,0xC000);   // Load up to 48K
      sg1000_reset();                               // Reset the SG-1000
  }
  else if (sordm5_mode)  // Load Sord M5 cartridge
  {
      colecoWipeRAM();
      RetFct = loadrom(szGame,pColecoMem+0x2000,0x4000);  // Load up to 16K
  }
  else if (pv2000_mode)  // Casio PV-2000 cartridge loads at C000
  {
      colecoWipeRAM();
      RetFct = loadrom(szGame,pColecoMem+0xC000,0x4000);  // Load up to 16K
  }
  else if (creativision_mode)  // Creativision loads cart up against 0xC000
  {
      colecoWipeRAM();
      RetFct = loadrom(szGame,pColecoMem+0xC000,0x4000);  // Load up to 16K
  }
  else if (memotech_mode)  // Load Memotech MTX file
  {
      memcpy((u8 *)0x6820000+0x0000, mtx_os,    0x2000);  // Fast copy buffer
      memcpy((u8 *)0x6820000+0x2000, mtx_basic, 0x2000);  // Fast copy buffer
      memcpy((u8 *)0x6820000+0x4000, mtx_assem, 0x2000);  // Fast copy buffer
      memset((u8 *)0x6830000, 0x00, 0x10000);             // Clear RAM buffer
      memset(pColecoMem+0x4000, 0xFF, 0xC000);            // Wipe Memory above BIOS
      RetFct = loadrom(szGame,pColecoMem+0x4000,0xC000);  // Load up to 48K
  }
  else if (msx_mode)  // Load MSX cartridge ... 
  {
      // loadrom() will figure out how big and where to load it... the 0x8000 here is meaningless.
      RetFct = loadrom(szGame,pColecoMem+0x8000,0x8000);  
      
      // Wipe RAM area from 0xC000 upwards after ROM is loaded...
      colecoWipeRAM();
  }
  else if (svi_mode)  // Load SVI ROM ... 
  {
      memcpy((u8 *)0x6820000+0x0000, SVIBios, 0x8000);  // Fast copy buffer for BIOS
          
      // loadrom() will figure out how big and where to load it... the 0x8000 here is meaningless.
      RetFct = loadrom(szGame,pColecoMem+0x8000,0x8000);  
      
      // Wipe RAM area from 0x8000 upwards after ROM is loaded...
      colecoWipeRAM();
  }
  else if (adam_mode)  // Load Adam DDP
  {
      sgm_reset();                       // Make sure the super game module is disabled to start
      adam_CapsLock = 0;
      adam_unsaved_data = 0;
      colecoWipeRAM();
      RetFct = loadrom(szGame,pColecoMem,0x10000);  
  }
  else if (pencil2_mode)
  {
      // Wipe area between BIOS and RAM (often SGM RAM mapped here but until then we are 0xFF)
      memset(pColecoMem+0x2000, 0xFF, 0xE000);

      // Wipe RAM to Random Values
      colecoWipeRAM();

      RetFct = loadrom(szGame,pColecoMem+0x8000,0x8000);
  }
  else if (einstein_mode)  // Load Einstein COM file
  {
      colecoWipeRAM();
      RetFct = loadrom(szGame,pColecoMem+0x4000,0xC000);  // Load up to 48K
  }
  else  // Load coleco cartridge
  {
      // Wipe area between BIOS and RAM (often SGM RAM mapped here but until then we are 0xFF)
      memset(pColecoMem+0x2000, 0xFF, 0x4000);

      // Wipe RAM to Random Values
      colecoWipeRAM();

      // Set upper 32K ROM area to 0xFF before load
      memset(pColecoMem+0x8000, 0xFF, 0x8000);

      RetFct = loadrom(szGame,pColecoMem+0x8000,0x8000);
  }
    
  if (RetFct) 
  {
    RetFct = colecoCartVerify(pColecoMem+0x8000);
      
    // Perform a standard system RESET
    ResetColecovision();
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
    
    // ---------------------------------------------------------------------------------------------
    // And we don't have the AY envelope quite right so a few games don't want to reset the indexes
    // ---------------------------------------------------------------------------------------------
    bDontResetEnvelope = false;
    if (file_crc == 0x90f5f414) bDontResetEnvelope = true; // MSX Warp-and-Warp
    if (file_crc == 0x5e169d35) bDontResetEnvelope = true; // MSX Warp-and-Warp (alt)
    if (file_crc == 0xe66eaed9) bDontResetEnvelope = true; // MSX Warp-and-Warp (alt)
    if (file_crc == 0x785fc789) bDontResetEnvelope = true; // MSX Warp-and-Warp (alt)    
    if (file_crc == 0xe50d6e60) bDontResetEnvelope = true; // MSX Warp-and-Warp (cassette)    
    
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
    
    // ---------------------------------------------------------------------------------
    // A few of the ZX Spectrum ports actually use the MSX beeper for sound. Go figure!
    // ---------------------------------------------------------------------------------
    msx_beeper_enabled = 0;
    if (file_crc == 0x1b8873ca) msx_beeper_enabled = 1;     // MSX Avenger uses beeper
    if (file_crc == 0x111fc33b) msx_beeper_enabled = 1;     // MSX Avenger uses beeper    
    if (file_crc == 0x690f9715) msx_beeper_enabled = 1;     // MSX Batman (the movie) uses beeper
    if (file_crc == 0x3571f5d4) msx_beeper_enabled = 1;     // MSX Master of the Universe uses beeper
    
    // ------------------------------------------------------------------------------
    // And a handful of games require SRAM which is a special case-by-case basis...
    // ------------------------------------------------------------------------------
    msx_sram_enabled = 0;
    if (file_crc == 0x92943e5b) msx_sram_enabled = 0x10;       // MSX Hydlide 2 - Shine Of Darkness (EN) 
    if (file_crc == 0xb29edaec) msx_sram_enabled = 0x10;       // MSX Hydlide 2 - Shine Of Darkness (EN)
    if (file_crc == 0xd640deaf) msx_sram_enabled = 0x20;       // MSX Dragon Slayer 2 - Xanadu (EN)
    if (file_crc == 0x119b7ba8) msx_sram_enabled = 0x20;       // MSX Dragon Slayer 2 - Xanadu (JP)    
    if (file_crc == 0x27fd8f9a) msx_sram_enabled = 0x10;       // MSX Deep Dungeon I (JP)
    if (file_crc == 0x213da247) msx_sram_enabled = 0x10;       // MSX Deep Dungeon II (EN)
    if (file_crc == 0x101db19c) msx_sram_enabled = 0x10;       // MSX Deep Dungeon II (JP)
    
}


/** loadrom() ******************************************************************/
/* Open a rom file from file system                                            */
/*******************************************************************************/
u8 loadrom(const char *path,u8 * ptr, int nmemb) 
{
  u8 bOK = 0;

  FILE* handle = fopen(path, "rb");  
  if (handle != NULL) 
  {
    memset(romBuffer, 0xFF, (512 * 1024));          // Ensure our rom buffer is clear (0xFF to simulate unused memory on ROM/EE though probably 0x00 would be fine too)
    
    fseek(handle, 0, SEEK_END);                     // Figure out how big the file is
    int iSSize = ftell(handle);
    sg1000_double_reset = false;
    
    if (sg1000_mode && (iSSize == (2048 * 1024)))   // Look for .sc Multicart
    {
        fseek(handle, iSSize-0x8000, SEEK_SET);       // Seek to the last 32K block (this is the menu system)
        fread((void*) romBuffer, 0x8000, 1, handle);  // Read 32K from that last block
        memcpy(pColecoMem, romBuffer, 0x8000);        // And place it into the bottom ROM area of our SG-1000 / SC-3000
        fclose(handle);
        strcpy(lastAdamDataPath, path);
        romBankMask = 0x3F;
        sg1000_double_reset = true;
        return bOK;
    }
    else
    if (sg1000_mode && (iSSize == (4096 * 1024)))   // Look for .sc Megacart
    {
        fseek(handle, iSSize-0x8000, SEEK_SET);       // Seek to the last 32K block (this is the menu system)
        fread((void*) romBuffer, 0x8000, 1, handle);  // Read 32K from that last block
        memcpy(pColecoMem, romBuffer, 0x8000);        // And place it into the bottom ROM area of our SG-1000 / SC-3000
        fclose(handle);
        strcpy(lastAdamDataPath, path);
        romBankMask = 0x7F;
        sg1000_double_reset = true;
        return bOK;
    }
    else        
    if(iSSize <= (512 * 1024))  // Max size cart is 512KB - that's pretty huge...
    {
        fseek(handle, 0, SEEK_SET);
        fread((void*) romBuffer, iSSize, 1, handle); 
        fclose(handle);
        
        romBankMask = 0x00;         // No bank mask until proven otherwise
        bMagicMegaCart = false;     // No Mega Cart to start
        mapperMask = 0x00;          // No MSX mapper mask
        bActivisionPCB = 0;         // No Activision PCB

        // ------------------------------------------------------------------------------
        // For the MSX emulation, we will use fast VRAM to hold ROM and mirrors...
        // ------------------------------------------------------------------------------
        if (msx_mode)
        {
            tape_len = iSSize;  // For MSX, the tape size is saved for showing tape load progress
            tape_pos = 0;
            last_tape_pos = 9999;
            MSX_InitialMemoryLayout(iSSize);
        }
        // ---------------------------------------------------------------------------
        // For ADAM mode, we need to setup the memory banks and tape/disk access...
        // ---------------------------------------------------------------------------
        else if (adam_mode)
        {
            Port60 = 0x00;               // Adam Memory default
            Port20 = 0x00;               // Adam Net default
            adam_128k_mode = 0;          // Normal 64K ADAM to start
            SetupAdam(false);
            // The .ddp is now in romBuffer[]
            if (strstr(path, ".ddp") != 0)
            {
                ChangeTape(0, path);
                strcpy(lastAdamDataPath, path);
            }
            else
            {
                ChangeDisk(0, path);
                strcpy(lastAdamDataPath, path);
            }
        }
        else if (memotech_mode || svi_mode)     // Can be any size tapes... up to 512K
        {
            tape_len = iSSize;  // The tape size is saved for showing tape load progress
            tape_pos = 0;
            last_tape_pos = 9999;
        }
        else if (einstein_mode)
        {
            strcpy(lastAdamDataPath, path);
            tape_len = iSSize;  
            if (iSSize == 1626) // A bit of a hack... the size of the Diagnostic ROM
            {
                memcpy(pColecoMem+0x4000, romBuffer, iSSize);   // only for Diagnostics ROM
            }
        }
        else if (creativision_mode)
        {
            memset(pColecoMem+0x1000, 0xFF, 0xE800);    // Blank everything between RAM and the BIOS at 0xF800
            if (iSSize == 4096) // 4K
            {
                memcpy(pColecoMem+0x9000, romBuffer, iSSize);
                memcpy(pColecoMem+0xB000, romBuffer, iSSize);
            }
            if (iSSize == 1024 * 6) // 6K
            {
                memcpy(pColecoMem+0xB000, romBuffer+0x0000, 0x1000);   // main 4k at 0xB000
                memcpy(pColecoMem+0xA800, romBuffer+0x1000, 0x0800);   // main 2k at 0xA800
      
                memcpy(pColecoMem+0x9000, pColecoMem+0xB000, 0x1000);   // Mirror 4k
                memcpy(pColecoMem+0xA000, pColecoMem+0xA800, 0x0800);   // Mirror 2k
                memcpy(pColecoMem+0x8800, pColecoMem+0xA800, 0x0800);   // Mirror 2k
                memcpy(pColecoMem+0x8000, pColecoMem+0xA800, 0x0800);   // Mirror 2k
            }
            if (iSSize == 8192) // 8K
            {
                memcpy(pColecoMem+0x8000, romBuffer, iSSize);
                memcpy(pColecoMem+0xA000, romBuffer, iSSize);
            }
            if (iSSize == 1024 * 10) // 10K
            {
                memcpy(pColecoMem+0xA000, romBuffer+0x0000, 0x2000);    // main 8Kb	at 0xA000
                memcpy(pColecoMem+0x7800, romBuffer+0x2000, 0x0800);    // second 2Kb at 0x7800
                
                memcpy(pColecoMem+0x8000, pColecoMem+0xA000, 0x2000);   // Mirror 8k at 0x8000
                
                memcpy(pColecoMem+0x5800, pColecoMem+0x7800, 0x0800);   // Mirror 2k at 0x5800
                memcpy(pColecoMem+0x7000, pColecoMem+0x7800, 0x0800);   // Mirror 2k
                memcpy(pColecoMem+0x6800, pColecoMem+0x7800, 0x0800);   // Mirror 2k
                memcpy(pColecoMem+0x6000, pColecoMem+0x7800, 0x0800);   // Mirror 2k
                memcpy(pColecoMem+0x5000, pColecoMem+0x7800, 0x0800);   // Mirror 2k
                memcpy(pColecoMem+0x4800, pColecoMem+0x7800, 0x0800);   // Mirror 2k
                memcpy(pColecoMem+0x4000, pColecoMem+0x7800, 0x0800);   // Mirror 2k
            }
            if (iSSize == 1024 * 12) // 12K
            {
                memcpy(pColecoMem+0xA000, romBuffer+0x0000, 0x2000);    // main 8Kb	at 0xA000
                memcpy(pColecoMem+0x7000, romBuffer+0x2000, 0x1000);    // second 4Kb at 0x7000
                memcpy(pColecoMem+0x8000, pColecoMem+0xA000, 0x2000);   // Mirror 8k at 0x8000
                memcpy(pColecoMem+0x5000, pColecoMem+0x7000, 0x1000);   // Mirror 4k at 0x5000
                memcpy(pColecoMem+0x6000, pColecoMem+0x7000, 0x1000);   // Mirror 4k at 0x6000
                memcpy(pColecoMem+0x4000, pColecoMem+0x7000, 0x1000);   // Mirror 4k at 0x4000
            }
            if (iSSize == 1024 * 16) // 16K
            {
                memcpy(pColecoMem+0xA000, romBuffer+0x0000, 0x2000);    // main 8Kb	at 0xA000
                memcpy(pColecoMem+0x8000, romBuffer+0x2000, 0x2000);    // second 8Kb at 0x8000
            }
            if (iSSize == 1024 * 18) // 18K
            {
                memcpy(pColecoMem+0xA000, romBuffer+0x0000, 0x2000);
                memcpy(pColecoMem+0x8000, romBuffer+0x2000, 0x2000);
                memcpy(pColecoMem+0x7800, romBuffer+0x4000, 0x0800);
                
                memcpy(pColecoMem+0x6800, pColecoMem+0x7800, 0x0800);
                memcpy(pColecoMem+0x5800, pColecoMem+0x7800, 0x0800);
                memcpy(pColecoMem+0x4800, pColecoMem+0x7800, 0x0800);
                memcpy(pColecoMem+0x7000, pColecoMem+0x7800, 0x0800);
                memcpy(pColecoMem+0x6000, pColecoMem+0x7800, 0x0800);
                memcpy(pColecoMem+0x5000, pColecoMem+0x7800, 0x0800);
                memcpy(pColecoMem+0x4000, pColecoMem+0x7800, 0x0800);
            }
        }
        else
        // ----------------------------------------------------------------------
        // Do we fit within the standard 32K Colecovision Cart ROM memory space?
        // ----------------------------------------------------------------------
        if (iSSize <= (((sg1000_mode) ? 48:32)*1024)) // Allow SG ROMs to be up to 48K, otherwise 32K limit
        {
            memcpy(ptr, romBuffer, nmemb);
        }
        else    // No - must be Mega Cart (MC) Bankswitched!!  We have two banks of 128K VRAM to help with speed.
        {
            // Copy 128K worth up to the VRAM for faster bank switching on the first 8 banks
            u32 copySize = ((iSSize <= 128*1024) ? iSSize : (128*1024));
            u32 *dest = (u32*)0x06880000;
            u32 *src  = (u32*)romBuffer;
            for (u32 i=0; i<copySize/4; i++)
            {
                *dest++ = *src++;
            }

            // Copy another 128K worth up to the VRAM for faster bank switching on the next 8 banks
            if (iSSize > 128*1024)
            {
                u32 copySize = (128*1024);
                u32 *dest = (u32*)0x6820000;
                u32 *src  = (u32*)(romBuffer + (128*1024));
                for (u32 i=0; i<copySize/4; i++)
                {
                    *dest++ = *src++;
                }
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
                else romBankMask = 0x3F;    // Not sure what to do... good enough - this allows a wide range of banks
            }
        }
        bOK = 1;
    }
    else fclose(handle);
      
    // -------------------------------------------------------------------------  
    // For some combinations, we have hotspots or other memory stuff that 
    // needs to be more complicated than simply returning pColecoMem[].
    // -------------------------------------------------------------------------  
    bIsComplicatedRAM = (bMagicMegaCart || bActivisionPCB || adam_mode || pv2000_mode) ? 1:0;                          // Assume RAM is complicated until told otherweise
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
    if (adam_mode) return;
    
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

// ================================================================================================
// Setup Adam based on Port60 (Adam Memory) and Port20 (AdamNet)
// Most of this hinges around Port60:
// xxxx xxNN  : Lower address space code.
//       00 = Onboard ROM.  Can be switched between EOS and SmartWriter by output to port 0x20
//       01 = Onboard RAM (lower 32K)
//       10 = Expansion RAM.  Bank switch chosen by port 0x42
//       11 = OS-7 and 24K RAM (ColecoVision mode)
// 
// xxxx NNxx  : Upper address space code.
//       00 = Onboard RAM (upper 32K)
//       01 = Expansion ROM (those extra ROM sockets)
//       10 = Expansion RAM.  Bank switch chosen by port 0x42
//       11 = Cartridge ROM (ColecoVision mode).
// 
// And Port20: bit 1 (0x02) to determine if EOS.ROM is present on top of WRITER.ROM
// ================================================================================================
void SetupAdam(bool bResetAdamNet)
{
    if (!adam_mode) return;
    
    // ----------------------------------
    // Configure lower 32K of memory
    // ----------------------------------
    if ((Port60 & 0x03) == 0x00)    // WRITER.ROM (and possibly EOS.ROM)
    {
        adam_ram_lo = false;
        adam_ram_lo_exp = false;
        memcpy(pColecoMem, AdamWRITER, 0x8000);
        if (Port20 & 0x02) 
        {
            memcpy(pColecoMem+0x6000, AdamEOS, 0x2000);
        }
    }
    else if ((Port60 & 0x03) == 0x01)   // Onboard RAM
    {
        adam_ram_lo = true;
        adam_ram_lo_exp = false;
        memcpy(pColecoMem+0x0000, AdamRAM+0x0000, 0x8000);
    }
    else if ((Port60 & 0x03) == 0x03)   // Colecovision BIOS + RAM
    {
        adam_ram_lo = true;
        adam_ram_lo_exp = false;
        memcpy(pColecoMem+0x0000, AdamRAM+0x0000, 0x8000);
        memcpy(pColecoMem, ColecoBios, 0x2000);
    }
    else                                // Expanded RAM
    {
        adam_128k_mode = 1;
        adam_ram_lo = false;
        adam_ram_lo_exp = true;
        memcpy(pColecoMem+0x0000, AdamRAM+0x10000, 0x8000);
    }


    // ----------------------------------
    // Configure upper 32K of memory
    // ----------------------------------
    if ((Port60 & 0x0C) == 0x00)    // Onboard RAM
    {
        adam_ram_hi = true;
        adam_ram_hi_exp = false;
        memcpy(pColecoMem+0x8000, AdamRAM+0x8000, 0x8000);
    }
    else if ((Port60 & 0x0C) == 0x08)    // Expanded RAM
    {
        adam_128k_mode = 1;
        adam_ram_hi = false;
        adam_ram_hi_exp = true;
        memcpy(pColecoMem+0x8000, AdamRAM+0x18000, 0x8000);
    }
    else        // Nothing else exists so just return 0xFF
    {
        adam_ram_hi = false;
        adam_ram_hi_exp = false;
        memset(pColecoMem+0x8000, 0xFF, 0x8000);
    }
    
    // Check if we are to Reset the AdamNet
    if (bResetAdamNet)  ResetPCB();
}

/** InZ80() **************************************************/
/** Z80 emulation calls this function to read a byte from   **/
/** a given I/O port.                                       **/
/*************************************************************/
ITCM_CODE unsigned char cpu_readport16(register unsigned short Port) 
{
  if (sg1000_mode)   {return cpu_readport_sg(Port);}    
  if (sordm5_mode)   {return cpu_readport_m5(Port);}    
  if (pv2000_mode)   {return cpu_readport_pv2000(Port);}    
  if (memotech_mode) {return cpu_readport_memotech(Port);}    
  if (msx_mode)      {return cpu_readport_msx(Port);}
  if (svi_mode)      {return cpu_readport_svi(Port);}
  if (einstein_mode) {return cpu_readport_einstein(Port);}
    
  // Colecovision ports are 8-bit
  Port &= 0x00FF; 
  
  // Port 52 is used for the AY sound chip for the Super Game Module
  if (Port == 0x52)
  {
      return FakeAY_ReadData();
  } 

  switch(Port&0xE0) 
  {
    case 0x20:
      return Port20 & 0x0F;
      break;
          
    case 0x40: // Printer Status - not used
      return(0xFF);
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
void cpu_writeport16(register unsigned short Port,register unsigned char Value) 
{
  if      (sg1000_mode)   {cpu_writeport_sg(Port, Value); return;}
  else if (sordm5_mode)   {cpu_writeport_m5(Port, Value); return;}
  else if (pv2000_mode)   {cpu_writeport_pv2000(Port, Value); return;}
  else if (memotech_mode) {cpu_writeport_memotech(Port, Value); return;}
  else if (svi_mode)      {cpu_writeport_svi(Port, Value); return;}
  else if (einstein_mode) {cpu_writeport_einstein(Port, Value); return;}
  //if (msx_mode)    {cpu_writeport_msx(Port, Value); return;}      // This is now handled in DrZ80 and CZ80 directly
    
  // Colecovision ports are 8-bit
  Port &= 0x00FF;

  // -----------------------------------------------------------------
  // Port 53 is used for the Super Game Module to enable SGM mode...
  // -----------------------------------------------------------------
  if (Port == 0x53 && !adam_mode) {Port53 = Value; SetupSGM(); return;}
    
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
  bool resetAdamNet = false;
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
      return;
    case 0x20:  // AdamNet port
      resetAdamNet = (Port20 & 1) && ((Value & 1) == 0);
      Port20 = Value;
      if (adam_mode) SetupAdam(resetAdamNet); else SetupSGM();
      return;
    case 0x60:  // Adam/Memory port
      resetAdamNet = false;
      Port60 = Value;
      if (adam_mode) SetupAdam(resetAdamNet); else SetupSGM();
      return;   
  }
}


// ----------------------------------------------------------------
// Fires every scanline if we are in CTC mode - this provides
// some rough timing for the Z80-CTC chip. It's not perfectly
// accurate but it's good enough for our purposes.  Many of the
// M5 games use the CTC timers to generate sound/music.
// ----------------------------------------------------------------
void Z80CTC_Timer(void)
{
    if (einstein_mode) // Called every scanline... 313 * 50Hz = 15,650 times per second or 15.65KHz
    {
        for (u8 i=0; i<3; i++)  
        {
            if (--ctc_timer[i] <= 0 && !keyboard_interrupt)
            {
                ctc_timer[i] = ((((ctc_control[i] & 0x20) ? 256 : 16) * (ctc_time[i] ? ctc_time[i]:256)) / 170) + 1;
                if (ctc_control[i] & 0x80)  CPU.IRequest = ctc_vector[i];
                if (i==2) // Channel 2 clocks Channel 3 for RTC
                {
                    if (--ctc_timer[3] <= 0)
                    {
                        ctc_timer[3] = ((((ctc_control[3] & 0x20) ? 256 : 16) * (ctc_time[3] ? ctc_time[3]:256)) / 60) + 1;
                        if (ctc_control[3] & 0x80)  CPU.IRequest = ctc_vector[3];
                    }
                }
            }
        }
    }
    else
    if (memotech_mode)
    {
        for (u8 i=1; i<4; i++)
        {
            if (--ctc_timer[i] <= 0)
            {
                ctc_timer[i] = ((((ctc_control[i] & 0x20) ? 256 : 16) * (ctc_time[i] ? ctc_time[i]:256)) / MTX_CTC_SOUND_DIV) + 1;
                if (ctc_control[i] & 0x80)  CPU.IRequest = ctc_vector[i];
            }
        }
    }
    else    // Sord M5 mode
    {
        // -----------------------------------------------------------------------------------------
        // CTC Channel 1 is always the sound generator - it's the only one we have to contend with.
        // Originally we were handling channels 0, 1 and 2 but there was never any program usage
        // of channels 0 and 2 which were mainly for Serial IO for cassette drives, etc. which 
        // are not supported by ColecoDS.  So we save time and effort here and only deal with Port1.
        // -----------------------------------------------------------------------------------------
        if (--ctc_timer[1] <= 0)
        {
            ctc_timer[1] = ((((ctc_control[1] & 0x20) ? 256 : 16) * (ctc_time[1] ? ctc_time[1]:256)) / CTC_SOUND_DIV) + 1;
            if (ctc_control[1] & 0x80)  CPU.IRequest = ctc_vector[1];
        }
    }
}


// -------------------------------------------------------------------------
// For arious machines, we have patched the BIOS so that we trap calls 
// to various I/O routines: namely cassette access. We handle that here.
// -------------------------------------------------------------------------
void PatchZ80(register Z80 *r)
{
    if (msx_mode)               MSX_HandleCassette(r);
    else if (svi_mode)          SVI_HandleCassette(r);
    else if (memotech_mode)     MTX_HandleCassette(r);
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
    
  // ----------------------------------------------------------------------------
  // Special system as it runs an m6502 CPU core and is different than the Z80
  // ----------------------------------------------------------------------------
  if (creativision_mode)
  {
      creativision_run();     
  }
  else
  {    
      // Just in case there are AY audio envelopes... this is very rough timing.
      if (AY_EnvelopeOn)
      {
          extern u16 envelope_counter;
          extern u16 envelope_period;
          if (++envelope_counter > envelope_period) FakeAY_Loop();
      }

      // ------------------------------------------------------------------
      // Before we execute Z80 or Loop the 9918 (both of which can cause 
      // NMI interrupt to occur), we check and adjust the spinners which 
      // can generate a lower priority interrupt to the running Z80 code.
      // ------------------------------------------------------------------
      if (!msx_mode && !sordm5_mode && !memotech_mode && !svi_mode) //TBD clean this up
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
          DrZ80_execute(tms_cpu_line + timingAdjustment);

          // Refresh VDP 
          if(Loop9918()) cpuirequest = ((svi_mode || msx_mode || sg1000_mode) ? Z80_IRQ_INT : Z80_NMI_INT);

          // Generate interrupt if called for
          if (cpuirequest)
            DrZ80_Cause_Interrupt(cpuirequest);
          else
            DrZ80_Clear_Pending_Interrupts();
      }
      else  // CZ80 core from fMSX()... slower but higher accuracy
      {
          if (memotech_mode || einstein_mode || sordm5_mode)    // All of these have CTC Timers to deal with... 
          {
              // Execute 1 scanline worth of CPU instructions
              cycle_deficit = ExecZ80(tms_cpu_line + cycle_deficit);

              // Refresh VDP 
              if(Loop9918()) 
              {
                  CPU.IRequest = vdp_int_source;   // Sord M5 and Memotech MTX only works with the CZ80 core
              }
              else
              {
                  // -------------------------------------------------------------------------
                  // The Sord M5, Memotech MTX and the Tatung Einstein have a Z80 CTC timer 
                  // circuit that needs attention - this isnt timing accurate but it's good
                  // enough to allow those timers to trigger and the games to be played.
                  // -------------------------------------------------------------------------
                  if (CPU.IRequest == INT_NONE)
                  {
                      Z80CTC_Timer();    
                  }
                  if (einstein_mode && (CPU.IRequest == INT_NONE))  // If the keyboard is generating an interrupt...
                  {
                      einstein_handle_interrupts();
                      if (keyboard_interrupt) CPU.IRequest = keyboard_interrupt;
                  }
              }
          }
          else
          {
              // Execute 1 scanline worth of CPU instructions
              cycle_deficit = ExecZ80(tms_cpu_line + cycle_deficit);

              // Refresh VDP 
              if(Loop9918()) 
              {
                  CPU.IRequest = vdp_int_source;
              }
          }

          // Generate an interrupt if called for...
          if(CPU.IRequest!=INT_NONE) 
          {
              IntZ80(&CPU, CPU.IRequest);
    #ifdef DEBUG_Z80 
              CPU.User++;   // Track Interrupt Requests
    #endif          
              if (pv2000_mode) 
              {
                  extern void pv2000_check_kbd(void);
                  pv2000_check_kbd();
              }
          }
      }
  }
  
  // Drop out unless end of screen is reached 
  if (CurLine == tms_end_line)
  {
      // ------------------------------------------------------------------------------------
      // If the MSX Beeper is being used (rare but a few of the ZX Spectrum ports use it), 
      // then we need to service it here. We basically track the frequency at which the
      // game has hit the beeper and approximate that by using AY Channel A to produce the 
      // tone.  This is crude and doesn't sound quite right... but good enough.
      // ------------------------------------------------------------------------------------
      if (msx_beeper_enabled)
      {
          MSX_HandleBeeper();
      }
      return 0;
  }
  return 1;
}

// End of file
