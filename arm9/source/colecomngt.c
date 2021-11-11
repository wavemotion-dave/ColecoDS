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

//#include "fatlib/gba_nds_fat.h"
#include <unistd.h>
#include <fat.h>

#include "colecoDS.h"

#include "cpu/z80/drz80/Z80_interface.h"

#include "colecomngt.h"
#include "colecogeneric.h"

extern byte Loop9918(void);
extern void DrZ80_InitFonct(void);

//#include "cpu/sn76489/sn76489.h"
#include "cpu/tms9928a/tms9928a.h"

#include "cpu/sn76496/sn76496_c.h"

#define NORAM 0xFF

extern const unsigned short sprPause_Palette[16];
extern const unsigned char sprPause_Bitmap[2560];

u8 sgm_enable = false;

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
  
    DrZ80_Reset();
    Reset9918();

    JoyMode=0;                           // Joystick mode key
    JoyStat[0]=JoyStat[1]=0xFFFF;        // Joystick states

    SN76496_set_mixrate(&sncol,1);
    SN76496_set_frequency(&sncol,7159090/2);
    SN76496_init(&sncol,(u16 *) &freqtablcol);
    SN76496_reset(&sncol,1);

    sgm_enable = false;
      
    UCount=0;
    ExitNow = 0;
    
    DrZ80_Reset();
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
#ifdef NOCASH
  nocashMessage("colecoRun");
#endif
  DrZ80_Reset();

  // Try to load save state file
  colecoLoadState();
  showMainMenu();
}

/*********************************************************************************
 * Set coleco Palette
 ********************************************************************************/
void colecoSetPal(void) {
#ifdef NOCASH
  nocashMessage("colecoSetPal");
#endif

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
 * Load the current state
 ********************************************************************************/
void colecoLoadState() {
#ifdef NOCASH
  nocashMessage("colecoLoadState");
#endif
  u32 uNbO;
  long pSvg;
  char szFile[256];
  char szCh1[128],szCh2[128];

  if (isFATSystem) {
    // Init filename = romname and STA in place of ROM
    strcpy(szFile,gpFic[ucGameAct].szName);
    szFile[strlen(szFile)-3] = 'S';
    szFile[strlen(szFile)-2] = 'T';
    szFile[strlen(szFile)-1] = 'A';
    FILE* handle = fopen(szFile, "r"); 
    if (handle != NULL) {    
      if (showMessage("Do you want to load ", "the save state file ?") == ID_SHM_YES) { 
        sprintf(szCh1,"Loading %s ...",szFile);
        AffChaine(16-strlen(szCh1)/2,11,6,szCh1);
        // Load Z80 CPU
        uNbO = fread(&drz80, sizeof(struct DrZ80), 1, handle);
        DrZ80_InitFonct(); //DRZ80 saves a lot of binary code dependent stuff, regenerate it

        if (uNbO) uNbO = fread(pColecoMem+0x06000, 0x2000,1, handle); 
        //if (uNbO) uNbO = FAT_fread(pColecoMem, 0x10000,1, handle); 
        // Load VDP
        if (uNbO) uNbO = fread(VDP, sizeof(VDP),1, handle); 
        if (uNbO) uNbO = fread(&VKey, sizeof(VKey),1, handle); 
        if (uNbO) uNbO = fread(&VDPStatus, sizeof(VDPStatus),1, handle); 
        if (uNbO) uNbO = fread(&FGColor, sizeof(FGColor),1, handle); 
        if (uNbO) uNbO = fread(&BGColor, sizeof(BGColor),1, handle); 
        if (uNbO) uNbO = fread(&ScrMode, sizeof(ScrMode),1, handle); 
        if (uNbO) uNbO = fread(&VDPClatch, sizeof(VDPClatch),1, handle); 
        if (uNbO) uNbO = fread(&VAddr, sizeof(VAddr),1, handle); 
        if (uNbO) uNbO = fread(&WKey, sizeof(WKey),1, handle); 
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
//        if (uNbO) uNbO = fread(&PSG, sizeof(SN76489),1, handle); 
//        if (uNbO) uNbO = fread(CH, sizeof(CH),1, handle); 
        if (uNbO) 
          strcpy(szCh2,"Load OK !");
        else
          strcpy(szCh2,"Error loading data ...");
      }
      else {
        strcpy(szCh2,"Error opening STA file ...");
      }
      AffChaine(16-strlen(szCh2)/2,12,6,szCh2);
      strcpy(szCh1,(lgeEmul == 0 ? "TOUCHER L'ECRAN": "TOUCH SCREEN"));
      AffChaine(16-strlen(szCh2)/2,12,6,szCh2);
       AffChaine(16-strlen("                        ")/2,14,6,"                        ");
      AffChaine(16-strlen(szCh1)/2,14,6,szCh1);
      while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
      while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))==0);
      while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
    } // if (showMessage("Do you want to load ", 
    fclose(handle);
  } // if (isFATSystem)
}

