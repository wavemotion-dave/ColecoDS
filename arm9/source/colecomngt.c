// =====================================================================================
// Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)
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

#include "printf.h"

#include "colecoDS.h"
#include "AdamNet.h"
#include "FDIDisk.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/z80/ctc.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "MTX_BIOS.h"
#define NORAM 0xFF

// ------------------------------------------------
// Adam RAM is 128K (64K Intrinsic, 64K Expanded)
// ------------------------------------------------
u8 adam_128k_mode     = 0;
u8 sg1000_double_reset = false;

extern u8 msx_scc_enable;

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
u8 spinner_enabled      __attribute__((section(".dtcm"))) = 0;

u8 adam_ram_lo          __attribute__((section(".dtcm"))) = false;
u8 adam_ram_hi          __attribute__((section(".dtcm"))) = false;
u8 adam_ram_lo_exp      __attribute__((section(".dtcm"))) = false;
u8 adam_ram_hi_exp      __attribute__((section(".dtcm"))) = false;

Z80 CPU __attribute__((section(".dtcm")));

// --------------------------------------------------
// Some special ports for the MSX machine emu
// --------------------------------------------------
u8 Port_PPI_A __attribute__((section(".dtcm"))) = 0x00;
u8 Port_PPI_B __attribute__((section(".dtcm"))) = 0x00;
u8 Port_PPI_C __attribute__((section(".dtcm"))) = 0x00;

u8 bIsComplicatedRAM __attribute__((section(".dtcm"))) = 0;   // Set to 1 if we have hotspots or other RAM needs

char lastAdamDataPath[256];

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
u8 ctc_enabled     __attribute__((section(".dtcm"))) = false;

u8  JoyMode        __attribute__((section(".dtcm"))) = 0;           // Joystick Mode (1=Keypad, 0=Joystick)
u32 JoyState       __attribute__((section(".dtcm"))) = 0;           // Joystick State for P1 and P2


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

