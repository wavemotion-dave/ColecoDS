// =====================================================================================
// Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
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
#include <sys/stat.h>

#include "printf.h"

#include "colecoDS.h"
#include "Adam.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/z80/ctc.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "MTX_BIOS.h"

// ------------------------------------------------
// Adam RAM is 128K (64K Intrinsic, 64K Expanded)
// DSi will bump the Expanded RAM to 2MB!
// ------------------------------------------------
u8 adam_ext_ram_used   = 0;
u8 sg1000_double_reset = false;
u8 bSuperSimplifiedMemory = 0;

// -----------------------------------------------------------------------
// Used by various systems such as the ADAM and MSX to point to
// different 8K segments of memory as RAM and Carts are swapped in/out.
// -----------------------------------------------------------------------
u8 *MemoryMap[8]        __attribute__((section(".dtcm"))) = {0,0,0,0,0,0,0,0};

// -------------------------------------
// Some IO Port and Memory Map vars...
// -------------------------------------
u16 memotech_RAM_start  __attribute__((section(".dtcm"))) = 0x4000;
u8 svi_RAMinSegment[2]  __attribute__((section(".dtcm"))) = {0,1};
u8 IOBYTE               __attribute__((section(".dtcm"))) = 0x00;
u8 MTX_KBD_DRIVE        __attribute__((section(".dtcm"))) = 0x00;
u8 lastIOBYTE           __attribute__((section(".dtcm"))) = 99;
u32 tape_pos            __attribute__((section(".dtcm"))) = 0;
u32 tape_len            __attribute__((section(".dtcm"))) = 0;
u8 key_shift_hold       __attribute__((section(".dtcm"))) = 0;
u8 spinner_enabled      __attribute__((section(".dtcm"))) = 0;

Z80 CPU __attribute__((section(".dtcm")));      // Put the entire CPU state into fast memory for speed!

// --------------------------------------------------
// Some special ports for the MSX machine emu
// --------------------------------------------------
u8 Port_PPI_A __attribute__((section(".dtcm"))) = 0x00;
u8 Port_PPI_B __attribute__((section(".dtcm"))) = 0x00;
u8 Port_PPI_C __attribute__((section(".dtcm"))) = 0x00;

u8 bIsComplicatedRAM __attribute__((section(".dtcm"))) = 0;   // Set to 1 if we have hotspots or other RAM needs

u8 sg1000_sms_mapper __attribute__((section(".dtcm"))) = 0;   // Set to 1 if this is an SG-1000 game needing the SMS mapper

u8 romBankMask          __attribute__((section(".dtcm"))) = 0x00;
u8 sgm_enable           __attribute__((section(".dtcm"))) = false;
u16 sgm_low_addr        __attribute__((section(".dtcm"))) = 0x2000;
u16 simplifed_low_addr  __attribute__((section(".dtcm"))) = 0x6000;

u8 Port53         __attribute__((section(".dtcm"))) = 0x00;
u8 Port60         __attribute__((section(".dtcm"))) = 0x0F;
u8 Port20         __attribute__((section(".dtcm"))) = 0x00;
u8 Port42         __attribute__((section(".dtcm"))) = 0x00;

u8 bFirstSGMEnable __attribute__((section(".dtcm"))) = true;
u8 AY_Enable       __attribute__((section(".dtcm"))) = false;
u8 ctc_enabled     __attribute__((section(".dtcm"))) = false;
u8 M1_Wait         __attribute__((section(".dtcm"))) = false;

u8  JoyMode        __attribute__((section(".dtcm"))) = 0;           // Joystick Mode (1=Keypad, 0=Joystick)
u32 JoyState       __attribute__((section(".dtcm"))) = 0;           // Joystick State for P1 and P2

// ---------------------------------------------------------------
// We provide 5 "Sensitivity" settings for the X/Y spinner
// ---------------------------------------------------------------
// Hand Tweaked Speeds:                                  Norm   Fast   Fastest  Slow   Slowest
u16 SPINNER_SPEED[] __attribute__((section(".dtcm"))) = {120,   75,    50,      200,   300};

// ------------------------------------------------------------
// Some global vars to track what kind of cart/rom we have...
// ------------------------------------------------------------
u8 bMagicMegaCart __attribute__((section(".dtcm"))) = 0;      // Mega Carts support > 32K
u8 bActivisionPCB __attribute__((section(".dtcm"))) = 0;      // Activision PCB is 64K with EEPROM
u8 bSuperGameCart __attribute__((section(".dtcm"))) = 0;      // Super Game Cart (aka MegaCart2)
u8 sRamAtE000_OK  __attribute__((section(".dtcm"))) = 0;      // Lord of the Dungeon is the only game that needs this
u8 b31_in_1       __attribute__((section(".dtcm"))) = 0;      // 31 in 1 carts are a breed unto themselves

u32 file_crc __attribute__((section(".dtcm")))  = 0x00000000;  // Our global file CRC32 to uniquiely identify this game

// -----------------------------------------------------------
// The two master sound chips... both are mapped to SN sound.
// -----------------------------------------------------------
SN76496 mySN   __attribute__((section(".dtcm")));
AY38910 myAY   __attribute__((section(".dtcm")));


// ------------------------------------------------------------------------
// The Coleco (with or without SGM) and the ADAM have some key ports that
// need setup. We sort that out here and setup for the appopriate system.
// ------------------------------------------------------------------------
void coleco_adam_port_setup(void)
{
    if (adam_mode) // ADAM mode requires special handling of Port60
    {
        Port53 = 0x00;                          // Init the SGM Port 53
        Port60 = (adam_mode == 3) ? 0x0F:0x00;  // Adam/Memory Port 60 is in 'ADAM Mode' (unless mode==3 in which case we are loading a .rom while retaining ADAM emulation)
        Port20 = 0x00;                          // Adam Net Port 20
        Port42 = 0x00;                          // The first Epanded Bank of 64K
    }
    else // Is Colecovision mode .. make sure the SGM ports are correct
    {
        Port53 = 0x00;          // Init the SGM Port 53
        Port60 = 0x0F;          // Adam/Memory Port 60 is in 'Colecovision Mode'
        Port20 = 0x00;          // Adam Net Port 20 not used for CV/SGM mode
        Port42 = 0x00;          // Not used for CV/SGM mode

        if (bSuperGameCart) Port53 = 0x01; // Super Game Carts expect the SGM memory mapped in
    }
}

// ---------------------------------------------------------
// Reset the Super Game Module vars... we reset back to
// SGM disabled and no AY sound chip use to start.
// ---------------------------------------------------------
void sgm_reset(void)
{
    sgm_enable = false;            // Default to no SGM until enabled
    sgm_low_addr = 0x2000;         // And the first 8K is Coleco BIOS
    simplifed_low_addr = 0x6000;   // Normal CV RAM here
    if (!msx_mode && !svi_mode && !einstein_mode)
    {
        AY_Enable = false;         // Default to no AY use until accessed
    }
    bFirstSGMEnable = true;        // First time SGM enable we clear ram

    coleco_adam_port_setup();      // Ensure the memory ports are setup properly
}


