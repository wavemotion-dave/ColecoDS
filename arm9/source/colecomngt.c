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
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/sn76496/SN76496.h"
#include "cpu/sn76496/Fake_AY.h"
#define NORAM 0xFF

#define COLECODS_SAVE_VER 0x0011        // Change this if the basic format of the .SAV file changes. Invalidates older .sav files.

// ---------------------------------------
// Some MSX Mapper / Slot Handling stuff
// ---------------------------------------
u8 Slot0BIOS[0x10000] = {0xFF};
u8 Slot1ROM[0x10000]  = {0xFF};
u8 Slot3RAM[0x10000]  = {0x00};
u8* Slot3RAMPtr __attribute__((section(".dtcm"))) = (u8*)0;

u8 mapperType __attribute__((section(".dtcm"))) = 0;
u8 mapperMask __attribute__((section(".dtcm"))) = 0;
u8 bROMInSlot[4] __attribute__((section(".dtcm"))) = {0,0,0,0};
u8 bRAMInSlot[4] __attribute__((section(".dtcm"))) = {0,0,0,0};

u8 *Slot1ROMPtr[8] __attribute__((section(".dtcm"))) = {0,0,0,0,0,0,0,0};

Z80 CPU __attribute__((section(".dtcm")));

// --------------------------------------------------
// Some CPU and VDP and SGM stuff that we need
// --------------------------------------------------
extern byte Loop9918(void);
extern void DrZ80_InitHandlers(void);
extern u8 lastBank;
s16 timingAdjustment = 0;
u8 bDontResetEnvelope __attribute__((section(".dtcm"))) = false;

// --------------------------------------------------
// Some special ports for the MSX machine emu
// --------------------------------------------------
u8 PortA8 __attribute__((section(".dtcm"))) = 0x00;
u8 PortA9 __attribute__((section(".dtcm"))) = 0x00;
u8 PortAA __attribute__((section(".dtcm"))) = 0x00;

extern u8 MSXBios[];
extern u8 CBios[];

u16 msx_init = 0x4000;
u16 msx_basic = 0x0000;

// Some sprite data arrays for the Mario character that walks around the upper screen..
extern const unsigned short sprPause_Palette[16];
extern const unsigned char sprPause_Bitmap[2560];
extern u32* lutTablehh;
extern int cycle_deficit;

// ----------------------------------------------------------------------------
// Some vars for the Z80-CTC timer/counter chip which is only partially 
// emulated - enough that we can do rough timing and generate VDP 
// interrupts. This chip is only used on the Sord M5 (not the Colecovision
// nor the SG-1000 which just ties interrupts directly between VDP and CPU).
// ----------------------------------------------------------------------------
u8 ctc_control[4] __attribute__((section(".dtcm"))) = {0x02, 0x02, 0x02, 0x02};
u8 ctc_time[4]    __attribute__((section(".dtcm"))) = {0};
u16 ctc_timer[4]  __attribute__((section(".dtcm"))) = {0};
u8 ctc_vector[4]  __attribute__((section(".dtcm"))) = {0};
u8 ctc_latch[4]   __attribute__((section(".dtcm"))) = {0};
u8 sordm5_irq     __attribute__((section(".dtcm"))) = 0xFF;


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

u8 Port53  __attribute__((section(".dtcm"))) = 0x00;
u8 Port60  __attribute__((section(".dtcm"))) = 0x0F;

u8 bFirstSGMEnable __attribute__((section(".dtcm"))) = true;
u8 AY_Enable       __attribute__((section(".dtcm"))) = false;
u8 AY_NeverEnable  __attribute__((section(".dtcm"))) = false;
u8 SGM_NeverEnable __attribute__((section(".dtcm"))) = false;
u8 AY_EnvelopeOn   __attribute__((section(".dtcm"))) = false;

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

u32 file_crc __attribute__((section(".dtcm")))  = 0x00000000;  // Our global file CRC32 to uniquiely identify this game

u8 sgm_low_mem[8192] = {0}; // The 8K of SGM RAM that can be mapped into the BIOS area

// -----------------------------------------------------------
// The two master sound chips... both are mapped to SN sound.
// -----------------------------------------------------------
SN76496 sncol   __attribute__((section(".dtcm")));
SN76496 aycol   __attribute__((section(".dtcm")));

// --------------------------------------------------------------
// Some auxillary functions/vars to help with MSX memory layout
// --------------------------------------------------------------
void MSX_InitialMemoryLayout(u32 iSSize);
u32 LastROMSize = 0;

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
    AY_Enable = false;           // Default to no AY use until accessed
    AY_EnvelopeOn = false;       // No Envelope mode yet
    bFirstSGMEnable = true;      // First time SGM enable we clear ram
    
    Port53 = 0x00;               // Init the SGM Port 53
    Port60 = 0x0F;               // And the Adam/Memory Port 60
}

// ---------------------------------------------------------
// The Sord M5 has Z80-CTC vars that need to be reset.
// ---------------------------------------------------------
void sordm5_reset(void)
{
    // Reset the Z80-CTC stuff...
    memset(ctc_control, 0x00, 4);       // Set Software Reset Bit (freeze)
    memset(ctc_time, 0x00, 4);          // No time value set
    memset(ctc_vector, 0x00, 4);        // No vectors set
    memset(ctc_latch, 0x00, 4);         // No latch set
    sordm5_irq = 0xFF;                  // No IRQ set
}


// ---------------------------------------------------------
// The MSX has a few ports and special memory mapping
// ---------------------------------------------------------
void msx_reset(void)
{
    if (msx_mode)
    {
        MSX_InitialMemoryLayout(LastROMSize);
    }
}


/*********************************************************************************
 * Wipe main RAM with random patterns... or fill with 0x00 for some emulations.
 ********************************************************************************/
void colecoWipeRAM(void)
{
  if (sg1000_mode)
  {
    memset(pColecoMem+0xC000, 0x00, 0x4000);   
  }
  else if (sordm5_mode)
  {
    memset(pColecoMem+0x7000, 0x00, 0x9000);   
  }
  else if (msx_mode)
  {
    // Do nothing... MSX has all kinds of memory mapping that is handled elsewhere
  }
  else
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
}


/*********************************************************************************
 * Look for MSX 'AB' header in the ROM file
 ********************************************************************************/
