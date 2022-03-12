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
#include "AdamNet.h"
#include "FDIDisk.h"
#include "highscore.h"
#include "colecogeneric.h"
#include "colecomngt.h"
#include "cpu/tms9918a/tms9918a.h"
#include "intro.h"
#include "ecranBas.h"
#include "adam.h"
#include "msx.h"
#include "msx_full.h"
#include "adam_full.h"
#include "ecranDebug.h"
#include "ecranBasSel.h"
#include "ecranHaut.h"
#include "wargames.h"
#include "mousetrap.h"
#include "gateway.h"
#include "spyhunter.h"
#include "fixupmixup.h"
#include "boulder.h"
#include "quest.h"
#include "hal2010.h"

#include "soundbank.h"
#include "soundbank_bin.h"
#include "MSX_CBIOS.h"

#include "cpu/sn76496/SN76496.h"
#include "cpu/sn76496/Fake_AY.h"
#include "cpu/z80/Z80_interface.h"

extern Z80 CPU;
extern u8 Slot0BIOS[];
u32 debug1=0;
u32 debug2=0;
u32 debug3=0;
u32 debug4=0;
u32 debug5=0;
u32 debug6=0;
extern u8 adam_ram_lo, adam_ram_hi;
extern u8 io_show_status;
u8 adam_CapsLock = 0;

// --------------------------------------------------------------------------
// This is the full 64K coleco memory map.
// The memory is generally used as follows:
//    0x0000-0x1FFF  coleco.rom BIOS - Super Game Module can map RAM here
//    0x2000-0x5FFF  Usually unused - but Super Game Module maps RAM here
//    0x6000-0x7FFF  RAM - there is only 1K repeated with 8 mirrors
//    0x8000-0xFFFF  32K Cartridge Space
// --------------------------------------------------------------------------
u8 pColecoMem[0x10000] ALIGN(32) = {0};             
u8 ColecoBios[0x2000] = {0};  // We keep the Coleco  8K BIOS around to swap in/out
u8 SordM5Bios[0x2000] = {0};  // We keep the Sord M5 8K BIOS around to swap in/out
u8 MSXBios[0x8000]    = {0};  // We keep the MSX 32K BIOS around to swap in/out
u8 AdamEOS[0x2000]    = {0};  // We keep the ADAM EOS.ROM bios around to swap in/out
u8 AdamWRITER[0x8000] = {0};  // We keep the ADAM WRITER.ROM bios around to swap in/out

// Various sound chips in the system
extern SN76496 sncol;       // The SN sound chip is the main Colecovision sound
extern SN76496 aycol;       // The AY sound chip is for the Super Game Moudle

SN76496 snmute;             // We keep this handy as a simple way to mute the sound

// Some timing and frame rate comutations
u16 emuFps=0;
u16 emuActFrames=0;
u16 timingFrames=0;

u8 last_adam_key = -1;

// For the various BIOS files ... only the coleco.rom is required
u8 bColecoBiosFound = false;
u8 bSordBiosFound = false;
u8 bMSXBiosFound = false;
u8 bAdamBiosFound = false;

volatile u16 vusCptVBL = 0;    // We use this as a basic timer for the Mario sprite... could be removed if another timer can be utilized

u8 soundEmuPause __attribute__((section(".dtcm"))) = 1;     // Set to 1 to pause (mute) sound, 0 is sound unmuted (sound channels active)

u8 sg1000_mode __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .sg game is loaded for Sega SG-1000 support 
u8 sordm5_mode __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .m5 game is loaded for Sord M5 support 
u8 msx_mode    __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .msx game is loaded for basic MSX support 
u8 adam_mode   __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .ddp game is loaded for ADAM game support
u8 msx_key     __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the ASCII key (e.g. 'A', 'B', '3', etc)

u8 bStartSoundEngine = false;   // Set to true to unmute sound after 1 frame of rendering...

int bg0, bg1, bg0b, bg1b;      // Some vars for NDS background screen handling

u32 NDS_keyMap[12] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_X, KEY_Y, KEY_R, KEY_L, KEY_START, KEY_SELECT};

// The key map for the Colecovision... mapped into the NDS controller
u32 keyCoresp[MAX_KEY_OPTIONS] = {
    JST_UP, 
    JST_DOWN, 
    JST_LEFT, 
    JST_RIGHT, 
    JST_FIREL, 
    JST_FIRER,  
    JST_PURPLE, 
    JST_BLUE,
    JST_1, 
    JST_2, 
    JST_3, 
    JST_4, 
    JST_5, 
    JST_6, 
    JST_7, 
    JST_8, 
    JST_9, 
    JST_POUND, 
    JST_0, 
    JST_STAR, 
    
    JST_UP      << 16,      // P2 versions of the above...
    JST_DOWN    << 16, 
    JST_LEFT    << 16, 
    JST_RIGHT   << 16, 
    JST_FIREL   << 16, 
    JST_FIRER   << 16,  
    JST_PURPLE  << 16, 
    JST_BLUE    << 16,
    JST_1       << 16, 
    JST_2       << 16, 
    JST_3       << 16, 
    JST_4       << 16, 
    JST_5       << 16, 
    JST_6       << 16, 
    JST_7       << 16, 
    JST_8       << 16, 
    JST_9       << 16, 
    JST_POUND   << 16, 
    JST_0       << 16, 
    JST_STAR    << 16, 
    
    META_SPINX_LEFT, META_SPINX_RIGHT, META_SPINY_LEFT, META_SPINY_RIGHT
};

extern u8 msx_scc_enable;


// --------------------------------------------------------------------------------
// Spinners! X and Y taken together will actually replicate the roller controller.
// --------------------------------------------------------------------------------
u8 spinX_left   __attribute__((section(".dtcm"))) = 0;
u8 spinX_right  __attribute__((section(".dtcm"))) = 0;
u8 spinY_left   __attribute__((section(".dtcm"))) = 0;
u8 spinY_right  __attribute__((section(".dtcm"))) = 0;

// ------------------------------------------------------------
// Utility function to show the background for the main menu
// ------------------------------------------------------------
void showMainMenu(void) 
{
  dmaCopy((void*) bgGetMapPtr(bg0b),(void*) bgGetMapPtr(bg1b),32*24*2);
}

// ------------------------------------------------------------
// Utility function to pause the sound... 
// ------------------------------------------------------------
void SoundPause(void)
{
    soundEmuPause = 1;
}

// ------------------------------------------------------------
// Utility function to un pause the sound... 
// ------------------------------------------------------------
void SoundUnPause(void)
{
    soundEmuPause = 0;
}

// --------------------------------------------
// MAXMOD streaming setup and handling...
// We were using the normal ARM7 sound core but
// it sounded "scratchy" and so with the help
// of FluBBa, we've swiched over to the maxmod
// sound core which seems to perform better.
// --------------------------------------------
#define sample_rate  27965      // To match the driver in sn76496 - this is good enough quality for the DS
#define buffer_size  (512+12)   // Enough buffer that we don't have to fill it too often

