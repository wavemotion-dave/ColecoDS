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
#include <nds/fifomessages.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fat.h>
#include <maxmod9.h>

#include "colecoDS.h"
#include "highscore.h"
#include "colecogeneric.h"
#include "colecomngt.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/ay38910/AY38910.h"
#include "intro.h"
#include "ecranBas.h"
#include "ecranBasSel.h"
#include "ecranHaut.h"

#include "cpu/sn76496/SN76496.h"
#include "cpu/sn76496/Fake_AY.h"


u8 pColecoMem[0x10000] ALIGN(32) = {0};             // Coleco Memory... 64K addressable
u8 ColecoBios[8192] = {0};

// Various sound chips in the system
extern SN76496 sncol;
extern SN76496 aycol;
extern AY38910 ay_chip;

SN76496 snmute;

u16 emuFps=0;
u16 emuActFrames=0;
u16 timingFrames=0;


/*******************************************************************************/
volatile u16 vusCptVBL = 0;    // Video Management

typedef enum {
  EMUARM7_INIT_SND  = 0x123C,
  EMUARM7_STOP_SND  = 0x123D,
  EMUARM7_PLAY_SND  = 0x123E,
} FifoMesType;


u8 soundEmuPause __attribute__((section(".dtcm"))) = 1;

u8 bStartSoundEngine = false;

int bg0, bg1, bg0b,bg1b;

char szKeyName[18][15] = {
  "INPUT_UP",   "INPUT_DOWN",   "INPUT_LEFT",   "INPUT_RIGHT",
  "BUTTON 1",   "BUTTON 2",     "KEYPAD #1",    "KEYPAD #2",
  "KEYPAD #3",  "KEYPAD #4",    "KEYPAD #5",    "KEYPAD #6",
  "KEYPAD #7",  "KEYPAD #8",    "KEYPAD #9",    "KEYPAD ##",
  "KEYPAD #0",  "KEYPAD #*"
};

u16 keyCoresp[18] = {
  0x0100  ,0x0400, 0x0800, 0x0200, 0x4000, 0x0040, 0x000E, 0x000D, 0x000C, 
  0x000B, 0x000A  , 0x0009, 0x0008, 0x0007, 0x0006, 0x0004,0x000F, 0x0005
};

void showMainMenu(void) 
{
  dmaCopy((void*) bgGetMapPtr(bg0b),(void*) bgGetMapPtr(bg1b),32*24*2);
}

void SoundPause(void)
{
    soundEmuPause = 1;
}

void SoundUnPause(void)
{
    soundEmuPause = 0;
}


// ------------------------------------------
// MAXMOD streaming setup and handling...
// ------------------------------------------
#define sample_rate  55930
#define buffer_size  (512+12)

mm_ds_system sys;
mm_stream myStream;
s16 mixbuf1[2048];
s16 mixbuf2[2048];

mm_word OurSoundMixer(mm_word len, mm_addr dest, mm_stream_formats format)
{
    if (soundEmuPause)  // If paused, just "mix" in mute sound chip... all channels are OFF
    {
        sn76496Mixer(len*4, dest, &snmute);
    }
    else
    {
        if (AY_Enable)  // If AY is enabled we mix the normal SN chip with the AY chip sound
        {
          sn76496Mixer(len*4, mixbuf1, &aycol);
          sn76496Mixer(len*4, mixbuf2, &sncol);
          s16 *p = (s16*)dest;
          for (int i=0; i<len*2; i++)
          {
            *p++ = (s16) ((((s32)mixbuf1[i] + (s32)mixbuf2[i])) / 2); // In theory we should divide this by 2 so we don't overflow... but that halves the sound in many cases due to one channel being off
          }
        }
        else  // This is the 'normal' case of just Colecovision SN sound chip output
        {
            sn76496Mixer(len*4, dest, &sncol);
        }
    }
    return  len;
}