void CheckMSXHeaders(char *szGame)
{
  FILE* handle = fopen(szGame, "rb");  
  if (handle)
  {
      // ------------------------------------------------------------------------------------------
      // MSX Header Bytes:
      //  0 DEFB "AB" ; expansion ROM header
      //  2 DEFW initcode ; start of the init code, 0 if no initcode
      //  4 DEFW callstat; pointer to CALL statement handler, 0 if no such handler
      //  6 DEFW device; pointer to expansion device handler, 0 if no such handler
      //  8 DEFW basic ; pointer to the start of a tokenized basicprogram, 0 if no basicprogram
      // ------------------------------------------------------------------------------------------
      memset(romBuffer, 0xFF, 0x400A);
      fread((void*) romBuffer, 0x400A, 1, handle); 
      fclose(handle);
      
      // ---------------------------------------------------------------------
      // Do some auto-detection for game ROM. MSX games have 'AB' in their
      // header and we also want to track the INIT address for those ROMs
      // so we can take a better guess at mapping them into our Slot1 memory
      // ---------------------------------------------------------------------
      msx_init = 0x4000;
      msx_basic = 0x0000;
      if ((romBuffer[0] == 'A') && (romBuffer[1] == 'B'))
      {
          msx_mode = 1;      // MSX roms start with AB (might be in bank 0)
          msx_init = romBuffer[2] | (romBuffer[3]<<8);
          if (msx_init == 0x0000) msx_basic = romBuffer[8] | (romBuffer[8]<<8);
          if (msx_init == 0x0000)   // If 0, check for 2nd header... this might be a dummy
          {
              if ((romBuffer[0x4000] == 'A') && (romBuffer[0x4001] == 'B'))  
              {
                  msx_init = romBuffer[0x4002] | (romBuffer[0x4003]<<8);
                  if (msx_init == 0x0000) msx_basic = romBuffer[0x4008] | (romBuffer[0x4009]<<8);
              }
          }
      }
      else if ((romBuffer[0x4000] == 'A') && (romBuffer[0x4001] == 'B'))  
      {
          msx_mode = 1;      // MSX roms start with AB (might be in bank 1)
          msx_init = romBuffer[0x4002] | (romBuffer[0x4003]<<8);
          if (msx_init == 0x0000) msx_basic = romBuffer[0x4008] | (romBuffer[0x4009]<<8);
      }
  }
}


/*********************************************************************************
 * Init coleco Engine for that game
 ********************************************************************************/