mm_ds_system sys  __attribute__((section(".dtcm")));
mm_stream myStream __attribute__((section(".dtcm")));
s16 mixbuf1[2048];      // When we have SN and AY sound we have to mix 3+3 channels
s16 mixbuf2[2048];      // into a single output so we render to mix buffers first.

// -------------------------------------------------------------------------------------------
// maxmod will call this routine when the buffer is half-empty and requests that
// we fill the sound buffer with more samples. They will request 'len' samples and
// we will fill exactly that many. If the sound is paused, we fill with 'mute' samples.
// -------------------------------------------------------------------------------------------
mm_word OurSoundMixer(mm_word len, mm_addr dest, mm_stream_formats format)
{
    if (soundEmuPause)  // If paused, just "mix" in mute sound chip... all channels are OFF
    {
        sn76496Mixer(len*4, dest, &snmute);
    }
    else
    {
        if (msx_mode)   // If we are an MSX, we can just use the one AY sound core
        {
            ay76496Mixer(len*4, dest, &aycol);
        }
        else if (AY_Enable)  // If AY is enabled we mix the normal SN chip with the AY chip sound
        {
          ay76496Mixer(len*4, mixbuf1, &aycol);
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


// -------------------------------------------------------------------------------------------
// Setup the maxmod audio stream - this will be a 16-bit Stereo PCM output at 55KHz which
// sounds about right for the Colecovision.
// -------------------------------------------------------------------------------------------
void setupStream(void) 
{
  //----------------------------------------------------------------
  //  initialize maxmod with our small 2-effect soundbank
  //----------------------------------------------------------------
  mmInitDefaultMem((mm_addr)soundbank_bin);

  mmLoadEffect(SFX_CLICKNOQUIT);
  mmLoadEffect(SFX_KEYCLICK);

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


// -----------------------------------------------------------------------
// We setup the sound chips - disabling all volumes to start.
// -----------------------------------------------------------------------
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
    
  sn76496Mixer(8, mixbuf1, &snmute);  // Do  an initial mix conversion to clear the output
      
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
    
  sn76496Mixer(8, mixbuf1, &sncol);  // Do an initial mix conversion to clear the output

    
  //  ------------------------------------------------------------------
  //  The "fake AY" sound chip is for Super Game Module sound handling
  //  ------------------------------------------------------------------
  ay76496Reset(2, &aycol);         // Reset the "AY" sound chip
    
  ay76496W(0x80 | 0x00,&aycol);    // Write new Frequency for Channel A
  ay76496W(0x00 | 0x00,&aycol);    // Write new Frequency for Channel A
  ay76496W(0x90 | 0x0F,&aycol);    // Write new Volume for Channel A
    
  ay76496W(0xA0 | 0x00,&aycol);    // Write new Frequency for Channel B
  ay76496W(0x00 | 0x00,&aycol);    // Write new Frequency for Channel B
  ay76496W(0xB0 | 0x0F,&aycol);    // Write new Volume for Channel B
    
  ay76496W(0xC0 | 0x00,&aycol);    // Write new Frequency for Channel C
  ay76496W(0x00 | 0x00,&aycol);    // Write new Frequency for Channel C
  ay76496W(0xD0 | 0x0F,&aycol);    // Write new Volume for Channel C

  ay76496W(0xFF,  &aycol);         // Disable Noise Channel
    
  sn76496Mixer(8, mixbuf2, &aycol);  // Do an initial mix conversion to clear the output
  
  setupStream();    // Setup maxmod stream...

  bStartSoundEngine = true; // Volume will 'unpause' after 1 frame in the main loop.
}

//*****************************************************************************
// Reset the Colecovision - mostly CPU, Super Game Module and memory...
//*****************************************************************************
static u8 last_sgm_mode = false;
static u8 last_ay_mode = false;
static u8 last_mc_mode = 0;
static u8 last_sg1000_mode = 0;
static u8 last_sordm5_mode = 0;
static u8 last_msx_mode = 0;
static u8 last_adam_mode = 0;
static u8 last_scc_mode = 0;


void ResetStatusFlags(void)
{
  // Some utility flags for various expansion peripherals
  last_sgm_mode = false;
  last_ay_mode  = false;
  last_mc_mode  = 0;
  last_sg1000_mode = 0;
  last_sordm5_mode = 0;
  last_msx_mode = 0;
  last_scc_mode = 0;
  last_adam_mode = 0;
}

void ResetColecovision(void)
{
  JoyMode=JOYMODE_JOYSTICK;             // Joystick mode key
  JoyState = 0x00000000;                // Nothing pressed to start
    
  Reset9918();                          // Reset video chip

  sgm_reset();                          // Reset Super Game Module
    
  sn76496Reset(1, &sncol);              // Reset the SN sound chip
  sn76496W(0x90 | 0x0F  ,&sncol);       //  Write new Volume for Channel A (off) 
  sn76496W(0xB0 | 0x0F  ,&sncol);       //  Write new Volume for Channel B (off)
  sn76496W(0xD0 | 0x0F  ,&sncol);       //  Write new Volume for Channel C (off)

  ay76496Reset(2, &aycol);              // Reset the SN sound chip
  ay76496W(0x90 | 0x0F  ,&aycol);       //  Write new Volume for Channel A (off)
  ay76496W(0xB0 | 0x0F  ,&aycol);       //  Write new Volume for Channel B (off)
  ay76496W(0xD0 | 0x0F  ,&aycol);       //  Write new Volume for Channel C (off)
    
  DrZ80_Reset();                        // Reset the Z80 CPU Core
  ResetZ80(&CPU);                       // Reset the CZ80 core CPU
    
  sordm5_reset();                       // Reset the Sord M5 specific vars
  msx_reset();                          // Reset the MSX specific vars

  if (sg1000_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
  }
  else if (sordm5_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      memcpy(pColecoMem,SordM5Bios,0x2000);     // Restore Sord M5 BIOS
  }
  else if (msx_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      memcpy(pColecoMem,Slot0BIOS,0x8000);      // Restore MSX BIOS
  }
  else if (adam_mode)
  {
      colecoWipeRAM();
      SetupAdam(false);
  }
  else
  {
      memset(pColecoMem+0x2000, 0xFF, 0x6000);  // Reset non-mapped area between BIOS and RAM - SGM RAM might map here
      colecoWipeRAM();                          // Wipe main RAM area
      memcpy(pColecoMem,ColecoBios,0x2000);     // Restore Coleco BIOS
  }
  
  // -----------------------------------------------------------
  // Timer 1 is used to time frame-to-frame of actual emulation
  // -----------------------------------------------------------
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;
    
  // -----------------------------------------------------------
  // Timer 2 is used to time once per second events
  // -----------------------------------------------------------
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;

  ResetStatusFlags();   // Some static status flags for the UI mostly
    
  debug1 = 0;  debug2 = 0; debug3 = 0;  debug4 = 0;
}

//*********************************************************************************
// A mini Z80 debugger of sorts. Put out some Z80, VDP and SGM/Bank info on
// screen every frame to help us debug some of the problem games. This is enabled
// via a compile switch in colecoDS.h - uncomment the define for DEBUG_Z80 line.
//*********************************************************************************
const char *VModeNames[] =
{
    "VDP 0  ",  
    "VDP 3  ",  
    "VDP 2  ",  
    "VDP 2+3",  
    "VDP 1  ",  
    "VDP ?5 ",  
    "VDP 1+2",  
    "VDP ?7 ",  
};

void ShowDebugZ80(void)
{
    extern u8 a_idx, b_idx, c_idx;
    extern u8 lastBank;
    extern u8 romBankMask;
    extern u8 Port20, Port53, Port60;
    extern u16 PCBAddr;
    char tmp[33];
    u8 idx=1;
    
    siprintf(tmp, "VDP[] %02X %02X %02X %02X", VDP[0],VDP[1],VDP[2],VDP[3]);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "VDP[] %02X %02X %02X %02X", VDP[4],VDP[5],VDP[6],VDP[7]);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "VStat %02X Data=%02X", VDPStatus, VDPDlatch);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "VAddr %04X", VAddr);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "VLatc %02X  %c %c", VDPCtrlLatch, VDP[1]&TMS9918_REG1_IRQ ? 'E':'D', VDPStatus&TMS9918_STAT_VBLANK ? 'V':'-');
    AffChaine(0,idx++,7, tmp);
    idx++;
    if (myConfig.cpuCore == 0)
    {
        siprintf(tmp, "Z80PC %08X", drz80.Z80PC-drz80.Z80PC_BASE);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "Z80SP %08X", drz80.Z80SP - drz80.Z80SP_BASE);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "Z80A  %08X", drz80.Z80A);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "IRQ   %02X", drz80.Z80_IRQ);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "IF/IM %02X/%02X", drz80.Z80IF, drz80.Z80IM);
        AffChaine(0,idx++,7, tmp);
    }
    else
    {
        siprintf(tmp, "Z80PC %04X", CPU.PC.W);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "Z80SP %04X", CPU.SP.W);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "Z80A  %04X", CPU.AF.W);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "IRQ   %04X", CPU.IRequest);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "IREQ  %d", CPU.User);
        AffChaine(0,idx++,7, tmp);
    }
    idx++;
    siprintf(tmp, "AY[]  %02X %02X %02X %02X", ay_reg[0], ay_reg[1], ay_reg[2], ay_reg[3]);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "AY[]  %02X %02X %02X %02X", ay_reg[4], ay_reg[5], ay_reg[6], ay_reg[7]);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "AY[]  %02X %02X %02X %02X", ay_reg[8], ay_reg[9], ay_reg[10], ay_reg[11]);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "AY[]  %02X %02X %02X %02X", ay_reg[12], ay_reg[13], ay_reg[14], ay_reg[15]);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "ENVL  %d", AY_EnvelopeOn);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "ABC   %-2d %-2d %-2d", a_idx, b_idx, c_idx);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "Bank  %02X [%02X]", (lastBank != 199 ? lastBank:0), romBankMask);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "PORTS %02X %02X %02X", Port20, Port53, Port60);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "VMode %02X %s", TMS9918_Mode, VModeNames[TMS9918_Mode]);
    AffChaine(0,idx++,7, tmp);  
    siprintf(tmp, "DEBG  %lu %lu %04X", debug1, debug2, PCBAddr);
    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "DEBG  %lu %lu", debug3, debug4);
    AffChaine(0,idx++,7, tmp);
}
    
    