/*********************************************************************************
 * Save the current state
 ********************************************************************************/
void colecoSaveState() {
  u32 uNbO;
  long pSvg;
  char szFile[256];
  char szCh1[128],szCh2[128];

#ifdef DEBUG
    iprintf("colecoSaveState\n");
#endif
  // Init filename = romname and STA in place of ROM
  strcpy(szFile,gpFic[ucGameAct].szName);
  szFile[strlen(szFile)-3] = 'S';
  szFile[strlen(szFile)-2] = 'T';
  szFile[strlen(szFile)-1] = 'A';
  sprintf(szCh1,"Saving %s ...",szFile);
#ifdef DEBUG
  iprintf("%s\n",szCh1);
#endif
  AffChaine(16-strlen(szCh1)/2,11,6,szCh1);
  
  
  chdir(szFATDir);
  FILE *handle = fopen(szFile, "w+");  
  if (handle != NULL) {
    // Write Z80 CPU
    uNbO = fwrite(&drz80, sizeof(struct DrZ80), 1, handle);

    //if (uNbO) uNbO = FAT_fwrite(pColecoMem, 0x10000,1, handle); 
    if (uNbO) uNbO = fwrite(pColecoMem+0x06000, 0x2000,1, handle); 
    // Write VDP
    if (uNbO) uNbO = fwrite(VDP, sizeof(VDP),1, handle); 
    if (uNbO) uNbO = fwrite(&VKey, sizeof(VKey),1, handle); 
    if (uNbO) uNbO = fwrite(&VDPStatus, sizeof(VDPStatus),1, handle); 
    if (uNbO) uNbO = fwrite(&FGColor, sizeof(FGColor),1, handle); 
    if (uNbO) uNbO = fwrite(&BGColor, sizeof(BGColor),1, handle); 
    if (uNbO) uNbO = fwrite(&ScrMode, sizeof(ScrMode),1, handle); 
    if (uNbO) uNbO = fwrite(&VDPClatch, sizeof(VDPClatch),1, handle); 
    if (uNbO) uNbO = fwrite(&VAddr, sizeof(VAddr),1, handle); 
    if (uNbO) uNbO = fwrite(&WKey, sizeof(WKey),1, handle); 
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
//    if (uNbO) uNbO = fwrite(&PSG, sizeof(SN76489),1, handle); 
//    if (uNbO) uNbO = fwrite(CH, sizeof(CH),1, handle); 
    if (uNbO) 
      strcpy(szCh2,"Save OK !");
    else
      strcpy(szCh2,"Error saving data ...");
  }
  else {
    strcpy(szCh2,"Error opening STA file ...");
  }
  fclose(handle);

  AffChaine(16-strlen(szCh2)/2,12,6,szCh2);
  strcpy(szCh1,(lgeEmul == 0 ? "TOUCHER L'ECRAN": "TOUCH SCREEN"));
  AffChaine(16-strlen(szCh2)/2,12,6,szCh2);
  AffChaine(16-strlen(szCh1)/2,14,6,szCh1);
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))==0);
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
}