void setupStream(void) 
{
  //----------------------------------------------------------------
  //  initialize maxmod without any soundbank (unusual setup)
  //----------------------------------------------------------------
  sys.mod_count       = 0;
  sys.samp_count      = 0;
  sys.mem_bank        = 0;
  sys.fifo_channel    = FIFO_MAXMOD;
  mmInit( &sys );

  //----------------------------------------------------------------
  //  open stream
  //----------------------------------------------------------------
  myStream.sampling_rate  = sample_rate;        // sampling rate =
  myStream.buffer_length  = buffer_size;        // buffer length =
  myStream.callback   = OurSoundMixer;          // set callback function
  myStream.format     = MM_STREAM_16BIT_STEREO; // format = stereo  16-bit
  myStream.timer      = MM_TIMER0;              // use hardware timer 0
  myStream.manual     = false;                  // use automatic filling
  mmStreamOpen( &myStream );

  //----------------------------------------------------------------
  //  when using 'automatic' filling, your callback will be triggered
  //  every time half of the wave buffer is processed.
  //
  //  so: 
  //  25000 (rate)
  //  ----- = ~21 Hz for a full pass, and ~42hz for half pass
  //  1200  (length)
  //----------------------------------------------------------------
  //  with 'manual' filling, you must call mmStreamUpdate
  //  periodically (and often enough to avoid buffer underruns)
  //----------------------------------------------------------------
}


//---------------------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void) 
{
  SoundPause();

  
  // ---------------------------------------------------------------------
  // We setup a mute channel to cut sound for pause
  // ---------------------------------------------------------------------
  sn76496Reset(1, &snmute);         // Reset the SN sound chip
    
  sn76496W(0x80 | 0x00,&snmute);    // Write new Frequency for Channel A
  sn76496W(0x00 | 0x00,&snmute);    // Write new Frequency for Channel A
  sn76496W(0x90 | 0x0F,&snmute);    // Write new Volume for Channel A
    
  sn76496W(0xA0 | 0x00,&snmute);    // Write new Frequency for Channel B
  sn76496W(0x00 | 0x00,&snmute);    // Write new Frequency for Channel B
  sn76496W(0xB0 | 0x0F,&snmute);    // Write new Volume for Channel B
    
  sn76496W(0xC0 | 0x00,&snmute);    // Write new Frequency for Channel C
  sn76496W(0x00 | 0x00,&snmute);    // Write new Frequency for Channel C
  sn76496W(0xD0 | 0x0F,&snmute);    // Write new Volume for Channel C

  sn76496W(0xFF,  &snmute);         // Disable Noise Channel
    
  sn76496Mixer(8, mixbuf1, &snmute);     // Do  an initial mix conversion to clear the output
      
  //  ------------------------------------------------------------------
  //  The SN sound chip is for normal colecovision sound handling
  //  ------------------------------------------------------------------
  sn76496Reset(1, &sncol);         // Reset the SN sound chip
    
  sn76496W(0x80 | 0x00,&sncol);    // Write new Frequency for Channel A
  sn76496W(0x00 | 0x00,&sncol);    // Write new Frequency for Channel A
  sn76496W(0x90 | 0x0F,&sncol);    // Write new Volume for Channel A
    
  sn76496W(0xA0 | 0x00,&sncol);    // Write new Frequency for Channel B
  sn76496W(0x00 | 0x00,&sncol);    // Write new Frequency for Channel B
  sn76496W(0xB0 | 0x0F,&sncol);    // Write new Volume for Channel B
    
  sn76496W(0xC0 | 0x00,&sncol);    // Write new Frequency for Channel C
  sn76496W(0x00 | 0x00,&sncol);    // Write new Frequency for Channel C
  sn76496W(0xD0 | 0x0F,&sncol);    // Write new Volume for Channel C

  sn76496W(0xFF,  &sncol);         // Disable Noise Channel
    
  sn76496Mixer(8, mixbuf1, &sncol);     // Do  an initial mix conversion to clear the output

    
  //  ------------------------------------------------------------------
  //  The "fake AY" sound chip is for Super Game Module sound handling
  //  ------------------------------------------------------------------
  sn76496Reset(1, &aycol);         // Reset the "AY" sound chip
    
  sn76496W(0x80 | 0x00,&aycol);    // Write new Frequency for Channel A
  sn76496W(0x00 | 0x00,&aycol);    // Write new Frequency for Channel A
  sn76496W(0x90 | 0x0F,&aycol);    // Write new Volume for Channel A
    
  sn76496W(0xA0 | 0x00,&aycol);    // Write new Frequency for Channel B
  sn76496W(0x00 | 0x00,&aycol);    // Write new Frequency for Channel B
  sn76496W(0xB0 | 0x0F,&aycol);    // Write new Volume for Channel B
    
  sn76496W(0xC0 | 0x00,&aycol);    // Write new Frequency for Channel C
  sn76496W(0x00 | 0x00,&aycol);    // Write new Frequency for Channel C
  sn76496W(0xD0 | 0x0F,&aycol);    // Write new Volume for Channel C

  sn76496W(0xFF,  &aycol);         // Disable Noise Channel
    
  sn76496Mixer(8, mixbuf2, &aycol);     // Do  an initial mix conversion to clear the output
  
  setupStream();    // Setup maxmod stream...

  bStartSoundEngine = true;
}

