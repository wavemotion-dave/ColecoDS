/******************************************************************************
*  ColecoDS main file
*  Ver 1.0
*
*  Copyright (C) 2006 AlekMaul . All rights reserved.
*       http://www.portabledev.com
******************************************************************************/
#include <nds.h>
#include <nds/fifomessages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fat.h>

#include "colecoDS.h"
#include "highscore.h"

#include "colecogeneric.h"
#include "colecomngt.h"
#include "cpu/tms9928a/tms9928a.h"

#include "intro.h"
#include "wifimanage.h"

#include "ecranBas.h"
#include "ecranBasSel.h"
#include "ecranHaut.h"

#include "cpu/sn76496/sn76496_c.h"
extern s16 xfer_buf[4];
u32* aptr = (u32*)((u32)xfer_buf + 0xA000000);
extern sn76496 sncol;
u8 pColecoMem[0x10000] ALIGN(32) = {0};             // Coleco Memory

u8 ColecoBios[8192] = {0};

u16 emuFps=0;
u16 emuActFrames=0;
u16 timingFrames=0;


/*******************************************************************************/
volatile u16 vusCptVBL;                   // Video Management
extern u8 bFullSpeed;
typedef enum {
  EMUARM7_INIT_SND = 0x123C,
  EMUARM7_STOP_SND = 0x123D,
  EMUARM7_PLAY_SND = 0x123E,
} FifoMesType;

#define ds_GetTicks() (TIMER0_DATA)

unsigned int soundEmuPause=1;

int bg0, bg1, bg0b,bg1b;

char szKeyName[18][15] = {
  "INPUT_UP","INPUT_DOWN","INPUT_LEFT","INPUT_RIGHT","BUTTON 1","BUTTON 2", 
  "KEYPAD #1", "KEYPAD #2","KEYPAD #3", "KEYPAD #4","KEYPAD #5", "KEYPAD #6","KEYPAD #7", "KEYPAD #8","KEYPAD #9",
  "KEYPAD ##","KEYPAD #0", "KEYPAD #*"
};

u16 keyCoresp[18] = {
  0x0100 /*1<<8*/,0x0400 /*1<<10*/, 0x0800 /*1<<11*/, 0x0200 /*1<<9*/, 0x4000 /*1<<14*/, 0x0040 /*1<<6*/, 
  0x000E, 0x000D, 0x000C, 0x000B, 0x000A , 0x0009, 0x0008, 0x0007, 0x0006, 
  0x0004,0x000F,0x0005,
};

u16 keyboard_JoyNDS[12] = {
  // UP  DOWN  LEFT RIGHT  A  B  X  Y  R  L  START  SELECT
      0,    1,    2,    3, 4, 5, 8, 9, 10, 11,     6,      7
};

u32 lgeEmul;       // Langue emul : 0 = FR / 1 = UK

u16 *pVidFlipBuf= (u16*) (0x06000000);    // Video flipping buffer
u8 XBuf[256*256] ALIGN(32) = {0}; // Really it's only 256x192


//*****************************************************************************
// Boucle principale d'execution
//*****************************************************************************
void showMainMenu(void) 
{
  dmaCopy((void*) bgGetMapPtr(bg0b),(void*) bgGetMapPtr(bg1b),32*24*2);
}

ITCM_CODE void VsoundHandler(void)
{
    u16 samples[4];
    if (soundEmuPause) {*aptr=0; return;}
    sncol.mixlength=2;
    sncol.pcmptr=(u8*)samples;
    SN76496_mixer(&sncol);
    *aptr = *((u32*)samples);
}