// ------------------------------------------------------------
// The status line shows the status of the Super Game Moudle,
// AY sound chip support and MegaCart support.  Game players
// probably don't care, but it's really helpful for devs.
// ------------------------------------------------------------
void DisplayStatusLine(bool bForce)
{
    if (sg1000_mode)
    {
        if ((last_sg1000_mode != sg1000_mode) || bForce)
        {
            last_sg1000_mode = sg1000_mode;
            AffChaine(23,0,6, "SG-1000");
        }
    }
    else if (sordm5_mode)
    {
        if ((last_sordm5_mode != sordm5_mode) || bForce)
        {
            last_sordm5_mode = sordm5_mode;
            AffChaine(23,0,6, "SORD M5");
        }
    }
    else if (msx_mode)
    {
        if ((last_msx_mode != msx_mode) || bForce)
        {
            extern u32 LastROMSize;
            char tmp[12];
            last_msx_mode = msx_mode;
            siprintf(tmp, "MSX %dK ", (int)(LastROMSize/1024));
            AffChaine(23,0,6, tmp);
        }
    }
    else if (adam_mode)
    {
        if ((last_adam_mode != adam_mode) || bForce)
        {
            last_adam_mode = adam_mode;
            AffChaine(23,0,6, "ADAM");
        }
        if (io_show_status) 
        {
            AffChaine(30,0,6, (io_show_status == 2 ? "WR":"RD"));
            io_show_status = 0;
        }
        else
            AffChaine(30,0,6, "  ");
    }
    else    // Various Colecovision Possibilities 
    {    
        if ((last_sgm_mode != sgm_enable) || bForce)
        {
            last_sgm_mode = sgm_enable;
            AffChaine(28,0,6, (sgm_enable ? "SGM":"   "));
        }

        if ((last_ay_mode != AY_Enable) || bForce)
        {
            last_ay_mode = AY_Enable;
            AffChaine(25,0,6, (AY_Enable ? "AY":"  "));
        }

        if ((last_mc_mode != romBankMask) || bForce)
        {
            last_mc_mode = romBankMask;
            AffChaine(22,0,6, (romBankMask ? "MC":"  "));
        }
    }
}

void SaveAdamTapeOrDisk(void)
{
    extern char lastPath[];
    
    if (io_show_status) return; // Don't save while io status
    
    AffChaine(12,0,6, "SAVING");
    if (strstr(lastPath, ".ddp") != 0)
        SaveFDI(&Tapes[0], lastPath, FMT_DDP);
    else
        SaveFDI(&Disks[0], lastPath, FMT_ADMDSK);
    AffChaine(12,0,6, "      ");
}

u8 IsFullKeyboard(void)
{
    if ((myConfig.overlay == 9) || (myConfig.overlay == 10)) return 1; 
    return 0;
}