//*****************************************************************************
// Reset the Colecovision - mostly CPU, Super Game Module and memory...
//*****************************************************************************
static u8 last_sgm_mode = false;
static u8 last_ay_mode = false;
static u8 last_mc_mode = 0;

void ResetColecovision(void)
{
  JoyMode=JOYMODE_JOYSTICK;           // Joystick mode key
  JoyStat[0]=JoyStat[1]=0xCFFF;       // Joystick states
    
  Reset9918();                          // Reset video chip

  sgm_reset();                          // Reset Super Game Module
    
  sn76496Reset(1, &sncol);              // Reset the SN sound chip
  sn76496W(0x90 | 0x0F  ,&sncol);       //  Write new Volume for Channel A  
  sn76496W(0xB0 | 0x0F  ,&sncol);       //  Write new Volume for Channel B
  sn76496W(0xD0 | 0x0F  ,&sncol);       //  Write new Volume for Channel C      

  sn76496Reset(1, &aycol);              // Reset the SN sound chip
  sn76496W(0x90 | 0x0F  ,&aycol);       //  Write new Volume for Channel A  
  sn76496W(0xB0 | 0x0F  ,&aycol);       //  Write new Volume for Channel B
  sn76496W(0xD0 | 0x0F  ,&aycol);       //  Write new Volume for Channel C      
    
  DrZ80_Reset();                        //  Reset the Z80 CPU Core

  //  Clear Main RAM memory...
  memset(pColecoMem+0x2000, 0x00, 0x6000);
    
  //  Restore Coleco BIOS
  memcpy(pColecoMem,ColecoBios,0x2000);
    
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;
    
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;
  last_sgm_mode = false;
  last_ay_mode  = false;
  last_mc_mode  = 0;
}


void DisplayStatusLine(bool bForce)
{
    if ((last_sgm_mode != sgm_enable) || bForce)
    {
        last_sgm_mode = sgm_enable;
        AffChaine(25,0,6, (sgm_enable ? "SGM":"   "));
    }

    if ((last_ay_mode != AY_Enable) || bForce)
    {
        last_ay_mode = AY_Enable;
        AffChaine(22,0,6, (AY_Enable ? "AY":"  "));
    }

    if ((last_mc_mode != romBankMask) || bForce)
    {
        last_mc_mode = romBankMask;
        AffChaine(19,0,6, (romBankMask ? "MC":"  "));
    }
}