u8 colecoInit(char *szGame) 
{
  extern u8 bForceMSXLoad;
  u8 RetFct,uBcl;
  u16 uVide;

  memset(romBuffer, 0xFF, (512 * 1024));
  
  if (bForceMSXLoad) msx_mode = 1;
  if (msx_mode) AY_Enable=true;
  if (msx_mode) InitBottomScreen();  // Could Need to ensure the MSX layout is shown

  if (sg1000_mode)  // Load SG-1000 cartridge
  {
      memset(pColecoMem, 0x00, 0x10000);            // Wipe Memory
      RetFct = loadrom(szGame,pColecoMem,0xC000);   // Load up to 48K
  }
  else if (sordm5_mode)  // Load Sord M5 cartridge
  {
      memset(pColecoMem+0x2000, 0x00, 0xC000);            // Wipe Memory above BIOS
      RetFct = loadrom(szGame,pColecoMem+0x2000,0x4000);  // Load up to 16K
  }
  else if (msx_mode)  // Load MSX cartridge ... 
  {
      // loadrom() will figure out how big and where to load it... the 0x8000 here is meaningless.
      RetFct = loadrom(szGame,pColecoMem+0x8000,0x8000);  
      
      // Wipe RAM area from 0xC000 upwards after ROM is loaded...
      colecoWipeRAM();
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
      
    sordm5_reset();                     // Reset the Sord M5 CTC stuff
      
    XBuf = XBuf_A;                      // Set the initial screen ping-pong buffer to A
      
    ResetStatusFlags();                 // Some status flags for the UI mostly
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
        if (LastROMSize <= (64 * 1024)) memcpy(Slot3RAM, Slot3RAMPtr, 0x10000);
        if (uNbO) fwrite(Slot3RAM, 0x10000,1, handle);
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
                if (LastROMSize <= (64 * 1024)) memcpy(Slot3RAMPtr, Slot3RAM, 0x10000);
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
    
    // ---------------------------------------------------------------------------------------------
    // And we don't have the AY envelope quite right so a few games don't want to reset the indexes
    // ---------------------------------------------------------------------------------------------
    bDontResetEnvelope = false;
    if (file_crc == 0x90f5f414) bDontResetEnvelope = true; // MSX Warp-and-Warp
    if (file_crc == 0x5e169d35) bDontResetEnvelope = true; // MSX Warp-and-Warp (alt)
    if (file_crc == 0xe66eaed9) bDontResetEnvelope = true; // MSX Warp-and-Warp (alt)
    if (file_crc == 0x785fc789) bDontResetEnvelope = true; // MSX Warp-and-Warp (alt)    
    
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


// --------------------------------------------------------------------------
// Try to guess the ROM type from the loaded binary... basically we are
// counting the number of load addresses that would access a mapper hot-spot.
// --------------------------------------------------------------------------
u8 MSX_GuessROMType(u32 size)
{
    u8 type = KON8;  // Default to Konami 8K mapper
    u16 guess[MAX_MAPPERS] = {0,0,0,0};
    
    if (size == (64 * 1024)) return ASC16;      // Big percentage of 64K mapper ROMs are ASCII16
    
    for (int i=0; i<size - 3; i++)
    {
        if (romBuffer[i] == 0x32)   // LD,A instruction
        {
            u16 value = romBuffer[i+1] + (romBuffer[i+2] << 8);
            switch (value)
            {
				case 0x5000:
				case 0x9000:
				case 0xb000:
					guess[SCC8]++;
					break;
				case 0x4000:
				case 0x8000:
				case 0xa000:
					guess[KON8]++;
					break;
				case 0x6800:
				case 0x7800:
					guess[ASC8]++;guess[ASC8]++;
					break;
				case 0x6000:
					guess[KON8]++;
					guess[ASC8]++;
                    guess[ASC16]++;
					break;
				case 0x7000:
					guess[SCC8]++;
					guess[ASC8]++;
                    guess[ASC16]++;
					break;
                case 0x77FF:
                    guess[ASC16]++;guess[ASC16]++;
                    break;
            }
        }
    }

    // Now pick the mapper that had the most Load addresses above...
    if      ((guess[ASC16] > guess[KON8]) && (guess[ASC16] > guess[SCC8]) && (guess[ASC16] > guess[ASC8]))    type = ASC16;
    else if ((guess[ASC8]  > guess[KON8]) && (guess[ASC8]  > guess[SCC8]) && (guess[ASC8] >= guess[ASC16]))   type = ASC8;      // ASC8 wins "ties" over ASC16
    else if ((guess[SCC8]  > guess[KON8]) && (guess[SCC8]  > guess[ASC8]) && (guess[SCC8]  > guess[ASC16]))   type = SCC8;
    else type = KON8; 
    
    
    // ----------------------------------------------------------------------
    // Since mappers are hard to detect reliably, check a few special CRCs
    // ----------------------------------------------------------------------
    if (file_crc == 0x5dc45624) type = ASC8;   // Super Laydock
    if (file_crc == 0xb885a464) type = ZEN8;   // Super Laydock    
    if (file_crc == 0x7454ad5b) type = ASC16;  // Sorcery
    if (file_crc == 0x3891bc0f) type = ASC16;  // Govellious
    if (file_crc == 0x1d1ec602) type = ASC16;  // Eggerland 2
    if (file_crc == 0x704ec575) type = ASC16;  // Toobin
    if (file_crc == 0x885773f9) type = ASC16;  // Dragon Slayer 3
    if (file_crc == 0x0521ca7a) type = ASC16;  // Dynamite Dan
    if (file_crc == 0xab6cd62c) type = ASC16;  // King's Knight    
    if (file_crc == 0x00c5d5b5) type = ASC16;  // Hydlyde III
    if (file_crc == 0x2a019191) type = ASC8;   // R-Type 512k
    if (file_crc == 0xa3a51fbb) type = ASC16;  // R-Type 512k
    if (file_crc == 0x952bfaa4) type = SCC8;   // R-Type 512k
    if (file_crc == 0xfbd3f05b) type = ASC16;  // Alien Attack 3.5
    if (file_crc == 0xa6e924ab) type = ASC16;  // Alien2
    if (file_crc == 0x1306ccca) type = ASC8;   // Auf Wiedershen Monty [1.7]
    if (file_crc == 0xec036e37) type = ASC16;  // Gall Force
    if (file_crc == 0xa29176e3) type = ASC16;  // Mecha 9    
    if (file_crc == 0x03379ef8) type = ASC16;  // MSXDev Step Up 1.2
    if (file_crc == 0x8183bae1) type = LIN64;  // Mutants fro the Deep (fixed)
    
    return type;
}


// -------------------------------------------------------------------------
// Setup the initial MSX memory layout based on the size of the ROM loaded
// -------------------------------------------------------------------------
void MSX_InitialMemoryLayout(u32 iSSize)
{
    LastROMSize = iSSize;
    
    // -------------------------------------
    // Make sure the MSX ports are clear
    // -------------------------------------
    PortA8 = 0x00;
    PortA9 = 0x00;
    PortAA = 0x00;      
    
    // ---------------------------------------------
    // Start with reset memory - fill in MSX slots
    // ---------------------------------------------
    memset((u8*)0x06880000, 0xFF, 0x20000);
    memset(Slot0BIOS, 0xFF, 0x10000);
    memset(Slot1ROM,  0xFF, 0x10000);
    memset(pColecoMem,0xFF, 0x10000);
    
    // --------------------------------------------------
    // If we are using less than 64K of ROM, we can use 
    // the 2nd half of the fast VRAM buffer for RAM swap
    // --------------------------------------------------
    if (LastROMSize <= (64 * 1024))
    {
        Slot3RAMPtr = (u8*)0x06890000;
    }
    else
    {
        Slot3RAMPtr = (u8*)Slot3RAM;
    }
    memset(Slot3RAMPtr,  0x00, 0x10000);

    // --------------------------------------------------------------
    // Based on config, load up the C-BIOS or the real MSX.ROM BIOS
    // --------------------------------------------------------------
    if (myConfig.msxBios)
    {
        memcpy(Slot0BIOS, MSXBios, 0x8000);
        memcpy(pColecoMem, MSXBios, 0x8000);
    }
    else
    {
        memcpy(Slot0BIOS, CBios, 0x8000);
        memcpy(pColecoMem, CBios, 0x8000);
    }

    // -----------------------------------------
    // Setup RAM/ROM pointers back to defaults
    // -----------------------------------------
    memset(bRAMInSlot, 0, 4);   // Default to no RAM in slot until told so
    memset(bROMInSlot, 0, 4);   // Default to no ROM in slot until told so
    for (u8 i=0; i<8; i++)
    {
        Slot1ROMPtr[i] = 0;     // All pages normal until told otherwise by A8 writes
    }
    
    // ------------------------------------------------------------
    // Setup the Z80 memory based on the MSX game ROM size loaded
    // ------------------------------------------------------------
    if (iSSize == (8 * 1024))
    {
        if (msx_basic)  // Basic Game loads at 0x8000 ONLY
        {
            memcpy((u8*)Slot1ROM+0x8000, romBuffer, 0x2000);      // Load rom at 0x8000
        }
        else
        {
            for (u8 i=0; i<8; i++)
            {
                memcpy((u8*)Slot1ROM+(0x2000 * i), romBuffer, 0x2000);      // 8 Mirrors so every bank is the same
            }
        }
    }
    else if (iSSize == (16 * 1024))
    {
        if (myConfig.msxMapper == AT4K)
        {
                memcpy((u8*)Slot1ROM+0x4000, romBuffer,        0x4000);      // Load the 16K rom at 0x4000
        }
        else if (myConfig.msxMapper == AT8K)
        {
                memcpy((u8*)Slot1ROM+0x8000, romBuffer,        0x4000);      // Load the 16K rom at 0x8000
        }
        else
        {
            if (msx_basic)  // Basic Game loads at 0x8000 ONLY
            {
                memcpy((u8*)Slot1ROM+0x8000, romBuffer, 0x4000);      // Load rom at 0x8000
            }
            else
            {
                for (u8 i=0; i<4; i++)
                {
                    memcpy((u8*)Slot1ROM+(0x4000 * i), romBuffer, 0x4000);      // 4 Mirrors so every bank is the same
                }
            }
        }
    }
    else if (iSSize == (32 * 1024))
    {
        // ------------------------------------------------------------------------------------------------------
        // For 32K roms, we need more information to determine exactly where to load it... however
        // this simple algorithm handles at least 90% of all real-world games... basically the header
        // of the .ROM file has a INIT load address that we can use as a clue as to what banks the actual
        // code should be loaded... if the INIT is address 0x4000 or higher (this is fairly common) then we
        // load the 32K rom into banks 1+2 and we mirror the first 16K on page 0 and the upper 16K on page 3.
        // ------------------------------------------------------------------------------------------------------
        if (myConfig.msxMapper == AT0K)
        {
                memcpy((u8*)Slot1ROM+0x0000, romBuffer,        0x8000);      // Then the full 32K ROM is mapped here
        }
        else  if (myConfig.msxMapper == AT4K)
        {
                memcpy((u8*)Slot1ROM+0x4000, romBuffer,        0x8000);      // Then the full 32K ROM is mapped here
        }
        else if (myConfig.msxMapper == AT8K)
        {
                memcpy((u8*)Slot1ROM+0x8000, romBuffer,        0x8000);      // Then the full 32K ROM is mapped here
        }
        else
        {
            if (msx_init >= 0x4000 || msx_basic) // This comes from the .ROM header - if the init address is 0x4000 or higher, we load in bank 1+2
            {
                memcpy((u8*)Slot1ROM+0x0000, romBuffer,        0x4000);      // Lower 16K is mirror of first 16K of ROM
                memcpy((u8*)Slot1ROM+0x4000, romBuffer,        0x8000);      // Then the full 32K ROM is mapped here
                memcpy((u8*)Slot1ROM+0xC000, romBuffer+0x4000, 0x4000);      // Upper 16K is the mirror of the second 16K of ROM
            }
            else  // Otherwise we load in bank 0+1 and mirrors on 2+3
            {
                memcpy((u8*)Slot1ROM+0x0000, romBuffer,        0x8000);      // The full 32K ROM is mapped at 0x0000
                memcpy((u8*)Slot1ROM+0x8000, romBuffer,        0x8000);      // And the full mirror is at 0x8000
            }
        }
    }
    else if (iSSize == (48 * 1024))
    {
        if (myConfig.msxMapper == KON8)
        {
            Slot1ROMPtr[0] = (u8*)0x06880000+0x4000;        // Segment 2 Mirror
            Slot1ROMPtr[1] = (u8*)0x06880000+0x6000;        // Segment 3 Mirror
            Slot1ROMPtr[2] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[3] = (u8*)0x06880000+0x2000;        // Segment 1 default
            Slot1ROMPtr[4] = (u8*)0x06880000+0x4000;        // Segment 2 default
            Slot1ROMPtr[5] = (u8*)0x06880000+0x6000;        // Segment 3 default
            Slot1ROMPtr[6] = (u8*)0x06880000+0x0000;        // Segment 0 Mirror
            Slot1ROMPtr[7] = (u8*)0x06880000+0x2000;        // Segment 1 Mirror
            memcpy((u8*)0x06880000, romBuffer, iSSize);     // All 48K copied into our fast VRAM buffer
            mapperMask = 0x07;
        }
        else if (myConfig.msxMapper == ASC8)
        {
            Slot1ROMPtr[0] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[1] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[2] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[3] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[4] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[5] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[6] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[7] = (u8*)0x06880000+0x0000;        // Segment 0 default
            memcpy((u8*)0x06880000, romBuffer, iSSize);     // All 48K copied into our fast VRAM buffer
            mapperMask = 0x07;
        }
        else if (myConfig.msxMapper == ASC16)
        {
            Slot1ROMPtr[0] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[1] = (u8*)0x06880000+0x2000;        // Segment 0 default
            Slot1ROMPtr[2] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[3] = (u8*)0x06880000+0x2000;        // Segment 0 default
            Slot1ROMPtr[4] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[5] = (u8*)0x06880000+0x2000;        // Segment 0 default
            Slot1ROMPtr[6] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[7] = (u8*)0x06880000+0x2000;        // Segment 0 default
            memcpy((u8*)0x06880000, romBuffer, iSSize);     // All 48K copied into our fast VRAM buffer
            mapperMask = 0x07;
        }
        else if (myConfig.msxMapper == AT4K)
        {
            memcpy((u8*)Slot1ROM+0x4000, romBuffer,            0xC000);      // Full Rom starting at 0x4000
        }
        else
        {
            memcpy((u8*)Slot1ROM+0x0000, romBuffer,            0xC000);      // Full Rom starting at 0x0000 (this is common)
        }
    }
    else if ((iSSize == (64 * 1024)) && (myConfig.msxMapper == LIN64))   // 64K Linear ROM
    {
        memcpy((u8*)Slot1ROM+0x0000, romBuffer,           0x10000);      // Full Rom starting at 0x0000
    }
    else if ((iSSize >= (64 * 1024)) && (iSSize <= (512 * 1024)))   // We'll take anything between these two...
    {
        if (myConfig.msxMapper == GUESS)
        {
            mapperType = MSX_GuessROMType(iSSize);
        }
        else
        {
            mapperType = myConfig.msxMapper;   
        }

        if ((mapperType == KON8) || (mapperType == SCC8) || (mapperType == ZEN8))
        {
            Slot1ROMPtr[0] = (u8*)0x06880000+0x4000;        // Segment 2 Mirror
            Slot1ROMPtr[1] = (u8*)0x06880000+0x6000;        // Segment 3 Mirror
            Slot1ROMPtr[2] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[3] = (u8*)0x06880000+0x2000;        // Segment 1 default
            Slot1ROMPtr[4] = (u8*)0x06880000+0x4000;        // Segment 2 default
            Slot1ROMPtr[5] = (u8*)0x06880000+0x6000;        // Segment 3 default
            Slot1ROMPtr[6] = (u8*)0x06880000+0x0000;        // Segment 0 Mirror
            Slot1ROMPtr[7] = (u8*)0x06880000+0x2000;        // Segment 1 Mirror
        }
        else if (mapperType == ASC8)
        {
            Slot1ROMPtr[0] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[1] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[2] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[3] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[4] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[5] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[6] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[7] = (u8*)0x06880000+0x0000;        // Segment 0 default
        }                
        else if (mapperType == ASC16 || mapperType == ZEN16)
        {
            Slot1ROMPtr[0] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[1] = (u8*)0x06880000+0x2000;        // Segment 0 default
            Slot1ROMPtr[2] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[3] = (u8*)0x06880000+0x2000;        // Segment 0 default
            Slot1ROMPtr[4] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[5] = (u8*)0x06880000+0x2000;        // Segment 0 default
            Slot1ROMPtr[6] = (u8*)0x06880000+0x0000;        // Segment 0 default
            Slot1ROMPtr[7] = (u8*)0x06880000+0x2000;        // Segment 0 default
        }                

        // --------------------------------------------------------------------------------
        // Now copy as much of the ROM into fast VRAM as possible. We only have 128K of
        // VRAM available - anything beyond this will have to be fetched from slow RAM.
        // --------------------------------------------------------------------------------
        if (iSSize <= (128 * 1024))
        {
            memcpy((u8*)0x06880000, romBuffer,   iSSize);        // All 64K or 128K copied into our fast VRAM buffer
            if (mapperType == ASC16 || mapperType == ZEN16)
                mapperMask = (iSSize == (64 * 1024)) ? 0x03:0x07;
            else
                mapperMask = (iSSize == (64 * 1024)) ? 0x07:0x0F;
        }
        else
        {
            memcpy((u8*)0x06880000, romBuffer,   0x20000);       // First 128K copied into our fast VRAM buffer
            if (mapperType == ASC16 || mapperType == ZEN16)
                mapperMask = (iSSize == (512 * 1024)) ? 0x1F:0x0F;
            else
                mapperMask = (iSSize == (512 * 1024)) ? 0x3F:0x1F;
        }        
    }
    else    
    {
        // Size not right for MSX support... we've already pre-filled 0xFF so nothing more to do here...
    }
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
    fseek(handle, 0, SEEK_END);
    int iSSize = ftell(handle);
    fseek(handle, 0, SEEK_SET);
    if(iSSize <= (512 * 1024))  // Max size cart is 512KB - that's pretty huge...
    {
        memset(romBuffer, 0xFF, (512 * 1024));
        fread((void*) romBuffer, iSSize, 1, handle); 
        
        romBankMask = 0x00;         // No bank mask until proven otherwise
        bMagicMegaCart = false;     // No Mega Cart to start
        mapperMask = 0x00;          // No MSX mapper mask
        bActivisionPCB = 0;         // No Activision PCB

        // ------------------------------------------------------------------------------
        // For the MSX emulation, we will use fast VRAM to hold ROM and mirrors...
        // These need to be loaded in a special way:
        //    8K ROMs will mirror every 8K
        //   16K ROMs will mirror every 16K
        //   32K ROMs will mirror lower 16K at 0x0000 and then ROM is loaded at 0x4000
        // ------------------------------------------------------------------------------
        if (msx_mode)
        {
            MSX_InitialMemoryLayout(iSSize);
        }            
        else
        // ----------------------------------------------------------------------
        // Do we fit within the standard 32K Colecovision Cart ROM memory space?
        // ----------------------------------------------------------------------
        if (iSSize <= ((sg1000_mode ? 48:32)*1024)) // Allow SG ROMs to be up to 48K
        {
            memcpy(ptr, romBuffer, nmemb);
        }
        else    // No - must be MC Bankswitched Cart!!
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
  if (sg1000_mode) {return cpu_readport_sg(Port);}    
  if (sordm5_mode) {return cpu_readport_m5(Port);}    
  if (msx_mode)    {return cpu_readport_msx(Port);}
    
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
  if      (sg1000_mode) {cpu_writeport_sg(Port, Value); return;}
  else if (sordm5_mode) {cpu_writeport_m5(Port, Value); return;}
  //if (msx_mode)    {cpu_writeport_msx(Port, Value); return;}      // This is now handled in DrZ80 and CZ80 directly
    
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

// ------------------------------------------------------------------
// SG-1000 IO Port Read - just VDP and Joystick to contend with...
// ------------------------------------------------------------------
unsigned char cpu_readport_sg(register unsigned short Port) 
{
  // SG-1000 ports are 8-bit
  Port &= 0x00FF; 

  if ((Port & 0xE0) == 0xA0)  // VDP Area
  {
      if (Port & 1) return(RdCtrl9918()); 
      else return(RdData9918());
  }
  else if ((Port == 0xDC) || (Port == 0xC0)) // Joystick Port 1
  {
      u8 joy1 = 0x00;

      if (JoyState & JST_UP)    joy1 |= 0x01;
      if (JoyState & JST_DOWN)  joy1 |= 0x02;
      if (JoyState & JST_LEFT)  joy1 |= 0x04;
      if (JoyState & JST_RIGHT) joy1 |= 0x08;
      
      if (JoyState & JST_FIREL) joy1 |= 0x10;
      if (JoyState & JST_FIRER) joy1 |= 0x20;
      return (~joy1);
  }
  else if ((Port == 0xDD) || (Port == 0xC1)) // Joystick Port 2
  {
      u8 joy2 = 0x00;
      if (JoyState & JST_BLUE) joy2 |= 0x10;    // Reset (not sure this is used)
      return (~joy2);
  }
    
  // No such port
  return(NORAM);
}

// ----------------------------------------------------------------------
// SG-1000 IO Port Write - just VDP and SN Sound Chip to contend with...
// ----------------------------------------------------------------------
void cpu_writeport_sg(register unsigned short Port,register unsigned char Value) 
{
    // SG-1000 ports are 8-bit
    Port &= 0x00FF;

    if ((Port & 0xE0) == 0xA0)  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) { CPU.IRequest=INT_RST38; cpuirequest=Z80_IRQ_INT; }    // SG-1000 does not use NMI like Colecovision does...
    }
    else if (Port == 0x7E) sn76496W(Value, &sncol);
    else if (Port == 0x7F) sn76496W(Value, &sncol);
}


// ------------------------------------------------------------------
// MSX IO Port Read - just VDP and Joystick to contend with...
// ------------------------------------------------------------------
unsigned char cpu_readport_msx(register unsigned short Port) 
{
  // MSX ports are 8-bit
  Port &= 0x00FF; 

  //98h~9Bh   Access to the VDP I/O ports.    
  if ((Port >= 0x98 && Port <= 0x9B))  // VDP Area
  {
      if (Port & 1) return(RdCtrl9918()); 
      else return(RdData9918());
  }
  else if (Port == 0xA2)  // PSG Read... might be joypad data
  {
      // -------------------------------------------
      // Only port 1 is used for the first Joystick
      // -------------------------------------------
      if (ay_reg_idx == 14)
      {
          u8 joy1 = 0x00;

          // Only port 1... not port 2
          if ((ay_reg[15] & 0x40) == 0)
          {
              if (JoyState & JST_UP)    joy1 |= 0x01;
              if (JoyState & JST_DOWN)  joy1 |= 0x02;
              if (JoyState & JST_LEFT)  joy1 |= 0x04;
              if (JoyState & JST_RIGHT) joy1 |= 0x08;

              if (JoyState & JST_FIREL) joy1 |= 0x10;
              if (JoyState & JST_FIRER) joy1 |= 0x20;
          }

          ay_reg[14] = ~joy1;
      }
      return FakeAY_ReadData();
  }
  else if (Port == 0xA8) return PortA8;
  else if (Port == 0xA9)
  {
      // Keyboard Port:
      //  Line  Bit_7 Bit_6 Bit_5 Bit_4 Bit_3 Bit_2 Bit_1 Bit_0
      //   0     "7"   "6"   "5"   "4"   "3"   "2"   "1"   "0"
      //   1     ";"   "]"   "["   "\"   "="   "-"   "9"   "8"
      //   2     "B"   "A"   ???   "/"   "."   ","   "'"   "`"
      //   3     "J"   "I"   "H"   "G"   "F"   "E"   "D"   "C"
      //   4     "R"   "Q"   "P"   "O"   "N"   "M"   "L"   "K"
      //   5     "Z"   "Y"   "X"   "W"   "V"   "U"   "T"   "S"
      //   6     F3    F2    F1   CODE   CAP  GRAPH CTRL  SHIFT
      //   7     RET   SEL   BS   STOP   TAB   ESC   F5    F4
      //   8    RIGHT DOWN   UP   LEFT   DEL   INS  HOME  SPACE
      
      u8 key1 = 0x00;
      if ((PortAA & 0x0F) == 0)      // Row 0
      {
          if (JoyState == JST_0)   key1 |= 0x01;  // '0'
          if (JoyState == JST_1)   key1 |= 0x02;  // '1'
          if (JoyState == JST_2)   key1 |= 0x04;  // '2'
          if (JoyState == JST_3)   key1 |= 0x08;  // '3'
          if (JoyState == JST_4)   key1 |= 0x10;  // '4'
          if (JoyState == JST_5)   // This one can be user-defined
          {
              if (myConfig.msxKey5 == 0) key1 |= 0x20;  // '5'
              if (myConfig.msxKey5 == 6) key1 |= 0x40;  // '6'
              if (myConfig.msxKey5 == 7) key1 |= 0x80;  // '7'
          }
      }
      else if ((PortAA & 0x0F) == 1)  // Row 1
      {
          if (JoyState == JST_5)
          {
              if (myConfig.msxKey5 == 8) key1 |= 0x01;  // '8'
              if (myConfig.msxKey5 == 9) key1 |= 0x02;  // '9'
          }
      }
      else if ((PortAA & 0x0F) == 2)  // Row 2
      {
          if (JoyState == JST_5)
          {
              if (myConfig.msxKey5 == 10) key1 |= 0x40;  // 'A'
              if (myConfig.msxKey5 == 11) key1 |= 0x80;  // 'B'
          }
      }
      else if ((PortAA & 0x0F) == 3)  // Row 3
      {
          if (JoyState == JST_5)
          {
              if (myConfig.msxKey5 == 12) key1 |= 0x01;  // 'C'
              if (myConfig.msxKey5 == 13) key1 |= 0x02;  // 'D'
              if (myConfig.msxKey5 == 14) key1 |= 0x04;  // 'E'
              if (myConfig.msxKey5 == 15) key1 |= 0x08;  // 'F'
              if (myConfig.msxKey5 == 16) key1 |= 0x10;  // 'G'
              if (myConfig.msxKey5 == 17) key1 |= 0x20;  // 'H'
              if (myConfig.msxKey5 == 18) key1 |= 0x40;  // 'I'
              if (myConfig.msxKey5 == 19) key1 |= 0x80;  // 'J'
          }
      }
      else if ((PortAA & 0x0F) == 4)  // Row 4
      {
          if (JoyState == JST_5)
          {
              if (myConfig.msxKey5 == 20) key1 |= 0x01;  // 'K'
              if (myConfig.msxKey5 == 21) key1 |= 0x02;  // 'L'
              if (myConfig.msxKey5 == 22) key1 |= 0x04;  // 'M'
              if (myConfig.msxKey5 == 23) key1 |= 0x08;  // 'N'
              if (myConfig.msxKey5 == 24) key1 |= 0x10;  // 'O'
              if (myConfig.msxKey5 == 25) key1 |= 0x20;  // 'P'
              if (myConfig.msxKey5 == 26) key1 |= 0x40;  // 'Q'
              if (myConfig.msxKey5 == 27) key1 |= 0x80;  // 'R'
          }
      }
      else if ((PortAA & 0x0F) == 5)  // Row 5
      {
          if (JoyState == JST_5)
          {
              if (myConfig.msxKey5 == 28) key1 |= 0x01;  // 'S'
              if (myConfig.msxKey5 == 29) key1 |= 0x02;  // 'T'
              if (myConfig.msxKey5 == 30) key1 |= 0x04;  // 'U'
              if (myConfig.msxKey5 == 31) key1 |= 0x08;  // 'V'
              if (myConfig.msxKey5 == 32) key1 |= 0x10;  // 'W'
              if (myConfig.msxKey5 == 33) key1 |= 0x20;  // 'X'
              if (myConfig.msxKey5 == 34) key1 |= 0x40;  // 'Y'
              if (myConfig.msxKey5 == 35) key1 |= 0x80;  // 'Z'
          }
      }      
      else if ((PortAA & 0x0F) == 6) // Row 6
      {
          if (JoyState == JST_7) key1 |= 0x20;    // F1
          if (JoyState == JST_8) key1 |= 0x40;    // F2
          if (JoyState == JST_9) key1 |= 0x80;    // F3
          if (JoyState == JST_5)
          {
            if (myConfig.msxKey5 == 1) key1 |= 0x01;  // SHIFT
            if (myConfig.msxKey5 == 2) key1 |= 0x02;  // CTRL
          }
      }
      else if ((PortAA & 0x0F) == 7) // Row 7
      {
          if (JoyState == JST_6)     key1 |= 0x10;  // STOP
          if (JoyState == JST_POUND) key1 |= 0x80;  // RETURN
          if (JoyState == JST_5)
          {
            if (myConfig.msxKey5 == 4) key1 |= 0x01;  // F4
            if (myConfig.msxKey5 == 5) key1 |= 0x02;  // F5
            if (myConfig.msxKey5 == 3) key1 |= 0x04;  // ESC
          }
      }
      else if ((PortAA & 0x0F) == 8) // Row 8
      {
          if (JoyState == JST_STAR) key1 |= 0x01;  // SPACE
      }
      return ~key1;
  }
  else if (Port == 0xAA) return PortAA;
    
  // No such port
  return(NORAM);
}

// -------------------------------------------------------
// Move numBytes of memory as 32-bit words for best speed
// -------------------------------------------------------
inline void FastMemCopy(u8* dest, u8* src, u16 numBytes)
{
    u32 *d=(u32*)dest;   u32 *s=(u32*)src;
    for (u16 i=0; i<numBytes/4; i++) *d++ = *s++;
}


// ----------------------------
// Save MSX Ram from Slot
// ----------------------------
ITCM_CODE void SaveRAM(u8 slot)
{
    // Only save if we had RAM in this slot previously
    if (bRAMInSlot[slot] == 1)
    {
        if (LastROMSize <= (64 * 1024)) // If RAM buffer in VRAM, use DMA 
        {
            DC_FlushRange(pColecoMem+(slot*0x4000), 0x4000);
            dmaCopyWords(3, pColecoMem+(slot*0x4000), Slot3RAMPtr+(slot*0x4000), 0x4000);
        }
        else
        {
            FastMemCopy(Slot3RAMPtr+(slot*0x4000), pColecoMem+(slot*0x4000), 0x4000);  // Move 16K of RAM from main memory into the MSX RAM buffer
        }
    }
}

// ----------------------------
// Restore MSX Ram to Slot
// ----------------------------
ITCM_CODE void RestoreRAM(u8 slot)
{
    // Only restore if we didn't have RAM here already...
    if (bRAMInSlot[slot] == 0)
    {
        if (LastROMSize <= (64 * 1024)) // If RAM buffer in VRAM, use DMA 
        {
            DC_FlushRange(pColecoMem+(slot*0x4000), 0x4000);
            dmaCopyWords(3, Slot3RAMPtr+(slot*0x4000), pColecoMem+(slot*0x4000), 0x4000);
        }
        else
        {
            FastMemCopy(pColecoMem+(slot*0x4000), Slot3RAMPtr+(slot*0x4000), 0x4000);  // Move 16K of RAM from MSX RAM buffer back into main RAM
        }
    }
}

// ----------------------------------------------------------------------
// MSX IO Port Write - VDP and AY Sound Chip plus Slot Mapper $A8
// ----------------------------------------------------------------------
void cpu_writeport_msx(register unsigned short Port,register unsigned char Value) 
{
    // MSX ports are 8-bit
    Port &= 0x00FF;

    if ((Port >= 0x98 && Port <= 0x9B))  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) { CPU.IRequest=INT_RST38; cpuirequest=Z80_IRQ_INT; }    // MSX does not use NMI like Colecovision does...
    }
    else if (Port == 0xA8) // Slot system for MSX
    {
        if (PortA8 != Value)
        {
            // ---------------------------------------------------------------------
            // bits 7-6     bits 5-4     bits 3-2      bits 1-0
            // C000h~FFFF   8000h~BFFF   4000h~7FFF    0000h~3FFF
            // 
            // Slot 0 holds the 32K of MSX BIOS (0xFF above 32K)
            // Slot 1 is where the Game Cartridge Lives (up to 64K)
            // Slot 2 is empty (0xFF always)
            // Slot 3 is our main RAM. We emulate 64K of RAM
            // ---------------------------------------------------------------------
            if (((Value>>0) & 0x03) != ((PortA8>>0) & 0x03))

            switch ((Value>>0) & 0x03)  // Main Memory - Slot 0 [0x0000~0x3FFF]
            {
                case 0x00:  // Slot 0:  Maps to BIOS Rom
                    SaveRAM(0);
                    FastMemCopy(pColecoMem+0x0000, (u8 *)(Slot0BIOS+0x0000), 0x4000);
                    bROMInSlot[0] = 0;
                    bRAMInSlot[0] = 0;
                    break;
                case 0x01:  // Slot 1:  Maps to Game Cart
                    SaveRAM(0);
                    if (Slot1ROMPtr[0])  FastMemCopy(pColecoMem+0x0000, (u8 *)(Slot1ROMPtr[0]), 0x2000);
                    else  FastMemCopy(pColecoMem+0x0000, (u8 *)(Slot1ROM+0x0000), 0x2000);
                    if (Slot1ROMPtr[1])  FastMemCopy(pColecoMem+0x2000, (u8 *)(Slot1ROMPtr[1]), 0x2000);
                    else  FastMemCopy(pColecoMem+0x2000, (u8 *)(Slot1ROM+0x2000), 0x2000);
                    bROMInSlot[0] = 1;
                    bRAMInSlot[0] = 0;
                    break;
                case 0x02:  // Slot 2:  Maps to nothing... 0xFF
                    SaveRAM(0);
                    memset(pColecoMem+0x0000, 0xFF, 0x4000);
                    bROMInSlot[0] = 0;
                    bRAMInSlot[0] = 0;
                    break;
                case 0x03:  // Slot 3:  Maps to our 64K of RAM
                    RestoreRAM(0);
                    bROMInSlot[0] = 0;
                    bRAMInSlot[0] = 1;
                    break;
            }
            
            if (((Value>>2) & 0x03) != ((PortA8>>2) & 0x03))
            switch ((Value>>2) & 0x03)  // Main Memory - Slot 1  [0x4000~0x7FFF]
            {
                case 0x00:  // Slot 0:  Maps to BIOS Rom
                    SaveRAM(1);
                    FastMemCopy(pColecoMem+0x4000, (u8 *)(Slot0BIOS+0x4000), 0x4000);
                    bROMInSlot[1] = 0;
                    bRAMInSlot[1] = 0;
                    break;
                case 0x01:  // Slot 1:  Maps to Game Cart
                    SaveRAM(1);
                    if (Slot1ROMPtr[2])  FastMemCopy(pColecoMem+0x4000, (u8 *)(Slot1ROMPtr[2]), 0x2000);
                    else  FastMemCopy(pColecoMem+0x4000, (u8 *)(Slot1ROM+0x4000), 0x2000);
                    if (Slot1ROMPtr[3])  FastMemCopy(pColecoMem+0x6000, (u8 *)(Slot1ROMPtr[3]), 0x2000);
                    else  FastMemCopy(pColecoMem+0x6000, (u8 *)(Slot1ROM+0x6000), 0x2000);                    
                    bROMInSlot[1] = 1;
                    bRAMInSlot[1] = 0;
                    break;
                case 0x02:  // Slot 2:  Maps to nothing... 0xFF
                    SaveRAM(1);
                    memset(pColecoMem+0x4000, 0xFF, 0x4000);
                    bROMInSlot[1] = 0;
                    bRAMInSlot[1] = 0;
                    break;
                case 0x03:  // Slot 3:  Maps to our 64K of RAM
                    RestoreRAM(1);
                    bROMInSlot[1] = 0;
                    bRAMInSlot[1] = 1;
                    break;
            }
            
            if (((Value>>4) & 0x03) != ((PortA8>>4) & 0x03))
            switch ((Value>>4) & 0x03)  // Main Memory - Slot 2  [0x8000~0xBFFF]
            {
                case 0x00:  // Slot 0:  Maps to nothing... 0xFF
                    SaveRAM(2);
                    memset(pColecoMem+0x8000, 0xFF, 0x4000);
                    bROMInSlot[2] = 0;
                    bRAMInSlot[2] = 0;
                    break;
                case 0x01:  // Slot 1:  Maps to Game Cart
                    SaveRAM(2);
                    if (Slot1ROMPtr[4])  FastMemCopy(pColecoMem+0x8000, (u8 *)(Slot1ROMPtr[4]), 0x2000);
                    else  FastMemCopy(pColecoMem+0x8000, (u8 *)(Slot1ROM+0x8000), 0x2000);
                    if (Slot1ROMPtr[5])  FastMemCopy(pColecoMem+0xA000, (u8 *)(Slot1ROMPtr[5]), 0x2000);
                    else  FastMemCopy(pColecoMem+0xA000, (u8 *)(Slot1ROM+0xA000), 0x2000);                    
                    bROMInSlot[2] = 1;
                    bRAMInSlot[2] = 0;
                    break;
                case 0x02:  // Slot 2:  Maps to nothing... 0xFF
                    SaveRAM(2);
                    memset(pColecoMem+0x8000, 0xFF, 0x4000);
                    bROMInSlot[2] = 0;
                    bRAMInSlot[2] = 0;
                    break;
                case 0x03:  // Slot 3:  Maps to our 64K of RAM
                    RestoreRAM(2);
                    bROMInSlot[2] = 0;
                    bRAMInSlot[2] = 1;
                    break;
            }
            
            if (((Value>>6) & 0x03) != ((PortA8>>6) & 0x03))
            switch ((Value>>6) & 0x03)  // Main Memory - Slot 3  [0xC000~0xFFFF]
            {
                case 0x00:  // Slot 0:  Maps to nothing... 0xFF
                    SaveRAM(3);
                    memset(pColecoMem+0xC000, 0xFF, 0x4000);
                    bROMInSlot[3] = 0;
                    bRAMInSlot[3] = 0;
                    break;
                case 0x01:  // Slot 1:  Maps to Game Cart
                    SaveRAM(3);
                    if (Slot1ROMPtr[6])  FastMemCopy(pColecoMem+0xC000, (u8 *)(Slot1ROMPtr[6]), 0x2000);
                    else  FastMemCopy(pColecoMem+0xC000, (u8 *)(Slot1ROM+0xC000), 0x2000);
                    if (Slot1ROMPtr[7])  FastMemCopy(pColecoMem+0xE000, (u8 *)(Slot1ROMPtr[7]), 0x2000);
                    else  FastMemCopy(pColecoMem+0xE000, (u8 *)(Slot1ROM+0xE000), 0x2000);
                    bROMInSlot[3] = 1;
                    bRAMInSlot[3] = 0;
                    break;
                case 0x02:  // Slot 2:  Maps to nothing... 0xFF
                    SaveRAM(3);
                    memset(pColecoMem+0xC000, 0xFF, 0x4000);
                    bROMInSlot[3] = 0;
                    bRAMInSlot[3] = 0;
                    break;
                case 0x03:  // Slot 3 is RAM so we allow RAM writes now
                    RestoreRAM(3);
                    bROMInSlot[3] = 0;
                    bRAMInSlot[3] = 1;
                    break;
            }
            
            PortA8 = Value;             // Useful when read back
        }
    }
    else if (Port == 0xA9)  // PPI - Register B
    {
        PortA9 = Value;
    }
    else if (Port == 0xAA)  // PPI - Register C
    {
        PortAA = Value;
    }
    else if (Port == 0xA0) {FakeAY_WriteIndex(Value & 0x0F);}
    else if (Port == 0xA1) FakeAY_WriteData(Value);
}


