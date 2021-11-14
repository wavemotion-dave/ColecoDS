/******************************************************************************
*  ColecoDS managment file
*  Ver 1.0
*
*  Copyright (C) 2006 AlekMaul . All rights reserved.
*       http://www.portabledev.com
******************************************************************************/
#include <nds.h>
#include <nds/arm9/console.h> //basic print funcionality

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <fat.h>

#include "colecoDS.h"

#include "cpu/z80/drz80/Z80_interface.h"

#include "colecomngt.h"
#include "colecogeneric.h"

extern byte Loop9918(void);
extern void DrZ80_InitFonct(void);

#include "cpu/tms9928a/tms9928a.h"

#include "cpu/sn76496/sn76496_c.h"

#define NORAM 0xFF

#define COLECODS_SAVE_VER 0x0003

extern const unsigned short sprPause_Palette[16];
extern const unsigned char sprPause_Bitmap[2560];
extern u32*lutTablehh;

u8 romBuffer[512 * 1024] ALIGN(32);   // We support MegaCarts up to 512KB
u8 romBankMask = 0x00;

u8 bBlendMode = false;

u8 sgm_enable = false;
u8 sgm_idx=0;
u8 sgm_reg[256] = {0};
u16 sgm_low_addr = 0x2000;

u8 channel_a_enable = 0;
u8 channel_b_enable = 0;
u8 channel_c_enable = 0;
u8 noise_enable = 0;

// Reset the Super Game Module vars...
void sgm_reset(void)
{
    //make sure Super Game Module registers for AY chip are clear...
    memset(sgm_reg, 0x00, 256);
    sgm_reg[0x07] = 0xFF; // Everything turned off to start...
    channel_a_enable = 0;
    channel_b_enable = 0;
    channel_c_enable = 0;
    noise_enable = 0;      
    sgm_enable = false;
}

/********************************************************************************/

u32 JoyMode;                     // Joystick / Paddle management
u32 JoyStat[2];                  // Joystick / Paddle management

u32 JoyState=0;                  // Joystick V2

u32 ExitNow=0;

u8 VDPInit[8] = { 0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00 };   // VDP control register states

sn76496 sncol __attribute__((section(".dtcm")));
u16 freqtablcol[1024*2] __attribute__((section(".dtcm")));


/*********************************************************************************
 * Init coleco Engine for that game
 ********************************************************************************/
u8 colecoInit(char *szGame) {
  u8 RetFct,uBcl;
  u16 uVide;

  // Wipe RAM
  memset(pColecoMem+0x2000, 0x00, 0x6000);
  
  // Set ROM area to 0xFF before load
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
      uVide=(uBcl/12);//+((uBcl/12)<<8);
      dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
    }
  
    // Make sure the super game module is disabled to start
    sgm_reset();

    JoyMode=0;                           // Joystick mode key
    JoyStat[0]=JoyStat[1]=0xFFFF;        // Joystick states

    SN76496_set_mixrate(&sncol,1);
    SN76496_set_frequency(&sncol,TMS9918_BASE);
    SN76496_init(&sncol,(u16 *) &freqtablcol);
    SN76496_reset(&sncol,0);
      
    ExitNow = 0;

    DrZ80_Reset();
    Reset9918();
      
    soundEmuPause=0;
  }
  
  fifoSendValue32(FIFO_USER_01,(1<<16) | (127) | SOUND_SET_VOLUME);
        
  // Return with result
  return (RetFct);
}

/*********************************************************************************
 * Run the emul
 ********************************************************************************/
void colecoRun(void) {
  DrZ80_Reset();

  showMainMenu();
}

/*********************************************************************************
 * Set coleco Palette
 ********************************************************************************/