/*********************************************************************************
 * Update the screen for the current cycle
 ********************************************************************************/
ITCM_CODE void colecoUpdateScreen(void) 
{
  // Change background
  dmaCopyWordsAsynch(2, (u32*)XBuf, (u32*)pVidFlipBuf, 256*192);
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
 * Manage key / Paddle
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

  // Quit with verification cheched
  return RetFct;
}

/** loadrom() ******************************************************************/
/* Open a rom file from file system                                            */
/*******************************************************************************/
u8 romBuffer[512 * 1024];   // We support MegaCarts up to 512KB
u8 romBankMask = 0x00;
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
            memcpy(ptr, romBuffer+(iSSize-0x4000), 0x4000);
            memcpy(ptr+0x4000, romBuffer, 0x4000);
            if (iSSize == (64 * 1024)) romBankMask = 0x03;
            if (iSSize == (128 * 1024)) romBankMask = 0x07;
            if (iSSize == (256 * 1024)) romBankMask = 0x0F;
            if (iSSize == (512 * 1024)) romBankMask = 0x1F;
        }
        bOK = 1;
    }
    fclose(handle);
  }
  return bOK;
}

u8 sgm_idx=0;
u8 sgm_reg[256] = {0};
u16 sgm_low_addr = 0x2000;

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

/** OutZ80() *************************************************/
/** Z80 emulation calls this function to write a byte to a  **/
/** given I/O port.                                         **/
/*************************************************************/
ITCM_CODE void cpu_writeport16(register unsigned short Port,register unsigned char Value) {
  //JGD 18/04/2007 
  Port &= 0x00FF;

  if ((Port == 0x53) && (Value & 0x01))
  {
      sgm_enable = true;
      return;
  }
  if (Port == 0x50)
  {
      sgm_idx=Value;
      return;
  }
  else if (Port == 0x51)
  {
      sgm_reg[sgm_idx]=Value;
#if 0
//+--+--+--+--+--+--+--+--+
//|1 |R2|R1|R0|D3|D2|D1|D0|
//+--+--+--+--+--+--+--+--+
//
//1: This denotes that this is a control word
//R2-R0 the register number:
//000 Tone 1 Frequency
//001 Tone 1 Volume
//010 Tone 2 Frequency
//011 Tone 2 Volume
//100 Tone 3 Frequency
//101 Tone 3 Volume
//110 Noise Control
//111 Noise Volume      
      switch (sgm_idx)
      {
          case 0x00:
              SN76496_w(&sncol, 0x80 | ((sgm_reg[0x01]>>2)&0xF));
              SN76496_w(&sncol, sgm_reg[0x00]);
              break;
          case 0x01:
              SN76496_w(&sncol, 0x80 | ((sgm_reg[0x01]>>2)&0xF));
              SN76496_w(&sncol, sgm_reg[0x00]);
              break;
          case 0x02:
              SN76496_w(&sncol, 0xA0 | ((sgm_reg[0x03]>>2)&0xF));
              SN76496_w(&sncol, sgm_reg[0x02]);
              break;
          case 0x03:
              SN76496_w(&sncol, 0xA0 | ((sgm_reg[0x03]>>2)&0xF));
              SN76496_w(&sncol, sgm_reg[0x02]);
              break;
          case 0x04:
              SN76496_w(&sncol, 0xC0 | ((sgm_reg[0x05]>>2)&0xF));
              SN76496_w(&sncol, sgm_reg[0x04]);
              break;
          case 0x05:
              SN76496_w(&sncol, 0xC0 | ((sgm_reg[0x05]>>2)&0xF));
              SN76496_w(&sncol, sgm_reg[0x04]);
              break;
          case 0x06:
              break;
          case 0x08:
              SN76496_w(&sncol, 0x90 | ((Value>>1) & 0xF));
              break;
          case 0x09:
              SN76496_w(&sncol, 0xB0 | ((Value>>1) & 0xF));
              break;
          case 0x0A:
              SN76496_w(&sncol, 0xD0 | ((Value>>1) & 0xF));
              break;
      }
#endif      
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