// ------------------------------------------------------------------
// Sord M5 IO Port Read - just VDP, Joystick/Keyboard and Z80-CTC
// ------------------------------------------------------------------
unsigned char cpu_readport_m5(register unsigned short Port) 
{
  // M5 ports are 8-bit
  Port &= 0x00FF; 

  if (Port >= 0x00 && Port <= 0x03)      // Z80-CTC Area
  {
      return ctc_timer[Port];
  }
  else if ((Port == 0x10) || (Port == 0x11))  // VDP Area
  {
      return(Port&0x01 ? RdCtrl9918():RdData9918());      
  }
  else if (Port == 0x30)    // Y0
  {
      u8 joy1 = 0x00;
      return (joy1);
  }
  else if (Port == 0x31)    // Y1
  {
      u8 joy1 = 0x00;
      if (JoyState & JST_FIREL) joy1 |= 0x01;  // '1' (joystick button 1)
      if (JoyState & JST_FIRER) joy1 |= 0x02;  // '2' (joystick button 2)
      if (JoyState & JST_1)   joy1 |= 0x01;  // '1'
      if (JoyState & JST_2)   joy1 |= 0x02;  // '2'
      if (JoyState & JST_3)   joy1 |= 0x04;  // '3'
      if (JoyState & JST_4)   joy1 |= 0x08;  // '4'
      if (JoyState & JST_5)   joy1 |= 0x10;  // '5'
      if (JoyState & JST_6)   joy1 |= 0x20;  // '6'
      if (JoyState & JST_7)   joy1 |= 0x40;  // '7'
      if (JoyState & JST_8)   joy1 |= 0x80;  // '8'
      return (joy1);
  }
  else if (Port >= 0x32 && Port < 0x37)
  {
      return 0x00;
  }
  else if (Port == 0x37)    // Joystick Port 1
  {
      u8 joy1 = 0x00;
      if (JoyState & JST_UP)     joy1 |= 0x02;
      if (JoyState & JST_DOWN)   joy1 |= 0x08;
      if (JoyState & JST_LEFT)   joy1 |= 0x04;
      if (JoyState & JST_RIGHT)  joy1 |= 0x01;      
      return (joy1);
  }
  else if (Port == 0x50)
  {
      return 0x00;
  }
    
  // No such port
  return(NORAM);
}