// -----------------------------------------------------------
// The two master sound chips... both are mapped to SN sound.
// -----------------------------------------------------------
SN76496 sncol   __attribute__((section(".dtcm")));
SN76496 aycol   __attribute__((section(".dtcm")));
SN76496 sccABC  __attribute__((section(".dtcm")));
SN76496 sccDE   __attribute__((section(".dtcm")));

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
    for (int i=0; i< 0x8000; i++) RAM_Memory[0x8000+i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
    memset(RAM_Memory,  0x00, 0x10000);
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
      // ADAM has special handling...
      u8 pattern = 0x00;                               // Default to all-clear
      if (myConfig.memWipe == 1) pattern = 0x30;       // The 0x30 pattern tends to make most things start up properly... don't ask.
      if (myConfig.memWipe == 2) pattern = 0x38;       // The 0x38 pattern tends to make CPM disk games start up properly... don't ask.
      for (int i=0; i< 0x20000; i++) RAM_Memory[i] = (myConfig.memWipe ? pattern : (rand() & 0xFF));
  }
  else if (einstein_mode)
  {
      for (int i=0; i<0x10000; i++) RAM_Memory[i] = (myConfig.memWipe ? 0x00:  (rand() & 0xFF));
  }
  else  // Normal colecovision which has 1K of RAM and is mirrored
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
    if (creativision_mode) return;  // Special handling in creativision.c
    
    if (++dampen >= next_dampen_time) // Roughly 50ms... experimentally good enough for all systems.
    {
        if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
        {
            buf_held = BufferedKeys[BufferedKeysReadIdx];
            BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
            if (buf_held == KBD_KEY_SHIFT) buf_shift = 2; else {if (buf_shift) buf_shift--;}
            if (buf_held == 255) {buf_held = 0; next_dampen_time=60;} else next_dampen_time = (memotech_mode ? 1:5);
        } else buf_held = 0;
        dampen = 0;
    }

    // See if the shift key should be virtually pressed along with this buffered key...
    if (buf_held) {kbd_keys[kbd_keys_pressed++] = buf_held; if (buf_shift) key_shift=1;}
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
  memset(ROM_Memory, 0xFF, (MAX_CART_SIZE * 1024));
  
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
  for(u8 J=0;J<MAX_DISKS;++J) ChangeDisk(J,0);
  for(u8 J=0;J<MAX_TAPES;++J) ChangeTape(J,0);

  // Init the page flipping buffer...
  for (uBcl=0;uBcl<192;uBcl++) 
  {
     uVide=(uBcl/12);
     dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
  }
    
  write_EE_counter=0;
  spinner_enabled = false;
  ctc_enabled = false;
    
  if (sg1000_mode)  // Load SG-1000 cartridge
  {
      colecoWipeRAM();                              // Wipe RAM
      RetFct = loadrom(szGame,RAM_Memory,0xC000);   // Load up to 48K
      sg1000_reset();                               // Reset the SG-1000
  }
  else if (sordm5_mode)  // Load Sord M5 cartridge
  {
      ctc_enabled = true;
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory+0x2000,0x5000);  // Load up to 20K
  }
  else if (pv2000_mode)  // Casio PV-2000 cartridge loads at C000
  {
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory+0xC000,0x4000);  // Load up to 16K
  }
  else if (creativision_mode)  // Creativision loads cart up against 0xC000
  {
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory+0xC000,0x4000);  // Load up to 16K
  }
  else if (memotech_mode)  // Load Memotech MTX file
  {
      ctc_enabled = true;
      RetFct = loadrom(szGame,RAM_Memory+0x4000,0xC000);  // Load up to 48K
  }
  else if (msx_mode)  // Load MSX cartridge ... 
  {
      // loadrom() will figure out how big and where to load it... the 0x8000 here is meaningless.
      RetFct = loadrom(szGame,RAM_Memory+0x8000,0x8000);  
      
      // Wipe RAM area from 0xC000 upwards after ROM is loaded...
      colecoWipeRAM();
  }
  else if (svi_mode)  // Load SVI ROM ... 
  {
      // loadrom() will figure out how big and where to load it... the 0x8000 here is meaningless.
      RetFct = loadrom(szGame,RAM_Memory+0x8000,0x8000);  
      
      // Wipe RAM area from 0x8000 upwards after ROM is loaded...
      colecoWipeRAM();
  }
  else if (adam_mode)  // Load Adam DDP
  {
      spinner_enabled = (myConfig.spinSpeed != 5) ? true:false;
      sgm_reset();                       // Make sure the super game module is disabled to start
      adam_CapsLock = 0;
      adam_unsaved_data = 0;
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory,0x10000);  
  }
  else if (pencil2_mode)
  {
      // Wipe area from BIOS onwards and then wipe RAM to random values below
      memset(RAM_Memory+0x2000, 0xFF, 0xE000);

      // Wipe RAM to Random Values
      colecoWipeRAM();

      RetFct = loadrom(szGame,RAM_Memory+0x8000,0x8000);    // Load up to 32K
  }
  else if (einstein_mode)  // Load Einstein COM file
  {
      ctc_enabled = true;
      colecoWipeRAM();
      RetFct = loadrom(szGame,RAM_Memory+0x4000,0xC000);  // Load up to 48K
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

      RetFct = loadrom(szGame,RAM_Memory+0x8000,0x8000);
      
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
  DrZ80_Reset();                        // Reset the DrZ80 CPU core
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


/*******************************************************************************
 * Compute the file CRC - this will be our unique identifier for the game
 * for saving HI SCORES and Configuration / Key Mapping data.
 *******************************************************************************/
void getfile_crc(const char *path)
{
    file_crc = getFileCrc(path);        // The CRC is used as a unique ID to save out High Scores and Configuration...
    
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
/* Open a rom file from file system                                            */
/*******************************************************************************/
u8 loadrom(const char *path,u8 * ptr, int nmemb) 
{
  u8 bOK = 0;

  DSPrint(0,0,6, "LOADING...");
    
  FILE* handle = fopen(path, "rb");  
  if (handle != NULL) 
  {
    memset(ROM_Memory, 0xFF, (MAX_CART_SIZE * 1024));          // Ensure our rom buffer is clear (0xFF to simulate unused memory on ROM/EE though probably 0x00 would be fine too)
    
    fseek(handle, 0, SEEK_END);                     // Figure out how big the file is
    int romSize = ftell(handle);
    sg1000_double_reset = false;
    
    // ----------------------------------------------------------------------
    // Look for the Survivors .sc Multicart  (2MB!) or .sc MegaCart (4MB!)
    // ----------------------------------------------------------------------
    if (sg1000_mode && ((romSize == (2048 * 1024)) || (romSize == (4096 * 1024))))   
    {
        fseek(handle, romSize-0x8000, SEEK_SET);              // Seek to the last 32K block (this is the menu system)
        fread((void*) RAM_Memory, 1, 0x8000, handle);         // Read 32K from that last block directly into the RAM buffer
        memcpy(ROM_Memory, RAM_Memory, 0x8000);               // And save the last block so we can switch back as needed...
        fclose(handle);
        strcpy(lastAdamDataPath, path);
        romBankMask = (romSize == (2048 * 1024) ? 0x3F:0x7F);
        sg1000_double_reset = true;
        machine_mode = MODE_SG_1000;
        return bOK;
    }
    else        
    if (romSize <= (MAX_CART_SIZE * 1024))  // Max size cart is 1MB - that's pretty huge...
    {
        fseek(handle, 0, SEEK_SET);
        fread((void*) ROM_Memory, romSize, 1, handle); 
        fclose(handle);
        
        if (file_crc == 0x68c85890) // M5 Up Up Balloon needs a patch to add 0x00 at the front
        {
            for (u16 i=romSize; i>0; i--)
            {
                ROM_Memory[i] = ROM_Memory[i-1];  // Shift everything up 1 byte
            }
            ROM_Memory[0] = 0x00;    // Add 0x00 to the first byte which is the patch
            romSize++;               // Make sure the size is now correct
        }
        
        romBankMask = 0x00;         // No bank mask until proven otherwise
        bMagicMegaCart = false;     // No Mega Cart to start
        mapperMask = 0x00;          // No MSX mapper mask
        bActivisionPCB = 0;         // No Activision PCB

        // ------------------------------------------------------------------------------
        // For the MSX emulation, we will use fast VRAM to hold ROM and mirrors...
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
            Port60 = 0x00;               // Adam Memory default
            Port20 = 0x00;               // Adam Net default
            adam_128k_mode = 0;          // Normal 64K ADAM to start
            SetupAdam(false);
            // The .ddp or .dsk is now in ROM_Memory[]
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
        else if (memotech_mode || svi_mode)     // Can be any size tapes... up to 1024K
        {
            tape_len = romSize;  // The tape size is saved for showing tape load progress
            tape_pos = 0;
            last_tape_pos = 9999;
        }
        else if (einstein_mode)
        {
            strcpy(lastAdamDataPath, path);
            tape_len = romSize;  
            if (romSize == 1626) // A bit of a hack... the size of the Diagnostic ROM
            {
                memcpy(RAM_Memory+0x4000, ROM_Memory, romSize);   // only for Diagnostics ROM
            }
        }
        else if (creativision_mode)
        {
            strcpy(lastAdamDataPath, path);
            creativision_loadrom(romSize);
        }
        else
        // ----------------------------------------------------------------------
        // Do we fit within the standard 32K Colecovision Cart ROM memory space?
        // ----------------------------------------------------------------------
        if (romSize <= (((sg1000_mode) ? 48:32)*1024)) // Allow SG ROMs to be up to 48K, otherwise 32K limit
        {
            memcpy(ptr, ROM_Memory, nmemb);
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
            lastBank = 199;                                 // Force load of the first bank when asked to bankswitch
            if ((romSize == (64 * 1024)) && !bMagicMegaCart)
            {
                bActivisionPCB = 1;
                memcpy(ptr, ROM_Memory, 0x4000);                     // bank 0
                memcpy(ptr+0x4000, ROM_Memory+0x4000, 0x4000);       // bank 1
                romBankMask = 0x03;
            }
            else    // We will assume Megacart then...
            {
                bMagicMegaCart = 1;
                memcpy(ptr, ROM_Memory+(romSize-0x4000), 0x4000); // For MegaCart, we map highest 16K bank into fixed ROM
                MegaCartBankSwitch(0);                            // The initial 16K "switchable" bank is bank 0 (based on a post from Nanochess in AA forums)
                
                if      (romSize == (64  * 1024)) romBankMask = 0x03;
                else if (romSize == (128 * 1024)) romBankMask = 0x07;
                else if (romSize == (256 * 1024)) romBankMask = 0x0F;
                else if (romSize == (512 * 1024)) romBankMask = 0x1F;
                else                              romBankMask = 0x3F;    // Up to 1024KB... huge!
            }
        }
        bOK = 1;
    }
    else fclose(handle);
      
    // -------------------------------------------------------------------------  
    // For some combinations, we have hotspots or other memory stuff that 
    // needs to be more complicated than simply returning RAM_Memory[].
    // -------------------------------------------------------------------------  
    bIsComplicatedRAM = (bMagicMegaCart || bActivisionPCB || adam_mode || msx_sram_enabled || pv2000_mode) ? 1:0;  // Set to 1 if we have to do more than just simple memory read...

    // -----------------------------------------------------------------------
    // To speed up processing in the memory write functions, we accumulate 
    // the bits so we only have to fetch one machine_mode variable.
    // -----------------------------------------------------------------------
    if (pencil2_mode)           machine_mode = MODE_PENCIL2;
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
        memset(RAM_Memory+0x2000, 0x00, 0x6000);
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
          sgm_low_addr = 0x2000;
          MemoryMap[0] = BIOS_Memory + 0x0000;
      }
    }
    else 
    {
      sgm_enable = true;    // Force this if someone disabled the BIOS.... based on reading some comments in the AA forum...
      if (sgm_low_addr != 0x0000)
      {
          MemoryMap[0] = RAM_Memory + 0x0000;
          sgm_low_addr = 0x0000; 
      }
    }
}


void adam_setup_bios(void)
{
    memcpy(BIOS_Memory+0x0000, AdamWRITER, 0x8000);
    memcpy(BIOS_Memory+0x8000, AdamEOS,    0x2000);
    memcpy(BIOS_Memory+0xA000, ColecoBios, 0x2000);
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
        MemoryMap[0] = BIOS_Memory + 0x0000;
        MemoryMap[1] = BIOS_Memory + 0x2000;
        MemoryMap[2] = BIOS_Memory + 0x4000;
        if (Port20 & 0x02) 
        {
            MemoryMap[3] = BIOS_Memory + 0x8000;    // EOS
        }
        else
        {
            MemoryMap[3] = BIOS_Memory + 0x6000;    // Lst block of Adam WRITER
        }
    }
    else if ((Port60 & 0x03) == 0x01)   // Onboard RAM
    {
        adam_ram_lo = true;
        adam_ram_lo_exp = false;
        MemoryMap[0] = RAM_Memory + 0x0000;
        MemoryMap[1] = RAM_Memory + 0x2000;
        MemoryMap[2] = RAM_Memory + 0x4000;
        MemoryMap[3] = RAM_Memory + 0x6000;
    }
    else if ((Port60 & 0x03) == 0x03)   // Colecovision BIOS + RAM
    {
        adam_ram_lo = true;
        adam_ram_lo_exp = false;
        MemoryMap[0] = BIOS_Memory + 0xA000;    // Coleco.rom BIOS
        MemoryMap[1] = RAM_Memory + 0x2000;
        MemoryMap[2] = RAM_Memory + 0x4000;
        MemoryMap[3] = RAM_Memory + 0x6000;
    }
    else                                // Expanded RAM
    {
        adam_128k_mode = 1;
        adam_ram_lo = false;
        adam_ram_lo_exp = true;
        MemoryMap[0] = RAM_Memory + 0x10000;
        MemoryMap[1] = RAM_Memory + 0x12000;
        MemoryMap[2] = RAM_Memory + 0x14000;
        MemoryMap[3] = RAM_Memory + 0x16000;
    }


    // ----------------------------------
    // Configure upper 32K of memory
    // ----------------------------------
    if ((Port60 & 0x0C) == 0x00)    // Onboard RAM
    {
        adam_ram_hi = true;
        adam_ram_hi_exp = false;
        MemoryMap[4] = RAM_Memory + 0x8000;
        MemoryMap[5] = RAM_Memory + 0xA000;
        MemoryMap[6] = RAM_Memory + 0xC000;
        MemoryMap[7] = RAM_Memory + 0xE000;
    }
    else if ((Port60 & 0x0C) == 0x08)    // Expanded RAM
    {
        adam_128k_mode = 1;
        adam_ram_hi = false;
        adam_ram_hi_exp = true;
        MemoryMap[4] = RAM_Memory + 0x18000;
        MemoryMap[5] = RAM_Memory + 0x1A000;
        MemoryMap[6] = RAM_Memory + 0x1C000;
        MemoryMap[7] = RAM_Memory + 0x1E000;
    }
    else        // Nothing else exists so just return 0xFF
    {
        adam_ram_hi = false;
        adam_ram_hi_exp = false;
    }
    
    // Check if we are to Reset the AdamNet
    if (bResetAdamNet)  ResetPCB();
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
unsigned char cpu_readport16(register unsigned short Port) 
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

    case 0xE0: // Joystick/Keypad Data
      Port = (Port&0x02) ? (JoyState>>16):JoyState;
      Port = JoyMode     ? (Port>>8):Port;
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
      else if (WrCtrl9918(Value)) { CPU.IRequest=INT_NMI; cpuirequest=Z80_NMI_INT;}
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
      
      // If the MSX is trying to use the SCC sound chip, call into that loop here...
      if (msx_scc_enable)
      {
          FakeSCC_Loop();
      }

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
          cycle_deficit = DrZ80_execute(tms_cpu_line + cycle_deficit);

          // Refresh VDP 
          if(Loop9918()) cpuirequest = ((machine_mode & (MODE_MSX | MODE_SG_1000 | MODE_SVI)) ? Z80_IRQ_INT : Z80_NMI_INT);

          // Generate interrupt if called for
          if (cpuirequest)
            DrZ80_Cause_Interrupt(cpuirequest);
          else
            DrZ80_Clear_Pending_Interrupts();
      }
      else  // CZ80 core from fMSX()... slower but higher accuracy
      {
          // Execute 1 scanline worth of CPU instructions
          u32 cycles_to_process = tms_cpu_line + cycle_deficit;
          cycle_deficit = ExecZ80(tms_cpu_line + cycle_deficit);

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
              if (einstein_mode && (CPU.IRequest == INT_NONE))  // If the keyboard is generating an interrupt...
              {
                  einstein_handle_interrupts();
                  if (keyboard_interrupt) CPU.IRequest = keyboard_interrupt;
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
      if (myConfig.msxBeeper)
      {
          MSX_HandleBeeper();
      }
      return 0;
  }
  return 1;
    
}

// End of file
