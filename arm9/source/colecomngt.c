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
extern u8 lastBank;

#include "cpu/tms9918a/tms9918a.h"

#include "cpu/sn76496/SN76496.h"
#include "cpu/ay38910/AY38910.h"
#include "cpu/sn76496/Fake_AY.h"
#define NORAM 0xFF

#define COLECODS_SAVE_VER 0x0007

extern const unsigned short sprPause_Palette[16];
extern const unsigned char sprPause_Bitmap[2560];
extern u32* lutTablehh;

u8 romBuffer[512 * 1024] ALIGN(32);   // We support MegaCarts up to 512KB
u8 romBankMask __attribute__((section(".dtcm"))) = 0x00;
u8 bBlendMode __attribute__((section(".dtcm"))) = false;

u8 sgm_enable __attribute__((section(".dtcm"))) = false;
u8 sgm_idx __attribute__((section(".dtcm"))) = 0;
u8 sgm_reg[256] __attribute__((section(".dtcm"))) = {0};
u16 sgm_low_addr __attribute__((section(".dtcm"))) = 0x2000;

static u8 Port53 __attribute__((section(".dtcm"))) = 0x00;
static u8 Port60 __attribute__((section(".dtcm"))) = 0x0F;

u8 bFirstTimeAY __attribute__((section(".dtcm"))) = true;
u8 AY_Enable __attribute__((section(".dtcm")))    = false;

u16 JoyMode=0;                   // Joystick / Paddle management
u16 JoyStat[2];                  // Joystick / Paddle management

SN76496 sncol   __attribute__((section(".dtcm")));
SN76496 aycol   __attribute__((section(".dtcm")));
AY38910 ay_chip __attribute__((section(".dtcm")));

// Reset the Super Game Module vars...
void sgm_reset(void)
{
    //make sure Super Game Module registers for AY chip are clear...
    memset(sgm_reg, 0x00, 256);
    sgm_reg[0x07] = 0xFF; // Everything turned off to start...
    sgm_reg[0x0E] = 0xFF;
    sgm_reg[0x0F] = 0xFF;
    channel_a_enable = 0;
    channel_b_enable = 0;
    channel_c_enable = 0;
    noise_enable = 0;      
    
#ifdef REAL_AY
    bFirstTimeAY = true;
    memset(&ay_chip, 0x00, sizeof(ay_chip));
    ay38910Reset(&ay_chip);
    for (u8 i=0; i<16; i++)
    {
        ay38910IndexW(i, &ay_chip);
        ay38910DataW(0x00, &ay_chip);        
    }
#else
    bFirstTimeAY = false;        // We are using FAKE AY for now...
#endif    
    sgm_enable = false;
    sgm_low_addr = 0x2000;
    
    AY_Enable = false;
    
    Port53 = 0x00;
    Port60 = 0x0F;    
}



/*********************************************************************************
 * Init coleco Engine for that game
 ********************************************************************************/
u8 colecoInit(char *szGame) {
  u8 RetFct,uBcl;
  u16 uVide;
  
  // Wipe area between BIOS and RAM
  memset(pColecoMem+0x2000, 0xFF, 0x4000);
    
  // Wipe RAM
  memset(pColecoMem+0x6000, 0x00, 0x2000);
  
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
      uVide=(uBcl/12);
      dmaFillWords(uVide | (uVide<<16),pVidFlipBuf+uBcl*128,256);
    }
    
    // Make sure the super game module is disabled to start
    sgm_reset();

    JoyMode=JOYMODE_JOYSTICK;            // Joystick mode key
    JoyStat[0]=JoyStat[1]=0xCFFF;        // Joystick states

    sn76496Reset(1, &sncol);             // Reset the SN sound chip
    sn76496W(0x90 | 0x0F ,&sncol);     // Write new Volume for Channel A  
    sn76496W(0xB0 | 0x0F ,&sncol);     // Write new Volume for Channel B
    sn76496W(0xD0 | 0x0F ,&sncol);     // Write new Volume for Channel C  
    u16 tmp_samples[32];
    sn76496Mixer(32, tmp_samples, &sncol);

    sn76496Reset(1, &aycol);             // Reset the SN sound chip
    sn76496W(0x90 | 0x0F ,&aycol);     // Write new Volume for Channel A  
    sn76496W(0xB0 | 0x0F ,&aycol);     // Write new Volume for Channel B
    sn76496W(0xD0 | 0x0F ,&aycol);     // Write new Volume for Channel C  
    sn76496Mixer(32, tmp_samples, &aycol);
      
    DrZ80_Reset();
    Reset9918();
      
    XBuf = XBuf_A;
  }
  
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
    r = (u8) ((float) TMS9918A_palette[uBcl*3+0]*0.121568f);
    g = (u8) ((float) TMS9918A_palette[uBcl*3+1]*0.121568f);
    b = (u8) ((float) TMS9918A_palette[uBcl*3+2]*0.121568f);

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
  char szFile[128];
  char szCh1[32],szCh2[32];

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
    if (uNbO) uNbO = fwrite(&ay_chip, sizeof(ay_chip), 1, handle);       
    
      
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
    if (uNbO) uNbO = fwrite(&aycol, sizeof(aycol),1, handle);       
      
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
  char szFile[128];
  char szCh1[32],szCh2[32];

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
            if (uNbO) uNbO = fread(&ay_chip, sizeof(ay_chip), 1, handle);       
            
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
            if (uNbO) uNbO = fread(&aycol, sizeof(aycol),1, handle); 
            
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
 * Check if the cart is valid...
 ********************************************************************************/
u8 colecoCartVerify(const u8 *cartData) 
{
  //Who are we to argue? Some SGM roms shift this up to bank 0 and it's not worth the hassle. The game either runs or it won't.
  return IMAGE_VERIFY_PASS;
}