// ------------------------------------------------------------------------
// The main emulation loop is here... call into the Z80, VDP and PSG 
// ------------------------------------------------------------------------
void colecoDS_main(void) 
{
  u32 keys_pressed;
  u16 iTx,  iTy;
  u16 ResetNow  = 0, SaveNow = 0, LoadNow = 0;
  u32 ucUN, ucDEUX;
  static u32 lastUN = 0;
  static u8 dampenClick = 0;

  // Returns when  user has asked for a game to run...
  showMainMenu();

  // Get the Coleco Machine Emualtor ready
  colecoInit(gpFic[ucGameAct].szName);

  colecoSetPal();
  colecoRun();
  
  // Frame-to-frame timing...
  TIMER1_CR = 0;
  TIMER1_DATA=0;
  TIMER1_CR=TIMER_ENABLE  | TIMER_DIV_1024;

  // Once/second timing...
  TIMER2_CR=0;
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE  | TIMER_DIV_1024;
  timingFrames  = 0;
  emuFps=0;
    
  // Default SGM statics back to init state
  last_sgm_mode = false;
  last_ay_mode  = false;
  last_mc_mode  = 0;
  
  // Force the sound engine to turn on when we start emulation
  bStartSoundEngine = true;
    
  // -------------------------------------------------------------------
  // Stay in this loop running the Coleco game until the user exits...
  // -------------------------------------------------------------------
  while(1)  
  {
    // Take a tour of the Z80 counter and display the screen if necessary
    if (!LoopZ80()) 
    {   
        // If we've been asked to start the sound engine, rock-and-roll!
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
            if (myConfig.showFPS)
            {
                if (emuFps == 61) emuFps=60;
                else if (emuFps == 59) emuFps=60;            
                if (emuFps/100) szChai[0] = '0' + emuFps/100;
                else szChai[0] = ' ';
                szChai[1] = '0' + (emuFps%100) / 10;
                szChai[2] = '0' + (emuFps%100) % 10;
                szChai[3] = 0;
                AffChaine(0,0,6,szChai);
            }
            DisplayStatusLine(false);
            emuActFrames = 0;
        }
        emuActFrames++;

        // -------------------------------------------------------------
        // Vertical Sync reduces tearing but costs CPU time so this
        // is configurable - default to '1' on DSi and '0' on DS-LITE
        // -------------------------------------------------------------
        if (myConfig.vertSync)
        {
            // --------------------------------------------
            // NTSC Frame to Frame timing is handled by 
            // the swiWaitForVBlank() call in TMS9918a.c
            // This way we keep tearing to a minimum.
            // --------------------------------------------            
        }
        else
        {
            // -------------------------------------------------------------------
            // We only support NTSC 60 frames... there are PAL colecovisions
            // but the games really don't adjust well and so we stick to basics.
            // -------------------------------------------------------------------
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
                if (myConfig.showFPS == 2) break;   // If Full Speed, break out...
            }
        }
        
      // If the Z80 Debugger is enabled, call it
#ifdef DEBUG_Z80
      ShowDebugZ80();      
#endif
        
      // ------------------------------------------
      // Handle any screen touch events
      // ------------------------------------------
      ucUN  = 0;
      msx_key = 0;
      if  (keysCurrent() & KEY_TOUCH) {
        touchPosition touch;
        touchRead(&touch);
        iTx = touch.px;
        iTy = touch.py;
    
        // Test if "Reset Game" selected
        if  ((IsFullKeyboard() && ((iTx>=1) && (iTy>=38) && (iTx<= 35) && (iTy<63))) ||
            ((!IsFullKeyboard()) && ((iTx>=6) && (iTy>=40) && (iTx<=130) && (iTy<67))))
        {
          if  (!ResetNow) {
            ResetNow = 1;
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
        if  ((IsFullKeyboard() && ((iTx>=1) && (iTy>=63) && (iTx<= 35) && (iTy<86))) ||
            ((!IsFullKeyboard()) && ((iTx>=6) && (iTy>=67) && (iTx<=130) && (iTy<95))))
        {
          //  Stop sound
          SoundPause();
    
          //  Ask for verification
          if  (showMessage("DO YOU REALLY WANT TO","QUIT THE CURRENT GAME ?") == ID_SHM_YES) 
          { 
              memset((u8*)0x6820000, 0x00, 0x20000);    // Reset VRAM to 0x00 to clear any potential display garbage on way out
              return;
          }
          showMainMenu();
          DisplayStatusLine(true);            
          SoundUnPause();
        }

        // Test if "High Score" selected
        if  ((IsFullKeyboard() && ((iTx>=1) && (iTy>=86) && (iTx<= 35) && (iTy<110))) ||
            ((!IsFullKeyboard()) && ((iTx>=6) && (iTy>=95) && (iTx<=130) && (iTy<125))))
        {
          //  Stop sound
          SoundPause();
          highscore_display(file_crc);
          DisplayStatusLine(true);
          SoundUnPause();
        }
          
        // Test if "Save State" selected
        if  ((IsFullKeyboard() && ((iTx>=1) && (iTy>=110) && (iTx<= 35) && (iTy<134))) ||
            ((!IsFullKeyboard()) && ((iTx>=6) && (iTy>=125) && (iTx<=130) && (iTy<155)))) 
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
        if  ((IsFullKeyboard() && ((iTx>=1) && (iTy>=134) && (iTx<= 35) && (iTy<157))) ||
             ((!IsFullKeyboard()) && ((iTx>=6) && (iTy>=155) && (iTx<=130) && (iTy<184))))
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
          
        // For ADAM, the standard overlay has a CASSETTE icon to save data...
        if (adam_mode && (myConfig.overlay == 0))
        {
            if ((iTy >= 9) && (iTy < 30) && (iTx >= 120) && (iTx <= 155))
            {
                SaveAdamTapeOrDisk();
            }
        }
  
        // --------------------------------------------------------------------------
        // Test the touchscreen rendering of the Coleco/MSX KEYPAD
        // --------------------------------------------------------------------------
        if (myConfig.overlay == 9)  // MSX-Full Keyboard ~60 keys
        {
            if ((iTy >= 38) && (iTy < 63))        // Row 1 (top row)
            {
                if      ((iTx >= 1)   && (iTx < 35))   msx_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   msx_key = '0';
                else if ((iTx >= 57)  && (iTx < 79))   msx_key = '1';
                else if ((iTx >= 79)  && (iTx < 101))  msx_key = '2';
                else if ((iTx >= 101) && (iTx < 123))  msx_key = '3';
                else if ((iTx >= 123) && (iTx < 145))  msx_key = '4';
                else if ((iTx >= 145) && (iTx < 167))  msx_key = '5';
                else if ((iTx >= 167) && (iTx < 189))  msx_key = '6';
                else if ((iTx >= 189) && (iTx < 211))  msx_key = '7';
                else if ((iTx >= 211) && (iTx < 233))  msx_key = '8';
                else if ((iTx >= 233) && (iTx < 255))  msx_key = '9';
                
            }
            else if ((iTy >= 63) && (iTy < 86))   // Row 2
            {
                if      ((iTx >= 1)   && (iTx < 35))   msx_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   msx_key = 'A';
                else if ((iTx >= 57)  && (iTx < 79))   msx_key = 'B';
                else if ((iTx >= 79)  && (iTx < 101))  msx_key = 'C';
                else if ((iTx >= 101) && (iTx < 123))  msx_key = 'D';
                else if ((iTx >= 123) && (iTx < 145))  msx_key = 'E';
                else if ((iTx >= 145) && (iTx < 167))  msx_key = 'F';
                else if ((iTx >= 167) && (iTx < 189))  msx_key = 'G';
                else if ((iTx >= 189) && (iTx < 211))  msx_key = 'H';
                else if ((iTx >= 211) && (iTx < 233))  msx_key = 'I';
                else if ((iTx >= 233) && (iTx < 255))  msx_key = 'J';
            }
            else if ((iTy >= 86) && (iTy < 110))  // Row 3
            {
                if      ((iTx >= 1)   && (iTx < 35))   msx_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   msx_key = 'K';
                else if ((iTx >= 57)  && (iTx < 79))   msx_key = 'L';
                else if ((iTx >= 79)  && (iTx < 101))  msx_key = 'M';
                else if ((iTx >= 101) && (iTx < 123))  msx_key = 'N';
                else if ((iTx >= 123) && (iTx < 145))  msx_key = 'O';
                else if ((iTx >= 145) && (iTx < 167))  msx_key = 'P';
                else if ((iTx >= 167) && (iTx < 189))  msx_key = 'Q';
                else if ((iTx >= 189) && (iTx < 211))  msx_key = 'R';
                else if ((iTx >= 211) && (iTx < 233))  msx_key = 'S';
                else if ((iTx >= 233) && (iTx < 255))  msx_key = 'T';
            }
            else if ((iTy >= 110) && (iTy < 134)) // Row 4
            {
                if      ((iTx >= 1)   && (iTx < 35))   msx_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   msx_key = 'U';
                else if ((iTx >= 57)  && (iTx < 79))   msx_key = 'V';
                else if ((iTx >= 79)  && (iTx < 101))  msx_key = 'W';
                else if ((iTx >= 101) && (iTx < 123))  msx_key = 'X';
                else if ((iTx >= 123) && (iTx < 145))  msx_key = 'Y';
                else if ((iTx >= 145) && (iTx < 167))  msx_key = 'Z';
                else if ((iTx >= 167) && (iTx < 189))  msx_key = MSX_KEY_UP;
                else if ((iTx >= 189) && (iTx < 211))  msx_key = MSX_KEY_DOWN;
                else if ((iTx >= 211) && (iTx < 233))  msx_key = MSX_KEY_LEFT;
                else if ((iTx >= 233) && (iTx < 255))  msx_key = MSX_KEY_RIGHT;
            }
            else if ((iTy >= 134) && (iTy < 157)) // Row 5
            {
                if      ((iTx >= 1)   && (iTx < 35))   msx_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   msx_key = MSX_KEY_CTRL;
                else if ((iTx >= 57)  && (iTx < 79))   msx_key = MSX_KEY_CTRL;
                else if ((iTx >= 79)  && (iTx < 101))  msx_key = MSX_KEY_ESC;
                else if ((iTx >= 101) && (iTx < 123))  msx_key = MSX_KEY_ESC;
                else if ((iTx >= 123) && (iTx < 145))  msx_key = '.';
                else if ((iTx >= 145) && (iTx < 167))  msx_key = MSX_KEY_M1;
                else if ((iTx >= 167) && (iTx < 189))  msx_key = MSX_KEY_M2;
                else if ((iTx >= 189) && (iTx < 211))  msx_key = MSX_KEY_M3;
                else if ((iTx >= 211) && (iTx < 233))  msx_key = MSX_KEY_M4;
                else if ((iTx >= 233) && (iTx < 255))  msx_key = MSX_KEY_M5;
            }
            else if ((iTy >= 157) && (iTy < 192)) // Row 6
            {
                if      ((iTx >= 1)   && (iTx < 35))   msx_key = MSX_KEY_DEL;
                else if ((iTx >= 35)  && (iTx < 57))   msx_key = MSX_KEY_SHIFT;
                else if ((iTx >= 57)  && (iTx < 79))   msx_key = MSX_KEY_SHIFT;
                else if ((iTx >= 79)  && (iTx < 101))  msx_key = MSX_KEY_STOP;
                else if ((iTx >= 101) && (iTx < 123))  msx_key = MSX_KEY_STOP;
                else if ((iTx >= 123) && (iTx < 145))  msx_key = MSX_KEY_SEL;
                else if ((iTx >= 145) && (iTx < 167))  msx_key = MSX_KEY_SEL;
                else if ((iTx >= 167) && (iTx < 189))  msx_key = ' ';
                else if ((iTx >= 189) && (iTx < 211))  msx_key = ' ';
                else if ((iTx >= 211) && (iTx < 233))  msx_key = MSX_KEY_RET;
                else if ((iTx >= 233) && (iTx < 255))  msx_key = MSX_KEY_RET;
            }
        }
        
        u8 adam_key = 0;
        if (myConfig.overlay == 10)  // Adam Keyboard ~60 keys
        {
            if ((iTy >= 38) && (iTy < 63))        // Row 1 (top row)
            {
                if      ((iTx >= 1)   && (iTx < 35))   adam_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   adam_key = '0';
                else if ((iTx >= 57)  && (iTx < 79))   adam_key = '1';
                else if ((iTx >= 79)  && (iTx < 101))  adam_key = '2';
                else if ((iTx >= 101) && (iTx < 123))  adam_key = '3';
                else if ((iTx >= 123) && (iTx < 145))  adam_key = '4';
                else if ((iTx >= 145) && (iTx < 167))  adam_key = '5';
                else if ((iTx >= 167) && (iTx < 189))  adam_key = '6';
                else if ((iTx >= 189) && (iTx < 211))  adam_key = '7';
                else if ((iTx >= 211) && (iTx < 233))  adam_key = '8';
                else if ((iTx >= 233) && (iTx < 255))  adam_key = '9';
                
            }
            else if ((iTy >= 63) && (iTy < 86))   // Row 2
            {
                if      ((iTx >= 1)   && (iTx < 35))   adam_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   adam_key = 'A';
                else if ((iTx >= 57)  && (iTx < 79))   adam_key = 'B';
                else if ((iTx >= 79)  && (iTx < 101))  adam_key = 'C';
                else if ((iTx >= 101) && (iTx < 123))  adam_key = 'D';
                else if ((iTx >= 123) && (iTx < 145))  adam_key = 'E';
                else if ((iTx >= 145) && (iTx < 167))  adam_key = 'F';
                else if ((iTx >= 167) && (iTx < 189))  adam_key = 'G';
                else if ((iTx >= 189) && (iTx < 211))  adam_key = 'H';
                else if ((iTx >= 211) && (iTx < 233))  adam_key = 'I';
                else if ((iTx >= 233) && (iTx < 255))  adam_key = 'J';
            }
            else if ((iTy >= 86) && (iTy < 110))  // Row 3
            {
                if      ((iTx >= 1)   && (iTx < 35))   adam_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   adam_key = 'K';
                else if ((iTx >= 57)  && (iTx < 79))   adam_key = 'L';
                else if ((iTx >= 79)  && (iTx < 101))  adam_key = 'M';
                else if ((iTx >= 101) && (iTx < 123))  adam_key = 'N';
                else if ((iTx >= 123) && (iTx < 145))  adam_key = 'O';
                else if ((iTx >= 145) && (iTx < 167))  adam_key = 'P';
                else if ((iTx >= 167) && (iTx < 189))  adam_key = 'Q';
                else if ((iTx >= 189) && (iTx < 211))  adam_key = 'R';
                else if ((iTx >= 211) && (iTx < 233))  adam_key = 'S';
                else if ((iTx >= 233) && (iTx < 255))  adam_key = 'T';
            }
            else if ((iTy >= 110) && (iTy < 134)) // Row 4
            {
                if      ((iTx >= 1)   && (iTx < 35))   adam_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   adam_key = 'U';
                else if ((iTx >= 57)  && (iTx < 79))   adam_key = 'V';
                else if ((iTx >= 79)  && (iTx < 101))  adam_key = 'W';
                else if ((iTx >= 101) && (iTx < 123))  adam_key = 'X';
                else if ((iTx >= 123) && (iTx < 145))  adam_key = 'Y';
                else if ((iTx >= 145) && (iTx < 167))  adam_key = 'Z';
                else if ((iTx >= 167) && (iTx < 189))  adam_key = ADAM_KEY_UP;
                else if ((iTx >= 189) && (iTx < 211))  adam_key = ADAM_KEY_DOWN;
                else if ((iTx >= 211) && (iTx < 233))  adam_key = ADAM_KEY_LEFT;
                else if ((iTx >= 233) && (iTx < 255))  adam_key = ADAM_KEY_RIGHT;
            }
            else if ((iTy >= 134) && (iTy < 157)) // Row 5
            {
                if      ((iTx >= 1)   && (iTx < 35))   adam_key = 0;
                else if ((iTx >= 35)  && (iTx < 57))   adam_key = ':';
                else if ((iTx >= 57)  && (iTx < 79))   adam_key = '=';
                else if ((iTx >= 79)  && (iTx < 101))  adam_key = '#';
                else if ((iTx >= 101) && (iTx < 123))  adam_key = '.';
                else if ((iTx >= 123) && (iTx < 145))  adam_key = ADAM_KEY_F1;
                else if ((iTx >= 145) && (iTx < 167))  adam_key = ADAM_KEY_F2;
                else if ((iTx >= 167) && (iTx < 189))  adam_key = ADAM_KEY_F3;
                else if ((iTx >= 189) && (iTx < 211))  adam_key = ADAM_KEY_F4;
                else if ((iTx >= 211) && (iTx < 233))  adam_key = ADAM_KEY_F5;
                else if ((iTx >= 233) && (iTx < 255))  adam_key = ADAM_KEY_F6;
            }
            else if ((iTy >= 157) && (iTy < 192)) // Row 6
            {
                if      ((iTx >= 1)   && (iTx < 35))   SaveAdamTapeOrDisk();
                else if ((iTx >= 35)  && (iTx < 57))   {if (last_adam_key != 255) adam_CapsLock = 1-adam_CapsLock; last_adam_key=255;}
                else if ((iTx >= 57)  && (iTx < 79))   {if (last_adam_key != 255) adam_CapsLock = 1-adam_CapsLock; last_adam_key=255;}
                else if ((iTx >= 79)  && (iTx < 101))  adam_key = ADAM_KEY_BS;
                else if ((iTx >= 101) && (iTx < 123))  adam_key = ADAM_KEY_BS;
                else if ((iTx >= 123) && (iTx < 145))  adam_key = ADAM_KEY_ESC;
                else if ((iTx >= 145) && (iTx < 167))  adam_key = ADAM_KEY_ESC;
                else if ((iTx >= 167) && (iTx < 189))  adam_key = ' ';
                else if ((iTx >= 189) && (iTx < 211))  adam_key = ' ';
                else if ((iTx >= 211) && (iTx < 233))  adam_key = ADAM_KEY_ENTER;
                else if ((iTx >= 233) && (iTx < 255))  adam_key = ADAM_KEY_ENTER;
            }
            else {adam_key = 0; last_adam_key = 0;}
            
            if (adam_key != last_adam_key && (adam_key != 0) && (last_adam_key != 255))
            {
                PutKBD(adam_key | (adam_CapsLock ? CON_SHIFT:0));
                mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
            }
            if (last_adam_key != 255) last_adam_key = adam_key;
        }          
        else    // Normal 12 button virtual keypad
        {
            ucUN = ( ((iTx>=137) && (iTy>=38) && (iTx<=171) && (iTy<=72)) ? 0x02: 0x00);
            ucUN = ( ((iTx>=171) && (iTy>=38) && (iTx<=210) && (iTy<=72)) ? 0x08: ucUN);
            ucUN = ( ((iTx>=210) && (iTy>=38) && (iTx<=248) && (iTy<=72)) ? 0x03: ucUN);

            ucUN = ( ((iTx>=137) && (iTy>=73) && (iTx<=171) && (iTy<=110)) ? 0x0D: ucUN);
            ucUN = ( ((iTx>=171) && (iTy>=73) && (iTx<=210) && (iTy<=110)) ? 0x0C: ucUN);
            ucUN = ( ((iTx>=210) && (iTy>=73) && (iTx<=248) && (iTy<=110)) ? 0x01: ucUN);

            ucUN = ( ((iTx>=137) && (iTy>=111) && (iTx<=171) && (iTy<=147)) ? 0x0A: ucUN);
            ucUN = ( ((iTx>=171) && (iTy>=111) && (iTx<=210) && (iTy<=147)) ? 0x0E: ucUN);
            ucUN = ( ((iTx>=210) && (iTy>=111) && (iTx<=248) && (iTy<=147)) ? 0x04: ucUN);

            ucUN = ( ((iTx>=137) && (iTy>=148) && (iTx<=171) && (iTy<=186)) ? 0x06: ucUN);
            ucUN = ( ((iTx>=171) && (iTy>=148) && (iTx<=210) && (iTy<=186)) ? 0x05: ucUN);
            ucUN = ( ((iTx>=210) && (iTy>=148) && (iTx<=248) && (iTy<=186)) ? 0x09: ucUN);
        }
          
        // ---------------------------------------------------------------------
        // If we are mapping the touch-screen keypad to P2, we shift these up.
        // ---------------------------------------------------------------------
        if (myConfig.touchPad) ucUN = ucUN << 16;
          
        if (++dampenClick > 3)  // Make sure the key is pressed for an appreciable amount of time...
        {
            if (((ucUN != 0) || (msx_key != 0)) && (lastUN == 0))
            {
                mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
            }
            lastUN = (ucUN ? ucUN:msx_key);
        }
      } //  SCR_TOUCH
      else  
      {
        ResetNow=SaveNow=LoadNow = 0;
        lastUN = 0;  dampenClick = 0;
        last_adam_key = 0;
      }

      // ---------------------------------------------------
      // Assume no spinner action until we see it below...
      // ---------------------------------------------------
      spinX_left  = 0;
      spinX_right = 0;
      spinY_left  = 0;
      spinY_right = 0;
        
      // ------------------------------------------------------------------------
      //  Test DS keypresses (ABXY, L/R) and map to corresponding Coleco keys
      // ------------------------------------------------------------------------
      ucDEUX  = 0;  
      keys_pressed  = keysCurrent();
      if ((keys_pressed & KEY_L) && (keys_pressed & KEY_R) && (keys_pressed & KEY_X)) 
      {
            lcdSwap();
            WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
      }
      else        
      if  (keys_pressed & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_R | KEY_L | KEY_X | KEY_Y)) 
      {
          // --------------------------------------------------------------------------------------------------
          // There are 12 NDS buttons (D-Pad, XYAB, L/R and Start+Select) - we allow mapping of any of these.
          // --------------------------------------------------------------------------------------------------
          for (u8 i=0; i<12; i++)
          {
              if (keys_pressed & NDS_keyMap[i])
              {
                  if (keyCoresp[myConfig.keymap[i]] < 0xFFFF0000)   // Normal key map
                  {
                    ucDEUX  |= keyCoresp[myConfig.keymap[i]];
                  }
                  else // Special Spinner Handling
                  {
                      if      (keyCoresp[myConfig.keymap[i]] == META_SPINX_LEFT)  spinX_left  = 1;
                      else if (keyCoresp[myConfig.keymap[i]] == META_SPINX_RIGHT) spinX_right = 1;
                      else if (keyCoresp[myConfig.keymap[i]] == META_SPINY_LEFT)  spinY_left  = 1;
                      else if (keyCoresp[myConfig.keymap[i]] == META_SPINY_RIGHT) spinY_right = 1;
                  }
              }
          }
      }
        
      // ---------------------------------------------------------
      // Accumulate all bits above into the Joystick State var... 
      // ---------------------------------------------------------
      JoyState = ucUN | ucDEUX;

      // --------------------------------------------------
      // Handle Auto-Fire if enabled in configuration...
      // --------------------------------------------------
      static u8 autoFireTimer[2]={0,0};
      if (myConfig.autoFire1 && (JoyState & JST_FIRER))  // Fire Button 1
      {
         if ((++autoFireTimer[0] & 7) > 4)  JoyState &= ~JST_FIRER;
      }
      if (myConfig.autoFire2 && (JoyState & JST_FIREL))  // Fire Button 2
      {
          if ((++autoFireTimer[1] & 7) > 4) JoyState &= ~JST_FIREL;
      }
    }
  }
}