//*****************************************************************************
// The main emulation loop is here... call into the Z80, VDP and PSG 
//*****************************************************************************
ITCM_CODE void colecoDS_main (void) 
{
  u32 keys_pressed;
  u16 iTx,  iTy;
  u16 ucUN, ucDEUX, ResetNow  = 0, SaveNow = 0, LoadNow = 0;
  
  showMainMenu();      // Returns when  user has asked for a game to run...

  colecoInit(gpFic[ucGameAct].szName);

  colecoSetPal();
  colecoRun();
  
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;
    
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;
  last_sgm_mode = false;
  last_ay_mode  = false;
  last_mc_mode  = 0;
  
  bStartSoundEngine = true;
    
  while(1)  
  {
    // Take a tour of the Z80 counter and display the screen if necessary
    if (!LoopZ80()) 
    {   
        if (bStartSoundEngine)
        {
              bStartSoundEngine = false;
              SoundUnPause();
        }
        
        // -------------------------------------------------------------
        // Stuff to do once/second such as FPS display and Debug Data
        // -------------------------------------------------------------
        if (TIMER1_DATA >= 32728)   //  1000MS (1 sec)
        {
            char szChai[4];
            
            TIMER1_CR = 0;
            TIMER1_DATA = 0;
            TIMER1_CR=TIMER_ENABLE | TIMER_DIV_1024;
            emuFps = emuActFrames;
            if (emuFps == 61) emuFps=60;
            
            if (myConfig.showFPS)
            {
                if (emuFps/100) szChai[0] = '0' + emuFps/100;
                else szChai[0] = ' ';
                szChai[1] = '0' + (emuFps%100) / 10;
                szChai[2] = '0' + (emuFps%100) % 10;
                szChai[3] = 0;
                AffChaine(29,0,6,szChai);
            }
            DisplayStatusLine(false);
            emuActFrames = 0;
        }
        emuActFrames++;

        if (++timingFrames == 60)
        {
            TIMER2_CR=0;
            TIMER2_DATA=0;
            TIMER2_CR=TIMER_ENABLE | TIMER_DIV_1024;
            timingFrames = 0;
        }

        // --------------------------------------------
        // Time 1 frame... 546 ticks of Timer2
        // This is how we time frame-to frame
        // to keep the game running at 60FPS
        // --------------------------------------------
        while(TIMER2_DATA < (546*(timingFrames+1)))
        {
            if (myConfig.fullSpeed) break;
        }

      //  gere touches
      ucUN  = 0;
      if  (keysCurrent() & KEY_TOUCH) {
        touchPosition touch;
        touchRead(&touch);
        iTx = touch.px;
        iTy = touch.py;
    
        // Test if "Reset Game" selected
        if ((iTx>=6) && (iTy>=40) && (iTx<=130) && (iTy<67)) 
        {
          if  (!ResetNow) {
            ResetNow = 1;
            // Stop sound
            SoundPause();
            
            // Ask for verification
            if (showMessage("DO YOU REALLY WANT TO", "RESET THE CURRENT GAME ?") == ID_SHM_YES) 
            { 
                ResetColecovision();
            }
              
            showMainMenu();
            SoundUnPause();
          }
        }
        else {
          ResetNow  = 0;
        }
        
        // Test if "End Game" selected
        if ((iTx>=6) && (iTy>=67) && (iTx<=130) && (iTy<95)) 
        {
          //  Stop sound
          SoundPause();
    
          //  Ask for verification
          if  (showMessage("DO YOU REALLY WANT TO","QUIT THE CURRENT GAME ?") == ID_SHM_YES) 
          { 
              return;
          }
          showMainMenu();
          DisplayStatusLine(true);            
          SoundUnPause();
        }

        // Test if "High Score" selected
        if ((iTx>=6) && (iTy>=95) && (iTx<=130) && (iTy<125)) 
        {
          //  Stop sound
          SoundPause();
          highscore_display(file_crc);
          DisplayStatusLine(true);
          SoundUnPause();
        }
          
        // Test if "Save State" selected
        if ((iTx>=6) && (iTy>=125) && (iTx<=130) && (iTy<155) ) 
        {
          if  (!SaveNow) 
          {
            // Stop sound
            SoundPause();
            SaveNow = 1;
            colecoSaveState();
            SoundUnPause();
          }
        }
        else
          SaveNow = 0;
          
        // Test if "Load State" selected
        if ((iTx>=6) && (iTy>=155) && (iTx<=130) && (iTy<184) ) 
        {
          if  (!LoadNow) 
          {
            // Stop sound
            SoundPause();
            LoadNow = 1;
            colecoLoadState();
            SoundUnPause();
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
      } //  SCR_TOUCH
      else  {
        ResetNow=SaveNow=LoadNow = 0;
      }
    
      //  Test DS keypresses and map to corresponding Coleco keys
      ucDEUX  = 0;  
      keys_pressed  = keysCurrent();
      if  (keys_pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_R | KEY_L | KEY_X | KEY_Y)) 
      {
        if (keys_pressed & KEY_UP)      ucDEUX  |= keyCoresp[myConfig.keymap[0]];
        if (keys_pressed & KEY_DOWN)    ucDEUX  |= keyCoresp[myConfig.keymap[1]];
        if (keys_pressed & KEY_LEFT)    ucDEUX  |= keyCoresp[myConfig.keymap[2]];
        if (keys_pressed & KEY_RIGHT)   ucDEUX  |= keyCoresp[myConfig.keymap[3]];
        if (keys_pressed & KEY_A)       ucDEUX  |= keyCoresp[myConfig.keymap[4]];
        if (keys_pressed & KEY_B)       ucDEUX  |= keyCoresp[myConfig.keymap[5]];
        if (keys_pressed & KEY_X)       ucDEUX  |= keyCoresp[myConfig.keymap[6]];
        if (keys_pressed & KEY_Y)       ucDEUX  |= keyCoresp[myConfig.keymap[7]];
        if (keys_pressed & KEY_R)       ucDEUX  |= keyCoresp[myConfig.keymap[8]];
        if (keys_pressed & KEY_L)       ucDEUX  |= keyCoresp[myConfig.keymap[9]];
        if (keys_pressed & KEY_START)   ucDEUX  |= keyCoresp[myConfig.keymap[10]];
        if (keys_pressed & KEY_SELECT)  ucDEUX  |= keyCoresp[myConfig.keymap[11]];
      }

      JoyStat[0]= ucUN  | ucDEUX;

      // --------------------------------------------------
      // Handle Auto-Fire if enabled in configuration...
      // --------------------------------------------------
      static u8 autoFireTimer[2]={0,0};
      if (myConfig.autoFire1 && (JoyStat[0] & 0x0040))  // Fire Button 1
      {
         if ((++autoFireTimer[0] & 7) > 4)  JoyStat[0] &= ~0x0040;
      }
      if (myConfig.autoFire2 && (JoyStat[0] & 0x4000))  // Fire Button 2
      {
          if ((++autoFireTimer[1] & 7) > 4)  JoyStat[0] &= ~0x4000;
      }

      JoyStat[0]=~JoyStat[0];        // Logic if flipped
      JoyStat[0]  &= ~0x3000;        // Reset spinner bits
      JoyStat[1]  &= ~0x3000;        // Reset spinner bits
    }
  }
}

/*********************************************************************************
 * Init EMul
 ********************************************************************************/
void colecoDSInit(void) 
{
  //  Init graphic mode (bitmap mode)
  videoSetMode(MODE_0_2D  | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE  | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_SPRITE);
  vramSetBankC(VRAM_C_SUB_BG);
  vramSetBankD(VRAM_D_SUB_SPRITE);
    
  vramSetBankE(VRAM_E_LCD );                 // Not using this  for video but 64K of faster RAM always useful!  Mapped  at 0x06880000 - Unused at this time - 128K available
  vramSetBankF(VRAM_F_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x06890000 -   ..
  vramSetBankG(VRAM_G_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x06894000 -   ..
  vramSetBankH(VRAM_H_LCD );                 // Not using this  for video but 32K of faster RAM always useful!  Mapped  at 0x06898000 -   ..
  vramSetBankI(VRAM_I_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x068A0000 - Used for Custom Tile Map Buffer

  //  Stop blending effect of intro
  REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;
  
  //  Render the top screen
  bg0 = bgInit(0, BgType_Text8bpp,  BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp,  BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(ecranHautTiles,  bgGetGfxPtr(bg0), LZ77Vram);
  decompress(ecranHautMap,  (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) ecranHautPal,(void*)  BG_PALETTE,256*2);
  unsigned  short dmaVal =*(bgGetMapPtr(bg0)+51*32);//  ecranHaut_map[51][0];           
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1),32*24*2);

  //  Render the bottom screen
  bg0b  = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1b  = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);
  decompress(ecranBasSelTiles,  bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(ecranBasSelMap,  (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void*) ecranBasSelPal,(void*)  BG_PALETTE_SUB,256*2);
  dmaVal  = *(bgGetMapPtr(bg0b)+24*32);// ecranBasSel_map[24][0];
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1b),32*24*2);

  //  Put out the initial messages looking for a file system
  AffChaine(2,6,0,"SEARCH FAT SYSTEM   ...   ");
  AffChaine(2,7,0,"FAT  SYSTEM FOUND   !");

  //  Find the files
  colecoDSFindFiles();
}

void InitBottomScreen(void)
{
  //  Init bottom screen
  decompress(ecranBasTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
  decompress(ecranBasMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
  dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
  dmaCopy((void*) ecranBasPal,(void*) BG_PALETTE_SUB,256*2);
  unsigned  short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1b),32*24*2);
}

/*********************************************************************************
 * Init CPU for the current game
 ********************************************************************************/
u16 colecoDSInitCPU(void) 
{ 
  u16 RetFct=0x0000;
  int iBcl;
  
  //  -----------------------------------------
  //  Init Main Memory and VDP Video Memory
  //  -----------------------------------------
  for (iBcl=0;iBcl<0x10000;iBcl++)
    *(pColecoMem+iBcl) = 0xFF;
  for (iBcl=0;iBcl<0x04000;iBcl++)
    *(pVDPVidMem+iBcl) = 0xFF;

  //  Init bottom screen
  decompress(ecranBasTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
  decompress(ecranBasMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
  dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
  dmaCopy((void*) ecranBasPal,(void*) BG_PALETTE_SUB,256*2);
  unsigned  short dmaVal = *(bgGetMapPtr(bg1b)+24*32);//ecranBas_map[24][0];
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1b),32*24*2);

  //  Load coleco Bios ROM
  memcpy(pColecoMem,ColecoBios,0x2000);
  
  //  Return with result
  return  (RetFct);
}

void irqVBlank(void) 
{ 
 // Manage time
  vusCptVBL++;
}

// ----------------------------------------------------------------
// Look for the coleco.rom bios in several possible locations...
// ----------------------------------------------------------------
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
  //  Init sound
  consoleDemoInit();
  soundEnable();
    
  if  (!fatInitDefault()) {
     iprintf("Unable to initialize libfat!\n");
     return -1;
  }
    
  highscore_init();

  lcdMainOnTop();

  //  Show the fade-away intro logo...
  intro_logo();
  
  //  Init timer for frame management
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE|TIMER_DIV_1024;  
  dsInstallSoundEmuFIFO();
    
  SetYtrigger(190); //trigger 2 lines before  vsync
  irqSet(IRQ_VBLANK,  irqVBlank);
  irqEnable(IRQ_VBLANK);
    
  //  Handle command line argument... mostly for TWL++
  if  (argc > 1) 
  {
      //  We want to start in the directory where the file is being launched...
      if  (strchr(argv[1], '/') != NULL)
      {
          char  path[128];
          strcpy(path,  argv[1]);
          char  *ptr = &path[strlen(path)-1];
          while (*ptr !=  '/') ptr--;
          ptr++;  
          strcpy(initial_file,  ptr);
          *ptr=0;
          chdir(path);
      }
      else
      {
          strcpy(initial_file,  argv[1]);
      }
  } else  initial_file[0]=0; // No file passed on command line...
    
  SoundPause();
  
  //  ------------------------------------------------------------
  //  We run this loop forever until game exit is selected...
  //  ------------------------------------------------------------
  while(1)  
  {
    colecoDSInit();

    if (ColecoBIOSFound())
    {
        AffChaine(2,9,0,"ALL IS OK ...");
        AffChaine(2,11,0,"coleco.rom BIOS FOUND");
        AffChaine(2,13,0,"TOUCH SCREEN / KEY TO BEGIN");
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
      SoundPause();
      //  Choose option
      if  (initial_file[0] != 0)
      {
          ucGameAct=0;
          strcpy(gpFic[ucGameAct].szName, initial_file);
          initial_file[0] = 0;  // No more initial file...
      }
      else  
      {
          colecoDSChangeOptions();
      }

      //  Run Machine
      colecoDSInitCPU();
      colecoDS_main();
    }
  }
  return(0);
}

/* END OF FILE */