// ------------------------------------------------------------
// Some global vars to track what kind of cart/rom we have...
// ------------------------------------------------------------
u8 bMagicMegaCart = 0;
u8 bActivisionPCB = 0;
u8 sRamAtE000_OK = 0;

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
        if (iSSize <= (24*1024)) sRamAtE000_OK = 1; else sRamAtE000_OK = 0;
        if (iSSize <= (32*1024))
        {
            memcpy(ptr, romBuffer, nmemb);
            romBankMask = 0x00;
        }
        else    // Bankswitched Cart!!
        {
            // Copy 128K worth up to the VRAM for faster bank switching on the first 8 banks
            u32 copySize = ((iSSize <= 128*1024) ? iSSize : (128*1024));
            u32 *dest = (u32*)0x06880000;
            u32 *src  = (u32*)romBuffer;
            for (u32 i=0; i<copySize/4; i++)
            {
                *dest++ = *src++;
            }
                
            bMagicMegaCart = ((romBuffer[0xC000] == 0x55 && romBuffer[0xC001] == 0xAA) ? 1:0);
            if ((iSSize == (64 * 1024)) && !bMagicMegaCart)
            {
                bActivisionPCB = 1;
                memcpy(ptr, romBuffer, 0x4000);                     // bank 0
                memcpy(ptr+0x4000, romBuffer+0x4000, 0x4000);       // bank 1
                romBankMask = 0x03;
                // TODO: Eventually handle EEPROM for these PCBs...
            }
            else
            {
                bMagicMegaCart = 1;
                memcpy(ptr, romBuffer+(iSSize-0x4000), 0x4000); // For MegaCart, we map highest bank into fixed ROM
                lastBank = 199;                                 // Force load of the first bank...
                BankSwitch(0);                                  // The initial 16K "switchable" bank is bank 0 (based on a post from Nanochess in AA forums)
                
                if (iSSize == (64  * 1024)) romBankMask = 0x03;
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
    sgm_enable = (Port53 & 0x01) ? true:false;
    
    if (Port60 & 0x02)
    {
      extern u8 ColecoBios[];
      sgm_low_addr = 0x2000;
      memcpy(pColecoMem,ColecoBios,0x2000);
    }
    else 
    {
      sgm_enable = true; /// Force this if someone disabled the BIOS....
      sgm_low_addr = 0x0000; 
      memset(pColecoMem, 0x00, 0x2000);
    }
}

/** InZ80() **************************************************/
/** Z80 emulation calls this function to read a byte from   **/
/** a given I/O port.                                       **/
/*************************************************************/
ITCM_CODE unsigned char cpu_readport16(register unsigned short Port) {
  static byte KeyCodes[16] = { 0x0A,0x0D,0x07,0x0C,0x02,0x03,0x0E,0x05, 0x01,0x0B,0x06,0x09,0x08,0x04,0x0F,0x0F, };

  // Colecovision ports are 8-bit
  Port &= 0x00FF; 
  
  // Port 52 is used for the AY sound chip for the Super Game Module
  if (Port == 0x52)
  {
#ifdef REAL_AY
      ay38910DataR(&ay_chip);
      return ay_chip.ayRegs[sgm_idx&0x0F];
#else
      return FakeAY_ReadData();
#endif
  }

  switch(Port&0xE0) {
    case 0x40: // Printer Status - not used
      break;

    case 0xE0: // Joysticks Data
      Port=(Port>>1)&0x01;
      Port=JoyMode ? (JoyStat[Port]>>8): (JoyStat[Port]&0xF0)|KeyCodes[JoyStat[Port]&0x0F];
      return(Port&0x7F);

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
      if ((Value & 0x0F) == 0x07) AY_Enable = true;
#ifdef REAL_AY      
      ay38910IndexW(Value, &ay_chip); 
#else
      FakeAY_WriteIndex(Value & 0x0F);
#endif      
      return;
  }
  // -----------------------------------------------
  // Port 51 is the AY Sound chip register write...
  // -----------------------------------------------
  else if (Port == 0x51) 
  {
#ifdef REAL_AY      
      ay38910DataW(Value, &ay_chip); 
#else
    FakeAY_WriteData(Value);
#endif      
    return;
  }
    
  switch(Port&0xE0) 
  {
    case 0x80: 
      JoyMode=JOYMODE_JOYSTICK;
      return;
    case 0xC0: 
      JoyMode=JOYMODE_KEYPAD;
      return;
    case 0xE0: 
      sn76496W(Value, &sncol);
      return;
    case 0xA0:
      if(!(Port&0x01)) WrData9918(Value);
      else if(WrCtrl9918(Value)) { cpuirequest=Z80_NMI_INT; }
      return;
    case 0x40:
    case 0x20:
      return;
    case 0x60:
      Port60 = Value;
      SetupSGM();
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
    
#if 0
  char str[33];
  sprintf(str, "PC:   %08X ", drz80.Z80PC);
  AffChaine(1,6,2,str);
  sprintf(str, "SP:   %08X ", drz80.Z80SP);
  AffChaine(1,7,2,str);
  sprintf(str, "PCB:  %08X ", drz80.Z80PC_BASE);
  AffChaine(1,8,2,str);
  sprintf(str, "SPB:  %08X ", drz80.Z80SP_BASE);
  AffChaine(1,9,2,str);
  sprintf(str, "Bank: %-7d ", lastBank);
  AffChaine(1,10,2,str);
  sprintf(str, "LowM: %04X  ", sgm_low_addr);
  AffChaine(1,11,2,str);
#endif    
  // Just in case there is AY audio envelopes...
#ifndef REAL_AY    
  if (AY_Enable) FakeAY_Loop();
#endif    
    
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