// --------------------------------------------------------------------------
// Sord M5 IO Port Write - Need to handle SN sound, VDP and the Z80-CTC chip
// --------------------------------------------------------------------------
#define CTC_SOUND_DIV 280
void cpu_writeport_m5(register unsigned short Port,register unsigned char Value) 
{
    // M5 ports are 8-bit
    Port &= 0x00FF;

    // ----------------------------------------------------------------------
    // Z80-CTC Area
    // This is only a partial implementation of the CTC logic - just enough
    // to handle the VDP and Sound Generation and very little else. This is
    // NOT accurate emulation - but it's good enough to render the Sord M5
    // games as playable in this emulator.
    // ----------------------------------------------------------------------
    if (Port >= 0x00 && Port <= 0x03)      
    {
        if (ctc_latch[Port])    // If latched, we now have the countdown timer value
        {
            ctc_time[Port] = Value;     // Latch the time constant and compute the countdown timer directly below.
            ctc_timer[Port] = ((((ctc_control[Port] & 0x20) ? 256 : 16) * (ctc_time[Port] ? ctc_time[Port]:256)) / CTC_SOUND_DIV) + 1;
            ctc_latch[Port] = 0x00;     // Reset the latch - we're back to looking for control words
        }
        else
        {
            if (Value & 1) // Control Word
            {
                ctc_control[Port] = Value;      // Keep track of the control port 
                ctc_latch[Port] = Value & 0x04; // If the caller wants to set a countdown timer, the next value read will latch the timer
            }
            else
            {
                if (Port == 0x00) // Channel 0, bit0 clear is special - this is where the 4 CTC vector addresses are setup
                {
                    ctc_vector[0] = (Value & 0xf8) | 0;     // Usually used for SIO which is not emulated here
                    ctc_vector[1] = (Value & 0xf8) | 2;     // Usually used for PSG sound generation which we must deal with
                    ctc_vector[2] = (Value & 0xf8) | 4;     // Usually used for SIO which is not emulated here
                    ctc_vector[3] = (Value & 0xf8) | 6;     // Used for the VDP interrupt - this one is crucial!
                    sordm5_irq = ctc_vector[3];             // When the VDP interrupts the CPU, it's channel 3 on the CTC
                }
            }
        }
    }
    else if ((Port == 0x10) || (Port == 0x11))  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) CPU.IRequest=sordm5_irq;    // Sord M5 must get vector from the Z80-CTC. Only the CZ80 core works with this.
    }
    else if (Port == 0x20) sn76496W(Value, &sncol);
}