/*********************************************************************************
 * Init DS Emulator - setup VRAM banks and background screen rendering banks
 ********************************************************************************/
void colecoDSInit(void) 
{
  //  Init graphic mode (bitmap mode)
  videoSetMode(MODE_0_2D  | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE  | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_SPRITE);          // Once emulation of game starts, we steal this back for an additional 128K of VRAM at 0x6820000
  vramSetBankC(VRAM_C_SUB_BG);
  vramSetBankD(VRAM_D_SUB_SPRITE);
    
  vramSetBankE(VRAM_E_LCD );                 // Not using this  for video but 64K of faster RAM always useful!  Mapped  at 0x06880000 - This block of faster RAM used for the first 128K of bankswitching 
  vramSetBankF(VRAM_F_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x06890000 -   ..
  vramSetBankG(VRAM_G_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x06894000 -   ..
  vramSetBankH(VRAM_H_LCD );                 // Not using this  for video but 32K of faster RAM always useful!  Mapped  at 0x06898000 -   ..
  vramSetBankI(VRAM_I_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x068A0000 -   Used for the Look Up Table

  //  Stop blending effect of intro
  REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;
  
  //  Render the top screen
  bg0 = bgInit(0, BgType_Text8bpp,  BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp,  BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(ecranHautTiles,  bgGetGfxPtr(bg0), LZ77Vram);
  decompress(ecranHautMap,  (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) ecranHautPal,(void*)  BG_PALETTE,256*2);
  unsigned  short dmaVal =*(bgGetMapPtr(bg0)+51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1),32*24*2);

  // Render the bottom screen for "options select" mode
  bg0b  = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1b  = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);
  decompress(ecranBasSelTiles,  bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(ecranBasSelMap,  (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void*) ecranBasSelPal,(void*)  BG_PALETTE_SUB,256*2);
  dmaVal  = *(bgGetMapPtr(bg0b)+24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1b),32*24*2);
    
  //  Put out the initial messages looking for a file system
  AffChaine(2,6,0,"SEARCH FAT SYSTEM   ...   ");
  AffChaine(2,7,0,"FAT  SYSTEM FOUND   !");

  //  Find the files
  colecoDSFindFiles();
}

// ---------------------------------------------------------------------------
// Setup the bottom screen - mostly for menu, high scores, options, etc.
// ---------------------------------------------------------------------------
void InitBottomScreen(void)
{
    if (myConfig.overlay == 1)  // Wargames
    {
      //  Init bottom screen
      decompress(wargamesTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(wargamesMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) wargamesPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 2)  // Mousetrap
    {
      //  Init bottom screen
      decompress(mousetrapTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(mousetrapMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) mousetrapPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 3)  // Gateway to Apshai
    {
      //  Init bottom screen
      decompress(gatewayTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(gatewayMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) gatewayPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 4)  // Spy Hunter
    {
      //  Init bottom screen
      decompress(spyhunterTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(spyhunterMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) spyhunterPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 5)  // Fix Up the Mix Up
    {
      //  Init bottom screen
      decompress(fixupmixupTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(fixupmixupMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) fixupmixupPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 6)  // Boulder Dash
    {
      //  Init bottom screen
      decompress(boulderTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(boulderMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) boulderPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 7)  // Quest for Quinta Roo
    {
      //  Init bottom screen
      decompress(questTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(questMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) questPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 8)  // 2010
    {
      //  Init bottom screen
      decompress(hal2010Tiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(hal2010Map, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) hal2010Pal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 9)  // MSX-Full Keyboard
    {
      //  Init bottom screen
      decompress(msx_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(msx_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) msx_fullPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 10)  // Adam Keyboard
    {
      //  Init bottom screen
      decompress(adam_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(adam_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) adam_fullPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else // Generic Overlay
    {
#ifdef DEBUG_Z80
          //  Init bottom screen
          decompress(ecranDebugTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(ecranDebugMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) ecranDebugPal,(void*) BG_PALETTE_SUB,256*2);
#else        
      if (msx_mode)
      {
          //  Init bottom screen
          decompress(msxTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(msxMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) msxPal,(void*) BG_PALETTE_SUB,256*2);
      }
      else if (adam_mode)
      {
          //  Init bottom screen
          decompress(adamTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(adamMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) adamPal,(void*) BG_PALETTE_SUB,256*2);
      }
      else
      {
          //  Init bottom screen
          decompress(ecranBasTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(ecranBasMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) ecranBasPal,(void*) BG_PALETTE_SUB,256*2);
      }
#endif 
    }
    
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
    *(pVDPVidMem+iBcl) = 0x00;

  //  Init bottom screen - might be an overlay
  InitBottomScreen();

  //  Load coleco Bios ROM
  if (sordm5_mode)
    memcpy(pColecoMem,SordM5Bios,0x2000);
  else if (msx_mode)
    memcpy(pColecoMem,Slot0BIOS,0x8000);
  else
    memcpy(pColecoMem,ColecoBios,0x2000);
  
  //  Return with result
  return  (RetFct);
}

// -------------------------------------------------------------
// Only used for basic timing of moving the Mario sprite...
// -------------------------------------------------------------
void irqVBlank(void) 
{ 
 // Manage time
  vusCptVBL++;
}

// ----------------------------------------------------------------
// Look for the coleco.rom bios in several possible locations...
// ----------------------------------------------------------------
void LoadBIOSFiles(void)
{
    FILE *fp;

    // --------------------------------------------------
    // We will look for all 3 BIOS files here but only 
    // the Colecovision coleco.rom is critical.
    // --------------------------------------------------
    bColecoBiosFound = false;
    bSordBiosFound = false;
    bMSXBiosFound = false;
    bAdamBiosFound = false;
    
    // -----------------------------------------------------------
    // First load Sord M5 bios - don't really care if this fails
    // -----------------------------------------------------------
    fp = fopen("sordm5.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/sordm5.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/sordm5.rom", "rb");
    if (fp != NULL)
    {
        bSordBiosFound = true;
        fread(SordM5Bios, 0x2000, 1, fp);
        fclose(fp);
    }

    // -----------------------------------------------------------
    // Next try to load the MSX.ROM - if this fails we still
    // have the C-BIOS as a good built-in backup.
    // -----------------------------------------------------------
    fp = fopen("msx.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/msx.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/sordm5.rom", "rb");
    if (fp != NULL)
    {
        bMSXBiosFound = true;
        fread(MSXBios, 0x8000, 1, fp);
        fclose(fp);
    }

    // -----------------------------------------------------------
    // Try loading the EOS.ROM and WRITER.ROM Adam files...
    // -----------------------------------------------------------
    fp = fopen("eos.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/eos.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/eos.rom", "rb");
    if (fp != NULL)
    {
        bAdamBiosFound = true;
        fread(AdamEOS, 0x2000, 1, fp);
        fclose(fp);
    }
    
    if (bAdamBiosFound)
    {
        fp = fopen("writer.rom", "rb");
        if (fp == NULL) fp = fopen("/roms/bios/writer.rom", "rb");
        if (fp == NULL) fp = fopen("/data/bios/writer.rom", "rb");
        if (fp != NULL)
        {
            bAdamBiosFound = true;
            fread(AdamWRITER, 0x8000, 1, fp);
            fclose(fp);
        }
        else bAdamBiosFound = false;    // Both EOS and WRITER need to be found...
    }
    
    // -----------------------------------------------------------
    // Coleco ROM BIOS must exist or the show is off!
    // -----------------------------------------------------------
    fp = fopen("coleco.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/coleco.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/coleco.rom", "rb");
    if (fp != NULL)
    {
        bColecoBiosFound = true;
        fread(ColecoBios, 0x2000, 1, fp);
        fclose(fp);
    }
}

/*********************************************************************************
 * Program entry point - check if an argument has been passed in probably from TWL++
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
    
  SetYtrigger(190); //trigger 2 lines before vsync    
    
  irqSet(IRQ_VBLANK,  irqVBlank);
  irqEnable(IRQ_VBLANK);
    
  // -----------------------------------------------------------------
  // Grab the BIOS before we try to switch any directories around...
  // -----------------------------------------------------------------
  LoadBIOSFiles();
    
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
  }
  else
  {
      initial_file[0]=0; // No file passed on command line...
      chdir("/roms");    // Try to start in roms area... doesn't matter if it fails
      chdir("coleco");   // And try to start in the subdir /coleco... doesn't matter if it fails.
  }
    
  SoundPause();
  
  //  ------------------------------------------------------------
  //  We run this loop forever until game exit is selected...
  //  ------------------------------------------------------------
  while(1)  
  {
    colecoDSInit();

    // ---------------------------------------------------------------
    // Let the user know what BIOS files were found - the only BIOS 
    // that must exist is coleco.rom or else the show is off...
    // ---------------------------------------------------------------
    if (bColecoBiosFound)
    {
        u8 idx = 9;
        AffChaine(2,idx++,0,"LOADING BIOS FILES ..."); idx++;
        AffChaine(2,idx++,0,"coleco.rom BIOS FOUND"); idx++;
        if (bMSXBiosFound) {AffChaine(2,idx++,0,"msx.rom BIOS FOUND"); idx++;}
        if (bSordBiosFound) {AffChaine(2,idx++,0,"sordm5.rom BIOS FOUND"); idx++;}
        if (bAdamBiosFound) {AffChaine(2,idx++,0,"eos.rom and writer.rom FOUND"); idx++;}
        
        AffChaine(2,idx++,0,"TOUCH SCREEN / KEY TO BEGIN"); idx++;
        
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
        while(1) ;  // We're done... Need a coleco bios to run a CV emulator
    }
  
    while(1) 
    {
      SoundPause();
      //  Choose option
      if  (initial_file[0] != 0)
      {
          ucGameChoice=0;
          ucGameAct=0;
          strcpy(gpFic[ucGameAct].szName, initial_file);
          initial_file[0] = 0;    // No more initial file...
          ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
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


// End of file