//---------------------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void) 
{
       
  FifoMessage msg;
  msg.SoundPlay.data = &xfer_buf;
  msg.SoundPlay.freq = 48000;
  msg.SoundPlay.volume = 127;
  msg.SoundPlay.pan = 64;
  msg.SoundPlay.loop = 1;
  msg.SoundPlay.format = ((1)<<4) | SoundFormat_16Bit;
  msg.SoundPlay.loopPoint = 0;
  msg.SoundPlay.dataSize = 4 >> 2;
  msg.type = EMUARM7_PLAY_SND;
  fifoSendDatamsg(FIFO_USER_01, sizeof(msg), (u8*)&msg);
    
  if (isDSiMode())
  {
      aptr = (u32*)((u32)xfer_buf + 0xA000000);
  }
  else
  {
      aptr = (u32*)((u32)xfer_buf + 0x00400000);
  }
    
  // We convert 2 samples per VSoundHandler interrupt...
  TIMER2_DATA = TIMER_FREQ(24000);
  TIMER2_CR = TIMER_DIV_1 | TIMER_IRQ_REQ | TIMER_ENABLE;
  irqSet(IRQ_TIMER2, VsoundHandler);
  irqEnable(IRQ_TIMER2);
}

//*****************************************************************************
// Boucle principale d'execution
//*****************************************************************************
ITCM_CODE void colecoDS_main (void) {
  u32 keys_pressed;
  u16 iTx, iTy,iBcl;
  u32 ucUN, ucDEUX, ResetNow = 0, SaveNow = 0, LoadNow = 0;
  
  // Affiche le nouveau menu
  showMainMenu();

  colecoInit(gpFic[ucGameAct].szName);

  colecoSetPal();
  colecoRun();
    
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;
    
  while(1) 
  {
    // Take a tour of the Z80 counter and display the screen if necessary
    if (!LoopZ80()) 
    {        
        // -------------------------------------------------------------
        // Stuff to do once/second such as FPS display and Debug Data
        // -------------------------------------------------------------
        if (TIMER1_DATA >= 32728)   // 1000MS (1 sec)
        {
            char szChai[4];
            
            TIMER1_CR = 0;
            TIMER1_DATA = 0;
            TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;
            emuFps = emuActFrames;
            if (emuFps == 61) emuFps=60;
            
            if (emuFps/100) szChai[0] = '0' + emuFps/100;
            else szChai[0] = ' ';
            szChai[1] = '0' + (emuFps%100) / 10;
            szChai[2] = '0' + (emuFps%100) % 10;
            szChai[3] = 0;
            AffChaine(29,0,6,szChai);
           
            emuActFrames = 0;
        }
        emuActFrames++;

        if (++timingFrames == 60)
        {
            TIMER0_CR=0;
            TIMER0_DATA=0;
            TIMER0_CR=TIMER_ENABLE | TIMER_DIV_1024;
            timingFrames = 0;
        }

        
        // Time 1 frame... 546 ticks of Timer0
        while(TIMER0_DATA < (546*(timingFrames+1)))
        {
            if (bFullSpeed) break;
        }

      // gere touches
      ucUN = 0;
      if (keysCurrent() & KEY_TOUCH) {
        touchPosition touch;
        touchRead(&touch);
        iTx = touch.px;
        iTy = touch.py;
    
        // Test if "Reset Game" selected
        if ((iTx>=1*8) && (iTy>=6*8) && (iTx<=(1+14)*8) && (iTy<9*8) ) {
          if (!ResetNow) {
            ResetNow = 1;
            // Stop sound
            soundEmuPause=1;
            
            // Ask for verification
            if (showMessage(szLang[lgeEmul][37],szLang[lgeEmul][38]) == ID_SHM_YES) { 
              memcpy(VDP,VDPInit,sizeof(VDP));   // Initialize VDP registers
              VKey=1;                              // VDP address latch key
              VDPStatus=0x9F;                      // VDP status register
              FGColor=BGColor=0;                   // Fore/Background color
              ScrMode=0;                           // Current screenmode
              CurLine=0;                           // Current scanline
              ChrTab=ColTab=ChrGen=pVDPVidMem;     // VDP tables (screen)
              SprTab=SprGen=pVDPVidMem;            // VDP tables (sprites)
              JoyMode=0;                           // Joystick mode key
              JoyStat[0]=JoyStat[1]=0xFFFF;        // Joystick states
                
              sgm_reset();
              
              DrZ80_Reset();
              for (iBcl=0x06000;iBcl<0x07FFF;iBcl++)
                *(pColecoMem+iBcl) = 0xFF;
              for (iBcl=0;iBcl<0x04000;iBcl++)
                *(pVDPVidMem+iBcl) = 0xFF;
            }
            showMainMenu();
            soundEmuPause=0;
          }
        }
        else {
          ResetNow = 0;
        }
        
        // Test if "End Game" selected
        if ((iTx>=1*8) && (iTy>=9*8) && (iTx<=(1+14)*8) && (iTy<12*8) ) {
          // Stop sound
          soundEmuPause=1;
    
          // Ask for verification
          if (showMessage(szLang[lgeEmul][37],szLang[lgeEmul][39]) == ID_SHM_YES) 
          { 
              return;
          }
          showMainMenu();
          soundEmuPause=0;
        }

        // Test if "High Score" selected
        if ((iTx>=1*8) && (iTy>=12*8) && (iTx<=(1+14)*8) && (iTy<15*8) ) {
          // Stop sound
          soundEmuPause=1;
          highscore_display(crc32(0xFFFFFFFF, pColecoMem+0x8000, 0x8000));
          soundEmuPause=0;
        }
          
        // Test if "Save State" selected
        if ((iTx>=1*8) && (iTy>=16*8) && (iTx<=(1+14)*8) && (iTy<19*8) ) 
        {
          if (!SaveNow) 
          {
            // Stop sound
            soundEmuPause=1;
            SaveNow = 1;
            colecoSaveState();
            soundEmuPause=0;
          }
        }
        else
          SaveNow = 0;
          
        // Test if "Load State" selected
        if ((iTx>=1*8) && (iTy>=20*8) && (iTx<=(1+14)*8) && (iTy<23*8) ) 
        {
          if (!LoadNow) 
          {
            // Stop sound
            soundEmuPause=1;
            LoadNow = 1;
            colecoLoadState();
            soundEmuPause=0;
          }
        }
        else
          LoadNow = 0;
  
        // Test KEYPAD
        ucUN = ( ((iTx>=160) && (iTy>=80) && (iTx<=183) && (iTy<=100)) ? 0x0E: 0x00);
        ucUN = ( ((iTx>=183) && (iTy>=80) && (iTx<=210) && (iTy<=100)) ? 0x0D: ucUN);
        ucUN = ( ((iTx>=210) && (iTy>=80) && (iTx<=234) && (iTy<=100)) ? 0x0C: ucUN);
        
        ucUN = ( ((iTx>=160) && (iTy>=101) && (iTx<=183) && (iTy<=122)) ? 0x0B: ucUN);
        ucUN = ( ((iTx>=183) && (iTy>=101) && (iTx<=210) && (iTy<=122)) ? 0x0A: ucUN);
        ucUN = ( ((iTx>=210) && (iTy>=101) && (iTx<=234) && (iTy<=122)) ? 0x09: ucUN);
        
        ucUN = ( ((iTx>=160) && (iTy>=123) && (iTx<=183) && (iTy<=143)) ? 0x08: ucUN);
        ucUN = ( ((iTx>=183) && (iTy>=123) && (iTx<=210) && (iTy<=143)) ? 0x07: ucUN);
        ucUN = ( ((iTx>=210) && (iTy>=123) && (iTx<=234) && (iTy<=143)) ? 0x06: ucUN);
        
        ucUN = ( ((iTx>=160) && (iTy>=144) && (iTx<=183) && (iTy<=164)) ? 0x04: ucUN);
        ucUN = ( ((iTx>=183) && (iTy>=144) && (iTx<=210) && (iTy<=164)) ? 0x0F: ucUN);
        ucUN = ( ((iTx>=210) && (iTy>=144) && (iTx<=234) && (iTy<=164)) ? 0x05: ucUN);
      } // SCR_TOUCH
      else {
        ResetNow=SaveNow=LoadNow = 0;
      }
    
      // Test touches
      ucDEUX = 0;  
      keys_pressed = keysCurrent();
      if (keys_pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_R | KEY_L | KEY_X | KEY_Y)) {
        if (keys_pressed & KEY_UP) ucDEUX |=  keyCoresp[keyboard_JoyNDS[0]];
        if (keys_pressed & KEY_DOWN) ucDEUX |=  keyCoresp[keyboard_JoyNDS[1]];
        if (keys_pressed & KEY_LEFT) ucDEUX |=  keyCoresp[keyboard_JoyNDS[2]];
        if (keys_pressed & KEY_RIGHT) ucDEUX |=  keyCoresp[keyboard_JoyNDS[3]];
        if (keys_pressed & KEY_A) ucDEUX |=  keyCoresp[keyboard_JoyNDS[4]];
        if (keys_pressed & KEY_B) ucDEUX |=  keyCoresp[keyboard_JoyNDS[5]];
        if (keys_pressed & KEY_X) ucDEUX |=  keyCoresp[keyboard_JoyNDS[6]];
        if (keys_pressed & KEY_Y) ucDEUX |=  keyCoresp[keyboard_JoyNDS[7]];
        if (keys_pressed & KEY_R) ucDEUX |=  keyCoresp[keyboard_JoyNDS[8]];
        if (keys_pressed & KEY_L)  ucDEUX |=  keyCoresp[keyboard_JoyNDS[9]];
        if (keys_pressed & KEY_START) ucDEUX |=  keyCoresp[keyboard_JoyNDS[10]];
        if (keys_pressed & KEY_SELECT) ucDEUX |=  keyCoresp[keyboard_JoyNDS[11]];
      }

      JoyStat[0]= ucUN | ucDEUX;

      JoyStat[0]=~JoyStat[0];
      JoyStat[1]=JoyStat[0];
    }
  }
}