// ----------------------------------------------------------------
// Fires every scanline if we are in Sord M5 mode - this provides
// some rough timing for the Z80-CTC chip. It's not perfectly
// accurate but it's good enough for our purposes.  Many of the
// M5 games use the CTC timers to generate sound/music.
// ----------------------------------------------------------------
void Z80CTC_Timer(void)
{
    if (CPU.IRequest == INT_NONE)
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
  if (!msx_mode && !sordm5_mode)
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
      if(Loop9918()) cpuirequest = ((msx_mode || sg1000_mode) ? Z80_IRQ_INT : Z80_NMI_INT);
      
      // Generate interrupt if called for
      if (cpuirequest)
        DrZ80_Cause_Interrupt(cpuirequest);
      else
        DrZ80_Clear_Pending_Interrupts();
  }
  else  // CZ80 core from fMSX()... slower but higher accuracy
  {
      // Execute 1 scanline worth of CPU instructions
      cycle_deficit = ExecZ80(TMS9918_LINE + cycle_deficit);
      
      // Refresh VDP 
      if(Loop9918()) 
      {
          if (sordm5_mode) CPU.IRequest = sordm5_irq;   // Sord M5 only works with the CZ80 core
          else CPU.IRequest = ((msx_mode || sg1000_mode) ? INT_RST38 : INT_NMI);
      }
      
      // -------------------------------------------------------------------------
      // The Sord M5 has a CTC timer circuit that needs attention - this isnt 
      // timing accurate but it's good enough to allow those timers to trigger.
      // -------------------------------------------------------------------------
      if (sordm5_mode) Z80CTC_Timer();

      // Generate an interrupt if called for...
      if(CPU.IRequest!=INT_NONE) 
      {
          IntZ80(&CPU, CPU.IRequest);
#ifdef DEBUG_Z80 
          CPU.User++;   // Track Interrupt Requests
#endif          
      }
  }
  
  // Drop out unless end of screen is reached 
  return (CurLine == TMS9918_END_LINE) ? 0:1;
}

// End of file