/*********************************************************************************
 * Wipe main RAM with random patterns... or fill with 0x00 for some emulations.
 ********************************************************************************/
void colecoWipeRAM(void)
{
  if (sg1000_mode)
  {
      for (int i=0xC000; i<0x10000; i++) RAM_Memory[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (pv2000_mode)
  {
      memset(RAM_Memory+0x4000, 0xFF, 0x8000);
      for (int i=0x7000; i<0x8000; i++) RAM_Memory[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (sordm5_mode)
  {
      for (int i=0x7000; i<0x10000; i++) RAM_Memory[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (memotech_mode)
  {
    for (int i=0; i< 0xC000; i++) RAM_Memory[0x4000+i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (svi_mode)
  {
    memset(RAM_Memory, 0xFF, 0x8000);
    for (int i=0; i< 0x8000; i++) RAM_Memory[0x8000+i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (msx_mode)
  {
    // Do nothing... MSX has all kinds of memory mapping that is handled elsewhere
  }
  else if (creativision_mode)
  {
      for (int i=0x0000; i<0x1000; i++) RAM_Memory[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else if (adam_mode)
  {
      // ----------------------------------------------------------------------------------------------
      // ADAM has special handling for memory clear. Thanks to Shawn Merrick who ran some experiments,
      // it appears the bulk (but not all) of memory is alternating 0x00 (even) and 0xFF (odd) pattern.
      // We replicate that here - it's good enough to run "picky" games like Adam Bomb 2. This is also
      // the pattern that AdamEM uses so it's likely very reasonable to use.
      // ----------------------------------------------------------------------------------------------
      if (myConfig.memWipe)
      {
          // Alternating 0x00 and 0xFF bytes throughout memory...
          for (int i=0; i< 0x10000; i+=2)
          {
              RAM_Memory[i] = 0x00; RAM_Memory[i+1]=0xFF;
          }
      }
      else
      {
          // Randomize all bytes...
          for (int i=0; i< 0x10000; i++)
          {
              RAM_Memory[i] = rand() & 0xFF;
          }
      }

      // The Expanded MEM is always cleared to zeros (helps with compression on save/load state)
      memset(EXP_Memory, 0x00, 0x10000);
      if (DSI_RAM_Buffer) memset(DSI_RAM_Buffer, 0x00, (2*1024*1024));

      RAM_Memory[0x38] = RAM_Memory[0x66] = 0xC9;       // Per AdamEM - put a return at the interrupt locations to solve problems with badly behaving 3rd party software
  }
  else if (einstein_mode)
  {
      for (int i=0; i<0x10000; i++) RAM_Memory[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else  // Normal colecovision which has 1K of RAM and is mirrored (so each mirror gets the same byte)
  {
      for (int i=0; i<0x400; i++)
      {
          u8 randbyte = rand() & 0xFF;
          RAM_Memory[0x6000 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          RAM_Memory[0x6400 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          RAM_Memory[0x6800 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          RAM_Memory[0x6C00 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          RAM_Memory[0x7000 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          RAM_Memory[0x7400 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          RAM_Memory[0x7800 + i] = (myConfig.memWipe ? 0x00 : randbyte);
          RAM_Memory[0x7C00 + i] = (myConfig.memWipe ? 0x00 : randbyte);
      }
  }
}


/*********************************************************************************
 * Keyboard Key Buffering Engine...
 ********************************************************************************/
u8 BufferedKeys[32];
u8 BufferedKeysWriteIdx=0;
u8 BufferedKeysReadIdx=0;
void BufferKey(u8 key)
{
    BufferedKeys[BufferedKeysWriteIdx] = key;
    BufferedKeysWriteIdx = (BufferedKeysWriteIdx+1) % 32;
}

// Buffer a whole string worth of characters...
void BufferKeys(char *str)
{
    for (int i=0; i<strlen(str); i++)  BufferKey((u8)str[i]);
}

// ---------------------------------------------------------------------------------------
// Called every frame... so 1/50th or 1/60th of a second. We will virtually 'press' and
// hold the key for roughly a tenth of a second and be smart about shift keys...
// ---------------------------------------------------------------------------------------
void ProcessBufferedKeys(void)
{
    static u8 next_dampen_time = 5;
    static u8 dampen = 0;
    static u8 buf_held = 0;
    static u8 buf_shift = 0;
    static u8 buf_ctrl = 0;
    if (creativision_mode) return;  // Special handling in creativision.c

    if (++dampen >= next_dampen_time) // Roughly 50ms... experimentally good enough for all systems.
    {
        if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
        {
            buf_held = BufferedKeys[BufferedKeysReadIdx];
            BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
            if (buf_held == KBD_KEY_SHIFT) buf_shift = 2; else {if (buf_shift) buf_shift--;}
            if (buf_held == KBD_KEY_CTRL)  buf_ctrl = 6; else {if (buf_ctrl) buf_ctrl--;}
            if (buf_held == 255) {buf_held = 0; next_dampen_time=60;} else next_dampen_time = (memotech_mode ? 1:5);
        } else buf_held = 0;
        dampen = 0;
    }

    // See if the shift key should be virtually pressed along with this buffered key...
    if (buf_held) {kbd_keys[kbd_keys_pressed++] = buf_held; if (buf_shift) key_shift=1; if (buf_ctrl) key_ctrl=1;}
}


/*********************************************************************************
 * Init coleco Engine for that game
 ********************************************************************************/
u8 colecoInit(char *szGame)
{
  extern u8 bForceMSXLoad;
  u8 RetFct,uBcl;
  u16 uVide;

  // We've got some debug data we can use for development... reset these.
  memset(debug, 0x00, sizeof(debug));


  // See if we have forced any specific modes on loading...
  if (bForceMSXLoad) msx_mode = 1;
  if (msx_mode)      AY_Enable=true;
  if (svi_mode)      AY_Enable=true;
  if (einstein_mode) AY_Enable=true;
  if (msx_mode) BottomScreenKeypad();  // Could Need to ensure the MSX layout is shown

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
  for(u8 J=0;J<MAX_DRIVES;++J) adam_drive_eject(J);

  // Init the page flipping buffer...
  for (uBcl=0;uBcl<192;uBcl++)
  {
     uVide=(uBcl/12);
     dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
  }

  write_NV_counter=0;
  spinner_enabled = false;
  ctc_enabled = false;
  M1_Wait = false;

  if (sg1000_mode)  // Load SG-1000 cartridge
  {
      colecoWipeRAM();                              // Wipe RAM
      RetFct = loadrom(szGame,RAM_Memory);          // Load up to 48K
      sg1000_reset();                               // Reset the SG-1000
  }
  else if (sordm5_mode)  // Load Sord M5 cartridge
  {
      ctc_enabled = true;
      if (file_crc == 0xb32c9e08)  ctc_enabled = 0;    // Sord M5 Mahjong (Jong Kyo) only works without CTC processing (unsure why)
      if (file_crc == 0xa2edc01d)  ctc_enabled = 0;    // Sord M5 Mahjong (Jong Kyo) only works without CTC processing (unsure why)
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory+0x2000);      // Load up to 20K
  }
  else if (pv2000_mode)  // Casio PV-2000 cartridge loads at C000
  {
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory+0xC000);      // Load up to 16K
  }
  else if (creativision_mode)  // Creativision loads cart up against 0xC000
  {
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory+0xC000);      // Load up to 16K
  }
  else if (memotech_mode)  // Load Memotech MTX file
  {
      ctc_enabled = true;
      RetFct = loadrom(szGame,RAM_Memory+0x4000);      // Load up to 48K
  }
  else if (msx_mode)  // Load MSX cartridge ...
  {
      // loadrom() will figure out how big and where to load it... the 0x8000 here is meaningless.
      RetFct = loadrom(szGame,RAM_Memory+0x8000);

      // Wipe RAM area from 0xC000 upwards after ROM is loaded...
      colecoWipeRAM();
      
      // MSX machines have an M1 wait state inserted
      M1_Wait = true;
  }
  else if (svi_mode)  // Load SVI ROM ...
  {
      // loadrom() will figure out how big and where to load it... the 0x8000 here is meaningless.
      RetFct = loadrom(szGame,RAM_Memory+0x8000);

      // Wipe RAM area from 0x8000 upwards after ROM is loaded...
      colecoWipeRAM();
  }
  else if (adam_mode)  // Load Adam DDP or DSK
  {
      spinner_enabled = (myConfig.spinSpeed != 5) ? true:false;
      sgm_reset();                       // Make sure the super game module is disabled to start
      adam_CapsLock = 0;                 // CAPS Lock disabled to start
      disk_unsaved_data[BAY_DISK1] = 0;  // No unsaved DISK data to start
      disk_unsaved_data[BAY_DISK2] = 0;  // No unsaved DISK data to start
      disk_unsaved_data[BAY_TAPE] = 0;   // No unsaved TAPE data to start
      adamnet_init();                    // Initialize the Adam Net and disk drives

      // Clear existing drives of any disks/tapes and load the new game up
      for(u8 J=0;J<MAX_DRIVES;++J) adam_drive_eject(J);

      // Load the game into memory
      RetFct = loadrom(szGame,RAM_Memory);

      RAM_Memory[0x38] = RAM_Memory[0x66] = 0xC9;       // Per AdamEM - put a return at the interrupt locations to solve problems with badly behaving 3rd party software
      
      // Coleco ADAM machines have an M1 wait state inserted
      M1_Wait = true;
  }
  else if (pencil2_mode)
  {
      // Wipe area from BIOS onwards and then wipe RAM to random values below
      memset(RAM_Memory+0x2000, 0xFF, 0xE000);

      // Wipe RAM to Random Values
      colecoWipeRAM();

      RetFct = loadrom(szGame,RAM_Memory+0x8000);    // Load up to 32K
  }
  else if (einstein_mode)  // Load Einstein COM file
  {
      ctc_enabled = true;
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory+0x4000);  // Load up to 48K
  }
  else  // Load coleco cartridge
  {
      spinner_enabled = (myConfig.spinSpeed != 5) ? true:false;

      // Wipe area between BIOS and RAM (often SGM RAM mapped here but until then we are 0xFF)
      memset(RAM_Memory+0x2000, 0xFF, 0x4000);

      // Wipe RAM to Random Values
      colecoWipeRAM();

      // Set upper 32K ROM area to 0xFF before load
      memset(RAM_Memory+0x8000, 0xFF, 0x8000);

      RetFct = loadrom(szGame,RAM_Memory+0x8000);

      // Coleco machines have an M1 wait state inserted
      M1_Wait = true;

      // Flag to let us know we're in simple colecovision mode
      coleco_mode = true;
  }

  if (RetFct)
  {
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
  Z80_Interface_Reset();                // Reset the Z80 Interface module
  ResetZ80(&CPU);                       // Reset the CZ80 core CPU
  BottomScreenKeypad();                 // Show the game-related screen with keypad / keyboard
}

/*********************************************************************************
 * Set coleco Palette
 ********************************************************************************/
void colecoSetPal(void)
{
  u8 uBcl,r,g,b;

  // -----------------------------------------------------------------------
  // The Colecovision has a 16 color pallette... we set that up here.
  // We always use the standard NTSC color palette which is fine for now
  // but maybe in the future we add the PAL color palette for a bit more
  // authenticity.
  // -----------------------------------------------------------------------
  for (uBcl=0;uBcl<16;uBcl++) 
  {
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

          // Because we are OR-ing two frames together, we only need to blend every other frame...
          u32 *p1 = (u32*)XBuf_A;
          u32 *p2 = (u32*)XBuf_B;
          u32 *destP = (u32*)pVidFlipBuf;

          for (u16 i=0; i<(256*192)/4; i++)
          {
              *destP++ = (*p1++ | *p2++);       // Simple OR blending of 2 frames...
          }
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


/*******************************************************************************
 * Compute the file CRC - this will be our unique identifier for the game
 * for saving HI SCORES and Configuration / Key Mapping data.
 *******************************************************************************/
void getfile_crc(const char *filename)
{
    DSPrint(11,13,6, "LOADING...");

    file_crc = getFileCrc(filename);        // The CRC is used as a unique ID to save out High Scores and Configuration...
    
    DSPrint(11,13,6, "          ");

    // -----------------------------------------------------------------
    // Only Lord of the Dungeon allows SRAM writting in this area...
    // -----------------------------------------------------------------
    sRamAtE000_OK = 0;
    if (file_crc == 0xfee15196) sRamAtE000_OK = 1;      // 32K version of Lord of the Dungeon
    if (file_crc == 0x1053f610) sRamAtE000_OK = 1;      // 24K version of Lord of the Dungeon

    // ------------------------------------------------------------------------------
    // And a handful of games require SRAM which is a special case-by-case basis...
    // ------------------------------------------------------------------------------
    msx_sram_enabled = 0;
    if (file_crc == 0x92943e5b) msx_sram_enabled = 0x10;       // MSX Hydlide 2 - Shine Of Darkness (EN)
    if (file_crc == 0xb29edaec) msx_sram_enabled = 0x10;       // MSX Hydlide 2 - Shine Of Darkness (EN)
    if (file_crc == 0xa0fd57cf) msx_sram_enabled = 0x10;       // MSX Hydlide 2 - Shine Of Darkness (EN)
    if (file_crc == 0xd640deaf) msx_sram_enabled = 0x20;       // MSX Dragon Slayer 2 - Xanadu (EN)
    if (file_crc == 0x119b7ba8) msx_sram_enabled = 0x20;       // MSX Dragon Slayer 2 - Xanadu (JP)
    if (file_crc == 0x27fd8f9a) msx_sram_enabled = 0x10;       // MSX Deep Dungeon I (JP)
    if (file_crc == 0x213da247) msx_sram_enabled = 0x10;       // MSX Deep Dungeon II (EN)
    if (file_crc == 0x101db19c) msx_sram_enabled = 0x10;       // MSX Deep Dungeon II (JP)
    if (file_crc == 0x96b7faca) msx_sram_enabled = 0x10;       // MSX Harry Fox Special (JP)
    if (file_crc == 0xb8fc19a4) msx_sram_enabled = 0x20;       // MSX Cosmic Soldier 2 - Psychic War
    if (file_crc == 0x4ead5098) msx_sram_enabled = 0x20;       // MSX Ghengis Khan
    if (file_crc == 0x3aa33a30) msx_sram_enabled = 0x20;       // MSX Nobunaga no Yabou - Zenkokuhan
}


/** loadrom() ******************************************************************/
/* Open a rom file from file system and load it into the ROM_Memory[] buffer   */
/*******************************************************************************/
u8 loadrom(const char *filename, u8 * ptr)
{
  u8 bOK = 0;
  int romSize = 0;

  bSuperSimplifiedMemory = 0;   // Assume the normal driver unless proven otherwise further below...

  FILE* handle = fopen(filename, "rb");
  if (handle != NULL)
  {
    // Save the initial filename and file - we need it for save/restore of state
    strcpy(initial_file, filename);
    getcwd(initial_path, MAX_ROM_NAME);

    // Get file size the 'fast' way - use fstat() instead of fseek() or ftell()
    struct stat stbuf;
    (void)fstat(fileno(handle), &stbuf);
    romSize = stbuf.st_size;
    
    bMagicMegaCart = false;     // No Mega Cart to start
    bActivisionPCB = 0;         // No Activision PCB
    bSuperGameCart = 0;         // No Super Game Cart (aka MegaCart2)
    b31_in_1 = 0;               // No 31-in-1 Cart

    // ----------------------------------------------------------------------
    // Look for the Survivors .sc Multicart  (2MB!) or .sc MegaCart (4MB!)
    // ----------------------------------------------------------------------
    sg1000_double_reset = false;
    if (sg1000_mode && ((romSize == (2048 * 1024)) || (romSize == (4096 * 1024))))
    {
        if (isDSiMode())
        {
            memcpy(RAM_Memory, ROM_Memory + (romSize - 0x8000), 0x8000);   // And put the last block directly into the RAM buffer
        }
        else // For DS-Lite/Phat, we can only read this in smaller chunks - slower
        {
            fseek(handle, romSize-0x8000, SEEK_SET);              // Seek to the last 32K block (this is the menu system)
            fread((void*) RAM_Memory, 1, 0x8000, handle);         // Read 32K from that last block directly into the RAM buffer
            memcpy(ROM_Memory, RAM_Memory, 0x8000);               // And save the last block so we can switch back as needed...
            strcpy(disk_last_file[0], filename);
            strcpy(disk_last_path[0], initial_path);
        }
        fclose(handle);
        romBankMask = (romSize == (2048 * 1024) ? 0x3F:0x7F);
        sg1000_double_reset = true;
        machine_mode = MODE_SG_1000;
        return 1;
    }
    else if (myConfig.cvMode == CV_MODE_31IN1) // These are special 32K mappers for large 31-in-1 or 63-in-1 carts
    {
        b31_in_1 = 1;
        fseek(handle, romSize-0x8000, SEEK_SET);              // Seek to the last 32K block (this is the menu system)
        fread((void*) RAM_Memory+0x8000, 1, 0x8000, handle);  // Read 32K from that last block directly into the RAM buffer
        fclose(handle);
        strcpy(disk_last_file[0], filename);
        strcpy(disk_last_path[0], initial_path);
        romBankMask = (romSize == (1024 * 1024) ? 0x1F:0x3F);
        bIsComplicatedRAM = true;
        machine_mode = MODE_COLECO;
        return 1;
    }
    else
    if (romSize <= (MAX_CART_SIZE * 1024))  // Max size cart is 1MB/4MB - that's pretty huge...
    {
        fclose(handle); // We only need to close the file - the game ROM is now sitting in ROM_Memory[] from the getFileCrc() handler

        romBankMask = 0x00;         // No bank mask until proven otherwise
        mapperMask = 0x00;          // No MSX mapper mask

        // The SordM5 has one game that needs patching to run...
        if (sordm5_mode)
        {
            if (file_crc == 0x68c85890) // M5 Up Up Balloon needs a patch to add 0x00 at the front
            {
                for (u16 i=romSize; i>0; i--)
                {
                    ROM_Memory[i] = ROM_Memory[i-1];  // Shift everything up 1 byte
                }
                ROM_Memory[0] = 0x00;    // Add 0x00 to the first byte which is the patch
                romSize++;               // Make sure the size is now correct
            }
        }

        // ------------------------------------------------------------------------------
        // For the MSX emulation, we setup the initial memory map based on ROM size
        // ------------------------------------------------------------------------------
        if (msx_mode)
        {
            tape_len = romSize;  // For MSX, the tape size is saved for showing tape load progress
            tape_pos = 0;
            last_tape_pos = 9999;
            MSX_InitialMemoryLayout(romSize);
        }
        // ---------------------------------------------------------------------------
        // For ADAM mode, we need to setup the memory banks and tape/disk access...
        // ---------------------------------------------------------------------------
        else if (adam_mode)
        {
            coleco_adam_port_setup();    // Ensure the memory ports are setup properly
            adam_ext_ram_used = 0;       // Normal 64K ADAM to start
            SetupAdam(false);            // And make sure the ADAM is ready

            strcpy(disk_last_file[BAY_DISK1], "");   // Nothing loaded in the DISK drive yet
            strcpy(disk_last_file[BAY_DISK2], "");   // Nothing loaded in the DISK drive yet
            strcpy(disk_last_file[BAY_TAPE], "");    // Nothing loaded in the TAPE drive yet

            strcpy(disk_last_path[BAY_DISK1], "");   // Nothing loaded in the DISK drive yet
            strcpy(disk_last_path[BAY_DISK2], "");   // Nothing loaded in the DISK drive yet
            strcpy(disk_last_path[BAY_TAPE], "");    // Nothing loaded in the TAPE drive yet

            disk_last_size[BAY_DISK1] = 0;           // Nothing loaded in the DISK drive yet
            disk_last_size[BAY_DISK2] = 0;           // Nothing loaded in the DISK drive yet
            disk_last_size[BAY_TAPE]  = 0;           // Nothing loaded in the TAPE drive yet

            // ------------------------------------------
            // The .ddp or .dsk is now in ROM_Memory[]
            // We need to convert this to an FDID image
            // for use with the core emulation.
            // ------------------------------------------
            if ((strcasecmp(strrchr(filename, '.'), ".ddp") == 0))  // Is this a TAPE image (.ddp)?
            {
                // Insert the tape into the virtual TAPE drive
                strcpy(disk_last_file[BAY_TAPE], filename);
                strcpy(disk_last_path[BAY_TAPE], initial_path);
                disk_last_size[BAY_TAPE] = romSize;
                adam_drive_insert(BAY_TAPE, (char*)filename);
            }
            else if ((strcasecmp(strrchr(filename, '.'), ".dsk") == 0))  // Is this a DISK image (.dsk)?
            {
                // Insert the disk into the virtual DISK drive
                strcpy(disk_last_file[BAY_DISK1], filename);
                strcpy(disk_last_path[BAY_DISK1], initial_path);
                disk_last_size[BAY_DISK1] = romSize;
                adam_drive_insert(BAY_DISK1, (char*)filename);
            }
            else if (adam_mode >= 2) // else must be a ROM which is okay...
            {
                memcpy(ROM_Memory + (992*1024), ROM_Memory, 0x8000); // Copy 32K to back end of ROM Memory
                memset(ROM_Memory, 0xFF, (992*1024));
            }
        }
        else if (memotech_mode || svi_mode)     // Can be any size tapes... up to 1024K
        {
            tape_len = romSize;  // The tape size is saved for showing tape load progress
            tape_pos = 0;
            last_tape_pos = 9999;
        }
        else if (einstein_mode)
        {
            strcpy(disk_last_file[0], filename);
            strcpy(disk_last_path[0], initial_path);
            disk_last_size[0] = romSize;    // Might be a .COM file but we just reuse the einstein disk size variable
            if (romSize == 1626)            // A bit of a hack... the size of the Diagnostic ROM
            {
                extern u8 EinsteinBios2[];
                memcpy(EinsteinBios2, ROM_Memory, romSize);   // only for Diagnostics ROM
            }
        }
        else if (creativision_mode)
        {
            strcpy(disk_last_file[0], filename);
            strcpy(disk_last_path[0], initial_path);
            creativision_loadrom(romSize);
        }
        else if (sg1000_mode)
        {
            sg1000_sms_mapper = 0;
            if (romSize <= (48*1024))
            {
                memcpy(ptr, ROM_Memory, romSize);     // Copy up to 48K flat into our memory map...
            }
            else // Assume this is one of the rare SMS memory mapper SG-1000 games unless it's a Survivors Cart
            {
                memcpy(ptr, ROM_Memory, 48*1024);     // Copy exactly 48K flat into our memory map... larger than this is the SMS mapper handled directly below
                if      (romSize <= 128*1024) sg1000_sms_mapper = 0x07;   // Up to 128K
                else if (romSize <= 256*1024) sg1000_sms_mapper = 0x0F;   // Up to 256K
                else if (romSize <= 512*1024) sg1000_sms_mapper = 0x1F;   // Up to 512K
                else                          sg1000_sms_mapper = 0x00;   // It's probably one of the Multi/Mega Survivors Carts. No mapper.
            }
        }
        else
        // ----------------------------------------------------------------------
        // Do we fit within the standard 32K Colecovision Cart ROM memory space?
        // ----------------------------------------------------------------------
        if (romSize <= (32*1024)) // Allow up to 32K limit on Coleco Roms
        {
            memcpy(ptr, ROM_Memory, romSize);
        }
        else    // No - must be Mega Cart (MC) Bankswitched!!
        {
            // --------------------------------------------------------------
            // Mega Carts have a special byte pattern in the upper block...
            // but we need to distinguish between 64k Activision PCB and
            // possible 64K Megacart (theoretically MC should be 128K+ but
            // there are examples of 64K MegaCarts). This code does that...
            // --------------------------------------------------------------
            bMagicMegaCart = ((ROM_Memory[0xC000] == 0x55 && ROM_Memory[0xC001] == 0xAA) ? 1:0);
            last_mega_bank = 199;                                 // Force load of the first bank when asked to bankswitch
            if ((myConfig.cvMode == CV_MODE_ACTCART) || ((romSize == (64 * 1024)) && !bMagicMegaCart)) // Some 64K carts are in the 'Activision PCB' style with EEPROM
            {
                bActivisionPCB = 1;
                memcpy(ptr, ROM_Memory, 0x4000);                     // bank 0
                memcpy(ptr+0x4000, ROM_Memory+0x4000, 0x4000);       // bank 1
                romBankMask = 0x03;
            }
            else if (myConfig.cvMode == CV_MODE_SUPERCART) // These are the Super Game Cart types... of varying EE sizes
            {
                bSuperGameCart = 1;
                SuperGameCartSetup(romSize);
                memcpy(ptr, ROM_Memory, 0x2000);
            }
            else    // We will assume Megacart for everything else...
            {
                bMagicMegaCart = 1;
                memcpy(ptr, ROM_Memory+(romSize-0x4000), 0x4000); // For MegaCart, we map highest 16K bank into fixed ROM
                MegaCartBankSwap(0);                              // Copy Bank 0 into RAM memory in case we're using the optimized driver
                MegaCartBankSwitch(0);                            // The initial 16K "switchable" bank is bank 0 (based on a post from Nanochess in AA forums)

                if      (romSize <= (64  * 1024))  romBankMask = 0x03;    // 64K   Megacart (unlikely this exists in Hardware but it's supported)
                else if (romSize <= (128 * 1024))  romBankMask = 0x07;    // 128K  Megacart
                else if (romSize <= (256 * 1024))  romBankMask = 0x0F;    // 256K  Megacart
                else if (romSize <= (512 * 1024))  romBankMask = 0x1F;    // 512K  Megacart
                else                               romBankMask = 0x3F;    // 1024K Megacart - this is as big as any MC board supports given that the hotspots are FFC0 to FFFF
            }
        }
        bOK = 1;
    }
    else fclose(handle);

    // -------------------------------------------------------------------------
    // For some combinations, we have hotspots or other memory stuff that
    // needs to be more complicated than simply returning RAM_Memory[].
    // -------------------------------------------------------------------------
    bIsComplicatedRAM = (b31_in_1 || bMagicMegaCart || bActivisionPCB || bSuperGameCart || adam_mode || msx_sram_enabled || pv2000_mode) ? 1:0;  // Set to 1 if we have to do more than just simple memory read...

    // -----------------------------------------------------------------------
    // To speed up processing in the memory write functions, we accumulate
    // the bits so we only have to fetch one machine_mode variable.
    // -----------------------------------------------------------------------
    if      (pencil2_mode)      machine_mode = MODE_PENCIL2;
    else if (msx_mode)          machine_mode = MODE_MSX;
    else if (svi_mode)          machine_mode = MODE_SVI;
    else if (einstein_mode)     machine_mode = MODE_EINSTEIN;
    else if (memotech_mode)     machine_mode = MODE_MEMOTECH;
    else if (pv2000_mode)       machine_mode = MODE_PV2000;
    else if (sordm5_mode)       machine_mode = MODE_SORDM5;
    else if (sg1000_mode)       machine_mode = MODE_SG_1000;
    else if (adam_mode)         machine_mode = MODE_ADAM;
    else if (creativision_mode) machine_mode = MODE_CREATIVISION;
    else                        machine_mode = MODE_COLECO;
  }
  
  // -------------------------------------------------------------
  // If we are a DS-Lite/Phat, we can swap in a simplified memory 
  // driver to help speedup games that are 32K or less...
  // -------------------------------------------------------------
  if (!isDSiMode() && (machine_mode == MODE_COLECO))
  {
      u8 *fastROM = (u8*) (0x06860000);
      memcpy(fastROM, ROM_Memory, (256 * 1024));
      
      // Turn on the optimized Colecovision CPU driver for carts less than 32K by default
      bSuperSimplifiedMemory = ((!bActivisionPCB && !bSuperGameCart && !adam_mode && (romSize <= (32*1024))) ? 1:0);
      
    
      // And then selectively turn on the optimized driver for some of the larger games...       
      if (file_crc == 0x851efe57) bSuperSimplifiedMemory = 1;  // 1942
      if (file_crc == 0x91636750) bSuperSimplifiedMemory = 1;  // Adventure Island
      if (file_crc == 0x6068db13) bSuperSimplifiedMemory = 1;  // Alpharoid SGM
      if (file_crc == 0xc4f1a85a) bSuperSimplifiedMemory = 1;  // Buck Rogers Super Game
      if (file_crc == 0x257a2565) bSuperSimplifiedMemory = 1;  // Bull And Mighty's Critical Moment
      if (file_crc == 0x55b36d53) bSuperSimplifiedMemory = 1;  // Children of the Night SGM
      if (file_crc == 0x77900970) bSuperSimplifiedMemory = 1;  // Deep Dungeon Adventure
      if (file_crc == 0x3b434ec2) bSuperSimplifiedMemory = 1;  // Donkey Kong 3
      if (file_crc == 0x45345709) bSuperSimplifiedMemory = 1;  // Donkey Kong Arcade SGM
      if (file_crc == 0x12ceee08) bSuperSimplifiedMemory = 1;  // Dragons Lair SGM
      if (file_crc == 0x2488ca1a) bSuperSimplifiedMemory = 1;  // Eggerland Mystery SGM
      if (file_crc == 0x652d533e) bSuperSimplifiedMemory = 1;  // Gauntlet
      if (file_crc == 0xfc935cdd) bSuperSimplifiedMemory = 1;  // Ghostbusters
      if (file_crc == 0xd55bbb66) bSuperSimplifiedMemory = 1;  // Ghost
      if (file_crc == 0x01581fa8) bSuperSimplifiedMemory = 1;  // Goonies
      if (file_crc == 0x2b0bb712) bSuperSimplifiedMemory = 1;  // Guardic SGM
      if (file_crc == 0x083d13ee) bSuperSimplifiedMemory = 1;  // Gun Fright SGM
      if (file_crc == 0x01cacd0d) bSuperSimplifiedMemory = 1;  // Knightmare SGM
      if (file_crc == 0xa078f273) bSuperSimplifiedMemory = 1;  // Kung-Fu Master
      if (file_crc == 0xb11a6d23) bSuperSimplifiedMemory = 1;  // L'Abbaye des Morts
      if (file_crc == 0x53da40bc) bSuperSimplifiedMemory = 1;  // Mecha 8      
      if (file_crc == 0x318d6bcb) bSuperSimplifiedMemory = 1;  // Mobile Planet Styllus
      if (file_crc == 0x13d53b3c) bSuperSimplifiedMemory = 1;  // Mr. Do! Run Run SGM
      if (file_crc == 0xd3ea5876) bSuperSimplifiedMemory = 1;  // Mr. Do's Wild Ride
      if (file_crc == 0x4657bb8c) bSuperSimplifiedMemory = 1;  // Nether Dungeon
      if (file_crc == 0xf3ccacb3) bSuperSimplifiedMemory = 1;  // Pac-Man Collection
      if (file_crc == 0xee530ad2) bSuperSimplifiedMemory = 1;  // Qbiqs SGM
      if (file_crc == 0xb6df4148) bSuperSimplifiedMemory = 1;  // Quatre
      if (file_crc == 0xb9788f47) bSuperSimplifiedMemory = 1;  // Raid On Bungeling Bay
      if (file_crc == 0xb753a8ca) bSuperSimplifiedMemory = 1;  // Secret of the Moai SGM
      if (file_crc == 0xbb0f6678) bSuperSimplifiedMemory = 1;  // Space Shuttle
      if (file_crc == 0x75f84889) bSuperSimplifiedMemory = 1;  // Spelunker SGM
      if (file_crc == 0x3e7d0520) bSuperSimplifiedMemory = 1;  // Star Soldier SGM
      if (file_crc == 0x342c73ca) bSuperSimplifiedMemory = 1;  // Stone of Wisdom SGM
      if (file_crc == 0x02a600cd) bSuperSimplifiedMemory = 1;  // Suite Macabre
      if (file_crc == 0x5b96145e) bSuperSimplifiedMemory = 1;  // Tank Mission
      if (file_crc == 0x09e3fdda) bSuperSimplifiedMemory = 1;  // Thexder SGM
      if (file_crc == 0xe7e07a70) bSuperSimplifiedMemory = 1;  // Twinbee SGM
      if (file_crc == 0xbc8320a0) bSuperSimplifiedMemory = 1;  // Uridium
      if (file_crc == 0xd9207f30) bSuperSimplifiedMemory = 1;  // Wizard of Wor SGM
      if (file_crc == 0xe290a941) bSuperSimplifiedMemory = 1;  // Zanac SGM
      if (file_crc == 0xa5a90f63) bSuperSimplifiedMemory = 1;  // Zaxxon Super Game 
      if (file_crc == 0x8027dad7) bSuperSimplifiedMemory = 1;  // Zombie Incident
      if (file_crc == 0xc89d281d) bSuperSimplifiedMemory = 1;  // Zombie Near
      
      // If the user has enabled mirrors, we can't use the simplified driver
      if (myConfig.mirrorRAM) bSuperSimplifiedMemory = 0;
  }
  
  return bOK;
}

// --------------------------------------------------------------------------
// Based on writes to Port53 and Port60 we configure the SGM handling of
// memory... this includes 24K vs 32K of RAM (the latter is BIOS disabled).
// --------------------------------------------------------------------------
u8 under_ram[0x2000];
__attribute__ ((noinline)) void SetupSGM(void)
{
    if (adam_mode) return;                          // ADAM has its own setup handler
    if (myConfig.cvMode == CV_MODE_NOSGM) return;   // There are a couple of games were we don't want to enable the SGM. Most notably Super DK won't play with SGM emulation.

    sgm_enable = (Port53 & 0x01) ? true:false;  // Port 53 lowest bit dictates SGM memory support enable.

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
          sgm_low_addr = 0x2000;
          simplifed_low_addr = sgm_low_addr;
          MemoryMap[0] = BIOS_Memory + 0x0000;
          
          if (bSuperSimplifiedMemory)
          {
              memcpy(under_ram, RAM_Memory, 0x2000);
              memcpy(RAM_Memory, BIOS_Memory, 0x2000);
          }
      }
    }
    else
    {
      if (sgm_low_addr != 0x0000)
      {
          sgm_low_addr = 0x0000;
          simplifed_low_addr = sgm_low_addr;
          MemoryMap[0] = RAM_Memory + 0x0000;
          
          if (bSuperSimplifiedMemory)
          {
              memcpy(RAM_Memory, under_ram, 0x2000);
          }
      }
    }
    
    // ----------------------------------------------------------------
    // The first time we enable the SGM expansion RAM, we clear it out
    // ----------------------------------------------------------------
    if (sgm_enable && bFirstSGMEnable)
    {
        simplifed_low_addr = sgm_low_addr;
        memset(RAM_Memory+0x2000, 0x00, 0x6000);
        bFirstSGMEnable = false;
    }    
}


unsigned char cpu_readport_pencil2(register unsigned short Port)
{
    u8 key = 0x00;
    // PencilII ports are 8-bit
    Port &= 0x00FF;

    if ((Port & 0xE0) == 0xE0)
    {
        switch(Port)
        {
            case 0xE0: // Joystick/Keypad Data
              Port = (Port&0x02) ? (JoyState>>16):JoyState;
              Port = JoyMode     ? (Port>>8):Port;
              return(~Port&0x7F);

            case 0xE1: // Keyboard Data
              if (kbd_key == 'J')           key |= 0x01;
              if (kbd_key == ',')           key |= 0x02;
              if (kbd_key == '.')           key |= 0x04;
              if (kbd_key == 'M')           key |= 0x08;
              if (kbd_key == 'F')           key |= 0x20;
              if (kbd_key == 'N')           key |= 0x40;
              return(~key);

            case 0xE3: // Keyboard Data
              if (kbd_key == ' ')           key |= 0x04;
              if (kbd_key == 'C')           key |= 0x08;
              if (kbd_key == 'B')           key |= 0x20;
              if (kbd_key == 'H')           key |= 0x40;
              if (key_ctrl)                 key |= 0x01;
              if (key_shift)                key |= 0x10;
              return(~key);

            case 0xE4: // Keyboard Data
              if (kbd_key == KBD_KEY_RET)   key |= 0x01;
              if (kbd_key == 'O')           key |= 0x02;
              if (kbd_key == 'P')           key |= 0x04;
              if (kbd_key == 'I')           key |= 0x08;
              if (kbd_key == '4')           key |= 0x20;
              if (kbd_key == 'T')           key |= 0x40;
              return(~key);

            case 0xE6: // Keyboard Data
              if (kbd_key == 'Q')           key |= 0x01;
              if (kbd_key == 'W')           key |= 0x02;
              if (kbd_key == 'X')           key |= 0x04;
              if (kbd_key == 'E')           key |= 0x08;
              if (kbd_key == '7')           key |= 0x10;
              if (kbd_key == '5')           key |= 0x20;
              if (kbd_key == '6')           key |= 0x40;
              return(~key);

            case 0xE8: // Keyboard Data
              if (kbd_key == ':')           key |= 0x01;
              if (kbd_key == 'L')           key |= 0x02;
              if (kbd_key == ';')           key |= 0x04;
              if (kbd_key == 'K')           key |= 0x08;
              if (kbd_key == 'R')           key |= 0x20;
              if (kbd_key == 'G')           key |= 0x40;
              return(~key);

            case 0xEA: // Keyboard Data
              if (kbd_key == 'Z')           key |= 0x01;
              if (kbd_key == 'A')           key |= 0x02;
              if (kbd_key == 'S')           key |= 0x04;
              if (kbd_key == 'D')           key |= 0x08;
              if (kbd_key == 'U')           key |= 0x10;
              if (kbd_key == 'V')           key |= 0x20;
              if (kbd_key == 'Y')           key |= 0x40;
              return(~key);

            case 0xF0: // Keyboard Data
              if (kbd_key == '-')           key |= 0x01;
              if (kbd_key == '9')           key |= 0x02;
              if (kbd_key == '0')           key |= 0x04;
              if (kbd_key == '8')           key |= 0x08;
              return(~key);

            case 0xF2: // Keyboard Data
              if (kbd_key == '1')           key |= 0x01;
              if (kbd_key == '2')           key |= 0x02;
              if (kbd_key == '3')           key |= 0x04;
              if (kbd_key == KBD_KEY_F1)    key |= 0x08;
              return(~key);
        }
    }
    else
    {
        switch(Port & 0xE0)
        {
            case 0x20:
              return Port20 & 0x0F;
              break;

            case 0x40: // Printer Status - not used
              return(0xFF);
              break;

            case 0x60:  // Memory Port - probably not used on Pencil2
              return Port60;
              break;

            case 0xA0: /* VDP Status/Data */
              return(Port&0x01 ? RdCtrl9918():RdData9918());
        }
    }

    // No such port
    return(NORAM);
}

/** InZ80() **************************************************/
/** Z80 emulation calls this function to read a byte from   **/
/** a given I/O port.                                       **/
/*************************************************************/
ITCM_CODE unsigned char cpu_readport16(register unsigned short Port)
{
  if (machine_mode & (MODE_MSX | MODE_SG_1000 | MODE_SORDM5 | MODE_PV2000 | MODE_MEMOTECH | MODE_SVI | MODE_EINSTEIN | MODE_PENCIL2))
  {
      if (machine_mode & MODE_MSX)      {return cpu_readport_msx(Port);}
      if (machine_mode & MODE_SG_1000)  {return cpu_readport_sg(Port);}
      if (machine_mode & MODE_SORDM5)   {return cpu_readport_m5(Port);}
      if (machine_mode & MODE_PV2000)   {return cpu_readport_pv2000(Port);}
      if (machine_mode & MODE_MEMOTECH) {return cpu_readport_memotech(Port);}
      if (machine_mode & MODE_SVI)      {return cpu_readport_svi(Port);}
      if (machine_mode & MODE_EINSTEIN) {return cpu_readport_einstein(Port);}
      if (machine_mode & MODE_PENCIL2)  {return cpu_readport_pencil2(Port);}
  }

  // Colecovision ports are 8-bit
  Port &= 0x00FF;

  // Port 52 is used for the AY sound chip for the Super Game Module
  if (Port == 0x52)
  {
      return ay38910DataR(&myAY);
  }

  switch(Port&0xE0)
  {
    case 0x20:  // AdamNet Port from 0x20 to 0x3F
      return Port20 & 0x0F;
      break;

    case 0x40: // Printer Status - not used
      return(0xFF);
      break;

    case 0x60:  // Adam/Memory Port from 0x60 to 0x7F
      return Port60;
      break;

    case 0xE0: // Joystick/Keypad Data
      Port = (Port&0x02) ? (JoyState>>16):JoyState;
      Port = JoyMode     ? (Port>>8):Port;
      return(~Port&0x7F);

    case 0xA0: // VDP Status/Data Port from 0xA0 to 0xBF
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
  if (machine_mode & (MODE_MSX | MODE_SG_1000 | MODE_SORDM5 | MODE_PV2000 | MODE_MEMOTECH | MODE_SVI | MODE_EINSTEIN))
  {
      if (machine_mode & MODE_MSX)      {cpu_writeport_msx(Port, Value);      return;}
      if (machine_mode & MODE_SG_1000)  {cpu_writeport_sg(Port, Value);       return;}
      if (machine_mode & MODE_SORDM5)   {cpu_writeport_m5(Port, Value);       return;}
      if (machine_mode & MODE_PV2000)   {cpu_writeport_pv2000(Port, Value);   return;}
      if (machine_mode & MODE_MEMOTECH) {cpu_writeport_memotech(Port, Value); return;}
      if (machine_mode & MODE_SVI)      {cpu_writeport_svi(Port, Value);      return;}
      if (machine_mode & MODE_EINSTEIN) {cpu_writeport_einstein(Port, Value); return;}
  }

  // VDP data write is the most common - handle it first
  if ((Port&0xE1) == 0xA0)
  {
      WrData9918(Value);
      return;
  }
  
  // Colecovision ports are 8-bit
  Port &= 0x00FF;

  // ---------------------------------------------------------------------------
  // Now handle the rest of the CV ports - this handles the mirroring of
  // port writes - for example, a write to port 0x7F will hit 0x60 Memory Port
  // ---------------------------------------------------------------------------
  switch(Port&0xE0)
  {
    case 0x80:  // Ports 80-9F: Set Joystick Read Mode
      JoyMode=JOYMODE_JOYSTICK;
      return;
    case 0xC0:  // Ports C0-DF: Set Keypad Read Mode
      JoyMode=JOYMODE_KEYPAD;
      return;
    case 0xE0:  // Ports E0-FF: The SN Sound port
      sn76496W(Value, &mySN);
      return;
    case 0xA0: // We know it's a VDP control write as data writes are trapped above
      if (WrCtrl9918(Value)) { CPU.IRequest=INT_NMI;}
      return;
    case 0x40:  // Ports 40-5F: SGM/AY port and ADAM expanded memory
      // -----------------------------------------------
      // Port 50 is the AY sound chip register index...
      // -----------------------------------------------
      if (Port == 0x50)
      {
          if (!AY_Enable) AY_Enable = ((Value & 0x0F) == 0x07);
          ay38910IndexW(Value&0xF, &myAY);
      }
      // -----------------------------------------------
      // Port 51 is the AY Sound chip register write...
      // -----------------------------------------------
      else if (Port == 0x51)
      {
        ay38910DataW(Value, &myAY);
      }
      // -----------------------------------------------
      // Port 42 is the Expanded Memory for the ADAM
      // -----------------------------------------------
      else if (Port == 0x42)
      {
          if (isDSiMode())
          {
              Port42 = Value & 0x1F;        // 2MB worth of banks (32 banks of 64K)
              if (adam_mode) SetupAdam(false);
          }
          else Port42 = 0x00; // No extra banking for DS-Lite/Phat (just the stock 64K plus 64K expansion)
      }
      // -----------------------------------------------------------------
      // Port 53 is used for the Super Game Module to enable SGM mode...
      // -----------------------------------------------------------------
      else if (Port == 0x53 && !adam_mode) {Port53 = Value; SetupSGM();}
      
      return;
    case 0x20:  // Ports 20-3F:  AdamNet port
      bool resetAdamNet = (Port20 & 1) && ((Value & 1) == 0);
      Port20 = Value;
      if (adam_mode) SetupAdam(resetAdamNet); else SetupSGM();
      return;
    case 0x60:  // Ports 60-7F: Adam/Memory and SGM setup port
      Port60 = Value;
      if (adam_mode) SetupAdam(false); else SetupSGM();
      return;
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

  // ----------------------------------------------------------------------------
  // Special system as it runs an m6502 CPU core and is different than the Z80
  // ----------------------------------------------------------------------------
  if (creativision_mode)
  {
      creativision_run();
  }
  else
  {
      // ------------------------------------------------------------------
      // Before we execute Z80 or Loop the 9918 (both of which can cause
      // NMI interrupt to occur), we check and adjust the spinners which
      // can generate a lower priority interrupt to the running Z80 code.
      // ------------------------------------------------------------------
      if (spinner_enabled)
      {
          if ((++spinnerDampen % SPINNER_SPEED[myConfig.spinSpeed]) == 0)
          {
              if (spinX_left)
              {
                  CPU.IRequest=INT_RST38;       // The CZ80 way of requesting interrupt
                  JoyState   &= 0xFFFFCFFF;
                  JoyState   |= 0x00003000;
              }
              else if (spinX_right)
              {
                  CPU.IRequest=INT_RST38;       // The CZ80 way of requesting interrupt
                  JoyState   &= 0xFFFFCFFF;
                  JoyState   |= 0x00001000;
              }

              if (spinY_left)
              {
                  CPU.IRequest=INT_RST38;       // The CZ80 way of requesting interrupt
                  JoyState   &= 0xCFFFFFFF;
                  JoyState   |= 0x30000000;
              }
              else if (spinY_right)
              {
                  CPU.IRequest=INT_RST38;       // The CZ80 way of requesting interrupt
                  JoyState   &= 0xCFFFFFFF;
                  JoyState   |= 0x10000000;
              }
          }
      }

      // Execute 1 scanline worth of CPU instructions
      u32 cycles_to_process = tms_cpu_line + CPU.CycleDeficit;
      if (bSuperSimplifiedMemory) CPU.CycleDeficit = ExecZ80_Simplified(cycles_to_process);
      else CPU.CycleDeficit = ExecZ80(cycles_to_process);
      

      // Refresh VDP
      if(Loop9918())
      {
          CPU.IRequest = vdp_int_source;    // Use the proper VDP interrupt souce (set in TMS9918 init)
      }
      else if (ctc_enabled)
      {
          // -------------------------------------------------------------------------
          // The Sord M5, Memotech MTX and the Tatung Einstein have a Z80 CTC timer
          // circuit that needs attention - this isnt timing accurate but it's good
          // enough to allow those timers to trigger and the games to be played.
          // -------------------------------------------------------------------------
          if (CPU.IRequest == INT_NONE)
          {
              CTC_Timer(cycles_to_process);
          }
          if (CPU.IRequest == INT_NONE)
          {
              if (einstein_mode)  // For Einstein, check if the keyboard is generating an interrupt...
              {
                  einstein_handle_interrupts();
                  if (keyboard_interrupt) CPU.IRequest = keyboard_interrupt;
                  else if (joystick_interrupt) CPU.IRequest = joystick_interrupt;
              }
              else if (sordm5_mode)  // For Sord M5, check if the keyboard is generating an interrupt...
              {
                  CPU.IRequest = keyboard_interrupt;    // This will either be INT_NONE or the CTC interrupt for a keypress... set in sordm5_check_keyboard_interrupt()
                  keyboard_interrupt = INT_NONE;
              }
          }
      }

      // Generate an interrupt if called for...
      if(CPU.IRequest!=INT_NONE)
      {
          IntZ80(&CPU, CPU.IRequest);
          CPU.User++;   // Track Interrupt Requests
          if (pv2000_mode)
          {
              extern void pv2000_check_kbd(void);
              pv2000_check_kbd();
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
      if (myConfig.msxBeeper)
      {
          if (msx_mode) MSX_HandleBeeper();
          else if (einstein_mode) einstein_HandleBeeper();
      }
      else if (adam_mode)
      {
          adam_drive_cache_check();    // Make sure the disk and tape buffers are up to date
      }
      return 0;
  }
  return 1;
}

// End of file