void colecoSetPal(void) {
  u8 uBcl,r,g,b;
  
  for (uBcl=0;uBcl<16;uBcl++) {
    r = (u8) ((float) TMS9928A_palette[uBcl*3+0]*0.121568f);
    g = (u8) ((float) TMS9928A_palette[uBcl*3+1]*0.121568f);
    b = (u8) ((float) TMS9928A_palette[uBcl*3+2]*0.121568f);

    SPRITE_PALETTE[uBcl] = RGB15(r,g,b);
    BG_PALETTE[uBcl] = RGB15(r,g,b);
  }
}


/*********************************************************************************
 * Save the current state
 ********************************************************************************/
u8  spare[512] = {0x00};
void colecoSaveState() 
{
  u32 uNbO;
  long pSvg;
  char szFile[256];
  char szCh1[128],szCh2[128];

  // Init filename = romname and STA in place of ROM
  strcpy(szFile,gpFic[ucGameAct].szName);
  szFile[strlen(szFile)-3] = 's';
  szFile[strlen(szFile)-2] = 'a';
  szFile[strlen(szFile)-1] = 'v';
  sprintf(szCh1,"SAVING...");
  AffChaine(19,5,0,szCh1);
  
  FILE *handle = fopen(szFile, "w+");  
  if (handle != NULL) 
  {
    // Write Version
    u16 save_ver = COLECODS_SAVE_VER;
    uNbO = fwrite(&save_ver, sizeof(u16), 1, handle);
      
    // Write Z80 CPU
    uNbO = fwrite(&drz80, sizeof(struct DrZ80), 1, handle);

    // Save Coleco Memory (yes, all of it!)
    if (uNbO) uNbO = fwrite(pColecoMem, 0x10000,1, handle); 
      
    // Write XBuf Video Buffer (yes all of it!)
    if (uNbO) uNbO = fwrite(XBuf, sizeof(XBuf),1, handle); 
      
    // Write look-up-table
    if (uNbO) uNbO = fwrite(lutTablehh, 16*1024,1, handle);      

    // Write the Super Game Module stuff
    if (uNbO) uNbO = fwrite(sgm_reg, 256, 1, handle);      
    if (uNbO) uNbO = fwrite(&sgm_enable, sizeof(sgm_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&sgm_idx, sizeof(sgm_idx), 1, handle); 
    if (uNbO) uNbO = fwrite(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
    if (uNbO) uNbO = fwrite(&channel_a_enable, sizeof(channel_a_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&channel_b_enable, sizeof(channel_b_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&channel_c_enable, sizeof(channel_c_enable), 1, handle); 
    if (uNbO) uNbO = fwrite(&noise_enable, sizeof(noise_enable), 1, handle); 
      
    // A few frame counters
    if (uNbO) uNbO = fwrite(&emuActFrames, sizeof(emuActFrames), 1, handle); 
    if (uNbO) uNbO = fwrite(&timingFrames, sizeof(timingFrames), 1, handle); 
      
    // Some spare memory we can eat into...
    if (uNbO) uNbO = fwrite(&spare, sizeof(spare),1, handle); 
      
    // Write VDP
    if (uNbO) uNbO = fwrite(VDP, sizeof(VDP),1, handle); 
    if (uNbO) uNbO = fwrite(&VKey, sizeof(VKey),1, handle); 
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

    // Write PSG
    if (uNbO) uNbO = fwrite(&sncol, sizeof(sncol),1, handle); 
    if (uNbO) uNbO = fwrite(freqtablcol, sizeof(freqtablcol),1, handle); 
      
    if (uNbO) 
      strcpy(szCh2,"OK ");
    else
      strcpy(szCh2,"ERR");
     AffChaine(28,5,0,szCh2);
    WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
    AffChaine(18,5,0,"              ");  
  }
  else {
    strcpy(szCh2,"Error opening STA file ...");
  }
  fclose(handle);
}


/*********************************************************************************
 * Load the current state
 ********************************************************************************/
void colecoLoadState() 
{
  u32 uNbO;
  long pSvg;
  char szFile[256];
  char szCh1[128],szCh2[128];

    // Init filename = romname and STA in place of ROM
    strcpy(szFile,gpFic[ucGameAct].szName);
    szFile[strlen(szFile)-3] = 's';
    szFile[strlen(szFile)-2] = 'a';
    szFile[strlen(szFile)-1] = 'v';
    FILE* handle = fopen(szFile, "r"); 
    if (handle != NULL) 
    {    
         strcpy(szCh1,"LOADING...");
         AffChaine(19,5,0,szCh1);
       
        // Read Version
        u16 save_ver = 0xBEEF;
        uNbO = fread(&save_ver, sizeof(u16), 1, handle);
        
        if (save_ver == COLECODS_SAVE_VER)
        {
            // Load Z80 CPU
            uNbO = fread(&drz80, sizeof(struct DrZ80), 1, handle);
            DrZ80_InitFonct(); //DRZ80 saves a lot of binary code dependent stuff, regenerate it

            // Load Coleco Memory (yes, all of it!)
            if (uNbO) uNbO = fread(pColecoMem, 0x10000,1, handle); 

            // Load XBuf video buffer (yes, all of it!)
            if (uNbO) uNbO = fread(XBuf, sizeof(XBuf),1, handle); 

            // Load look-up-table
            if (uNbO) uNbO = fread(lutTablehh, 16*1024,1, handle);         
            
            // Load the Super Game Module stuff
            if (uNbO) uNbO = fread(sgm_reg, 256, 1, handle);      
            if (uNbO) uNbO = fread(&sgm_enable, sizeof(sgm_enable), 1, handle); 
            if (uNbO) uNbO = fread(&sgm_idx, sizeof(sgm_idx), 1, handle); 
            if (uNbO) uNbO = fread(&sgm_low_addr, sizeof(sgm_low_addr), 1, handle); 
            if (uNbO) uNbO = fread(&channel_a_enable, sizeof(channel_a_enable), 1, handle); 
            if (uNbO) uNbO = fread(&channel_b_enable, sizeof(channel_b_enable), 1, handle); 
            if (uNbO) uNbO = fread(&channel_c_enable, sizeof(channel_c_enable), 1, handle); 
            if (uNbO) uNbO = fread(&noise_enable, sizeof(noise_enable), 1, handle); 
            
            // A few frame counters
            if (uNbO) uNbO = fread(&emuActFrames, sizeof(emuActFrames), 1, handle); 
            if (uNbO) uNbO = fread(&timingFrames, sizeof(timingFrames), 1, handle); 
            
            // Load spare memory for future use
            if (uNbO) uNbO = fread(&spare, sizeof(spare),1, handle); 

            // Load VDP
            if (uNbO) uNbO = fread(VDP, sizeof(VDP),1, handle); 
            if (uNbO) uNbO = fread(&VKey, sizeof(VKey),1, handle); 
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
            
            // Load PSG
            if (uNbO) uNbO = fread(&sncol, sizeof(sncol),1, handle); 
            if (uNbO) uNbO = fread(freqtablcol, sizeof(freqtablcol),1, handle); 
            
              if (BGColor)
              {
                 u8 r = (u8) ((float) TMS9928A_palette[BGColor*3+0]*0.121568f);
                 u8 g = (u8) ((float) TMS9928A_palette[BGColor*3+1]*0.121568f);
                 u8 b = (u8) ((float) TMS9928A_palette[BGColor*3+2]*0.121568f);
                 BG_PALETTE[0] = RGB15(r,g,b);
              }
              else
              {
                  BG_PALETTE[0] = RGB15(0x00,0x00,0x00);
              }
            
            // Restore the screen as it was...
            dmaCopyWords(2, (u32*)XBuf, (u32*)pVidFlipBuf, 256*192);
            
            extern u8 lastBank;
            lastBank = 199;  // Force load of bank if needed
        }
        else uNbO = 0;
        
        if (uNbO) 
          strcpy(szCh2,"OK ");
        else
          strcpy(szCh2,"ERR");
         AffChaine(28,5,0,szCh2);
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        AffChaine(18,5,0,"              ");  
      }

    fclose(handle);
}

/*********************************************************************************
 * Update the screen for the current cycle
 ********************************************************************************/
ITCM_CODE void colecoUpdateScreen(void) 
{
    // ----------------------------------------------------------------   
    // If we are in 'blendMode' we will OR the last two frames. 
    // This helps on some games where things are just 1 pixel 
    // wide and the non XL/LL DSi will just not hold onto the
    // image long enough to render it properly for the eye to 
    // pick up. This takes CPU speed, however, and will not be
    // supported for older DS-LITE/PHAT units with slower processors.
    // ----------------------------------------------------------------   
    if (bBlendMode)
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
        // Not blend mode... just blast it out via DMA as fast as we can...
        dmaCopyWordsAsynch(2, (u32*)XBuf_A, (u32*)pVidFlipBuf, 256*192);
    }
}

/*********************************************************************************
 * Manage key / Paddle
 ********************************************************************************/
/*
    PORT_START_TAG("IN0")
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("0 (pad 1)") PORT_CODE(KEYCODE_0)
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("1 (pad 1)") PORT_CODE(KEYCODE_1)
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("2 (pad 1)") PORT_CODE(KEYCODE_2)
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("3 (pad 1)") PORT_CODE(KEYCODE_3)
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("4 (pad 1)") PORT_CODE(KEYCODE_4)
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("5 (pad 1)") PORT_CODE(KEYCODE_5)
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("6 (pad 1)") PORT_CODE(KEYCODE_6)
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("7 (pad 1)") PORT_CODE(KEYCODE_7)

    PORT_START_TAG("IN1")
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("8 (pad 1)") PORT_CODE(KEYCODE_8)
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("9 (pad 1)") PORT_CODE(KEYCODE_9)
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("# (pad 1)") PORT_CODE(KEYCODE_MINUS)
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME(". (pad 1)") PORT_CODE(KEYCODE_EQUALS)
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )
    PORT_BIT( 0xb0, IP_ACTIVE_LOW, IPT_UNKNOWN )

    PORT_START_TAG("IN2")
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )
    PORT_BIT( 0xb0, IP_ACTIVE_LOW, IPT_UNKNOWN )

    PORT_START_TAG("IN3")
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("0 (pad 2)") PORT_CODE(KEYCODE_0_PAD)
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("1 (pad 2)") PORT_CODE(KEYCODE_1_PAD)
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("2 (pad 2)") PORT_CODE(KEYCODE_2_PAD)
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("3 (pad 2)") PORT_CODE(KEYCODE_3_PAD)
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("4 (pad 2)") PORT_CODE(KEYCODE_4_PAD)
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("5 (pad 2)") PORT_CODE(KEYCODE_5_PAD)
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("6 (pad 2)") PORT_CODE(KEYCODE_6_PAD)
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("7 (pad 2)") PORT_CODE(KEYCODE_7_PAD)

    PORT_START_TAG("IN4")
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("8 (pad 2)") PORT_CODE(KEYCODE_8_PAD)
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("9 (pad 2)") PORT_CODE(KEYCODE_9_PAD)
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME("# (pad 2)") PORT_CODE(KEYCODE_MINUS_PAD)
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_KEYBOARD ) PORT_NAME(". (pad 2)") PORT_CODE(KEYCODE_PLUS_PAD)
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 )		PORT_PLAYER(2)
    PORT_BIT( 0xb0, IP_ACTIVE_LOW, IPT_UNKNOWN )

    PORT_START_TAG("IN5")
    PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )	PORT_PLAYER(2)
    PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )	PORT_PLAYER(2)
    PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )	PORT_PLAYER(2)
    PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )	PORT_PLAYER(2)
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON1 )		PORT_PLAYER(2)
    PORT_BIT( 0xb0, IP_ACTIVE_LOW, IPT_UNKNOWN )

    PORT_START_TAG("IN6")
    PORT_DIPNAME( 0x07, 0x00, "Extra Controllers" )
    PORT_DIPSETTING(	0x00, DEF_STR( None ) )
    PORT_DIPSETTING(	0x01, "Driving Controller" )
    PORT_DIPSETTING(	0x02, "Roller Controller" )
    PORT_DIPSETTING(	0x04, "Super Action Controllers" )
    PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_NAME("SAC Blue Button P1")	PORT_CODE(KEYCODE_Z)
    PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_NAME("SAC Purple Button P1")	PORT_CODE(KEYCODE_X)
    PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_NAME("SAC Blue Button P2")	PORT_CODE(KEYCODE_Q) PORT_PLAYER(2)
    PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_NAME("SAC Purple Button P2")	PORT_CODE(KEYCODE_W) PORT_PLAYER(2)

    PORT_START_TAG("IN7")	// Extra Controls (Driving Controller, SAC P1 slider, Roller Controller X Axis)
    PORT_BIT( 0x0f, 0x00, IPT_TRACKBALL_X ) PORT_SENSITIVITY(20) PORT_KEYDELTA(10) PORT_MINMAX(0, 0) PORT_CODE_DEC(KEYCODE_L) PORT_CODE_INC(KEYCODE_J) PORT_RESET

    PORT_START_TAG("IN8")	// Extra Controls (SAC P2 slider, Roller Controller Y Axis)
    PORT_BIT( 0x0f, 0x00, IPT_TRACKBALL_Y ) PORT_SENSITIVITY(20) PORT_KEYDELTA(10) PORT_MINMAX(0, 0) PORT_CODE_DEC(KEYCODE_I) PORT_CODE_INC(KEYCODE_K) PORT_RESET PORT_PLAYER(2)
*/

/*********************************************************************************
 * Check if the cart is valid...
 ********************************************************************************/
u8 colecoCartVerify(const u8 *cartData) {
  u8 RetFct = IMAGE_VERIFY_FAIL;

  // Verify the file is in Colecovision format
  // 1) Production Cartridge 
  if ((cartData[0] == 0xAA) && (cartData[1] == 0x55)) 
    RetFct = IMAGE_VERIFY_PASS;
  // 2) "Test" Cartridge. Some games use this method to skip ColecoVision title screen and delay
  if ((cartData[0] == 0x55) && (cartData[1] == 0xAA)) 
    RetFct = IMAGE_VERIFY_PASS;

  // TODO: for now... who are we to argue? Some SGM roms shift this up to bank 0 and it's not worth the hassle. The game either runs or it won't.
   RetFct = IMAGE_VERIFY_PASS;

  // Quit with verification cheched
  return RetFct;
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
        if (iSSize <= (32*1024))
        {
            memcpy(ptr, romBuffer, nmemb);
            romBankMask = 0x00;
        }
        else    // Bankswitched Cart!!
        {
            if (iSSize == (64 * 1024))  // Activision PCB is different... bank0 is placed in fixed ROM
            {
                memcpy(ptr, romBuffer, 0x4000);                     // bank 0
                memcpy(ptr+0x4000, romBuffer+0x4000, 0x4000);       // bank 1
                romBankMask = 0x03;
            }
            else
            {
                memcpy(ptr, romBuffer+(iSSize-0x4000), 0x4000); // For MegaCart, we map highest bank into fixed ROM
                memcpy(ptr+0x4000, romBuffer, 0x4000);          // Unclear what goes in the 16K "switchable" bank - we'll put bank 0 in there
                romBankMask = 0x07;
                if (iSSize == (128 * 1024)) romBankMask = 0x07;
                if (iSSize == (256 * 1024)) romBankMask = 0x0F;
                if (iSSize == (512 * 1024)) romBankMask = 0x1F;
            }
        }
        bOK = 1;
    }
    fclose(handle);
  }
  return bOK;
}


/** InZ80() **************************************************/
/** Z80 emulation calls this function to read a byte from   **/
/** a given I/O port.                                       **/
/*************************************************************/
ITCM_CODE unsigned char cpu_readport16(register unsigned short Port) {
#ifdef DEBUG
  //iprintf("cpu_readport16 %d \n",Port);
#endif
  static byte KeyCodes[16] = { 0x0A,0x0D,0x07,0x0C,0x02,0x03,0x0E,0x05, 0x01,0x0B,0x06,0x09,0x08,0x04,0x0F,0x0F, };

  //JGD 18/04/2007  
  Port &= 0x00FF; 
  
  // Port 52 is used for the AY sound chip for the Super Game Module
  if (Port == 0x52)
  {
      return sgm_reg[sgm_idx];
  }

  switch(Port&0xE0) {
    case 0x40: // Printer Status
      //if(Adam&&(Port==0x40)) return(0xFF);
      break;

    case 0xE0: // Joysticks Data
/* JGD 18/04/2007
      Port = Port&0x02? (JoyState>>16):JoyState;
      Port = JoyMode?   (Port>>8):Port;
      return(~Port&0x7F);
*/
      Port=(Port>>1)&0x01;
      Port=JoyMode? (JoyStat[Port]>>8): (JoyStat[Port]&0xF0)|KeyCodes[JoyStat[Port]&0x0F];
      return((Port|0xB0)&0x7F);

    case 0xA0: /* VDP Status/Data */
      return(Port&0x01? RdCtrl9918():RdData9918());
  }

  // No such port
  return(NORAM);
}


static const unsigned char Envelopes[16][32] =
{
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
};

static const u8 Volumes[16] = { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 };
u8 a_idx=0;
u8 b_idx=0;
u8 c_idx=0;
ITCM_CODE void LoopAY(void)
{
    static u16 delay=0;
    
    if (sgm_reg[0x07] == 0xFF) return;  // Nothing enabled - nobody using the AY chip.
    
    u16 envelope_period = (((sgm_reg[0x0C] << 8) | sgm_reg[0x0B])>>4) & 0xFFFF;
    if (envelope_period == 0) return;
    if (++delay > (envelope_period+1))
    {
        delay = 0;
        u8 shape=0;
        shape = sgm_reg[0x0D] & 0x0F;
        if ((sgm_reg[0x08] & 0x20) && (!(sgm_reg[0x07] & 0x01)))
        {
            u8 vol = Envelopes[shape][a_idx]; 
            vol = Volumes[vol];
            if (++a_idx > 31)
            {
                if ((shape & 0x09) == 0x08) a_idx = 0; else a_idx=31;
            }
            SN76496_w(&sncol, 0x90 | vol);
        }
        
        if ((sgm_reg[0x09] & 0x20) && (!(sgm_reg[0x07] & 0x02)))
        {
            u8 vol = Envelopes[shape][b_idx];
            vol = Volumes[vol];
            if (++b_idx > 31)
            {
                if ((shape & 0x09) == 0x08) b_idx = 0; else b_idx=31;
            }
            SN76496_w(&sncol, 0xB0 | vol);
        }

        if ((sgm_reg[0x0A] & 0x20) && (!(sgm_reg[0x07] & 0x04)))
        {
            u8 vol = Envelopes[shape][c_idx];
            vol = Volumes[vol];
            if (++c_idx > 31)
            {
                if ((shape & 0x09) == 0x08) c_idx = 0; else c_idx=31;
            }
            SN76496_w(&sncol, 0xD0 | vol);
        }
    }
}

/** OutZ80() *************************************************/
/** Z80 emulation calls this function to write a byte to a  **/
/** given I/O port.                                         **/
/*************************************************************/
ITCM_CODE void cpu_writeport16(register unsigned short Port,register unsigned char Value) 
{
  //JGD 18/04/2007 
  Port &= 0x00FF;

  // -----------------------------------------------------------------
  // Port 53 is used for the Super Game Module to enable SGM mode...
  // -----------------------------------------------------------------
  if ((Port == 0x53) && (Value & 0x01))
  {
      sgm_enable = true;
      return;
  }
  // -----------------------------------------------
  // Port 50 is the AY sound chip register set...
  // -----------------------------------------------
  if (Port == 0x50)
  {
      sgm_idx=Value&0x0F;
      return;
  }
  else if (Port == 0x51)
  {
      // ----------------------------------------------------------------------------------------
      // This is the AY sound chip support... we're cheating here and just mapping those sounds
      // onto the original Colecovision SN sound chip. Not perfect but good enough for now...
      // ----------------------------------------------------------------------------------------
      sgm_reg[sgm_idx]=Value;
      u16 freq=0;
      switch (sgm_idx)
      {
          // Channel A frequency (period) - low and high
          case 0x00:
          case 0x01:
              if (!(sgm_reg[0x07] & 0x01))
              {
                  freq = (sgm_reg[0x01] << 8) | sgm_reg[0x00];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  SN76496_w(&sncol, 0x80 | (freq & 0xF));
                  SN76496_w(&sncol, (freq >> 4) & 0x3F);
              }
              break;
              
             
          // Channel B frequency (period)
          case 0x02:
          case 0x03:
              if (!(sgm_reg[0x07] & 0x02))
              {
                  freq = (sgm_reg[0x03] << 8) | sgm_reg[0x02];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  SN76496_w(&sncol, 0xA0 | (freq & 0xF));
                  SN76496_w(&sncol, (freq >> 4) & 0x3F);
              }
              break;
          
           // Channel C frequency (period)
          case 0x04:
          case 0x05:
              if (!(sgm_reg[0x07] & 0x04))
              {
                  freq = (sgm_reg[0x05] << 8) | sgm_reg[0x04];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  SN76496_w(&sncol, 0xC0 | (freq & 0xF));
                  SN76496_w(&sncol, (freq >> 4) & 0x3F);
              }
              break;
              
              
          case 0x06:
               //noise_period= Value & 0x1F;
               if (noise_enable)
               {
                  SN76496_w(&sncol, 0xE2);
                  SN76496_w(&sncol, 0xF6);
               }
              break;
              
          case 0x07:
              // Channel A Enable/Disable
              if (!(sgm_reg[0x07] & 0x01))
              {
                  if (!channel_a_enable)
                  {
                      channel_a_enable=1;
                      a_idx=0;
                      freq = (sgm_reg[0x01] << 8) | sgm_reg[0x00];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      SN76496_w(&sncol, 0x80 | (freq & 0xF));
                      SN76496_w(&sncol, (freq >> 4) & 0x3F);
                      SN76496_w(&sncol, 0x90 | Volumes[((sgm_reg[0x08]>>1) & 0xF)]);
                  }
              }
              else
              {
                  if (channel_a_enable)
                  {
                      channel_a_enable = 0;
                      SN76496_w(&sncol, 0x90 | 0x0F);
                  }
              }
              
              // Channel B Enable/Disable
              if (!(sgm_reg[0x07] & 0x02))
              {
                  if (!channel_b_enable)
                  {
                      channel_b_enable=1;
                      b_idx=0;
                      freq = (sgm_reg[0x03] << 8) | sgm_reg[0x02];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      SN76496_w(&sncol, 0xA0 | (freq & 0xF));
                      SN76496_w(&sncol, (freq >> 4) & 0x3F);
                      SN76496_w(&sncol, 0xB0 | Volumes[((sgm_reg[0x09]>>1) & 0xF)]);
                  }
              }
              else
              {
                  if (channel_b_enable)
                  {
                      channel_b_enable = 0;
                      SN76496_w(&sncol, 0xB0 | 0x0F);
                  }
              }
              
              
              // Channel C Enable/Disable
              if (!(sgm_reg[0x07] & 0x04))
              {
                  if (!channel_c_enable)
                  {
                      channel_c_enable=1;
                      c_idx=0;
                      freq = (sgm_reg[0x05] << 8) | sgm_reg[0x04];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      SN76496_w(&sncol, 0xC0 | (freq & 0xF));
                      SN76496_w(&sncol, (freq >> 4) & 0x3F);
                      SN76496_w(&sncol, 0xD0 | Volumes[((sgm_reg[0x0A]>>1) & 0xF)]);
                  }
              }
              else
              {
                  if (channel_c_enable)
                  {
                      channel_c_enable = 0;
                      SN76496_w(&sncol, 0xD0 | 0x0F);
                  }
              }
              
              // Noise Channel
              if ((sgm_reg[0x07] & 0x38) != 0x38)
              {
                  if (!noise_enable)
                  {
                      noise_enable=1;
                      SN76496_w(&sncol, 0xE2);
                      SN76496_w(&sncol, 0xF6);
                  }
              }
              else
              {
                  SN76496_w(&sncol, 0xFF);
              }
              
              break;
              
              
          case 0x08:
              if (Value & 0x20) Value = 0x0;                    // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x01) Value = 0x0;            // If Channel A is disabled, volume OFF
              SN76496_w(&sncol, 0x90 | Volumes[((Value>>1) & 0xF)]);     // Write new Volume for Channel A
              a_idx=0;
              break;
          case 0x09:
              if (Value & 0x20) Value = 0x0;                    // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x02) Value = 0x0;            // If Channel B is disabled, volume OFF
              SN76496_w(&sncol, 0xB0 | Volumes[((Value>>1) & 0xF)]);     // Write new Volume for Channel B
              b_idx=0;
              break;
          case 0x0A:
              if (Value & 0x20) Value = 0x0;                    // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x04) Value = 0x0;            // If Channel C is disabled, volume OFF
              SN76496_w(&sncol, 0xD0 | Volumes[((Value>>1) & 0xF)]);     // Write new Volume for Channel C
              c_idx=0;
              break;

      }
     
      return;
  }
  else if (Port == 0x7F)
  {
      if (Value & 0x02)
      {
          extern u8 ColecoBios[];
          sgm_low_addr = 0x2000;
          memcpy(pColecoMem,ColecoBios,0x2000);
      }
      else 
      {
          sgm_low_addr = 0x0000; 
          memset(pColecoMem, 0x00, 0x2000);
      }
      return;   
  }
  switch(Port&0xE0) {
    case 0x80: JoyMode=0;return;
    case 0xC0: JoyMode=1;return;
    case 0xE0: SN76496_w(&sncol, Value);
      return;
    case 0xA0:
      if(!(Port&0x01)) WrData9918(Value);
      else if(WrCtrl9918(Value)) { cpuirequest=Z80_NMI_INT; }
      return;
    case 0x40:
    case 0x20:
    case 0x60:
      return;

  }
}


/** LoopZ80() ************************************************/
/** Z80 emulation calls this function periodically to check **/
/** if the system hardware requires any interrupts.         **/
/*************************************************************/
ITCM_CODE u32 LoopZ80() 
{
  cpuirequest=0;
  
  // Execute 1 scanline worth of CPU
  DrZ80_execute(TMS9918_LINE);

  // Just in case there is AY audio envelopes...
  if (sgm_enable) LoopAY();

  // Refresh VDP 
  if(Loop9918()) cpuirequest=Z80_NMI_INT;
    
  // Generate VDP interrupt
  if (cpuirequest==Z80_NMI_INT ) 
    Z80_Cause_Interrupt(Z80_NMI_INT);
  else
    Z80_Clear_Pending_Interrupts();
    
  // Drop out unless end of screen is reached 
  return (CurLine == TMS9918_END_LINE) ? 0:1;
}