/*********************************************************************************
 * Init EMul
 ********************************************************************************/
void colecoDSInit(void) {

  // Get the personnals infos (language, name)
  lgeEmul = (PersonalData->language == 2 ? 0 : 1);

  // Init graphic mode (bitmap mode)
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_SPRITE);
  vramSetBankC(VRAM_C_SUB_BG);
  vramSetBankD(VRAM_D_SUB_SPRITE);
    
  vramSetBankE(VRAM_E_LCD );                // Not using this for video but 64K of faster RAM always useful!  Mapped at 0x06880000 - Unused at this time - 128K available
  vramSetBankF(VRAM_F_LCD );                // Not using this for video but 16K of faster RAM always useful!  Mapped at 0x06890000 -   ..
  vramSetBankG(VRAM_G_LCD );                // Not using this for video but 16K of faster RAM always useful!  Mapped at 0x06894000 -   ..
  vramSetBankH(VRAM_H_LCD );                // Not using this for video but 32K of faster RAM always useful!  Mapped at 0x06898000 -   ..
  vramSetBankI(VRAM_I_LCD );                // Not using this for video but 16K of faster RAM always useful!  Mapped at 0x068A0000 - Used for Custom Tile Map Buffer

  // Stop blending effect of intro
  REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;
  
  // Affiche l'ecran en haut
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(ecranHautTiles, bgGetGfxPtr(bg0), LZ77Vram);
  decompress(ecranHautMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) ecranHautPal,(void*) BG_PALETTE,256*2);
  //dmaCopy((void*) ecranHaut_tiles,bgGetGfxPtr(bg0),sizeof(ecranHaut_tiles));
  //dmaCopy((void*) ecranHaut_map,(void*) bgGetMapPtr(bg0),32*24*2);
  unsigned short dmaVal =*(bgGetMapPtr(bg0)+51*32);//  ecranHaut_map[51][0];            
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1),32*24*2);

  // Affiche le clavier en bas
  bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);
  decompress(ecranBasSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(ecranBasSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  //dmaCopy((void*) ecranBasSel_tiles,bgGetGfxPtr(bg0b),sizeof(ecranBasSel_tiles));
  //dmaCopy((void*) ecranBasSel_map,(void*) bgGetMapPtr(bg1b),32*24*2);
  dmaCopy((void*) ecranBasSelPal,(void*) BG_PALETTE_SUB,256*2);
  dmaVal = *(bgGetMapPtr(bg0b)+24*32);// ecranBasSel_map[24][0];
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

  // Init sprites

  AffChaine(2,6,0,szLang[lgeEmul][7]);
  AffChaine(2,7,0,szLang[lgeEmul][8]);

  // Find the files
  colecoDSFindFiles();
}

void InitBottomScreen(void)
{
  // Init bottom screen
  decompress(ecranBasTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(ecranBasMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
  dmaCopy((void*) ecranBasPal,(void*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);//ecranBas_map[24][0];
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
}

/*********************************************************************************
 * Init CPU for the current game
 ********************************************************************************/
u16 colecoDSInitCPU(void) {
#ifdef NOCASH
    nocashMessage("colecoDSInitCPU !\n");
#endif  
  u16 RetFct=0x0000;
  int iBcl;
  
  // Init Rom
  for (iBcl=0;iBcl<0x10000;iBcl++)
    *(pColecoMem+iBcl) = 0xFF;
  for (iBcl=0;iBcl<0x04000;iBcl++)
    *(pVDPVidMem+iBcl) = 0xFF;

  // Init bottom screen
  decompress(ecranBasTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(ecranBasMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
  dmaCopy((void*) ecranBasPal,(void*) BG_PALETTE_SUB,256*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);//ecranBas_map[24][0];
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

  // Load coleco Bios ROM
  memcpy(pColecoMem,ColecoBios,0x2000);
  
  // Return with result
  return (RetFct);
}

void irqVBlank(void) 
{ 
 // Manage time
  vusCptVBL++;
}

bool ColecoBIOSFound(void)
{
    FILE *fp;
    
    fp = fopen("coleco.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/coleco.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/coleco.rom", "rb");
    if (fp != NULL)
    {
        fread(ColecoBios, 8192, 1, fp);
        fclose(fp);
        return true;   
    }
    return false;
}

/*********************************************************************************
 * Program entry point
 ********************************************************************************/
char initial_file[256];
int main(int argc, char **argv) 
{
  // Init sound
  consoleDemoInit();
  soundEnable();

  if (!fatInitDefault()) {
	  iprintf("Unable to initialize libfat!\n");
	  return -1;
  }
    
  highscore_init();

  // Met les ecran comme il faut
  lcdMainOnTop();

  // Affichage de l'intro PortableDev
  intro_logo();
  
  // Init timer for frame management
  TIMER0_DATA=0;
  TIMER0_CR=TIMER_ENABLE|TIMER_DIV_1024; 
  dsInstallSoundEmuFIFO();
    
  SetYtrigger(190); //trigger 2 lines before vsync
  irqSet(IRQ_VBLANK, irqVBlank);
  irqEnable(IRQ_VBLANK);
    
  // Handle command line argument... mostly for TWL++
  if (argc > 1) 
  {
      // We want to start in the directory where the file is being launched...
      if (strchr(argv[1], '/') != NULL)
      {
          char path[128];
          strcpy(path, argv[1]);
          char *ptr = &path[strlen(path)-1];
          while (*ptr != '/') ptr--;
          ptr++; 
          strcpy(initial_file, ptr);
          *ptr=0;
          chdir(path);
      }
      else
      {
          strcpy(initial_file, argv[1]);
      }
  } else initial_file[0]=0; // No file passed on command line...
    
  // BOUCLE INFINIE !!!!
  while(1) {
    // init de l'emul et chargement des roms
    colecoDSInit();

    if (ColecoBIOSFound())
    {
        AffChaine(2,9,0,szLang[lgeEmul][13]);
        AffChaine(2,11,0,"coleco.rom BIOS FOUND");
        AffChaine(2,13,0,szLang[lgeEmul][14]);
        while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
        while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))==0);
        while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
    }
    else
    {
        AffChaine(2,10,0,"ERROR: coleco.rom NOT FOUND");
        AffChaine(2,12,0,"ERROR: CANT RUN WITHOUT BIOS");
        AffChaine(2,14,0,"Put coleco.rom in same dir");
        AffChaine(2,15,0,"as EMULATOR or /ROMS/BIOS");
        while(1) ;
    }
  
    while(1) 
    {
      // Choose option
      if (initial_file[0] != 0)
      {
          ucGameAct=0;
          strcpy(gpFic[ucGameAct].szName, initial_file);
          initial_file[0] = 0; // No more initial file...
      }
      else 
      {
          colecoDSChangeOptions();
      }

      // Run Machine
      colecoDSInitCPU();
      colecoDS_main();
    }
  }
  return(0);
}

/* END OF FILE */
