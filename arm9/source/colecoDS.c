// =====================================================================================
// Copyright (c) 2021-2024 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty. Please see readme.md
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
#include "cpu/z80/ctc.h"
#include "intro.h"
#include "adam_sm.h"
#include "cvision_kbd.h"
#include "msx_sm.h"
#include "msx_full.h"
#include "mtx_full.h"
#include "svi_full.h"
#include "msx_japan.h"
#include "adam_full.h"
#include "alpha_kbd.h"
#include "einstein_kbd.h"
#include "sc3000_kbd.h"
#include "m5_kbd.h"
#include "pv2000_sm.h"
#include "debug_ovl.h"
#include "options.h"
#include "colecovision.h"
#include "topscreen.h"
#include "wargames.h"
#include "mousetrap.h"
#include "gateway.h"
#include "spyhunter.h"
#include "fixupmixup.h"
#include "boulder.h"
#include "quest.h"
#include "hal2010.h"
#include "shuttle.h"
#include "utopia.h"
#include "cvision.h"
#include "fdc.h"

#include "soundbank.h"
#include "soundbank_bin.h"
#include "MSX_CBIOS.h"
#include "MTX_BIOS.h"
#include "C24XX.h"
#include "screenshot.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/scc/SCC.h"

#include "printf.h"

extern Z80 CPU;
u32 debug[0x10]={0};

// -------------------------------------------------------------------------------------------
// All emulated systems have ROM, RAM and possibly BIOS or SRAM. So we create generic buffers
// for all this here... these are sized big enough to handle the largest memory necessary
// to render games playable. There are a few MSX games that are larger than 512k but they
// are mostly demos or foreign-language adventures... not enough interest to try to squeeze
// in a larger ROM buffer to include them - we are still trying to keep compatible with the
// smaller memory model of the original DS/DS-LITE.
//
// These memory buffers will be pointed to by the MemoryMap[] array. This array contains 8
// pointers that can break down the Z80 memory into 8k chunks.  For the few games that have
// a smaller than 8k boundary (e.g. Creativision uses a 2k BIOS) we can just stage/build
// up the memory into the RAM_Memory[] buffer and point into that as a single 64k layout.
// -------------------------------------------------------------------------------------------

u8 ROM_Memory[MAX_CART_SIZE * 1024]   ALIGN(32) = {0};        // ROM Carts up to 1MB (that's pretty huge in the Z80 world!)
u8 RAM_Memory[0x20000]                ALIGN(32) = {0};        // RAM up to 128K (mostly for the ADAM... other systems utilize less)
u8 BIOS_Memory[0x10000]               ALIGN(32) = {0};        // To hold our BIOS and related OS memory (64K as the BIOS  for various machines ends up in different spots)
u8 SRAM_Memory[0x4000]                ALIGN(32) = {0};        // SRAM up to 16K for the few carts which use it (e.g. MSX Deep Dungeon II, Hydlide II, etc)


// --------------------------------------------------------------------------
// This is the full 64K coleco memory map.
// The memory is generally used as follows:
//    0x0000-0x1FFF  coleco.rom BIOS - Super Game Module can map RAM here
//    0x2000-0x5FFF  Usually unused - but Super Game Module maps RAM here
//    0x6000-0x7FFF  RAM - there is only 1K repeated with 8 mirrors
//    0x8000-0xFFFF  32K Cartridge Space
//
// Other emulated machines will have different memory layouts and different
// IO port mappings... these are handled on a machine-by-machine basis.
// We need to keep the BIOS files around for possible use as the player
// switches between gaming systems.
// --------------------------------------------------------------------------
u8 ColecoBios[0x2000]     = {0};  // We keep the Coleco  8K BIOS around to swap in/out
u8 SordM5BiosJP[0x2000]   = {0};  // We keep the Sord M5 8K BIOS around to swap in/out
u8 SordM5BiosEU[0x2000]   = {0};  // We keep the Sord M5 8K BIOS around to swap in/out
u8 PV2000Bios[0x4000]     = {0};  // We keep the Casio PV-2000 16K BIOS around to swap in/out
u8 AdamEOS[0x2000]        = {0};  // We keep the ADAM EOS.ROM bios around to swap in/out
u8 AdamWRITER[0x8000]     = {0};  // We keep the ADAM WRITER.ROM bios around to swap in/out
u8 SVIBios[0x8000]        = {0};  // We keep the SVI 32K BIOS around to swap in/out
u8 Pencil2Bios[0x2000]    = {0};  // We keep the 8K Pencil 2 BIOS around to swap in/out
u8 EinsteinBios[0x2000]   = {0};  // We keep the 8k Einstein BIOS around
u8 EinsteinBios2[0x2000]  = {0};  // We keep the 8k Einstein diagnostics/peripheral BIOS around
u8 CreativisionBios[0x800]= {0};  // We keep the 2k Creativision BIOS around
u8 MSX_Bios[0x8000]       = {0};  // We store several kinds of MSX bios files in VRAM and copy out the one we want to use in msx_restore_bios() but this is for the ubiquitious MSX.ROM

// --------------------------------------------------------------------------------
// For Activision PCBs we have up to 32K of EEPROM (not all games use all 32K)
// --------------------------------------------------------------------------------
C24XX EEPROM;

// ------------------------------------------
// Some ADAM and Tape related vars...
// ------------------------------------------
u8 adam_CapsLock        = 0;
u8 disk_unsaved_data[2] = {0,0};
u8 write_EE_counter     = 0;
u32 last_tape_pos       = 9999;

// --------------------------------------------------------------------------
// For machines that have a full keybaord, we use the Left and Right
// shoulder buttons on the NDS to emulate the SHIFT and CTRL keys...
// --------------------------------------------------------------------------
u8 key_shift __attribute__((section(".dtcm"))) = false;
u8 key_ctrl  __attribute__((section(".dtcm"))) = false;
u8 key_code  __attribute__((section(".dtcm"))) = false;
u8 key_graph __attribute__((section(".dtcm"))) = false;
u8 key_dia   __attribute__((section(".dtcm"))) = false;

// ---------------------------------------------------------------------------
// Some timing and frame rate comutations to keep the emulation on pace...
// ---------------------------------------------------------------------------
u16 emuFps          __attribute__((section(".dtcm"))) = 0;
u16 emuActFrames    __attribute__((section(".dtcm"))) = 0;
u16 timingFrames    __attribute__((section(".dtcm"))) = 0;

// -----------------------------------------------------------------------------------------------
// For the various BIOS files ... only the coleco.rom is required - everything else is optional.
// -----------------------------------------------------------------------------------------------
u8 bColecoBiosFound         = false;
u8 bSordBiosFound           = false;
u8 bMSXBiosFound            = false;
u8 bSVIBiosFound            = false;
u8 bAdamBiosFound           = false;
u8 bPV2000BiosFound         = false;
u8 bPencilBiosFound         = false;
u8 bEinsteinBiosFound       = false;
u8 bCreativisionBiosFound   = false;

u8 soundEmuPause     __attribute__((section(".dtcm"))) = 1;       // Set to 1 to pause (mute) sound, 0 is sound unmuted (sound channels active)

// -----------------------------------------------------------------------------------------------
// This set of critical vars is what determines the machine type - Coleco vs MSX vs SVI, etc.
// -----------------------------------------------------------------------------------------------
u8 sg1000_mode       __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .sg game is loaded for Sega SG-1000 support
u8 sordm5_mode       __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .m5 game is loaded for Sord M5 support
u8 pv2000_mode       __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .sg game is loaded for Sega SG-1000 support
u8 memotech_mode     __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .mtx or .run game is loaded for Memotech MTX support
u8 msx_mode          __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .msx game is loaded for basic MSX support
u8 svi_mode          __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .svi game is loaded for basic SVI-3x8 support
u8 adam_mode         __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .ddp game is loaded for ADAM game support
u8 pencil2_mode      __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .pen Pencil 2 ROM is loaded (only one known to exist!)
u8 einstein_mode     __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .com Einstien ROM is loaded
u8 creativision_mode __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .cv ROM is loaded (Creativision)
u8 coleco_mode       __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a Colecovision ROM is loaded

u16 machine_mode     __attribute__((section(".dtcm"))) = 0x0000;  // A faster way to know what type of machine we are. No bits set = COLECO_MODE

u8 kbd_key           __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the ASCII key (e.g. 'A', 'B', '3', etc)
u16 nds_key          __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the NDS keys from keysCurrent() or similar
u8 last_mapped_key   __attribute__((section(".dtcm"))) = 0;       // The last mapped key which has been pressed - used for key click feedback
u8 kbd_keys_pressed  __attribute__((section(".dtcm"))) = 0;       // Each frame we check for keys pressed - since we can map keyboard keys to the NDS, there may be several pressed at once
u8 kbd_keys[12]      __attribute__((section(".dtcm")));           // Up to 12 possible keys pressed at the same time (we have 12 NDS physical buttons though it's unlikely that more than 2 or maybe 3 would be pressed)

u8 IssueCtrlBreak    __attribute__((section(".dtcm"))) = 0;       // For the Tatung Einstein to hold Ctrl-Break for a second or so...

u8 bStartSoundEngine = false;  // Set to true to unmute sound after 1 frame of rendering...
int bg0, bg1, bg0b, bg1b;      // Some vars for NDS background screen handling
volatile u16 vusCptVBL = 0;    // We use this as a basic timer for the Mario sprite... could be removed if another timer can be utilized
u8 touch_debounce = 0;         // A bit of touch-screen debounce
u8 key_debounce = 0;           // A bit of key debounce
u8 playingSFX = 0;             // To prevent sound effects like disk/tape loading from happening too frequently

// The DS/DSi has 12 keys that can be mapped
u16 NDS_keyMap[12] __attribute__((section(".dtcm"))) = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_A, KEY_B, KEY_X, KEY_Y, KEY_R, KEY_L, KEY_START, KEY_SELECT};

// --------------------------------------------------------------------
// The key map for the Colecovision... mapped into the NDS controller
// --------------------------------------------------------------------
u32 keyCoresp[MAX_KEY_OPTIONS] __attribute__((section(".dtcm"))) = {
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

    META_SPINX_LEFT,
    META_SPINX_RIGHT,
    META_SPINY_LEFT,
    META_SPINY_RIGHT,

    META_KBD_A,
    META_KBD_B,
    META_KBD_C,
    META_KBD_D,
    META_KBD_E,
    META_KBD_F,
    META_KBD_G,
    META_KBD_H,
    META_KBD_I,
    META_KBD_J,
    META_KBD_K,
    META_KBD_L,
    META_KBD_M,
    META_KBD_N,
    META_KBD_O,
    META_KBD_P,
    META_KBD_Q,
    META_KBD_R,
    META_KBD_S,
    META_KBD_T,
    META_KBD_U,
    META_KBD_V,
    META_KBD_W,
    META_KBD_X,
    META_KBD_Y,
    META_KBD_Z,
    META_KBD_0,
    META_KBD_1,
    META_KBD_2,
    META_KBD_3,
    META_KBD_4,
    META_KBD_5,
    META_KBD_6,
    META_KBD_7,
    META_KBD_8,
    META_KBD_9,
    META_KBD_SHIFT,
    META_KBD_CTRL,
    META_KBD_CODE,
    META_KBD_GRAPH,
    META_KBD_SPACE,
    META_KBD_RETURN,
    META_KBD_ESC,
    META_KBD_HOME,
    META_KBD_UP,
    META_KBD_DOWN,
    META_KBD_LEFT,
    META_KBD_RIGHT,
    META_KBD_PERIOD,
    META_KBD_COMMA,
    META_KBD_COLON,
    META_KBD_SEMI,
    META_KBD_QUOTE,
    META_KBD_SLASH,
    META_KBD_BACKSLASH,
    META_KBD_PLUS,
    META_KBD_MINUS,
    META_KBD_LBRACKET,
    META_KBD_RBRACKET,
    META_KBD_CARET,
    META_KBD_ASTERISK,
    META_KBD_ATSIGN,
    META_KBD_BS,
    META_KBD_TAB,
    META_KBD_INS,
    META_KBD_DEL,
    META_KBD_CLR,
    META_KBD_UNDO,
    META_KBD_MOVE,    
    META_KBD_WILDCARD,
    META_KBD_STORE,
    META_KBD_PRINT,
    META_KBD_STOP_BRK,
    META_KBD_F1,
    META_KBD_F2,
    META_KBD_F3,
    META_KBD_F4,
    META_KBD_F5,
    META_KBD_F6,
    META_KBD_F7,
    META_KBD_F8
};

// ----------------------------------------------
// Game speeds constants... first entry is 100%
// ----------------------------------------------
u16 GAME_SPEED_NTSC[] __attribute__((section(".dtcm"))) = {546, 497, 455, 420, 607 };
u16 GAME_SPEED_PAL[]  __attribute__((section(".dtcm"))) = {656, 596, 547, 505, 729 };

// --------------------------------------------------------------------------------
// Spinners! X and Y taken together will actually replicate the roller controller.
// --------------------------------------------------------------------------------
u8 spinX_left   __attribute__((section(".dtcm"))) = 0;
u8 spinX_right  __attribute__((section(".dtcm"))) = 0;
u8 spinY_left   __attribute__((section(".dtcm"))) = 0;
u8 spinY_right  __attribute__((section(".dtcm"))) = 0;

static char tmp[64];    // For various sprintf() calls

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

// --------------------------------------------------------------------------------------------
// MAXMOD streaming setup and handling...
// We were using the normal ARM7 sound core but it sounded "scratchy" and so with the help
// of FluBBa, we've swiched over to the maxmod sound core which performs much better.
// --------------------------------------------------------------------------------------------
#define sample_rate  (27965)    // To match the driver in sn76496 - this is good enough quality for the DS
#define buffer_size  (512+12)     // Enough buffer that we don't have to fill it too often

mm_ds_system sys   __attribute__((section(".dtcm")));
mm_stream myStream __attribute__((section(".dtcm")));

s16 mixbuf1[4096+64];      // When we have SN and AY sound we have to mix 3+3 channels
s16 mixbuf2[4096+64];      // into a single output so we render to mix buffers first.

// -------------------------------------------------------------------------------------------
// maxmod will call this routine when the buffer is half-empty and requests that
// we fill the sound buffer with more samples. They will request 'len' samples and
// we will fill exactly that many. If the sound is paused, we fill with 'mute' samples.
// -------------------------------------------------------------------------------------------
s16 last_sample __attribute__((section(".dtcm"))) = 0;
ITCM_CODE mm_word OurSoundMixer(mm_word len, mm_addr dest, mm_stream_formats format)
{
    if (soundEmuPause)  // If paused, just "mix" in mute sound chip... all channels are OFF
    {
        s16 *p = (s16*)dest;
        for (int i=0; i<len*2; i++)
        {
           *p++ = last_sample;      // To prevent pops and clicks... just keep outputting the last sample
        }
    }
    else
    {
        if (machine_mode & (MODE_MSX | MODE_SVI | MODE_EINSTEIN))
        {
          if (msx_scc_enable)   // If SCC is enabled, we need to mix the AY with the SCC chips
          {
              ay38910Mixer(len*2, mixbuf1, &myAY);
              SCCMixer(len*4, mixbuf2, &mySCC);
              
              s16 *p = (s16*)dest;
              int j=0;
              for (int i=0; i<len*2; i++)
              {
                  // ------------------------------------------------------------------------
                  // We normalize the samples and mix them carefully to minimize clipping...
                  // ------------------------------------------------------------------------
                  s32 combined = (mixbuf1[i]) + ((mixbuf2[j] + mixbuf2[j+1])/2) + 32768;
                  j+=2;
                  if (combined >  32767) combined = 32767;
                  *p++ = (s16)combined;
              }
              p--; last_sample = *p;
          }
          else  // Pretty simple... just AY
          {
              if (myConfig.msxBeeper) 
              {
                    // If the Beeper is active, we mix in the SN chip which is producing the key beeper tone
                    ay38910Mixer(len*2, mixbuf1, &myAY);
                    sn76496Mixer(len*2, mixbuf2, &mySN);
                    s16 *p = (s16*)dest;
                    for (int i=0; i<len*2; i++)
                    {
                        // ------------------------------------------------------------------------
                        // We normalize the samples and mix them carefully to minimize clipping...
                        // ------------------------------------------------------------------------
                        s32 combined = (mixbuf1[i]) + (mixbuf2[i]) + 32768;
                        if (combined >  32767) combined = 32767;
                        *p++ = (s16)combined;
                    }
                    p--; last_sample = *p;
              }
              else
              {
                  ay38910Mixer(len*2, dest, &myAY);
                  last_sample = ((s16*)dest)[len*2 - 1];
              }
          }
        }
        else if (AY_Enable)  // If AY is enabled we mix the normal SN chip with the AY chip sound
        {
            ay38910Mixer(len*2, mixbuf1, &myAY);
            sn76496Mixer(len*2, mixbuf2, &mySN);
            s16 *p = (s16*)dest;
            for (int i=0; i<len*2; i++)
            {
                // ------------------------------------------------------------------------
                // We normalize the samples and mix them carefully to minimize clipping...
                // ------------------------------------------------------------------------
                s32 combined = (mixbuf1[i]) + (mixbuf2[i]) + 32768;
                if (combined >  32767) combined = 32767;
                *p++ = (s16)combined;
            }
            p--; last_sample = *p;
        }
        else  // This is the 'normal' case of just Colecovision SN sound chip output
        {
            sn76496Mixer(len*2, dest, &mySN);
            last_sample = ((s16*)dest)[len*2 - 1];
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
  //  initialize maxmod with our small 5-effect soundbank
  //----------------------------------------------------------------
  mmInitDefaultMem((mm_addr)soundbank_bin);

  mmLoadEffect(SFX_CLICKNOQUIT);
  mmLoadEffect(SFX_KEYCLICK);
  mmLoadEffect(SFX_MUS_INTRO);
  mmLoadEffect(SFX_FLOPPY);
  mmLoadEffect(SFX_ADAM_DDP);

  //----------------------------------------------------------------
  //  open stream
  //----------------------------------------------------------------
  myStream.sampling_rate  = sample_rate;            // sampling rate = (27965)
  myStream.buffer_length  = buffer_size;            // buffer length = (512+16)
  myStream.callback       = OurSoundMixer;          // set callback function
  myStream.format         = MM_STREAM_16BIT_STEREO;   // format = mono 16-bit
  myStream.timer          = MM_TIMER0;              // use hardware timer 0
  myStream.manual         = false;                  // use automatic filling
  mmStreamOpen(&myStream);

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

void sound_chip_reset()
{
  memset(mixbuf1, 0x00, sizeof(mixbuf1));
  memset(mixbuf2, 0x00, sizeof(mixbuf2));

  //  ------------------------------------------------------------------
  //  The SN sound chip is for normal Colecovision sound handling
  //  ------------------------------------------------------------------
  sn76496Reset(1, &mySN);         // Reset the SN sound chip

  sn76496W(0x80 | 0x00,&mySN);    // Write new Frequency for Channel A
  sn76496W(0x00 | 0x00,&mySN);    // Write new Frequency for Channel A
  sn76496W(0x90 | 0x0F,&mySN);    // Write new Volume for Channel A

  sn76496W(0xA0 | 0x00,&mySN);    // Write new Frequency for Channel B
  sn76496W(0x00 | 0x00,&mySN);    // Write new Frequency for Channel B
  sn76496W(0xB0 | 0x0F,&mySN);    // Write new Volume for Channel B

  sn76496W(0xC0 | 0x00,&mySN);    // Write new Frequency for Channel C
  sn76496W(0x00 | 0x00,&mySN);    // Write new Frequency for Channel C
  sn76496W(0xD0 | 0x0F,&mySN);    // Write new Volume for Channel C

  sn76496W(0xFF,  &mySN);         // Disable Noise Channel

  sn76496Mixer(8, mixbuf1, &mySN);  // Do an initial mix conversion to clear the output
  
  //  --------------------------------------------------------------------
  //  The AY sound chip is for Super Game Module and MSX sound handling
  //  --------------------------------------------------------------------
  ay38910Reset(&myAY);             // Reset the "AY" sound chip
  ay38910IndexW(0x07, &myAY);      // Register 7 is ENABLE
  ay38910DataW(0x3F, &myAY);       // All OFF (negative logic)
  ay38910Mixer(8, mixbuf2, &myAY); // Do an initial mix conversion to clear the output

  // -----------------------------------------------------------------
  // The SCC sound chip is just for a few select Konami MSX1 games 
  // -----------------------------------------------------------------
  SCCReset(&mySCC);
  
  SCCWrite(0x00, 0x988A, &mySCC);
  SCCWrite(0x00, 0x988B, &mySCC);
  SCCWrite(0x00, 0x988C, &mySCC);
  SCCWrite(0x00, 0x988D, &mySCC);
  SCCWrite(0x00, 0x988E, &mySCC);
  SCCWrite(0x00, 0x988F, &mySCC);
  
  SCCMixer(16, mixbuf2, &mySCC);     // Do an initial mix conversion to clear the output
}

// -----------------------------------------------------------------------
// We setup the sound chips - disabling all volumes to start.
// -----------------------------------------------------------------------
void dsInstallSoundEmuFIFO(void)
{
  SoundPause();             // Pause any sound output  
  sound_chip_reset();       // Reset the SN, AY and SCC chips 
  setupStream();            // Setup maxmod stream...
  bStartSoundEngine = true; // Volume will 'unpause' after 1 frame in the main loop.
}

//*****************************************************************************
// Reset the Colecovision - mostly CPU, Super Game Module and memory...
//*****************************************************************************
static u8 last_sgm_mode = false;
static u8 last_pencil_mode = false;
static u8 last_einstein_mode = false;
static u8 last_ay_mode = false;
static u8 last_mc_mode = 0;
static u8 last_sg1000_mode = 0;
static u8 last_pv2000_mode = 0;
static u8 last_sordm5_mode = 0;
static u8 last_memotech_mode = 0;
static u8 last_msx_mode = 0;
static u8 last_msx_scc_enable = 0;
static u8 last_svi_mode = 0;
static u8 last_adam_mode = 0;
static u8 last_pal_mode = 99;

// --------------------------------------------------------------
// When we reset the machine, there are many small utility flags
// for various expansion peripherals that must be reset.
// --------------------------------------------------------------
void ResetStatusFlags(void)
{
  last_sgm_mode = false;
  last_ay_mode  = false;
  last_mc_mode  = 0;
  last_pencil_mode = 0;
  last_sg1000_mode = 0;
  last_pv2000_mode = 0;
  last_sordm5_mode = 0;
  last_memotech_mode = 0;
  last_msx_mode = 0;
  last_msx_scc_enable = 0;
  last_svi_mode = 0;
  last_adam_mode = 0;
}

// --------------------------------------------------------------
// When we first load a ROM/CASSETTE or when the user presses
// the RESET button on the touch-screen...
// --------------------------------------------------------------
void ResetColecovision(void)
{    
  JoyMode=JOYMODE_JOYSTICK;             // Joystick mode key
  JoyState = 0x00000000;                // Nothing pressed to start

  Reset9918();                          // Reset video chip

  sgm_reset();                          // Reset Super Game Module

  sound_chip_reset();                   // Reset the SN, AY and SCC chips
  
  DrZ80_Reset();                        // Reset the DrZ80 CPU core
  ResetZ80(&CPU);                       // Reset the Z80 CPU core

  sordm5_reset();                       // Reset the Sord M5 specific vars
  memotech_reset();                     // Reset the memotech MTX specific vars
  svi_reset();                          // Reset the SVI specific vars
  msx_reset();                          // Reset the MSX specific vars
  pv2000_reset();                       // Reset the PV2000 specific vars
  einstein_reset();                     // Reset the Tatung Einstein specific vars

  adam_CapsLock = 0;                    // On reset the CAPS lock if OFF
  disk_unsaved_data[0] = 0;             // No unsaved ADAM tape/disk data to start
  disk_unsaved_data[1] = 0;             // No unsaved ADAM tape/disk data to start

  write_EE_counter=0;                   // Nothing to write for EEPROM yet
    
  playingSFX = 0;                       // No sound effects playing yet

  // -----------------------------------------------------------------------
  // By default, many of the machines use a simple flat 64K memory model
  // which is simple to emulate by having all the memory map point to the
  // internal 64K ram memory. So we default to that - specific machines
  // can change this up as required for more complicated memory handling.
  // -----------------------------------------------------------------------
  MemoryMap[0] = RAM_Memory + 0x0000;
  MemoryMap[1] = RAM_Memory + 0x2000;
  MemoryMap[2] = RAM_Memory + 0x4000;
  MemoryMap[3] = RAM_Memory + 0x6000;
  MemoryMap[4] = RAM_Memory + 0x8000;
  MemoryMap[5] = RAM_Memory + 0xA000;
  MemoryMap[6] = RAM_Memory + 0xC000;
  MemoryMap[7] = RAM_Memory + 0xE000;
    
  // ----------------------------------------------------------------------
  // Based on the system that we are running, wipe memory appopriately...
  // ----------------------------------------------------------------------
  if (sg1000_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      sg1000_reset();                           // Reset the SG-1000 to restore memory
  }
  else if (pv2000_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      memcpy(RAM_Memory,PV2000Bios,0x4000);     // Restore the Casio PV-2000 BIOS
  }
  else if (sordm5_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      if (myConfig.isPAL)
        memcpy(RAM_Memory,SordM5BiosEU,0x2000); // Restore Sord M5 BIOS - PAL from Europe
      else
        memcpy(RAM_Memory,SordM5BiosJP,0x2000); // Restore Sord M5 BIOS - NTSC from Japan
  }
  else if (memotech_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      memotech_restore_bios();                  // Put the BIOS back in place and point to it
  }
  else if (msx_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      msx_restore_bios();                       // Put the BIOS back in place and point to it
      if (msx_sram_enabled) msxLoadEEPROM();    // If this cart uses SRAM, we can try to load it from file
  }
  else if (svi_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      svi_restore_bios();                       // Put the BIOS back in place and point to it
  }
  else if (pencil2_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      memcpy(RAM_Memory,Pencil2Bios,0x2000);    // Load the Pencil II BIOS into place
  }
  else if (einstein_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      einstein_restore_bios();                  // Put the BIOS back in place and point to it
  }
  else if (creativision_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      creativision_restore_bios();              // Put the CreatiVision BIOS into place
      creativision_reset();                     // Reset the Creativision and 6502 CPU - must be done after BIOS is loaded to get reset vector properly loaded
  }
  else if (adam_mode)
  {
      colecoWipeRAM();                          // Wipe the RAM area
      adam_128k_mode = 0;                       // Normal 64K ADAM to start
      sgm_reset();                              // Make sure the SGM memory is not functional
      SetupAdam(true);                          // Full reset of ADAMNet
  }
  else // Oh yeah... we're an actual Colecovision emulator! Almost forgot.
  {
      memset(RAM_Memory+0x2000, 0xFF, 0x6000);  // Reset non-mapped area between BIOS and RAM - SGM RAM might map here
      colecoWipeRAM();                          // Wipe main RAM area

      // Setup the Coleco BIOS and point to it
      memset(BIOS_Memory+0x2000, 0xFF, 0xE000);
      memcpy(BIOS_Memory+0x0000, ColecoBios, 0x2000);
      MemoryMap[0] = BIOS_Memory+0x0000;

      if (bActivisionPCB)
      {
          Reset24XX(&EEPROM, myConfig.cvEESize);
          colecoLoadEEPROM();
      }
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

  XBuf = XBuf_A;        // Set the initial screen ping-pong buffer to A

  ResetStatusFlags();   // Some static status flags for the UI mostly
}

//*********************************************************************************
// A mini Z80 debugger of sorts. Put out some Z80, VDP and SGM/Bank info on
// screen every frame to help us debug some of the problem games. This is enabled
// via global configuration for debugger.
//*********************************************************************************
const char *VModeNames[] =
{
    "GRA1",
    "GRA2",
    "MULC",
    "HBIT",
    "TEXT",
    "----",
    "HBIT",
    "----",
};

void ShowDebugZ80(void)
{
    u8 idx=1;

    if (myGlobalConfig.debugger == 3)
    {
        extern u8 lastBank;
        extern u8 romBankMask;
        extern u8 Port20, Port53, Port60;

        sprintf(tmp, "VDP[] %02X %02X %02X %02X", VDP[0],VDP[1],VDP[2],VDP[3]);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "VDP[] %02X %02X %02X %02X", VDP[4],VDP[5],VDP[6],VDP[7]);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "VStat %02X Data=%02X", VDPStatus, VDPDlatch);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "VAddr %04X", VAddr);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "VLatc %02X  %c %c", VDPCtrlLatch, VDP[1]&TMS9918_REG1_IRQ ? 'E':'D', VDPStatus&TMS9918_STAT_VBLANK ? 'V':'-');
        DSPrint(0,idx++,7, tmp);
        idx++;
        sprintf(tmp, "Z80PC %04X", CPU.PC.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "Z80SP %04X", CPU.SP.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "Z80A  %04X", CPU.AF.W);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "IRQ   %04X", CPU.IRequest);
        DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "IREQ  %d", CPU.User);
        DSPrint(0,idx++,7, tmp);
        idx++;

        if (AY_Enable)
        {
            sprintf(tmp, "AY[] %02X %02X %02X %02X", myAY.ayRegs[0], myAY.ayRegs[1], myAY.ayRegs[2], myAY.ayRegs[3]);
            DSPrint(0,idx++,7, tmp);
            sprintf(tmp, "AY[] %02X %02X %02X %02X", myAY.ayRegs[4], myAY.ayRegs[5], myAY.ayRegs[6], myAY.ayRegs[7]);
            DSPrint(0,idx++,7, tmp);
            sprintf(tmp, "AY[] %02X %02X %02X %02X", myAY.ayRegs[8], myAY.ayRegs[9], myAY.ayRegs[10], myAY.ayRegs[11]);
            DSPrint(0,idx++,7, tmp);
            sprintf(tmp, "AY[] %02X %02X %02X %02X", myAY.ayRegs[12], myAY.ayRegs[13], myAY.ayRegs[14], myAY.ayRegs[15]);
            DSPrint(0,idx++,7, tmp);
            
            if (!(einstein_mode || (msx_mode == 3))) // If Einstein or MSX, the FDC stuff will go here...
            {
                idx -= 4;
                sprintf(tmp, "SN0 %04X %04X %1X", mySN.ch0Frq, mySN.ch0Reg, mySN.ch0Att);
                DSPrint(17,idx++,7, tmp);
                sprintf(tmp, "SN1 %04X %04X %1X", mySN.ch1Frq, mySN.ch1Reg, mySN.ch1Att);
                DSPrint(17,idx++,7, tmp);
                sprintf(tmp, "SN2 %04X %04X %1X", mySN.ch2Frq, mySN.ch2Reg, mySN.ch2Att);
                DSPrint(17,idx++,7, tmp);
                sprintf(tmp, "SN3 %04X %04X %1X", mySN.ch3Frq, mySN.ch3Reg, mySN.ch3Att);
                DSPrint(17,idx++,7, tmp);
            }
            idx++;            
        }
        else
        {
            sprintf(tmp, "SN0 %04X %04X %2d", mySN.ch0Frq, mySN.ch0Reg, mySN.ch0Att);
            DSPrint(0,idx++,7, tmp);
            sprintf(tmp, "SN1 %04X %04X %2d", mySN.ch1Frq, mySN.ch1Reg, mySN.ch1Att);
            DSPrint(0,idx++,7, tmp);
            sprintf(tmp, "SN2 %04X %04X %2d", mySN.ch2Frq, mySN.ch2Reg, mySN.ch2Att);
            DSPrint(0,idx++,7, tmp);
            sprintf(tmp, "SN3 %04X %04X %2d", mySN.ch3Frq, mySN.ch3Reg, mySN.ch3Att);
            DSPrint(0,idx++,7, tmp);
            idx++;
        }

        if (einstein_mode || (msx_mode == 3)) // Put out some Floppy Drive Controller stuff...
        {
            idx -= 6;
            sprintf(tmp, " FDC.Sta %-3d %02X", FDC.status, FDC.status);
            DSPrint(17,idx++,7, tmp);
            sprintf(tmp, " FDC.Cmd %-3d %02X", FDC.command, FDC.command);
            DSPrint(17,idx++,7, tmp);
            sprintf(tmp, " FDC.dat %-3d %02X", FDC.data, FDC.data);
            DSPrint(17,idx++,7, tmp);
            sprintf(tmp, " FDC.tra %-3d %02X", FDC.track, FDC.track);
            DSPrint(17,idx++,7, tmp);
            sprintf(tmp, " FDC.sec %-3d %02X", FDC.sector, FDC.sector);
            DSPrint(17,idx++,7, tmp);
            extern u32 halt_counter;
            sprintf(tmp, "HALT %-12lu", halt_counter); 
            DSPrint(18,idx++,7, tmp);
            idx--;
        }

        sprintf(tmp, "Bank  %02X [%02X]", (lastBank != 199 ? lastBank:0), romBankMask);    DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "PORTS P23=%02X P53=%02X P60=%02X", Port20, Port53, Port60);          DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "VMode %02X %4s", TMS9918_Mode, VModeNames[TMS9918_Mode]);            DSPrint(0,idx++,7, tmp);
        sprintf(tmp, "VSize %s", ((TMS9918_VRAMMask == 0x3FFF) ? "16K":" 4K"));            DSPrint(0,idx++,7, tmp);

        idx = 1;
        if (einstein_mode || sordm5_mode || memotech_mode)
        {
            for (int chan=0; chan<4; chan++)
            {
                sprintf(tmp, "C%d %02X %-4lu%-4lu", chan, CTC[chan].control, (u32)CTC[chan].constant, (u32)CTC[chan].counter); 
                DSPrint(18,idx++,7, tmp);
            }    
        }
        else
        {
            sprintf(tmp, "SVI %s %s", (svi_RAM[0] ? "RAM":"ROM"), (svi_RAM[1] ? "RAM":"ROM")); DSPrint(19,idx++,7, tmp);
            sprintf(tmp, "PPI A=%02X B=%02X",Port_PPI_A,Port_PPI_B);    DSPrint(19,idx++,7, tmp);
            sprintf(tmp, "PPI C=%02X M=%02X",Port_PPI_C,Port_PPI_CTRL); DSPrint(19,idx++,7, tmp);
        }
       
        idx++;
        for (u8 i=0; i<= ((einstein_mode || (msx_mode == 3)) ? 4:6); i++)
        {
            sprintf(tmp, "D%d %-9lu %04X", i, debug[i], (u16)debug[i]); DSPrint(15,idx++,7, tmp);
        }
    }
    else
    {
        idx = 1;
        for (u8 i=0; i<3; i++)
        {
            sprintf(tmp, "D%d %-7lu %04lX ", i, debug[i], (debug[i] < 0xFFFF ? debug[i]:0xFFFF)); DSPrint(0,idx++,7, tmp);
        }
        idx = 1;
        for (u8 i=3; i<6; i++)
        {
            sprintf(tmp, "D%d %-7lu %04lX", i, debug[i], (debug[i] < 0xFFFF ? debug[i]:0xFFFF)); DSPrint(17,idx++,7, tmp);
        }
        if (einstein_mode || (msx_mode == 3))
        {
            sprintf(tmp, "FD.ST=%02X CM=%02X TR=%02X SI=%02X SE=%02X", FDC.status, FDC.command, FDC.track, FDC.side, FDC.sector); DSPrint(0,idx++,7, tmp);
        }
    }
    idx++;
}

// ----------------------------------------------------------------
// Check if we have an ADAM .ddp file (otherwise assume .dsk file)
// ----------------------------------------------------------------
bool isAdamDDP(u8 disk)
{
    if ((strstr(lastDiskDataPath[disk], ".ddp") != 0) || (strstr(lastDiskDataPath[disk], ".DDP") != 0)) return true;
    return false;
}


// ------------------------------------------------------------
// The status line shows the status of the Super Game Moudle,
// AY sound chip support and MegaCart support.  Game players
// probably don't care, but it's really helpful for devs.
// ------------------------------------------------------------
void DisplayStatusLine(bool bForce)
{
    if (myGlobalConfig.emuText == 0) return;
    
    if (bForce) last_tape_pos = 999999;
    if (bForce) last_pal_mode = 99;
    if (sg1000_mode)
    {
        if ((last_sg1000_mode != sg1000_mode) || bForce)
        {
            last_sg1000_mode = sg1000_mode;
            DSPrint(23,0,6, (sg1000_mode == 2 ? "SC-3000":"SG-1000"));
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL  && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            DSPrint(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
    }
    else if (pv2000_mode)
    {
        if ((last_pv2000_mode != pv2000_mode) || bForce)
        {
            last_pv2000_mode = pv2000_mode;
            DSPrint(23,0,6, "PV-2000");
        }
    }
    else if (sordm5_mode)
    {
        if ((last_sordm5_mode != sordm5_mode) || bForce)
        {
            last_sordm5_mode = sordm5_mode;
            DSPrint(23,0,6, "SORD M5");
        }
        if (last_pal_mode != myConfig.isPAL && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            DSPrint(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
        
    }
    else if (memotech_mode)
    {
        if ((last_memotech_mode != memotech_mode) || bForce)
        {
            last_memotech_mode = memotech_mode;
            DSPrint(20,0,6, "MEMOTECH MTX");
            last_pal_mode = 99;
        }
        if ((memotech_mode == 2) && (last_tape_pos != tape_pos) && (!memotech_magrom_present))
        {
            last_tape_pos = tape_pos;
            sprintf(tmp, "CAS %d%%  ", (int)(100 * (int)tape_pos)/(int)tape_len);
            DSPrint(9,0,6, tmp);
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            DSPrint(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
    }
    else if (msx_mode)
    {
        if ((last_msx_mode != msx_mode) || bForce)
        {
            last_msx_mode = msx_mode;
            int rom_size = (((LastROMSize/1024) <= 999) ? (LastROMSize/1024) : 999); // Good enough - 1MB will show as 999K 
            switch (myConfig.msxBios)
            {
                case 1: sprintf(tmp, "%-7s %3dK", msx_rom_str_short,  rom_size);    break;     // MSX (64K machine... use variable name)
                case 2: sprintf(tmp, "CX5M    %3dK",                  rom_size);    break;     // Yamaha CX5M (32K mapped in slot 0)
                case 3: sprintf(tmp, "HX-10   %3dK",                  rom_size);    break;     // Toshiba HX-10 (64K mapped in slot 2)
                case 4: sprintf(tmp, "HB-10   %3dK",                  rom_size);    break;     // Sony HB-10 (16K mapped in slot 0)
                case 5: sprintf(tmp, "FS-1300 %3dK",                  rom_size);    break;     // National FS-1300 (64K mapped in slot 3)
                case 6: sprintf(tmp, "PV-7    %3dK",                  rom_size);    break;     // Casio PV-7 (just 8K at the top of slot 0)
                default:sprintf(tmp, "MSX     %3dK",                  rom_size);    break;     // C-BIOS as a fall-back (64K mapped in slot 3)
            }            
            DSPrint(20,0,6, tmp);
            last_pal_mode = 99;
        }
        if (last_msx_scc_enable != msx_scc_enable)
        {   
            // SCC and CAS are mutually exclusive so we can reuse the same area on screen...
            DSPrint(9,0,6, (msx_scc_enable ? "SCC":"   "));
            last_msx_scc_enable = msx_scc_enable;
        }
        if ((last_tape_pos != tape_pos) && (msx_mode == 2))
        {
            last_tape_pos = tape_pos;
            sprintf(tmp, "CAS %d%%  ", (int)(100 * (int)tape_pos)/(int)tape_len);
            DSPrint(9,0,6, tmp);
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL  && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            DSPrint(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
        if (write_EE_counter > 0)
        {
            --write_EE_counter;
            if (write_EE_counter == 0)
            {
                // Save EE now!
                msxSaveEEPROM();
            }
            DSPrint(5,0,6, (write_EE_counter ? "EE":"  "));
        }
        if (msx_mode == 3)
        {
            if (io_show_status)
            {
                if (io_show_status == 5) {DSPrint(8,0,6, "DISK WRITE"); io_show_status = 3;}
                if (io_show_status == 4) {DSPrint(8,0,6, "DISK READ "); io_show_status = 3;}
                if (io_show_status == 3)
                {
                    if (!myGlobalConfig.diskSfxMute) mmEffect(SFX_FLOPPY);
                }
                io_show_status--;
            }
            else
            {
                DSPrint(8,0,6, "          ");
            }            
        }
    }
    else if (svi_mode)
    {
        if ((last_svi_mode != svi_mode) || bForce)
        {
            last_svi_mode = svi_mode;
            DSPrint(20,0,6, "SPECTRAVIDEO");
            last_pal_mode = 99;
        }
        if ((last_tape_pos != tape_pos) && (svi_mode == 1))
        {
            last_tape_pos = tape_pos;
            sprintf(tmp, "CAS %d%%  ", (int)(100 * (int)tape_pos)/(int)tape_len);
            DSPrint(9,0,6, tmp);
        }
        if (last_pal_mode != myConfig.isPAL)
        {
            last_pal_mode = myConfig.isPAL;
            DSPrint(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
    }
    else if (adam_mode)
    {
        if ((last_adam_mode != adam_mode) || bForce)
        {
            last_adam_mode = adam_mode;
            DSPrint(25,0,6, "ADAM");
        }

        // If we are showing the ADAM keyboard, indicate CAPS LOCK
        if (myConfig.overlay == 1)
        {
            DSPrint(1,23,0, (adam_CapsLock ? "@":" "));
        }

        if (io_show_status)
        {
            DSPrint(30,0,6, (io_show_status == 2 ? "WR":"RD"));
            io_show_status = 0;
            if (playingSFX)
            {
                playingSFX--;
            }
            else
            {
                // If global settings does not MUTE the sound effects...
                if (!myGlobalConfig.diskSfxMute)
                {
                    if (!isAdamDDP(0)) mmEffect(SFX_FLOPPY); else mmEffect(SFX_ADAM_DDP);
                    playingSFX = 1;
                }
            }
        }
        else
        {
            DSPrint(30,0,6, "  ");
        }
    }
    else if (pencil2_mode)
    {
        if ((pencil2_mode != last_pencil_mode) || bForce)
        {
            last_pencil_mode = pencil2_mode;
            DSPrint(22,0,6, "PENCIL II");
        }
    }
    else if (creativision_mode)
    {
        if ((creativision_mode != last_pencil_mode) || bForce)
        {
            last_pencil_mode = creativision_mode;
            DSPrint(20,0,6, "CREATIVISION");
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL  && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            DSPrint(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
    }
    else if (einstein_mode)
    {
        if ((einstein_mode != last_einstein_mode) || bForce)
        {
            DSPrint(22,0,6, "EINSTEIN");
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL  && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            DSPrint(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
        
        DSPrint(2,19,6, (ein_alpha_lock ? "@":" "));
        
        if (io_show_status)
        {
            if (io_show_status == 5) {DSPrint(8,0,6, "DISK WRITE"); io_show_status = 3;}
            if (io_show_status == 4) {DSPrint(8,0,6, "DISK READ "); io_show_status = 3;}
            if (io_show_status == 3)
            {
                if (!myGlobalConfig.diskSfxMute) mmEffect(SFX_FLOPPY);
            }
            io_show_status--;
        }
        else
        {
            DSPrint(8,0,6, "          ");
        }
    }
    else    // Various Colecovision Possibilities
    {
        if ((last_sgm_mode != sgm_enable) || bForce)
        {
            last_sgm_mode = sgm_enable;
            DSPrint(28,0,6, (sgm_enable ? "SGM":"   "));
        }

        if ((last_ay_mode != AY_Enable) || bForce)
        {
            last_ay_mode = AY_Enable;
            DSPrint(25,0,6, (AY_Enable ? "AY":"  "));
        }

        if ((last_mc_mode != romBankMask) || bForce)
        {
            last_mc_mode = romBankMask;
            DSPrint(22,0,6, (romBankMask ? "MC":"  "));
        }

        if (write_EE_counter > 0)
        {
            --write_EE_counter;
            if (write_EE_counter == 0)
            {
                // Save EE now!
                colecoSaveEEPROM();
            }
            DSPrint(30,0,6, (write_EE_counter ? "EE":"  "));
        }
    }
}



// ------------------------------------------------------------------------
// Save out the ADAM .ddp or .dsk file and show 'SAVING' on screen
// ------------------------------------------------------------------------
void SaveAdamTapeOrDisk(u8 disk)
{
    if (io_show_status) return; // Don't save while io status

    DSPrint(12,0,6, "SAVING");
    if (isAdamDDP(disk))
        SaveFDI(&Tapes[disk], lastDiskDataPath[disk], FMT_DDP);
    else
        SaveFDI(&Disks[disk], lastDiskDataPath[disk], (LastROMSize == (320*1024) ? FMT_ADMDSK320:FMT_ADMDSK));
    DSPrint(12,0,6, "      ");
    DisplayStatusLine(true);
    disk_unsaved_data[disk] = 0;
}


// -----------------------------------------------------
// Load a new Adam .ddp or .dsk file into main memory.
// -----------------------------------------------------
void DigitalDataInsert(u8 disk, char *filename)
{
    FILE *fp;

    // --------------------------------------------
    // Read the .DDP or .DSK into the ROM_Memory[]
    // --------------------------------------------
    fp = fopen(filename, "rb");
    if (fp)
    {
        memset(ROM_Memory, 0xFF, (320 * 1024));
        LastROMSize = fread(ROM_Memory, 1, (320 * 1024), fp);    
        fclose(fp);
        
        // --------------------------------------------
        // And set it as the active ddp or dsk...
        // --------------------------------------------
        strcpy(lastDiskDataPath[disk], filename);
        if ((strstr(lastDiskDataPath[disk], ".ddp") != 0) || (strstr(lastDiskDataPath[disk], ".DDP") != 0))
        {
            ChangeTape(disk, lastDiskDataPath[disk]);
        }
        else
        {
            ChangeDisk(disk, lastDiskDataPath[disk]);
        }
   } else LastROMSize = 0;
}

// ------------------------------------------------------------------------
// Swap in a new .cas Cassette/Tape - reset position counter to zero.
// ------------------------------------------------------------------------
void CassetteInsert(char *filename)
{
    FILE *fp;

    fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    LastROMSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    memset(ROM_Memory, 0xFF, (MAX_CART_SIZE * 1024));
    fread(ROM_Memory, tape_len, 1, fp);
    tape_pos = 0;
    tape_len = LastROMSize;
    fclose(fp);
}

#define MENU_ACTION_END             255 // Always the last sentinal value
#define MENU_ACTION_EXIT            0   // Exit the menu
#define MENU_ACTION_SAVE            1   // Save Disk or Cassette in primary drive
#define MENU_ACTION_SWAP            2   // Swap Disk or Cassette in primary drive
#define MENU_ACTION_SAVE1           3   // Save Disk or Cassette in secondary drive
#define MENU_ACTION_SWAP1           4   // Swap Disk or Cassette in secondary drive
#define MENU_ACTION_REWIND          5   // Rewind the Cassette
#define MENU_ACTION_CLOAD_RUN       6   // Issue CLOAD RUN
#define MENU_ACTION_BLOAD_CAS       7   // Issue BLOAD CAS
#define MENU_ACTION_RUN_CAS         8   // Issue RUN CAS
#define MENU_ACTION_LOAD            9   // Issue LOAD
#define MENU_ACTION_RUN             10  // Issue RUN

#define MENU_ACTION_RUN_EIN         20  // Load Einstein .COM FILE
#define MENU_ACTION_RUN_MTX         21  // Load MTX .RUN or .COM FILE
#define MENU_ACTION_INST_RAMDISK    22  // Install Einstein RAMDISK
#define MENU_ACTION_INIT_RAMDISK    23  // Init Einstein RAMDISK

typedef struct 
{
    char *menu_string;
    u8    menu_action;
} MenuItem_t;

typedef struct 
{
    char *title;
    u8   start_row;
    MenuItem_t menulist[10];
} CassetteDiskMenu_t;


CassetteDiskMenu_t adam_ddp_menu =
{
    "ADAM DIGITAL DATA MENU",
    5,
    {
        {" SAVE DDP/DSK 1 ",        MENU_ACTION_SAVE},
        {" SWAP DDP/DSK 1 ",        MENU_ACTION_SWAP},
        {" SAVE DDP/DSK 2 ",        MENU_ACTION_SAVE1},
        {" SWAP DDP/DSK 2 ",        MENU_ACTION_SWAP1},
        {" EXIT MENU      ",        MENU_ACTION_EXIT},
        {" NULL           ",        MENU_ACTION_END},
    },
};

CassetteDiskMenu_t msx_digital_menu =
{
    "MSX CASSETTE/DISK MENU",
    5,
    {
        {" SAVE   CASSETTE  ",      MENU_ACTION_SAVE},
        {" SWAP   CASSETTE  ",      MENU_ACTION_SWAP},
        {" REWIND CASSETTE  ",      MENU_ACTION_REWIND},
        {" CLOAD  RUN       ",      MENU_ACTION_CLOAD_RUN},
        {" BLOAD 'CAS:',R   ",      MENU_ACTION_BLOAD_CAS},
        {" RUN   'CAS:'     ",      MENU_ACTION_RUN_CAS},
        {" SAVE   DISK DATA ",      MENU_ACTION_SAVE},
        {" EXIT   MENU      ",      MENU_ACTION_EXIT},
        {" NULL             ",      MENU_ACTION_END},
    },
};

CassetteDiskMenu_t svi_digital_menu =
{
    "SVI CASSETTE/DISK MENU",
    5,
    {
        {" SAVE   CASSETTE  ",      MENU_ACTION_SAVE},
        {" SWAP   CASSETTE  ",      MENU_ACTION_SWAP},
        {" REWIND CASSETTE  ",      MENU_ACTION_REWIND},
        {" CLOAD  RUN       ",      MENU_ACTION_CLOAD_RUN},
        {" BLOAD 'CAS:',R   ",      MENU_ACTION_BLOAD_CAS},
        {" RUN   'CAS:'     ",      MENU_ACTION_RUN_CAS},
        {" EXIT   MENU      ",      MENU_ACTION_EXIT},
        {" NULL             ",      MENU_ACTION_END},
    },
};


CassetteDiskMenu_t einstein_disk_menu =
{
    "EINSTEIN DISK MENU",
    5,
    {
        {" SAVE    DISK0     ",     MENU_ACTION_SAVE},
        {" SWAP    DISK0     ",     MENU_ACTION_SWAP},
        {" SAVE    DISK1     ",     MENU_ACTION_SAVE1},
        {" SWAP    DISK1     ",     MENU_ACTION_SWAP1},
        {" RAMDISK DISK1     ",     MENU_ACTION_INST_RAMDISK},
        {" RAMDISK CLEAR     ",     MENU_ACTION_INIT_RAMDISK},
        {" RUN  EINSTEIN .COM",     MENU_ACTION_RUN_EIN},
        {" EXIT MENU         ",     MENU_ACTION_EXIT},
        {" NULL              ",     MENU_ACTION_END},
    },
};


CassetteDiskMenu_t mtx_cassette_menu =
{
    "CASSETTE MENU",
    5,
    {
        {" SAVE CASSETTE    ",      MENU_ACTION_SAVE},
        {" SWAP CASSETTE    ",      MENU_ACTION_SWAP},
        {" REWIND CASSETTE  ",      MENU_ACTION_REWIND},
        {" LOAD ''          ",      MENU_ACTION_LOAD},
        {" RUN              ",      MENU_ACTION_RUN},
        {" RUN MEMOTECH .RUN",      MENU_ACTION_RUN_MTX},
        {" RUN MEMOTECH .COM",      MENU_ACTION_RUN_MTX},
        {" EXIT MENU        ",      MENU_ACTION_EXIT},
        {" NULL             ",      MENU_ACTION_END},
    },
};

CassetteDiskMenu_t generic_cassette_menu =
{
    "CASSETTE MENU",
    5,
    {
        {" SAVE CASSETTE    ",      MENU_ACTION_SAVE},
        {" SWAP CASSETTE    ",      MENU_ACTION_SWAP},
        {" REWIND CASSETTE  ",      MENU_ACTION_REWIND},
        {" EXIT MENU        ",      MENU_ACTION_EXIT},
        {" NULL             ",      MENU_ACTION_END},
    },
};


CassetteDiskMenu_t *menu = &generic_cassette_menu;

// ------------------------------------------------------------------------
// Show the Cassette/Disk Menu text - highlight the selected row.
// ------------------------------------------------------------------------
u8 cassette_menu_items = 0;
void CassetteMenuShow(bool bClearScreen, u8 sel)
{
    cassette_menu_items = 0;
    
    if (bClearScreen)
    {
      // ---------------------------------------------------
      // Put up a generic background for this mini-menu...
      // ---------------------------------------------------
      BottomScreenOptions();
    }
    
    // ---------------------------------------------------
    // Pick the right context menu based on the machine
    // ---------------------------------------------------
                        menu = &generic_cassette_menu;
    if (adam_mode)      menu = &adam_ddp_menu;
    if (msx_mode)       menu = &msx_digital_menu;
    if (svi_mode)       menu = &svi_digital_menu;
    if (einstein_mode)  menu = &einstein_disk_menu;
    if (memotech_mode)  menu = &mtx_cassette_menu;
    
    // Display the menu title
    DSPrint(16-(strlen(menu->title)/2), menu->start_row, 6, menu->title);
    
    // And display all of the menu items
    while (menu->menulist[cassette_menu_items].menu_action != MENU_ACTION_END)
    {
        DSPrint(16-(strlen(menu->menulist[cassette_menu_items].menu_string)/2), menu->start_row+2+cassette_menu_items, (cassette_menu_items == sel) ? 7:6, menu->menulist[cassette_menu_items].menu_string);
        cassette_menu_items++;   
    }
    
    // --------------------------------------------------------------------
    // Some systems need to show if we hae unsaved data to warn the user.
    // --------------------------------------------------------------------
    if (adam_mode)
    {
        if (disk_unsaved_data[0]) DSPrint(3, menu->start_row+5+cassette_menu_items, 0,  " DRIVE 1 HAS UNSAVED DATA! ");
        if (disk_unsaved_data[1]) DSPrint(3, menu->start_row+6+cassette_menu_items, 0,  " DRIVE 2 HAS UNSAVED DATA! ");
        snprintf(tmp, 31, "DRV1: %s", lastDiskDataPath[0]); tmp[31] = 0;  DSPrint(1, 21, 0, tmp);
        snprintf(tmp, 31, "DRV2: %s", lastDiskDataPath[1]); tmp[31] = 0;  DSPrint(1, 22, 0, tmp);
    }
    else if (msx_mode)
    {
        if (disk_unsaved_data[0]) DSPrint(4, menu->start_row+5+cassette_menu_items, 0,  "  DISK HAS UNSAVED DATA! ");
    }
    else if (einstein_mode)
    {
        if (disk_unsaved_data[0]) DSPrint(4, menu->start_row+5+cassette_menu_items, 0,  " DISK0 HAS UNSAVED DATA! ");
        if (disk_unsaved_data[1]) DSPrint(4, menu->start_row+6+cassette_menu_items, 0,  " DISK1 HAS UNSAVED DATA! ");
        snprintf(tmp, 31, "DSK0: %s", einstein_disk_path[0]); tmp[31] = 0;  DSPrint((16 - (strlen(tmp)/2)), 21,0, tmp);
        snprintf(tmp, 31, "DSK1: %s", einstein_disk_path[1]); tmp[31] = 0;  DSPrint((16 - (strlen(tmp)/2)), 22,0, tmp);
    }
    
    // ----------------------------------------------------------------------------------------------
    // And near the bottom, display the file/rom/disk/cassette that is currently loaded into memory.
    // ----------------------------------------------------------------------------------------------
    if (!einstein_mode && !adam_mode) DisplayFileName();
}

// ------------------------------------------------------------------------
// Handle Cassette/Disk mini-menu interface...
// ------------------------------------------------------------------------
void CassetteMenu(void)
{
  u8 menuSelection = 0;

  SoundPause();
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  // -------------------------------------------------------------------------------------------------
  // Creativision is a bit special... if cassette button pressed we try to load a .BAS file directly.
  // -------------------------------------------------------------------------------------------------
  if (creativision_mode)
  {
      DSPrint(5,0,0, "BAS LOADING");
      creativision_loadBAS();
      WAITVBL;WAITVBL;WAITVBL;WAITVBL;
      BufferKeys("RUN");
      BufferKey(KBD_KEY_RET);
      DSPrint(5,0,0, "           ");
      SoundUnPause();
      return;
  }

  // --------------------------------------------------------------------------------------------
  // Otherwise we are showing the cassette menu based on the current machine being emulated...
  // --------------------------------------------------------------------------------------------
  CassetteMenuShow(true, menuSelection);

  u8 bExitMenu = false;
  while (true)
  {
    nds_key = keysCurrent();
    if (nds_key)
    {
        if (nds_key & KEY_UP)
        {
            menuSelection = (menuSelection > 0) ? (menuSelection-1):(cassette_menu_items-1);
            CassetteMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_DOWN)
        {
            menuSelection = (menuSelection+1) % cassette_menu_items;
            CassetteMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_A)    // User has picked a menu item... let's see what it is!
        {
            switch(menu->menulist[menuSelection].menu_action)
            {
                case MENU_ACTION_EXIT:
                    bExitMenu = true;
                    break;
                    
                case MENU_ACTION_SAVE:
                    if  (showMessage("DO YOU REALLY WANT TO","WRITE CASSETTE/DISK DATA?") == ID_SHM_YES)
                    {
                        if (adam_mode)
                        {
                            SaveAdamTapeOrDisk(0);
                        }
                        else if (einstein_mode)
                        {
                            DSPrint(12,0,6, "SAVING");
                            einstein_save_disk(0);
                            WAITVBL;WAITVBL;
                            DSPrint(12,0,6, "      ");
                            bExitMenu = true;
                        }
                        else
                        {
                            if (msx_mode || svi_mode)   // Not supporting Memotech MTX yet...
                            {
                                DSPrint(12,0,6, "SAVING");
                                FILE *fp;
                                fp = fopen(gpFic[ucGameChoice].szName, "wb");
                                fwrite(ROM_Memory, tape_len, 1, fp);
                                fclose(fp);
                                WAITVBL;WAITVBL;
                                DSPrint(12,0,6, "      ");
                                DisplayStatusLine(true);
                            }
                        }
                    }
                    CassetteMenuShow(true, menuSelection);
                    break;
                    
                case MENU_ACTION_SAVE1:
                    if (adam_mode)
                    {
                        SaveAdamTapeOrDisk(1);
                    }
                    else if (einstein_mode)
                    {
                        if  (showMessage("DO YOU REALLY WANT TO","WRITE CASSETTE/DISK DATA?") == ID_SHM_YES)
                        {
                            DSPrint(10,0,6, "SAVING");
                            einstein_save_disk(1);
                            WAITVBL;WAITVBL;
                            DSPrint(10,0,6, "      ");
                            bExitMenu = true;
                        }
                        CassetteMenuShow(true, menuSelection);
                    }
                    break;

                case MENU_ACTION_INST_RAMDISK:
                    if (einstein_mode)
                    {
                        einstein_install_ramdisk();
                        bExitMenu = true;
                    }
                    break;
                    
                case MENU_ACTION_INIT_RAMDISK:
                    if (einstein_mode)
                    {
                        if  (showMessage("DO YOU REALLY WANT TO","INITIALIZE THE RAMDISK?") == ID_SHM_YES)
                        {
                            DSPrint(10,0,6, "ERASING");
                            einstein_init_ramdisk();
                            einstein_load_disk(1);
                            WAITVBL;WAITVBL;
                            DSPrint(10,0,6, "       ");
                            bExitMenu = true;
                        }
                        CassetteMenuShow(true, menuSelection);
                    }
                    break;
                    
                case MENU_ACTION_SWAP:
                    colecoDSLoadFile();
                    if (ucGameChoice >= 0)
                    {
                        if (adam_mode)
                        {
                            DigitalDataInsert(0, gpFic[ucGameChoice].szName);
                        }
                        else if (einstein_mode)
                        {
                            einstein_swap_disk(0, gpFic[ucGameChoice].szName);
                        }
                        else
                        {
                            CassetteInsert(gpFic[ucGameChoice].szName);
                        }
                        bExitMenu = true;
                    }
                    else
                    {
                        CassetteMenuShow(true, menuSelection);
                    }
                    break;

                case MENU_ACTION_SWAP1:
                    colecoDSLoadFile();
                    if (ucGameChoice >= 0)
                    {
                        // ADAM and Einstein both support two drives... the MSX at 720K only supports one
                        if (adam_mode)
                        {
                            DigitalDataInsert(1, gpFic[ucGameChoice].szName);
                        }
                        else if (einstein_mode)
                        {
                            einstein_swap_disk(1, gpFic[ucGameChoice].szName);
                        }
                        bExitMenu = true;
                    }
                    else
                    {
                        CassetteMenuShow(true, menuSelection);
                    }
                    break;
                    
                case MENU_ACTION_REWIND:
                    if (tape_pos>0)
                    {
                        tape_pos = 0;
                        DSPrint(12,0,6, "REWOUND");
                        WAITVBL;WAITVBL;WAITVBL;WAITVBL;
                        DSPrint(12,0,6, "       ");
                        DisplayStatusLine(true);
                        CassetteMenuShow(true, menuSelection);
                    }
                    break;
                    
                case MENU_ACTION_CLOAD_RUN:
                    BufferKeys("CLOAD");
                    BufferKey(KBD_KEY_RET);
                    BufferKey(255);
                    BufferKeys("RUN");
                    BufferKey(KBD_KEY_RET);
                    bExitMenu = true;
                    break;
                    
                case MENU_ACTION_BLOAD_CAS:
                    BufferKeys("BLOAD");
                    BufferKey(KBD_KEY_SHIFT);
                    BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                    BufferKeys("CAS");
                    if (msx_mode && !msx_japanese_matrix) BufferKey(KBD_KEY_SHIFT);
                    BufferKey(msx_japanese_matrix ? KBD_KEY_QUOTE : ':');
                    BufferKey(KBD_KEY_SHIFT);
                    BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                    BufferKey(',');
                    BufferKey('R');
                    BufferKey(KBD_KEY_RET);
                    bExitMenu = true;
                    break;
                    
                case MENU_ACTION_RUN_CAS:
                    BufferKeys("RUN");
                    BufferKey(KBD_KEY_SHIFT);
                    BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                    BufferKeys("CAS");
                    if (msx_mode && !msx_japanese_matrix) BufferKey(KBD_KEY_SHIFT);
                    BufferKey(msx_japanese_matrix ? KBD_KEY_QUOTE : ':');
                    BufferKey(KBD_KEY_SHIFT);
                    BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                    BufferKey(KBD_KEY_RET);
                    bExitMenu = true;
                    break;
                    
                case MENU_ACTION_LOAD:
                    BufferKeys("LOAD");
                    BufferKey(KBD_KEY_SHIFT);
                    BufferKey('2');
                    BufferKey(KBD_KEY_SHIFT);
                    BufferKey('2');
                    BufferKey(KBD_KEY_RET);
                    bExitMenu = true;
                    break;
                    
                case MENU_ACTION_RUN:
                    BufferKeys("RUN");
                    BufferKey(KBD_KEY_RET);
                    bExitMenu = true;
                    break;

                case MENU_ACTION_RUN_EIN:
                    einstein_load_com_file();
                    bExitMenu = true;
                    break;
                    
                case MENU_ACTION_RUN_MTX:
                    memotech_launch_run_file();
                    bExitMenu = true;
                    break;
            }
        }
        if (nds_key & KEY_B)
        {
            bExitMenu = true;
        }

        if (bExitMenu) break;
        while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
        WAITVBL;WAITVBL;
    }
  }

  while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
  WAITVBL;WAITVBL;

  BottomScreenKeypad();  // Could be generic or overlay...

  SoundUnPause();
}



// ------------------------------------------------------------------------
// Show the Mini Menu - highlight the selected row.
// ------------------------------------------------------------------------
u8 mini_menu_items = 0;
void MiniMenuShow(bool bClearScreen, u8 sel)
{
    mini_menu_items = 0;
    if (bClearScreen)
    {
      // ---------------------------------------------------
      // Put up a generic background for this mini-menu...
      // ---------------------------------------------------
      BottomScreenOptions();
    }

    DSPrint(8,7,6,                                           " DS MINI MENU  ");
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " RESET  GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " QUIT   GAME   ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " HIGH   SCORE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " SAVE   STATE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " LOAD   STATE  ");  mini_menu_items++;
    DSPrint(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " EXIT   MENU   ");  mini_menu_items++;
}

// ------------------------------------------------------------------------
// Handle mini-menu interface...
// ------------------------------------------------------------------------
u8 MiniMenu(void)
{
  u8 retVal = MENU_CHOICE_NONE;
  u8 menuSelection = 0;

  SoundPause();
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  MiniMenuShow(true, menuSelection);

  while (true)
  {
    nds_key = keysCurrent();
    if (nds_key)
    {
        if (nds_key & KEY_UP)
        {
            menuSelection = (menuSelection > 0) ? (menuSelection-1):(mini_menu_items-1);
            MiniMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_DOWN)
        {
            menuSelection = (menuSelection+1) % mini_menu_items;
            MiniMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_A)
        {
            if      (menuSelection == 0) retVal = MENU_CHOICE_RESET_GAME;
            else if (menuSelection == 1) retVal = MENU_CHOICE_END_GAME;
            else if (menuSelection == 2) retVal = MENU_CHOICE_HI_SCORE;
            else if (menuSelection == 3) retVal = MENU_CHOICE_SAVE_GAME;
            else if (menuSelection == 4) retVal = MENU_CHOICE_LOAD_GAME;
            else if (menuSelection == 5) retVal = MENU_CHOICE_NONE;
            else retVal = MENU_CHOICE_NONE;
            break;
        }
        if (nds_key & KEY_B)
        {
            retVal = MENU_CHOICE_NONE;
            break;
        }

        while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
        WAITVBL;WAITVBL;
    }
  }

  while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
  WAITVBL;WAITVBL;

  BottomScreenKeypad();  // Could be generic or overlay...

  SoundUnPause();

  return retVal;
}


// ------------------------------------------------------------------------
// Return 1 if we are showing full keyboard... otherwise 0
// ------------------------------------------------------------------------
inline u8 IsFullKeyboard(void) {return ((myConfig.overlay == 1 || myConfig.overlay == 2) ? 1:0);}  // Full or Alpha keyboards report as 'full'

u8 last_special_key = 0;
u8 last_special_key_dampen = 0;
u8 last_kbd_key = 0;

u8 handle_adam_keyboard_press(u16 iTx, u16 iTy)
{
    if ((iTy >= 12) && (iTy < 42))        // Row 1 (top row with I-VI Smartkeys)
    {
        if      ((iTx >= 0)   && (iTx < 25))   kbd_key = ADAM_KEY_ESC;
        else if ((iTx >= 25)  && (iTx < 53))   kbd_key = ADAM_KEY_F1;
        else if ((iTx >= 53)  && (iTx < 81))   kbd_key = ADAM_KEY_F2;
        else if ((iTx >= 81)  && (iTx < 108))  kbd_key = ADAM_KEY_F3;
        else if ((iTx >= 108) && (iTx < 134))  kbd_key = ADAM_KEY_F4;
        else if ((iTx >= 134) && (iTx < 161))  kbd_key = ADAM_KEY_F5;
        else if ((iTx >= 161) && (iTx < 190))  kbd_key = ADAM_KEY_F6;
        else if ((iTx >= 190) && (iTx < 213))  kbd_key = (key_shift ? ADAM_KEY_STORE:ADAM_KEY_MOVE);
        else if ((iTx >= 213) && (iTx < 235))  kbd_key = ADAM_KEY_BS;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = ADAM_KEY_HOME;
    }
    else if ((iTy >= 42) && (iTy < 72))   // Row 2 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 15))   kbd_key = '\\';
        else if ((iTx >= 15)  && (iTx < 31))   kbd_key = '1';
        else if ((iTx >= 31)  && (iTx < 45))   kbd_key = '2';
        else if ((iTx >= 45)  && (iTx < 61))   kbd_key = '3';
        else if ((iTx >= 61)  && (iTx < 75))   kbd_key = '4';
        else if ((iTx >= 75)  && (iTx < 91))   kbd_key = '5';
        else if ((iTx >= 91)  && (iTx < 106))  kbd_key = '6';
        else if ((iTx >= 106) && (iTx < 121))  kbd_key = '7';
        else if ((iTx >= 121) && (iTx < 135))  kbd_key = '8';
        else if ((iTx >= 135) && (iTx < 151))  kbd_key = '9';
        else if ((iTx >= 151) && (iTx < 165))  kbd_key = '0';
        else if ((iTx >= 165) && (iTx < 181))  kbd_key = '-';
        else if ((iTx >= 181) && (iTx < 195))  kbd_key = '+';
        else if ((iTx >= 195) && (iTx < 210))  kbd_key = '^';
        else if ((iTx >= 210) && (iTx < 235))  kbd_key = ADAM_KEY_INS;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = ADAM_KEY_DEL;
    }
    else if ((iTy >= 72) && (iTy < 102))  // Row 3 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = ADAM_KEY_TAB;
        else if ((iTx >= 23)  && (iTx < 39))   kbd_key = 'Q';
        else if ((iTx >= 39)  && (iTx < 54))   kbd_key = 'W';
        else if ((iTx >= 54)  && (iTx < 69))   kbd_key = 'E';
        else if ((iTx >= 69)  && (iTx < 83))   kbd_key = 'R';
        else if ((iTx >= 83)  && (iTx < 99))   kbd_key = 'T';
        else if ((iTx >= 99)  && (iTx < 113))  kbd_key = 'Y';
        else if ((iTx >= 113) && (iTx < 129))  kbd_key = 'U';
        else if ((iTx >= 129) && (iTx < 143))  kbd_key = 'I';
        else if ((iTx >= 143) && (iTx < 158))  kbd_key = 'O';
        else if ((iTx >= 158) && (iTx < 174))  kbd_key = 'P';
        else if ((iTx >= 174) && (iTx < 189))  kbd_key = '[';
        else if ((iTx >= 189) && (iTx < 203))  kbd_key = ']';
        else if ((iTx >= 210) && (iTx < 235))  kbd_key = ADAM_KEY_UNDO;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = ADAM_KEY_CLEAR;
    }
    else if ((iTy >= 102) && (iTy < 132)) // Row 4 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 27))   {kbd_key = 0; last_kbd_key = 0; last_special_key = KBD_KEY_CTRL; DSPrint(4,0,6, "CTRL");}
        else if ((iTx >= 27)  && (iTx < 43))   kbd_key = 'A';
        else if ((iTx >= 43)  && (iTx < 58))   kbd_key = 'S';
        else if ((iTx >= 58)  && (iTx < 72))   kbd_key = 'D';
        else if ((iTx >= 72)  && (iTx < 87))   kbd_key = 'F';
        else if ((iTx >= 87)  && (iTx < 102))  kbd_key = 'G';
        else if ((iTx >= 102) && (iTx < 117))  kbd_key = 'H';
        else if ((iTx >= 117) && (iTx < 132))  kbd_key = 'J';
        else if ((iTx >= 132) && (iTx < 147))  kbd_key = 'K';
        else if ((iTx >= 147) && (iTx < 161))  kbd_key = 'L';
        else if ((iTx >= 161) && (iTx < 178))  kbd_key = ';';
        else if ((iTx >= 178) && (iTx < 192))  kbd_key = ADAM_KEY_QUOTE;
        else if ((iTx >= 192) && (iTx < 214))  kbd_key = ADAM_KEY_ENTER;
        else if ((iTx >= 214) && (iTx < 235))  kbd_key = ADAM_KEY_UP;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = ADAM_KEY_DOWN;
    }
    else if ((iTy >= 132) && (iTy < 162)) // Row 5 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 33))   {kbd_key = 0;  last_kbd_key = 0; last_special_key = KBD_KEY_SHIFT; DSPrint(4,0,6, "SHFT");}
        else if ((iTx >= 33)  && (iTx < 49))   kbd_key = 'Z';
        else if ((iTx >= 49)  && (iTx < 64))   kbd_key = 'X';
        else if ((iTx >= 64)  && (iTx < 78))   kbd_key = 'C';
        else if ((iTx >= 78)  && (iTx < 94))   kbd_key = 'V';
        else if ((iTx >= 94)  && (iTx < 109))  kbd_key = 'B';
        else if ((iTx >= 109) && (iTx < 123))  kbd_key = 'N';
        else if ((iTx >= 123) && (iTx < 139))  kbd_key = 'M';
        else if ((iTx >= 139) && (iTx < 154))  kbd_key = ',';
        else if ((iTx >= 154) && (iTx < 169))  kbd_key = '.';
        else if ((iTx >= 169) && (iTx < 184))  kbd_key = '/';
        else if ((iTx >= 184) && (iTx < 214))  kbd_key = ADAM_KEY_ENTER;
        else if ((iTx >= 214) && (iTx < 235))  kbd_key = ADAM_KEY_LEFT;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = ADAM_KEY_RIGHT;
    }
    else if ((iTy >= 162) && (iTy < 192)) // Row 6 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 33))   {if (last_kbd_key != 255) adam_CapsLock = 1-adam_CapsLock; last_kbd_key=255;}
        else if ((iTx >= 33)  && (iTx < 165))  kbd_key = ' ';
        else if ((iTx >= 165) && (iTx < 195))  return MENU_CHOICE_SWAP_KBD;
        else if ((iTx >= 195) && (iTx < 225))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 225) && (iTx < 255))  return MENU_CHOICE_MENU;
    }
    else {kbd_key = 0; last_kbd_key = 0;}

    if (kbd_key != last_kbd_key && (kbd_key != 0) && (last_kbd_key != 255))
    {
        if (last_special_key == KBD_KEY_CTRL)
        {
            PutKBD(CON_CONTROL | kbd_key | (((adam_CapsLock && (kbd_key >= 'A') && (kbd_key <= 'Z')) || key_shift) ? CON_SHIFT:0));
        }
        else if (last_special_key == KBD_KEY_SHIFT)
        {
            PutKBD(CON_SHIFT | kbd_key);
        }
        else
        {
            PutKBD(kbd_key | (((adam_CapsLock && (kbd_key >= 'A') && (kbd_key <= 'Z')) || key_shift) ? CON_SHIFT:0));
        }
        last_special_key = 0;
        if (!myConfig.keyMute) mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
        DSPrint(4,0,6, "    ");
    }
    if (last_kbd_key != 255) last_kbd_key = kbd_key;

    return MENU_CHOICE_NONE;
}


u8 handle_msx_keyboard_press(u16 iTx, u16 iTy)  // MSX Keyboard
{
    static u8 bKanaShown = 0;
    
    if ((iTx > 212) && (iTy >= 102) && (iTy < 162))  // Triangular Arrow Keys... do our best
    {
        if      (iTy < 120)   kbd_key = KBD_KEY_UP;
        else if (iTy > 145)   kbd_key = KBD_KEY_DOWN;
        else if (iTx < 234)   kbd_key = KBD_KEY_LEFT;
        else                  kbd_key = KBD_KEY_RIGHT;
    }
    else if ((iTy >= 12) && (iTy < 42))    // Row 1 (top row with Function Keys)
    {
        if      ((iTx >= 0)   && (iTx < 22))   kbd_key = KBD_KEY_ESC;
        else if ((iTx >= 22)  && (iTx < 44))   kbd_key = KBD_KEY_HOME;
        else if ((iTx >= 44)  && (iTx < 73))   kbd_key = KBD_KEY_F1;
        else if ((iTx >= 73)  && (iTx < 102))  kbd_key = KBD_KEY_F2;
        else if ((iTx >= 102) && (iTx < 131))  kbd_key = KBD_KEY_F3;
        else if ((iTx >= 131) && (iTx < 160))  kbd_key = KBD_KEY_F4;
        else if ((iTx >= 160) && (iTx < 190))  kbd_key = KBD_KEY_F5;
        else if ((iTx >= 190) && (iTx < 212))  kbd_key = KBD_KEY_BS;
        else if ((iTx >= 212) && (iTx < 235))  kbd_key = KBD_KEY_INS;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = KBD_KEY_DEL;
    }
    else if ((iTy >= 42) && (iTy < 72))   // Row 2 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 15))   kbd_key = (msx_japanese_matrix ? '[' : '`');
        else if ((iTx >= 15)  && (iTx < 31))   kbd_key = '1';
        else if ((iTx >= 31)  && (iTx < 45))   kbd_key = '2';
        else if ((iTx >= 45)  && (iTx < 61))   kbd_key = '3';
        else if ((iTx >= 61)  && (iTx < 75))   kbd_key = '4';
        else if ((iTx >= 75)  && (iTx < 91))   kbd_key = '5';
        else if ((iTx >= 91)  && (iTx < 106))  kbd_key = '6';
        else if ((iTx >= 106) && (iTx < 121))  kbd_key = '7';
        else if ((iTx >= 121) && (iTx < 135))  kbd_key = '8';
        else if ((iTx >= 135) && (iTx < 151))  kbd_key = '9';
        else if ((iTx >= 151) && (iTx < 165))  kbd_key = '0';
        else if ((iTx >= 165) && (iTx < 181))  kbd_key = '-';
        else if ((iTx >= 181) && (iTx < 195))  kbd_key = '=';
        else if ((iTx >= 195) && (iTx < 210))  kbd_key = '\\';
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_SEL;
    }
    else if ((iTy >= 72) && (iTy < 102))  // Row 3 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = KBD_KEY_TAB;
        else if ((iTx >= 23)  && (iTx < 39))   kbd_key = 'Q';
        else if ((iTx >= 39)  && (iTx < 54))   kbd_key = 'W';
        else if ((iTx >= 54)  && (iTx < 69))   kbd_key = 'E';
        else if ((iTx >= 69)  && (iTx < 83))   kbd_key = 'R';
        else if ((iTx >= 83)  && (iTx < 99))   kbd_key = 'T';
        else if ((iTx >= 99)  && (iTx < 113))  kbd_key = 'Y';
        else if ((iTx >= 113) && (iTx < 129))  kbd_key = 'U';
        else if ((iTx >= 129) && (iTx < 143))  kbd_key = 'I';
        else if ((iTx >= 143) && (iTx < 158))  kbd_key = 'O';
        else if ((iTx >= 158) && (iTx < 174))  kbd_key = 'P';
        else if ((iTx >= 174) && (iTx < 189))  kbd_key = (msx_japanese_matrix ? ']' : '[');
        else if ((iTx >= 189) && (iTx < 203))  kbd_key = (msx_japanese_matrix ? '`' : ']');
        else if ((iTx >= 203) && (iTx < 214))  kbd_key = KBD_KEY_DEAD;
        else if ((iTx >= 214) && (iTx < 255))  kbd_key = KBD_KEY_STOP;
    }
    else if ((iTy >= 102) && (iTy < 132)) // Row 4 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 27))   {kbd_key = KBD_KEY_CTRL; last_special_key = KBD_KEY_CTRL; last_special_key_dampen = 20;}
        else if ((iTx >= 27)  && (iTx < 43))   kbd_key = 'A';
        else if ((iTx >= 43)  && (iTx < 58))   kbd_key = 'S';
        else if ((iTx >= 58)  && (iTx < 72))   kbd_key = 'D';
        else if ((iTx >= 72)  && (iTx < 87))   kbd_key = 'F';
        else if ((iTx >= 87)  && (iTx < 102))  kbd_key = 'G';
        else if ((iTx >= 102) && (iTx < 117))  kbd_key = 'H';
        else if ((iTx >= 117) && (iTx < 132))  kbd_key = 'J';
        else if ((iTx >= 132) && (iTx < 147))  kbd_key = 'K';
        else if ((iTx >= 147) && (iTx < 161))  kbd_key = 'L';
        else if ((iTx >= 161) && (iTx < 178))  kbd_key = (msx_japanese_matrix ? ';' : KBD_KEY_QUOTE);
        else if ((iTx >= 178) && (iTx < 192))  kbd_key = (msx_japanese_matrix ? KBD_KEY_QUOTE : ';');
        else if ((iTx >= 192) && (iTx < 214))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 132) && (iTy < 162)) // Row 5 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 33))   {kbd_key = KBD_KEY_SHIFT; last_special_key = KBD_KEY_SHIFT; last_special_key_dampen = 20;}
        else if ((iTx >= 33)  && (iTx < 49))   kbd_key = 'Z';
        else if ((iTx >= 49)  && (iTx < 64))   kbd_key = 'X';
        else if ((iTx >= 64)  && (iTx < 78))   kbd_key = 'C';
        else if ((iTx >= 78)  && (iTx < 94))   kbd_key = 'V';
        else if ((iTx >= 94)  && (iTx < 109))  kbd_key = 'B';
        else if ((iTx >= 109) && (iTx < 123))  kbd_key = 'N';
        else if ((iTx >= 123) && (iTx < 139))  kbd_key = 'M';
        else if ((iTx >= 139) && (iTx < 154))  kbd_key = ',';
        else if ((iTx >= 154) && (iTx < 169))  kbd_key = '.';
        else if ((iTx >= 169) && (iTx < 184))  kbd_key = '/';
        else if ((iTx >= 184) && (iTx < 214))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 162) && (iTy < 192)) // Row 6 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 30))   kbd_key = KBD_KEY_CAPS;
        else if ((iTx >= 30)  && (iTx < 53))   {kbd_key = KBD_KEY_GRAPH; last_special_key = KBD_KEY_GRAPH; last_special_key_dampen = 20;}
        else if ((iTx >= 53)  && (iTx < 163))  kbd_key = ' ';
        else if ((iTx >= 163) && (iTx < 192))  {kbd_key = KBD_KEY_CODE; if (msx_japanese_matrix) {DSPrint(4,0,6,"KANA"); bKanaShown=1;} else {last_special_key = KBD_KEY_CODE; last_special_key_dampen = 20;}}
        else if ((iTx >= 192) && (iTx < 225))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 225) && (iTx < 255))  return MENU_CHOICE_MENU;
    }
    
    if ((kbd_key != 0) && (kbd_key != KBD_KEY_CODE) && bKanaShown)
    {
        DSPrint(4,0,6,"    ");
        bKanaShown = 0;
    }

    return MENU_CHOICE_NONE;
}

u8 handle_svi_keyboard_press(u16 iTx, u16 iTy)  // SVI Keyboard
{
    static u8 bKanaShown = 0;
    
    if ((iTx > 212) && (iTy >= 102) && (iTy < 162))  // Triangular Arrow Keys... do our best
    {
        if      (iTy < 120)   kbd_key = KBD_KEY_UP;
        else if (iTy > 145)   kbd_key = KBD_KEY_DOWN;
        else if (iTx < 234)   kbd_key = KBD_KEY_LEFT;
        else                  kbd_key = KBD_KEY_RIGHT;
    }
    else if ((iTy >= 12) && (iTy < 42))    // Row 1 (top row with Function Keys)
    {
        if      ((iTx >= 0)   && (iTx < 22))   kbd_key = KBD_KEY_ESC;
        else if ((iTx >= 22)  && (iTx < 44))   kbd_key = KBD_KEY_HOME;
        else if ((iTx >= 44)  && (iTx < 73))   kbd_key = KBD_KEY_F1;
        else if ((iTx >= 73)  && (iTx < 102))  kbd_key = KBD_KEY_F2;
        else if ((iTx >= 102) && (iTx < 131))  kbd_key = KBD_KEY_F3;
        else if ((iTx >= 131) && (iTx < 160))  kbd_key = KBD_KEY_F4;
        else if ((iTx >= 160) && (iTx < 190))  kbd_key = KBD_KEY_F5;
        else if ((iTx >= 190) && (iTx < 212))  kbd_key = KBD_KEY_BS;
        else if ((iTx >= 212) && (iTx < 235))  kbd_key = KBD_KEY_INS;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = KBD_KEY_DEL;
    }
    else if ((iTy >= 42) && (iTy < 72))   // Row 2 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 15))   kbd_key = 0;
        else if ((iTx >= 15)  && (iTx < 31))   kbd_key = '1';
        else if ((iTx >= 31)  && (iTx < 45))   kbd_key = '2';
        else if ((iTx >= 45)  && (iTx < 61))   kbd_key = '3';
        else if ((iTx >= 61)  && (iTx < 75))   kbd_key = '4';
        else if ((iTx >= 75)  && (iTx < 91))   kbd_key = '5';
        else if ((iTx >= 91)  && (iTx < 106))  kbd_key = '6';
        else if ((iTx >= 106) && (iTx < 121))  kbd_key = '7';
        else if ((iTx >= 121) && (iTx < 135))  kbd_key = '8';
        else if ((iTx >= 135) && (iTx < 151))  kbd_key = '9';
        else if ((iTx >= 151) && (iTx < 165))  kbd_key = '0';
        else if ((iTx >= 165) && (iTx < 181))  kbd_key = '-';
        else if ((iTx >= 181) && (iTx < 195))  kbd_key = '=';
        else if ((iTx >= 195) && (iTx < 210))  kbd_key = '`';
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_SEL;
    }
    else if ((iTy >= 72) && (iTy < 102))  // Row 3 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = KBD_KEY_TAB;
        else if ((iTx >= 23)  && (iTx < 39))   kbd_key = 'Q';
        else if ((iTx >= 39)  && (iTx < 54))   kbd_key = 'W';
        else if ((iTx >= 54)  && (iTx < 69))   kbd_key = 'E';
        else if ((iTx >= 69)  && (iTx < 83))   kbd_key = 'R';
        else if ((iTx >= 83)  && (iTx < 99))   kbd_key = 'T';
        else if ((iTx >= 99)  && (iTx < 113))  kbd_key = 'Y';
        else if ((iTx >= 113) && (iTx < 129))  kbd_key = 'U';
        else if ((iTx >= 129) && (iTx < 143))  kbd_key = 'I';
        else if ((iTx >= 143) && (iTx < 158))  kbd_key = 'O';
        else if ((iTx >= 158) && (iTx < 174))  kbd_key = 'P';
        else if ((iTx >= 174) && (iTx < 189))  kbd_key = '[';
        else if ((iTx >= 189) && (iTx < 203))  kbd_key = ']';
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_BREAK;
    }
    else if ((iTy >= 102) && (iTy < 132)) // Row 4 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 27))   {kbd_key = KBD_KEY_CTRL; last_special_key = KBD_KEY_CTRL; last_special_key_dampen = 20;}
        else if ((iTx >= 27)  && (iTx < 43))   kbd_key = 'A';
        else if ((iTx >= 43)  && (iTx < 58))   kbd_key = 'S';
        else if ((iTx >= 58)  && (iTx < 72))   kbd_key = 'D';
        else if ((iTx >= 72)  && (iTx < 87))   kbd_key = 'F';
        else if ((iTx >= 87)  && (iTx < 102))  kbd_key = 'G';
        else if ((iTx >= 102) && (iTx < 117))  kbd_key = 'H';
        else if ((iTx >= 117) && (iTx < 132))  kbd_key = 'J';
        else if ((iTx >= 132) && (iTx < 147))  kbd_key = 'K';
        else if ((iTx >= 147) && (iTx < 161))  kbd_key = 'L';
        else if ((iTx >= 161) && (iTx < 178))  kbd_key = ':';
        else if ((iTx >= 178) && (iTx < 192))  kbd_key = KBD_KEY_QUOTE;
        else if ((iTx >= 192) && (iTx < 214))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 132) && (iTy < 162)) // Row 5 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 33))   {kbd_key = KBD_KEY_SHIFT; last_special_key = KBD_KEY_SHIFT; last_special_key_dampen = 20;}
        else if ((iTx >= 33)  && (iTx < 49))   kbd_key = 'Z';
        else if ((iTx >= 49)  && (iTx < 64))   kbd_key = 'X';
        else if ((iTx >= 64)  && (iTx < 78))   kbd_key = 'C';
        else if ((iTx >= 78)  && (iTx < 94))   kbd_key = 'V';
        else if ((iTx >= 94)  && (iTx < 109))  kbd_key = 'B';
        else if ((iTx >= 109) && (iTx < 123))  kbd_key = 'N';
        else if ((iTx >= 123) && (iTx < 139))  kbd_key = 'M';
        else if ((iTx >= 139) && (iTx < 154))  kbd_key = ',';
        else if ((iTx >= 154) && (iTx < 169))  kbd_key = '.';
        else if ((iTx >= 169) && (iTx < 184))  kbd_key = '/';
        else if ((iTx >= 184) && (iTx < 214))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 162) && (iTy < 192)) // Row 6 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 30))   kbd_key = KBD_KEY_CAPS;
        else if ((iTx >= 30)  && (iTx < 53))   {kbd_key = KBD_KEY_GRAPH; last_special_key = KBD_KEY_GRAPH; last_special_key_dampen = 20;}
        else if ((iTx >= 53)  && (iTx < 156))  kbd_key = ' ';
        else if ((iTx >= 156) && (iTx < 180))  {kbd_key = KBD_KEY_CODE; last_special_key = KBD_KEY_CODE; last_special_key_dampen = 20;}
        else if ((iTx >= 180) && (iTx < 212))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 212) && (iTx < 255))  return MENU_CHOICE_MENU;
    }
    
    if ((kbd_key != 0) && (kbd_key != KBD_KEY_CODE) && bKanaShown)
    {
        DSPrint(4,0,6,"    ");
        bKanaShown = 0;
    }

    return MENU_CHOICE_NONE;
}

u8 handle_mtx_keyboard_press(u16 iTx, u16 iTy)  // MTX Keyboard
{
    if ((iTx > 212) && (iTy >= 102) && (iTy < 162))  // Triangular Arrow Keys... do our best
    {
        if      (iTy < 120)   kbd_key = KBD_KEY_UP;
        else if (iTy > 145)   kbd_key = KBD_KEY_DOWN;
        else if (iTx < 234)   kbd_key = KBD_KEY_LEFT;
        else                  kbd_key = KBD_KEY_RIGHT;
    }
    else if ((iTy >= 12) && (iTy < 42))    // Row 1 (top row with F1 thru F8)
    {
        if      ((iTx >= 0)   && (iTx < 22))   kbd_key = KBD_KEY_ESC;
        else if ((iTx >= 22)  && (iTx < 49))   kbd_key = KBD_KEY_F1;
        else if ((iTx >= 49)  && (iTx < 75))   kbd_key = KBD_KEY_F2;
        else if ((iTx >= 75)  && (iTx < 101))  kbd_key = KBD_KEY_F3;
        else if ((iTx >= 101) && (iTx < 127))  kbd_key = KBD_KEY_F4;
        else if ((iTx >= 127) && (iTx < 153))  kbd_key = KBD_KEY_F5;
        else if ((iTx >= 153) && (iTx < 180))  kbd_key = KBD_KEY_F6;
        else if ((iTx >= 180) && (iTx < 205))  kbd_key = KBD_KEY_F7;
        else if ((iTx >= 205) && (iTx < 232))  kbd_key = KBD_KEY_F8;
        else if ((iTx >= 232) && (iTx < 255))  kbd_key = KBD_KEY_HOME;
    }
    else if ((iTy >= 42) && (iTy < 72))   // Row 2 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 15))   kbd_key = '\\';
        else if ((iTx >= 15)  && (iTx < 31))   kbd_key = '1';
        else if ((iTx >= 31)  && (iTx < 45))   kbd_key = '2';
        else if ((iTx >= 45)  && (iTx < 61))   kbd_key = '3';
        else if ((iTx >= 61)  && (iTx < 75))   kbd_key = '4';
        else if ((iTx >= 75)  && (iTx < 91))   kbd_key = '5';
        else if ((iTx >= 91)  && (iTx < 106))  kbd_key = '6';
        else if ((iTx >= 106) && (iTx < 121))  kbd_key = '7';
        else if ((iTx >= 121) && (iTx < 135))  kbd_key = '8';
        else if ((iTx >= 135) && (iTx < 151))  kbd_key = '9';
        else if ((iTx >= 151) && (iTx < 165))  kbd_key = '0';
        else if ((iTx >= 165) && (iTx < 181))  kbd_key = '-';
        else if ((iTx >= 181) && (iTx < 195))  kbd_key = '^';
        else if ((iTx >= 195) && (iTx < 210))  kbd_key = '@';
        else if ((iTx >= 210) && (iTx < 235))  kbd_key = KBD_KEY_BS;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = KBD_KEY_DEL;
    }
    else if ((iTy >= 72) && (iTy < 102))  // Row 3 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   kbd_key = KBD_KEY_TAB;
        else if ((iTx >= 23)  && (iTx < 39))   kbd_key = 'Q';
        else if ((iTx >= 39)  && (iTx < 54))   kbd_key = 'W';
        else if ((iTx >= 54)  && (iTx < 69))   kbd_key = 'E';
        else if ((iTx >= 69)  && (iTx < 83))   kbd_key = 'R';
        else if ((iTx >= 83)  && (iTx < 99))   kbd_key = 'T';
        else if ((iTx >= 99)  && (iTx < 113))  kbd_key = 'Y';
        else if ((iTx >= 113) && (iTx < 129))  kbd_key = 'U';
        else if ((iTx >= 129) && (iTx < 143))  kbd_key = 'I';
        else if ((iTx >= 143) && (iTx < 158))  kbd_key = 'O';
        else if ((iTx >= 158) && (iTx < 174))  kbd_key = 'P';
        else if ((iTx >= 174) && (iTx < 189))  kbd_key = '[';
        else if ((iTx >= 189) && (iTx < 203))  kbd_key = ']';
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_BREAK;
    }
    else if ((iTy >= 102) && (iTy < 132)) // Row 4 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 27))   {kbd_key = KBD_KEY_CTRL; last_special_key = KBD_KEY_CTRL; last_special_key_dampen = 20;}
        else if ((iTx >= 27)  && (iTx < 43))   kbd_key = 'A';
        else if ((iTx >= 43)  && (iTx < 58))   kbd_key = 'S';
        else if ((iTx >= 58)  && (iTx < 72))   kbd_key = 'D';
        else if ((iTx >= 72)  && (iTx < 87))   kbd_key = 'F';
        else if ((iTx >= 87)  && (iTx < 102))  kbd_key = 'G';
        else if ((iTx >= 102) && (iTx < 117))  kbd_key = 'H';
        else if ((iTx >= 117) && (iTx < 132))  kbd_key = 'J';
        else if ((iTx >= 132) && (iTx < 147))  kbd_key = 'K';
        else if ((iTx >= 147) && (iTx < 161))  kbd_key = 'L';
        else if ((iTx >= 161) && (iTx < 178))  kbd_key = ';';
        else if ((iTx >= 178) && (iTx < 192))  kbd_key = ':';
        else if ((iTx >= 192) && (iTx < 214))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 132) && (iTy < 162)) // Row 5 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 33))   {kbd_key = KBD_KEY_SHIFT; last_special_key = KBD_KEY_SHIFT; last_special_key_dampen = 20;}
        else if ((iTx >= 33)  && (iTx < 49))   kbd_key = 'Z';
        else if ((iTx >= 49)  && (iTx < 64))   kbd_key = 'X';
        else if ((iTx >= 64)  && (iTx < 78))   kbd_key = 'C';
        else if ((iTx >= 78)  && (iTx < 94))   kbd_key = 'V';
        else if ((iTx >= 94)  && (iTx < 109))  kbd_key = 'B';
        else if ((iTx >= 109) && (iTx < 123))  kbd_key = 'N';
        else if ((iTx >= 123) && (iTx < 139))  kbd_key = 'M';
        else if ((iTx >= 139) && (iTx < 154))  kbd_key = ',';
        else if ((iTx >= 154) && (iTx < 169))  kbd_key = '.';
        else if ((iTx >= 169) && (iTx < 184))  kbd_key = '/';
        else if ((iTx >= 184) && (iTx < 214))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 162) && (iTy < 192)) // Row 6 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 30))   kbd_key = KBD_KEY_CAPS;
        else if ((iTx >= 30)  && (iTx < 190))  kbd_key = ' ';
        else if ((iTx >= 190) && (iTx < 225))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 225) && (iTx < 255))  return MENU_CHOICE_MENU;
    }

    return MENU_CHOICE_NONE;
}

u8 handle_sordm5_keyboard_press(u16 iTx, u16 iTy)  // Sord M5 Keyboard
{
    if ((iTy >= 44) && (iTy < 74))   // Row 2 (number row)
    {
        if      ((iTx >= 4)   && (iTx < 23))   kbd_key = '1';
        else if ((iTx >= 23)  && (iTx < 42))   kbd_key = '2';
        else if ((iTx >= 42)  && (iTx < 61))   kbd_key = '3';
        else if ((iTx >= 61)  && (iTx < 81))   kbd_key = '4';
        else if ((iTx >= 81)  && (iTx < 99))   kbd_key = '5';
        else if ((iTx >= 99)  && (iTx < 118))  kbd_key = '6';
        else if ((iTx >= 118) && (iTx < 137))  kbd_key = '7';
        else if ((iTx >= 137) && (iTx < 156))  kbd_key = '8';
        else if ((iTx >= 156) && (iTx < 175))  kbd_key = '9';
        else if ((iTx >= 175) && (iTx < 194))  kbd_key = '0';
        else if ((iTx >= 194) && (iTx < 213))  kbd_key = '-';
        else if ((iTx >= 213) && (iTx < 232))  kbd_key = '^';
        else if ((iTx >= 232) && (iTx < 255))  kbd_key = '\\';
    }
    else if ((iTy >= 74) && (iTy < 104))  // Row 3 (QWERTY row)
    {
        if      ((iTx >= 8)   && (iTx < 26))   kbd_key = 'Q';
        else if ((iTx >= 26)  && (iTx < 45))   kbd_key = 'W';
        else if ((iTx >= 45)  && (iTx < 64))   kbd_key = 'E';
        else if ((iTx >= 64)  && (iTx < 83))   kbd_key = 'R';
        else if ((iTx >= 84)  && (iTx < 102))  kbd_key = 'T';
        else if ((iTx >= 102) && (iTx < 121))  kbd_key = 'Y';
        else if ((iTx >= 121) && (iTx < 140))  kbd_key = 'U';
        else if ((iTx >= 140) && (iTx < 159))  kbd_key = 'I';
        else if ((iTx >= 159) && (iTx < 178))  kbd_key = 'O';
        else if ((iTx >= 178) && (iTx < 197))  kbd_key = 'P';
        else if ((iTx >= 197) && (iTx < 216))  kbd_key = '@';
        else if ((iTx >= 216) && (iTx < 235))  kbd_key = '[';
    }
    else if ((iTy >= 104) && (iTy < 134)) // Row 4 (ASDF row)
    {
        if      ((iTx >= 14)  && (iTx < 32))   kbd_key = 'A';
        else if ((iTx >= 32)  && (iTx < 51))   kbd_key = 'S';
        else if ((iTx >= 51)  && (iTx < 70))   kbd_key = 'D';
        else if ((iTx >= 70)  && (iTx < 89))   kbd_key = 'F';
        else if ((iTx >= 89)  && (iTx < 108))  kbd_key = 'G';
        else if ((iTx >= 108) && (iTx < 127))  kbd_key = 'H';
        else if ((iTx >= 127) && (iTx < 146))  kbd_key = 'J';
        else if ((iTx >= 146) && (iTx < 165))  kbd_key = 'K';
        else if ((iTx >= 165) && (iTx < 184))  kbd_key = 'L';
        else if ((iTx >= 184) && (iTx < 203))  kbd_key = ';';
        else if ((iTx >= 203) && (iTx < 222))  kbd_key = ':';
        else if ((iTx >= 222) && (iTx < 241))  kbd_key = ']';
    }
    else if ((iTy >= 134) && (iTy < 164)) // Row 5 (ZXCV row)
    {
        if      ((iTx >= 18)  && (iTx < 37))   kbd_key = 'Z';
        else if ((iTx >= 37)  && (iTx < 56))   kbd_key = 'X';
        else if ((iTx >= 56)  && (iTx < 75))   kbd_key = 'C';
        else if ((iTx >= 75)  && (iTx < 94))   kbd_key = 'V';
        else if ((iTx >= 94)  && (iTx < 113))  kbd_key = 'B';
        else if ((iTx >= 113) && (iTx < 132))  kbd_key = 'N';
        else if ((iTx >= 132) && (iTx < 151))  kbd_key = 'M';
        else if ((iTx >= 151) && (iTx < 170))  kbd_key = ',';
        else if ((iTx >= 170) && (iTx < 189))  kbd_key = '.';
        else if ((iTx >= 189) && (iTx < 208))  kbd_key = '/';
        else if ((iTx >= 208) && (iTx < 227))  kbd_key = '_';
        else if ((iTx >= 227) && (iTx < 255))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 164) && (iTy < 192)) // Row 6 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 32))  {kbd_key = KBD_KEY_SHIFT; last_special_key = KBD_KEY_SHIFT; last_special_key_dampen = 20;}
        else if ((iTx >= 32)  && (iTx < 64))  {kbd_key = KBD_KEY_CTRL; last_special_key = KBD_KEY_CTRL; last_special_key_dampen = 20;}
        else if ((iTx >= 64)  && (iTx < 96))  {kbd_key = KBD_KEY_CODE; last_special_key = KBD_KEY_CODE; last_special_key_dampen = 20;}
        else if ((iTx >= 96)  && (iTx < 190))  kbd_key = ' ';
        else if ((iTx >= 190) && (iTx < 222))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 222) && (iTx < 255))  return MENU_CHOICE_MENU;
    }

    return MENU_CHOICE_NONE;
}

u8 handle_sc3000_keyboard_press(u16 iTx, u16 iTy)  // SC-3000 Keyboard
{
    if ((iTx > 212) && (iTy >= 102) && (iTy < 162))  // Triangular Arrow Keys... do our best
    {
        if      (iTy < 120)   kbd_key = KBD_KEY_UP;
        else if (iTy > 145)   kbd_key = KBD_KEY_DOWN;
        else if (iTx < 234)   kbd_key = KBD_KEY_LEFT;
        else                  kbd_key = KBD_KEY_RIGHT;
    }
    else if ((iTy >= 12) && (iTy < 42))    // Row 1 (top row is mostly blank for the SC-3000... just the HOME key)
    {
        if      ((iTx >= 0)   && (iTx < 22))   kbd_key = 0;
        else if ((iTx >= 22)  && (iTx < 49))   kbd_key = 0;
        else if ((iTx >= 49)  && (iTx < 75))   kbd_key = 0;
        else if ((iTx >= 75)  && (iTx < 101))  kbd_key = 0;
        else if ((iTx >= 101) && (iTx < 127))  kbd_key = 0;
        else if ((iTx >= 127) && (iTx < 153))  kbd_key = 0;
        else if ((iTx >= 153) && (iTx < 180))  kbd_key = 0;
        else if ((iTx >= 180) && (iTx < 205))  kbd_key = 0;
        else if ((iTx >= 205) && (iTx < 232))  kbd_key = 0;
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_HOME;
    }
    else if ((iTy >= 42) && (iTy < 72))   // Row 2 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 15))   kbd_key = '`'; // Repurpose to PII
        else if ((iTx >= 15)  && (iTx < 31))   kbd_key = '1';
        else if ((iTx >= 31)  && (iTx < 45))   kbd_key = '2';
        else if ((iTx >= 45)  && (iTx < 61))   kbd_key = '3';
        else if ((iTx >= 61)  && (iTx < 75))   kbd_key = '4';
        else if ((iTx >= 75)  && (iTx < 91))   kbd_key = '5';
        else if ((iTx >= 91)  && (iTx < 106))  kbd_key = '6';
        else if ((iTx >= 106) && (iTx < 121))  kbd_key = '7';
        else if ((iTx >= 121) && (iTx < 135))  kbd_key = '8';
        else if ((iTx >= 135) && (iTx < 151))  kbd_key = '9';
        else if ((iTx >= 151) && (iTx < 165))  kbd_key = '0';
        else if ((iTx >= 165) && (iTx < 181))  kbd_key = '-';
        else if ((iTx >= 181) && (iTx < 195))  kbd_key = '^';
        else if ((iTx >= 195) && (iTx < 210))  kbd_key = '\\'; // British Pound 
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_DEL;
    }
    else if ((iTy >= 72) && (iTy < 102))  // Row 3 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 23))   {kbd_key = KBD_KEY_CODE; last_special_key = KBD_KEY_CODE; last_special_key_dampen = 20;}
        else if ((iTx >= 23)  && (iTx < 39))   kbd_key = 'Q';
        else if ((iTx >= 39)  && (iTx < 54))   kbd_key = 'W';
        else if ((iTx >= 54)  && (iTx < 69))   kbd_key = 'E';
        else if ((iTx >= 69)  && (iTx < 83))   kbd_key = 'R';
        else if ((iTx >= 83)  && (iTx < 99))   kbd_key = 'T';
        else if ((iTx >= 99)  && (iTx < 113))  kbd_key = 'Y';
        else if ((iTx >= 113) && (iTx < 129))  kbd_key = 'U';
        else if ((iTx >= 129) && (iTx < 143))  kbd_key = 'I';
        else if ((iTx >= 143) && (iTx < 158))  kbd_key = 'O';
        else if ((iTx >= 158) && (iTx < 174))  kbd_key = 'P';
        else if ((iTx >= 174) && (iTx < 189))  kbd_key = '[';
        else if ((iTx >= 189) && (iTx < 203))  kbd_key = ']';
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_BREAK;
    }
    else if ((iTy >= 102) && (iTy < 132)) // Row 4 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 27))   {kbd_key = KBD_KEY_CTRL; last_special_key = KBD_KEY_CTRL; last_special_key_dampen = 20;}
        else if ((iTx >= 27)  && (iTx < 43))   kbd_key = 'A';
        else if ((iTx >= 43)  && (iTx < 58))   kbd_key = 'S';
        else if ((iTx >= 58)  && (iTx < 72))   kbd_key = 'D';
        else if ((iTx >= 72)  && (iTx < 87))   kbd_key = 'F';
        else if ((iTx >= 87)  && (iTx < 102))  kbd_key = 'G';
        else if ((iTx >= 102) && (iTx < 117))  kbd_key = 'H';
        else if ((iTx >= 117) && (iTx < 132))  kbd_key = 'J';
        else if ((iTx >= 132) && (iTx < 147))  kbd_key = 'K';
        else if ((iTx >= 147) && (iTx < 161))  kbd_key = 'L';
        else if ((iTx >= 161) && (iTx < 178))  kbd_key = ';';
        else if ((iTx >= 178) && (iTx < 192))  kbd_key = ':';
        else if ((iTx >= 192) && (iTx < 208))  kbd_key = '@';
    }
    else if ((iTy >= 132) && (iTy < 162)) // Row 5 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 31))   {kbd_key = KBD_KEY_SHIFT; last_special_key = KBD_KEY_SHIFT; last_special_key_dampen = 20;}
        else if ((iTx >= 31)  && (iTx < 47))   kbd_key = 'Z';
        else if ((iTx >= 47)  && (iTx < 62))   kbd_key = 'X';
        else if ((iTx >= 62)  && (iTx < 76))   kbd_key = 'C';
        else if ((iTx >= 76)  && (iTx < 92))   kbd_key = 'V';
        else if ((iTx >= 92)  && (iTx < 107))  kbd_key = 'B';
        else if ((iTx >= 107) && (iTx < 121))  kbd_key = 'N';
        else if ((iTx >= 121) && (iTx < 137))  kbd_key = 'M';
        else if ((iTx >= 137) && (iTx < 152))  kbd_key = ',';
        else if ((iTx >= 152) && (iTx < 167))  kbd_key = '.';
        else if ((iTx >= 167) && (iTx < 181))  kbd_key = '/';
        else if ((iTx >= 181) && (iTx < 214))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 162) && (iTy < 192)) // Row 6 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 30))   kbd_key = KBD_KEY_GRAPH;
        else if ((iTx >= 30)  && (iTx < 61))   {kbd_key = KBD_KEY_DIA; last_special_key = KBD_KEY_DIA; last_special_key_dampen = 20;}
        else if ((iTx >= 61)  && (iTx < 190))  kbd_key = ' ';
        else if ((iTx >= 180) && (iTx < 212))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 212) && (iTx < 255))  return MENU_CHOICE_MENU;
    }

    return MENU_CHOICE_NONE;
}


u8 handle_einstein_keyboard_press(u16 iTx, u16 iTy)  // Einstein Keyboard
{
    if ((iTy >= 12) && (iTy < 42))    // Row 1 (top row with F1 thru F8)
    {
        if      ((iTx >= 0)   && (iTx < 22))   kbd_key = KBD_KEY_ESC;
        else if ((iTx >= 22)  && (iTx < 44))   kbd_key = KBD_KEY_F8; // For the Einstein, this is F0
        else if ((iTx >= 44)  && (iTx < 68))   kbd_key = KBD_KEY_F1;
        else if ((iTx >= 68)  && (iTx < 90))   kbd_key = KBD_KEY_F2;
        else if ((iTx >= 90)  && (iTx < 112))  kbd_key = KBD_KEY_F3;
        else if ((iTx >= 112) && (iTx < 144))  kbd_key = KBD_KEY_F4;
        else if ((iTx >= 144) && (iTx < 168))  kbd_key = KBD_KEY_F5;
        else if ((iTx >= 168) && (iTx < 190))  kbd_key = KBD_KEY_F6;
        else if ((iTx >= 190) && (iTx < 212))  kbd_key = KBD_KEY_F7;
        else if ((iTx >= 212) && (iTx < 255))  kbd_key = 0;
    }
    else if ((iTy >= 42) && (iTy < 72))   // Row 2 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 15))   kbd_key = 0;
        else if ((iTx >= 15)  && (iTx < 31))   kbd_key = '1';
        else if ((iTx >= 31)  && (iTx < 45))   kbd_key = '2';
        else if ((iTx >= 45)  && (iTx < 61))   kbd_key = '3';
        else if ((iTx >= 61)  && (iTx < 75))   kbd_key = '4';
        else if ((iTx >= 75)  && (iTx < 91))   kbd_key = '5';
        else if ((iTx >= 91)  && (iTx < 106))  kbd_key = '6';
        else if ((iTx >= 106) && (iTx < 121))  kbd_key = '7';
        else if ((iTx >= 121) && (iTx < 135))  kbd_key = '8';
        else if ((iTx >= 135) && (iTx < 151))  kbd_key = '9';
        else if ((iTx >= 151) && (iTx < 165))  kbd_key = '0';
        else if ((iTx >= 165) && (iTx < 181))  kbd_key = '=';
        else if ((iTx >= 181) && (iTx < 195))  kbd_key = KBD_KEY_UP;
        else if ((iTx >= 195) && (iTx < 210))  kbd_key = '|';
        else if ((iTx >= 210) && (iTx < 235))  kbd_key = KBD_KEY_BS;
        else if ((iTx >= 235) && (iTx < 255))  kbd_key = KBD_KEY_LF;
    }
    else if ((iTy >= 72) && (iTy < 102))  // Row 3 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 25))   {kbd_key = KBD_KEY_CTRL; last_special_key = KBD_KEY_CTRL; last_special_key_dampen = 50;}
        else if ((iTx >= 25)  && (iTx < 40))   kbd_key = 'Q';
        else if ((iTx >= 40)  && (iTx < 55))   kbd_key = 'W';
        else if ((iTx >= 55)  && (iTx < 70))   kbd_key = 'E';
        else if ((iTx >= 70)  && (iTx < 85))   kbd_key = 'R';
        else if ((iTx >= 85)  && (iTx < 100))  kbd_key = 'T';
        else if ((iTx >= 100) && (iTx < 115))  kbd_key = 'Y';
        else if ((iTx >= 115) && (iTx < 130))  kbd_key = 'U';
        else if ((iTx >= 130) && (iTx < 145))  kbd_key = 'I';
        else if ((iTx >= 145) && (iTx < 160))  kbd_key = 'O';
        else if ((iTx >= 160) && (iTx < 175))  kbd_key = 'P';
        else if ((iTx >= 175) && (iTx < 190))  kbd_key = '_';
        else if ((iTx >= 190) && (iTx < 205))  kbd_key = KBD_KEY_LEFT;
        else if ((iTx >= 213) && (iTx < 255))  kbd_key = KBD_KEY_INS;
    }
    else if ((iTy >= 102) && (iTy < 132)) // Row 4 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 27))   {kbd_key = KBD_KEY_SHIFT; last_special_key = KBD_KEY_SHIFT; last_special_key_dampen = 50;}
        else if ((iTx >= 29)  && (iTx < 45))   kbd_key = 'A';
        else if ((iTx >= 45)  && (iTx < 60))   kbd_key = 'S';
        else if ((iTx >= 60)  && (iTx < 75))   kbd_key = 'D';
        else if ((iTx >= 75)  && (iTx < 90))   kbd_key = 'F';
        else if ((iTx >= 90)  && (iTx < 105))  kbd_key = 'G';
        else if ((iTx >= 105) && (iTx < 120))  kbd_key = 'H';
        else if ((iTx >= 120) && (iTx < 135))  kbd_key = 'J';
        else if ((iTx >= 135) && (iTx < 150))  kbd_key = 'K';
        else if ((iTx >= 150) && (iTx < 165))  kbd_key = 'L';
        else if ((iTx >= 165) && (iTx < 180))  kbd_key = ';';
        else if ((iTx >= 180) && (iTx < 195))  kbd_key = ':';
        else if ((iTx >= 195) && (iTx < 210))  kbd_key = KBD_KEY_RIGHT;        
        else if ((iTx >= 214) && (iTx < 255))  {kbd_key = KBD_KEY_BREAK; if (last_special_key == KBD_KEY_CTRL) IssueCtrlBreak=30;}
    }
    else if ((iTy >= 132) && (iTy < 162)) // Row 5 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 34))   kbd_key = KBD_KEY_CAPS;
        else if ((iTx >= 34)  && (iTx < 49))   kbd_key = 'Z';
        else if ((iTx >= 49)  && (iTx < 64))   kbd_key = 'X';
        else if ((iTx >= 64)  && (iTx < 78))   kbd_key = 'C';
        else if ((iTx >= 78)  && (iTx < 94))   kbd_key = 'V';
        else if ((iTx >= 94)  && (iTx < 109))  kbd_key = 'B';
        else if ((iTx >= 109) && (iTx < 123))  kbd_key = 'N';
        else if ((iTx >= 123) && (iTx < 139))  kbd_key = 'M';
        else if ((iTx >= 139) && (iTx < 154))  kbd_key = ',';
        else if ((iTx >= 154) && (iTx < 169))  kbd_key = '.';
        else if ((iTx >= 169) && (iTx < 184))  kbd_key = '/';
        else if ((iTx >= 188) && (iTx < 255))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 162) && (iTy < 192)) // Row 6 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 34))   {kbd_key = KBD_KEY_GRAPH; last_special_key = KBD_KEY_GRAPH; last_special_key_dampen = 50;}
        else if ((iTx >= 34)  && (iTx < 182))  kbd_key = ' ';
        else if ((iTx >= 182) && (iTx < 213))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 213) && (iTx < 255))  return MENU_CHOICE_MENU;
    }

    return MENU_CHOICE_NONE;
}

u8 handle_cvision_keyboard_press(u16 iTx, u16 iTy)  // Special controller for the CreatiVision
{
    if ((iTy >= 12) && (iTy < 50))        // Row 1 (top row)
    {
        if      ((iTx >= 0)   && (iTx < 21))   kbd_key = '1';
        else if ((iTx >= 21)  && (iTx < 42))   kbd_key = '2';
        else if ((iTx >= 42)  && (iTx < 63))   kbd_key = '3';
        else if ((iTx >= 63)  && (iTx < 84))   kbd_key = '4';
        else if ((iTx >= 84)  && (iTx < 105))  kbd_key = '5';
        else if ((iTx >= 105) && (iTx < 128))  kbd_key = '6';
        else if ((iTx >= 128) && (iTx < 150))  kbd_key = '7';
        else if ((iTx >= 150) && (iTx < 171))  kbd_key = '8';
        else if ((iTx >= 171) && (iTx < 192))  kbd_key = '9';
        else if ((iTx >= 192) && (iTx < 213))  kbd_key = '0';
        else if ((iTx >= 213) && (iTx < 234))  kbd_key = ':';
        else if ((iTx >= 235) && (iTx < 256))  kbd_key = '-';
    }
    else if ((iTy >= 50) && (iTy < 89))   // Row 2
    {
        if      ((iTx >= 0)   && (iTx < 21))   {kbd_key = KBD_KEY_CTRL; last_special_key = KBD_KEY_CTRL; last_special_key_dampen = 20;}
        else if ((iTx >= 21)  && (iTx < 42))   kbd_key = 'Q';
        else if ((iTx >= 42)  && (iTx < 63))   kbd_key = 'W';
        else if ((iTx >= 63)  && (iTx < 84))   kbd_key = 'E';
        else if ((iTx >= 84)  && (iTx < 105))  kbd_key = 'R';
        else if ((iTx >= 105) && (iTx < 128))  kbd_key = 'T';
        else if ((iTx >= 128) && (iTx < 150))  kbd_key = 'Y';
        else if ((iTx >= 150) && (iTx < 171))  kbd_key = 'U';
        else if ((iTx >= 171) && (iTx < 192))  kbd_key = 'I';
        else if ((iTx >= 192) && (iTx < 213))  kbd_key = 'O';
        else if ((iTx >= 213) && (iTx < 234))  kbd_key = 'P';
        else if ((iTx >= 235) && (iTx < 256))  kbd_key = KBD_KEY_RET;
    }
    else if ((iTy >= 89) && (iTy < 128))  // Row 3
    {
        if      ((iTx >= 0)   && (iTx < 21))   kbd_key = KBD_KEY_LEFT;
        else if ((iTx >= 21)  && (iTx < 42))   kbd_key = 'A';
        else if ((iTx >= 42)  && (iTx < 63))   kbd_key = 'S';
        else if ((iTx >= 63)  && (iTx < 84))   kbd_key = 'D';
        else if ((iTx >= 84)  && (iTx < 105))  kbd_key = 'F';
        else if ((iTx >= 105) && (iTx < 128))  kbd_key = 'G';
        else if ((iTx >= 128) && (iTx < 150))  kbd_key = 'H';
        else if ((iTx >= 150) && (iTx < 171))  kbd_key = 'J';
        else if ((iTx >= 171) && (iTx < 192))  kbd_key = 'K';
        else if ((iTx >= 192) && (iTx < 213))  kbd_key = 'L';
        else if ((iTx >= 213) && (iTx < 234))  kbd_key = ';';
        else if ((iTx >= 235) && (iTx < 256))  kbd_key = KBD_KEY_RIGHT;
    }
    else if ((iTy >= 128) && (iTy < 167))  // Row 4
    {
        if      ((iTx >= 0)   && (iTx < 21))   {kbd_key = KBD_KEY_SHIFT; last_special_key = KBD_KEY_SHIFT; last_special_key_dampen = 20;}
        else if ((iTx >= 21)  && (iTx < 42))   kbd_key = 'Z';
        else if ((iTx >= 42)  && (iTx < 63))   kbd_key = 'X';
        else if ((iTx >= 63)  && (iTx < 84))   kbd_key = 'C';
        else if ((iTx >= 84)  && (iTx < 105))  kbd_key = 'V';
        else if ((iTx >= 105) && (iTx < 128))  kbd_key = 'B';
        else if ((iTx >= 128) && (iTx < 150))  kbd_key = 'N';
        else if ((iTx >= 150) && (iTx < 171))  kbd_key = 'M';
        else if ((iTx >= 171) && (iTx < 192))  kbd_key = ',';
        else if ((iTx >= 192) && (iTx < 213))  kbd_key = '.';
        else if ((iTx >= 213) && (iTx < 234))  kbd_key = '/';
        else if ((iTx >= 235) && (iTx < 256))  kbd_key = ' ';
    }
    else if ((iTy >= 167) && (iTy < 192))  // Row 5
    {
        if      ((iTx >= 0)   && (iTx < 36))   kbd_key = KBD_KEY_F1; // Reset for the CreatiVision
        else if ((iTx >= 36)  && (iTx < 85))   return MENU_CHOICE_MENU;
        else if ((iTx >= 85)  && (iTx < 134))  return MENU_CHOICE_SAVE_GAME;
        else if ((iTx >= 134) && (iTx < 184))  return MENU_CHOICE_LOAD_GAME;
        else if ((iTx >= 184) && (iTx < 220))  return MENU_CHOICE_END_GAME;
        else if ((iTx >= 220) && (iTx < 255))  return MENU_CHOICE_CASSETTE;
    }

    return MENU_CHOICE_NONE;
}

u8 handle_alpha_keyboard_press(u16 iTx, u16 iTy)  // Generic and Simplified Alpha-Numeric Keyboard
{
    if ((iTy >= 14) && (iTy < 48))   // Row 1 (number row)
    {
        if      ((iTx >= 0)   && (iTx < 28))   kbd_key = '1';
        else if ((iTx >= 28)  && (iTx < 54))   kbd_key = '2';
        else if ((iTx >= 54)  && (iTx < 80))   kbd_key = '3';
        else if ((iTx >= 80)  && (iTx < 106))  kbd_key = '4';
        else if ((iTx >= 106) && (iTx < 132))  kbd_key = '5';
        else if ((iTx >= 132) && (iTx < 148))  kbd_key = '6';
        else if ((iTx >= 148) && (iTx < 174))  kbd_key = '7';
        else if ((iTx >= 174) && (iTx < 200))  kbd_key = '8';
        else if ((iTx >= 200) && (iTx < 226))  kbd_key = '9';
        else if ((iTx >= 226) && (iTx < 255))  kbd_key = '0';
    }
    else if ((iTy >= 48) && (iTy < 85))  // Row 2 (QWERTY row)
    {
        if      ((iTx >= 0)   && (iTx < 28))   kbd_key = 'Q';
        else if ((iTx >= 28)  && (iTx < 54))   kbd_key = 'W';
        else if ((iTx >= 54)  && (iTx < 80))   kbd_key = 'E';
        else if ((iTx >= 80)  && (iTx < 106))  kbd_key = 'R';
        else if ((iTx >= 106) && (iTx < 132))  kbd_key = 'T';
        else if ((iTx >= 132) && (iTx < 148))  kbd_key = 'Y';
        else if ((iTx >= 148) && (iTx < 174))  kbd_key = 'U';
        else if ((iTx >= 174) && (iTx < 200))  kbd_key = 'I';
        else if ((iTx >= 200) && (iTx < 226))  kbd_key = 'O';
        else if ((iTx >= 226) && (iTx < 255))  kbd_key = 'P';
    }
    else if ((iTy >= 85) && (iTy < 122)) // Row 3 (ASDF row)
    {
        if      ((iTx >= 0)   && (iTx < 28))   kbd_key = 'A';
        else if ((iTx >= 28)  && (iTx < 54))   kbd_key = 'S';
        else if ((iTx >= 54)  && (iTx < 80))   kbd_key = 'D';
        else if ((iTx >= 80)  && (iTx < 106))  kbd_key = 'F';
        else if ((iTx >= 106) && (iTx < 132))  kbd_key = 'G';
        else if ((iTx >= 132) && (iTx < 148))  kbd_key = 'H';
        else if ((iTx >= 148) && (iTx < 174))  kbd_key = 'J';
        else if ((iTx >= 174) && (iTx < 200))  kbd_key = 'K';
        else if ((iTx >= 200) && (iTx < 226))  kbd_key = 'L';
        else if ((iTx >= 226) && (iTx < 255))  kbd_key = (adam_mode ? ADAM_KEY_BS : (einstein_mode ? KBD_KEY_INS : KBD_KEY_BS));
    }
    else if ((iTy >= 122) && (iTy < 159)) // Row 4 (ZXCV row)
    {
        if      ((iTx >= 0)   && (iTx < 28))   kbd_key = 'Z';
        else if ((iTx >= 28)  && (iTx < 54))   kbd_key = 'X';
        else if ((iTx >= 54)  && (iTx < 80))   kbd_key = 'C';
        else if ((iTx >= 80)  && (iTx < 106))  kbd_key = 'V';
        else if ((iTx >= 106) && (iTx < 132))  kbd_key = 'B';
        else if ((iTx >= 132) && (iTx < 148))  kbd_key = 'N';
        else if ((iTx >= 148) && (iTx < 174))  kbd_key = 'M';
        else if ((iTx >= 174) && (iTx < 200))  kbd_key = (key_shift ?  (adam_mode ? ADAM_KEY_QUOTE : KBD_KEY_QUOTE) : ',');
        else if ((iTx >= 200) && (iTx < 226))  kbd_key = (key_shift ?  (adam_mode ? ADAM_KEY_F1 : KBD_KEY_F1) : '.');
        else if ((iTx >= 226) && (iTx < 255))  kbd_key = (adam_mode ? ADAM_KEY_ENTER : KBD_KEY_RET);
    }
    else if ((iTy >= 159) && (iTy < 192)) // Row 5 (SPACE BAR and icons row)
    {
        if      ((iTx >= 1)   && (iTx < 52))   return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 54)  && (iTx < 202))  kbd_key = ' ';
        else if ((iTx >= 202) && (iTx < 255))  return MENU_CHOICE_MENU; 
    }
    
    if (adam_mode)
    {
        if (kbd_key != last_kbd_key && (kbd_key != 0) && (last_kbd_key != 255))
        {
            PutKBD(kbd_key | (((adam_CapsLock && (kbd_key >= 'A') && (kbd_key <= 'Z')) || key_shift) ? CON_SHIFT:0));
            if (!myConfig.keyMute) mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
            last_kbd_key = kbd_key;
        }
    }
    
    return MENU_CHOICE_NONE;
}

u8 handle_debugger_overlay(u16 iTx, u16 iTy)
{
    if ((iTy >= 175) && (iTy < 192)) // Bottom row is where the debugger keys are...
    {
        if      ((iTx >= 1)   && (iTx < 125))  kbd_key = ' ';
        if      ((iTx >= 125) && (iTx < 158))  return MENU_CHOICE_MENU;
        else if ((iTx >= 158) && (iTx < 192))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 192) && (iTx < 255))  return MENU_CHOICE_MENU;
    }
    else {kbd_key = 0; last_kbd_key = 0;}

    return MENU_CHOICE_NONE;
}


u8 handle_normal_virtual_keypad(u16 iTx, u16 iTy)  // All other normal overlays with keypad on the right and menu choices on the left
{
    // For ADAM, the standard overlay has a CASSETTE icon to save data...
    if (adam_mode && (myConfig.overlay == 0))
    {
        if ((iTy >= 5) && (iTy < 33) && (iTx >= 95) && (iTx <= 130))
        {
            return MENU_CHOICE_CASSETTE;
        }
        else if ((iTy >= 5) && (iTy < 33) && (iTx > 130) && (iTx <= 170))
        {
            return MENU_CHOICE_SWAP_KBD;
        }
    }

    if ((iTx >= 6) && (iTx <= 130))     // We're on the left-side of the screen...
    {
        if ((iTy>=40) && (iTy<67))      // RESET
        {
            return MENU_CHOICE_RESET_GAME;
        }
        else if ((iTy>=67) && (iTy<95)) // END GAME
        {
            return MENU_CHOICE_END_GAME;
        }
        else if ((iTy>=95) && (iTy<125)) // HI SCORE
        {
            return MENU_CHOICE_HI_SCORE;
        }
        else if ((iTy>=125) && (iTy<155)) // SAVE GAME
        {
            return MENU_CHOICE_SAVE_GAME;
        }
        else if ((iTy>=155) && (iTy<184)) // LOAD GAME
        {
            return MENU_CHOICE_LOAD_GAME;
        }
    }

    return MENU_CHOICE_NONE;
}

// ------------------------------------------------------------------------
// The main emulation loop is here... call into the Z80, VDP and PSG
// ------------------------------------------------------------------------
void colecoDS_main(void)
{
  u16 iTx,  iTy;
  u16 SaveNow = 0, LoadNow = 0;
  u32 cvTouchPad, ucDEUX;
  static u32 lastUN = 0;
  static u8 dampenClick = 0;
  u8 meta_key = 0;

  // Returns when  user has asked for a game to run...
  BottomScreenOptions();

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
            if (myGlobalConfig.showFPS)
            {
                if (emuFps == 61) emuFps=60;
                else if (emuFps == 59) emuFps=60;
                if (emuFps/100) szChai[0] = '0' + emuFps/100;
                else szChai[0] = ' ';
                szChai[1] = '0' + (emuFps%100) / 10;
                szChai[2] = '0' + (emuFps%100) % 10;
                szChai[3] = 0;
                DSPrint(0,0,6,szChai);
            }
            DisplayStatusLine(false);
            emuActFrames = 0;

            // A bit of a hack for the SC-3000 Survivors Multi-Cart
            if (sg1000_double_reset)
            {
                sg1000_double_reset=false;
                ResetColecovision();
            }

            if (myConfig.isPAL) myConfig.vertSync=0;    // Force Sync OFF always in PAL mode
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
            if (++timingFrames == (myConfig.isPAL ? 50:60))
            {
                TIMER2_CR=0;
                TIMER2_DATA=0;
                TIMER2_CR=TIMER_ENABLE | TIMER_DIV_1024;
                timingFrames = 0;
            }

            // ----------------------------------------------------------------------
            // Time 1 frame... 546 (NTSC) or 646 (PAL) ticks of Timer2
            // This is how we time frame-to frame to keep the game running at 60FPS
            // We also allow running the game faster/slower than 100% so we use the
            // GAME_SPEED_XXX[] array to handle that.
            // ----------------------------------------------------------------------
            while(TIMER2_DATA < ((myConfig.isPAL ? GAME_SPEED_PAL[myConfig.gameSpeed]:GAME_SPEED_NTSC[myConfig.gameSpeed])*(timingFrames+1)))
            {
                if (myGlobalConfig.showFPS == 2) break;   // If Full Speed, break out...
            }
        }

      // If the Z80 Debugger is enabled, call it
      if (myGlobalConfig.debugger >= 2)
      {
          ShowDebugZ80();
      }

      cvTouchPad  = 0;    // Assume no CV touchpad (buttons 0-9,*,#) press until proven otherwise.
        
      // ---------------------------------------------------------------------------------
      // Hold the key press for a brief instant... some machines take longer than others 
      // (eg MSX needs to see the keypress for many tens of milliseconds)... This allows
      // us to 'hold' the keypress in memory for several frames which is good enough.
      // ---------------------------------------------------------------------------------
      if (key_debounce > 0) key_debounce--;
      else
      {
          // -----------------------------------------------------------
          // This is where we accumualte the keys pressed... up to 12!
          // -----------------------------------------------------------
          kbd_keys_pressed = 0;
          memset(kbd_keys, 0x00, sizeof(kbd_keys));
          kbd_key = 0;

          // ------------------------------------------
          // Handle any screen touch events
          // ------------------------------------------
          if  (keysCurrent() & KEY_TOUCH)
          {
              // ------------------------------------------------------------------------------------------------
              // Just a tiny bit of touch debounce so ensure touch screen is pressed for a fraction of a second.
              // ------------------------------------------------------------------------------------------------
              if (++touch_debounce > 1)
              {
                touchPosition touch;
                touchRead(&touch);
                iTx = touch.px;
                iTy = touch.py;

                if (myGlobalConfig.debugger == 3)
                {
                    meta_key = handle_debugger_overlay(iTx, iTy);
                }
                // ------------------------------------------------------------
                // Test the touchscreen for various full keyboard handlers... 
                // ------------------------------------------------------------
                else if (myConfig.overlay == 1) // Full Keyboard Selected
                {
                    // ----------------------------------------------------------------
                    // Pick the right keyboard layout based on the machine emulated...
                    // ----------------------------------------------------------------
                    if      (adam_mode)         meta_key = handle_adam_keyboard_press(iTx, iTy);
                    else if (msx_mode)          meta_key = handle_msx_keyboard_press(iTx, iTy);
                    else if (memotech_mode)     meta_key = handle_mtx_keyboard_press(iTx, iTy);
                    else if (creativision_mode) meta_key = handle_cvision_keyboard_press(iTx, iTy);
                    else if (einstein_mode)     meta_key = handle_einstein_keyboard_press(iTx, iTy);
                    else if (svi_mode)          meta_key = handle_svi_keyboard_press(iTx, iTy);
                    else if (sg1000_mode)       meta_key = handle_sc3000_keyboard_press(iTx, iTy);
                    else if (sordm5_mode)       meta_key = handle_sordm5_keyboard_press(iTx, iTy);
                    else                        meta_key = handle_alpha_keyboard_press(iTx, iTy);
                }
                else if (myConfig.overlay == 2) // Simplified Alpha Keyboard (can be used with any emulated machine)
                {
                    meta_key = handle_alpha_keyboard_press(iTx, iTy);
                }                  
                else    // Normal 12 button virtual keypad (might be custom overlay but the number pad is in the same location on each)
                {
                    meta_key = handle_normal_virtual_keypad(iTx, iTy);

                    cvTouchPad = ( ((iTx>=137) && (iTy>=38) && (iTx<=171) && (iTy<=72)) ? 0x02: 0x00);
                    cvTouchPad = ( ((iTx>=171) && (iTy>=38) && (iTx<=210) && (iTy<=72)) ? 0x08: cvTouchPad);
                    cvTouchPad = ( ((iTx>=210) && (iTy>=38) && (iTx<=248) && (iTy<=72)) ? 0x03: cvTouchPad);

                    cvTouchPad = ( ((iTx>=137) && (iTy>=73) && (iTx<=171) && (iTy<=110)) ? 0x0D: cvTouchPad);
                    cvTouchPad = ( ((iTx>=171) && (iTy>=73) && (iTx<=210) && (iTy<=110)) ? 0x0C: cvTouchPad);
                    cvTouchPad = ( ((iTx>=210) && (iTy>=73) && (iTx<=248) && (iTy<=110)) ? 0x01: cvTouchPad);

                    cvTouchPad = ( ((iTx>=137) && (iTy>=111) && (iTx<=171) && (iTy<=147)) ? 0x0A: cvTouchPad);
                    cvTouchPad = ( ((iTx>=171) && (iTy>=111) && (iTx<=210) && (iTy<=147)) ? 0x0E: cvTouchPad);
                    cvTouchPad = ( ((iTx>=210) && (iTy>=111) && (iTx<=248) && (iTy<=147)) ? 0x04: cvTouchPad);

                    cvTouchPad = ( ((iTx>=137) && (iTy>=148) && (iTx<=171) && (iTy<=186)) ? 0x06: cvTouchPad);
                    cvTouchPad = ( ((iTx>=171) && (iTy>=148) && (iTx<=210) && (iTy<=186)) ? 0x05: cvTouchPad);
                    cvTouchPad = ( ((iTx>=210) && (iTy>=148) && (iTx<=248) && (iTy<=186)) ? 0x09: cvTouchPad);
                }

                if (kbd_key != 0)
                {
                    kbd_keys[kbd_keys_pressed++] = kbd_key;
                    key_debounce = 2;
                }

                // If the special menu key indicates we should show the choice menu, do so here...
                if (meta_key == MENU_CHOICE_MENU)
                {
                    meta_key = MiniMenu();
                }

                // -------------------------------------------------------------------
                // If one of the special meta keys was picked, we handle that here...
                // -------------------------------------------------------------------
                switch (meta_key)
                {
                    case MENU_CHOICE_RESET_GAME:
                        SoundPause();
                        // Ask for verification
                        if (showMessage("DO YOU REALLY WANT TO", "RESET THE CURRENT GAME ?") == ID_SHM_YES)
                        {
                            ResetColecovision();                            
                        }
                        BottomScreenKeypad();
                        SoundUnPause();
                        break;

                    case MENU_CHOICE_END_GAME:
                          SoundPause();
                          //  Ask for verification
                          if  (showMessage("DO YOU REALLY WANT TO","QUIT THE CURRENT GAME ?") == ID_SHM_YES)
                          {
                              memset((u8*)0x06000000, 0x00, 0x20000);    // Reset VRAM to 0x00 to clear any potential display garbage on way out
                              return;
                          }
                          BottomScreenKeypad();
                          DisplayStatusLine(true);
                          SoundUnPause();
                        break;

                    case MENU_CHOICE_HI_SCORE:
                        SoundPause();
                        highscore_display(file_crc);
                        DisplayStatusLine(true);
                        SoundUnPause();
                        break;

                    case MENU_CHOICE_SAVE_GAME:
                        if  (!SaveNow)
                        {
                            SoundPause();
                            if (IsFullKeyboard())
                            {
                                if  (showMessage("DO YOU REALLY WANT TO","SAVE GAME STATE ?") == ID_SHM_YES)
                                {
                                  SaveNow = 1;
                                  colecoSaveState();
                                }
                            }
                            else
                            {
                                SaveNow = 1;
                                colecoSaveState();
                            }
                            BottomScreenKeypad();
                            SoundUnPause();
                        }
                        break;

                    case MENU_CHOICE_LOAD_GAME:
                        if  (!LoadNow)
                        {
                            SoundPause();
                            if (IsFullKeyboard())
                            {
                                if (showMessage("DO YOU REALLY WANT TO","LOAD GAME STATE ?") == ID_SHM_YES)
                                {
                                  LoadNow = 1;
                                  colecoLoadState();
                                }
                            }
                            else
                            {
                                LoadNow = 1;
                                colecoLoadState();
                            }
                            BottomScreenKeypad();
                            SoundUnPause();
                        }
                        break;

                    case MENU_CHOICE_CASSETTE:
                        CassetteMenu();
                        break;
                        
                    case MENU_CHOICE_SWAP_KBD:
                        if (myConfig.overlay == 0) myConfig.overlay = 1; else myConfig.overlay = 0;
                        BottomScreenKeypad();
                        WAITVBL;WAITVBL;WAITVBL;
                        break;
                        
                    default:
                        SaveNow = 0;
                        LoadNow = 0;
                }

                // ---------------------------------------------------------------------
                // If we are mapping the touch-screen keypad to P2, we shift these up.
                // ---------------------------------------------------------------------
                if (myConfig.touchPad) cvTouchPad = cvTouchPad << 16;

                if (++dampenClick > 0)  // Make sure the key is pressed for an appreciable amount of time...
                {
                    if (((cvTouchPad != 0) || (kbd_key != 0)) && (lastUN == 0))
                    {
                        if (!adam_mode)
                        {
                            if (!myConfig.keyMute) mmEffect(SFX_KEYCLICK);  // Play short key click for feedback... ADAM handers do this for us
                        }
                    }
                    lastUN = (cvTouchPad ? cvTouchPad:kbd_key);
                }
              }
          } //  SCR_TOUCH
          else
          {
            touch_debounce = 0;
            SaveNow=LoadNow = 0;
            lastUN = 0;  dampenClick = 0;
            last_kbd_key = 0;
          }
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
      key_shift = false;
      key_ctrl = false;
      key_code = false;
      key_graph = false;
      key_dia = false;

      ucDEUX  = 0;
      nds_key  = keysCurrent();     // Get any current keys pressed on the NDS
          
      if (IssueCtrlBreak)   // For the Einstein only...
      {
          IssueCtrlBreak--;
          key_ctrl = 1;
          if (IssueCtrlBreak > 15)
          {
              kbd_key = KBD_KEY_STOP;
              kbd_keys[kbd_keys_pressed++] = kbd_key;
          }
      }
      else // Otherwise we're scanning for keys normally
      {
          if ((nds_key & KEY_L) && (nds_key & KEY_R) && (nds_key & KEY_X))
          {
                lcdSwap();
                WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
          }
          else if ((nds_key & KEY_L) && (nds_key & KEY_R))
          {
                DSPrint(5,0,0,"SNAPSHOT");
                screenshot();
                WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
                DSPrint(5,0,0,"        ");
          }
          else if  (nds_key & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_R | KEY_L | KEY_X | KEY_Y))
          {
              if (einstein_mode && (nds_key & KEY_START)) // Load .COM file directly
              {
                  if (einstein_mode == 2) // .dsk file
                  {
                      IssueCtrlBreak = 30;  // Long enough for the Einstein to see the Ctrl-Break and boot the diskette
                  }
                  else
                  {
                      einstein_load_com_file();
                      WAITVBL;WAITVBL;WAITVBL;
                  }
              }
              else if (memotech_mode && (nds_key & KEY_START))
              {
                  extern u8 memotech_magrom_present;

                  if (memotech_magrom_present)
                  {
                      BufferKeys("ROM 7");
                      BufferKey(KBD_KEY_RET);
                  }
                  else
                  if (memotech_mode == 2)   // .MTX file: enter LOAD "" into the keyboard buffer
                  {
                      if (memotech_mtx_500_only)
                      {
                          RAM_Memory[0xFA7A] = 0x00;

                          BufferKeys("NEW");
                          BufferKey(KBD_KEY_RET);
                          BufferKey(255);
                      }

                      BufferKeys("LOAD");
                      BufferKey(KBD_KEY_SHIFT);
                      BufferKey('2');
                      BufferKey(KBD_KEY_SHIFT);
                      BufferKey('2');
                      BufferKey(KBD_KEY_RET);
                  }
                  else  // .RUN file: load and jump to program
                  {
                      memotech_launch_run_file();
                  }
                  WAITVBL;WAITVBL;WAITVBL;
              }
              else if ((svi_mode || (msx_mode==2)) && ((nds_key & KEY_START) || (nds_key & KEY_SELECT)))
              {
                  if (nds_key & KEY_START)
                  {
                      BufferKeys("CLOAD");
                      BufferKey(KBD_KEY_RET);
                      BufferKey(255);
                      BufferKeys("RUN");
                      BufferKey(KBD_KEY_RET);
                  }
                  else
                  {
                      BufferKeys("BLOAD");
                      BufferKey(KBD_KEY_SHIFT);
                      BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                      BufferKeys("CAS");
                      if (msx_mode && !msx_japanese_matrix) BufferKey(KBD_KEY_SHIFT);
                      BufferKey(msx_japanese_matrix ? KBD_KEY_QUOTE : ':');
                      BufferKey(KBD_KEY_SHIFT);
                      BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                      BufferKey(',');
                      BufferKey('R');
                      BufferKey(KBD_KEY_RET);
                  }
                  WAITVBL;WAITVBL;WAITVBL;
              }
              else
              // --------------------------------------------------------------------------------------------------
              // There are 12 NDS buttons (D-Pad, XYAB, L/R and Start+Select) - we allow mapping of any of these.
              // --------------------------------------------------------------------------------------------------
              for (u8 i=0; i<12; i++)
              {
                  if (nds_key & NDS_keyMap[i])
                  {
                      if (keyCoresp[myConfig.keymap[i]] < 0xFFFE0000)   // Normal key map
                      {
                          ucDEUX  |= keyCoresp[myConfig.keymap[i]];
                      }
                      else if (keyCoresp[myConfig.keymap[i]] < 0xFFFF0000)   // Special Spinner Handling
                      {
                          if      (keyCoresp[myConfig.keymap[i]] == META_SPINX_LEFT)  spinX_left  = 1;
                          else if (keyCoresp[myConfig.keymap[i]] == META_SPINX_RIGHT) spinX_right = 1;
                          else if (keyCoresp[myConfig.keymap[i]] == META_SPINY_LEFT)  spinY_left  = 1;
                          else if (keyCoresp[myConfig.keymap[i]] == META_SPINY_RIGHT) spinY_right = 1;
                      }
                      else // This is a keyboard maping... handle that here... just set the appopriate kbd_key
                      {
                          if      ((keyCoresp[myConfig.keymap[i]] >= META_KBD_A) && (keyCoresp[myConfig.keymap[i]] <= META_KBD_Z))  kbd_key = ('A' + (keyCoresp[myConfig.keymap[i]] - META_KBD_A));
                          else if ((keyCoresp[myConfig.keymap[i]] >= META_KBD_0) && (keyCoresp[myConfig.keymap[i]] <= META_KBD_9))  kbd_key = ('0' + (keyCoresp[myConfig.keymap[i]] - META_KBD_0));
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_SPACE)     kbd_key = ' ';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_RETURN)    kbd_key = (adam_mode ? ADAM_KEY_ENTER : KBD_KEY_RET);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_ESC)       kbd_key = (adam_mode ? ADAM_KEY_ESC : KBD_KEY_ESC);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_SHIFT)     key_shift = 1;
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_CTRL)      key_ctrl  = 1;
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_CODE)      key_code  = 1;
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_GRAPH)     key_graph = 1;
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_HOME)      kbd_key = (adam_mode ? ADAM_KEY_HOME  : KBD_KEY_HOME);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_UP)        kbd_key = (adam_mode ? ADAM_KEY_UP    : KBD_KEY_UP);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_DOWN)      kbd_key = (adam_mode ? ADAM_KEY_DOWN  : KBD_KEY_DOWN);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_LEFT)      kbd_key = (adam_mode ? ADAM_KEY_LEFT  : KBD_KEY_LEFT);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_RIGHT)     kbd_key = (adam_mode ? ADAM_KEY_RIGHT : KBD_KEY_RIGHT);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PERIOD)    kbd_key = '.';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_COMMA)     kbd_key = ',';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_COLON)     kbd_key = ':';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_SEMI)      kbd_key = ';';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_QUOTE)     kbd_key = (adam_mode ? ADAM_KEY_QUOTE : KBD_KEY_QUOTE);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_SLASH)     kbd_key = '/';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_BACKSLASH) kbd_key = '\\';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PLUS)      kbd_key = '+';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_MINUS)     kbd_key = '-';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_LBRACKET)  kbd_key = '[';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_RBRACKET)  kbd_key = ']';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_BS)        kbd_key = (adam_mode ? ADAM_KEY_BS : KBD_KEY_BS);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_CARET)     kbd_key = '^';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_ASTERISK)  kbd_key = '*';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_ATSIGN)    kbd_key = '@';
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_TAB)       kbd_key = (adam_mode ? ADAM_KEY_TAB : KBD_KEY_TAB);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_INS)       kbd_key = (adam_mode ? ADAM_KEY_INS : KBD_KEY_INS);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_DEL)       kbd_key = (adam_mode ? ADAM_KEY_DEL : KBD_KEY_DEL);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_CLR)       kbd_key = (adam_mode ? ADAM_KEY_CLEAR : KBD_KEY_CLEAR);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_UNDO)      kbd_key = (adam_mode ? ADAM_KEY_UNDO : KBD_KEY_UNDO);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_MOVE)      kbd_key = (adam_mode ? ADAM_KEY_MOVE : KBD_KEY_MOVE);                     
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_WILDCARD)  kbd_key = (adam_mode ? ADAM_KEY_WILDCARD : KBD_KEY_WILDCARD);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_STORE)     kbd_key = (adam_mode ? ADAM_KEY_STORE : KBD_KEY_STORE);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_PRINT)     kbd_key = (adam_mode ? ADAM_KEY_PRINT : KBD_KEY_PRINT);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_STOP_BRK)  kbd_key = (adam_mode ? ADAM_KEY_CLEAR : KBD_KEY_STOP);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_F1)        kbd_key = (adam_mode ? ADAM_KEY_F1 : KBD_KEY_F1);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_F2)        kbd_key = (adam_mode ? ADAM_KEY_F2 : KBD_KEY_F2);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_F3)        kbd_key = (adam_mode ? ADAM_KEY_F3 : KBD_KEY_F3);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_F4)        kbd_key = (adam_mode ? ADAM_KEY_F4 : KBD_KEY_F4);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_F5)        kbd_key = (adam_mode ? ADAM_KEY_F5 : KBD_KEY_F5);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_F6)        kbd_key = (adam_mode ? ADAM_KEY_F6 : KBD_KEY_F6);
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_F7)        kbd_key = KBD_KEY_F7;
                          else if (keyCoresp[myConfig.keymap[i]] == META_KBD_F8)        kbd_key = KBD_KEY_F8;                      

                          if (adam_mode)
                          {
                              if (kbd_key != last_mapped_key && (kbd_key != 0) && (last_mapped_key != 255))
                              {
                                  PutKBD(kbd_key | (((adam_CapsLock && (kbd_key >= 'A') && (kbd_key <= 'Z')) || key_shift) ? CON_SHIFT:0));
                                  if (!myConfig.keyMute) mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
                                  last_mapped_key = kbd_key;
                              }                            
                          }
                          else if (kbd_key != 0)
                          {
                              kbd_keys[kbd_keys_pressed++] = kbd_key;
                          }
                      }
                  }
              }
          }
          else
          {
              last_mapped_key = 0;
          }
      }
      
      // ------------------------------------------------------------------------------------------
      // Finally, check if there are any buffered keys that need to go into the keyboard handling.
      // ------------------------------------------------------------------------------------------
      ProcessBufferedKeys();

      // ---------------------------------------------------------
      // Accumulate all bits above into the Joystick State var...
      // ---------------------------------------------------------
      JoyState = cvTouchPad | ucDEUX;
        
      // If we are Sord M5 we need to check if this generates a keyboard interrupt
      if (sordm5_mode)
      {
          sordm5_check_keyboard_interrupt();
      }

      // --------------------------------------------------
      // Handle Auto-Fire if enabled in configuration...
      // --------------------------------------------------
      static u8 autoFireTimer[2]={0,0};
      if ((myConfig.autoFire & 0x01) && (JoyState & JST_FIRER))  // Fire Button 1
      {
         if ((++autoFireTimer[0] & 7) > 4)  JoyState &= ~JST_FIRER;
      }
      if ((myConfig.autoFire & 0x02) && (JoyState & JST_FIREL))  // Fire Button 2
      {
          if ((++autoFireTimer[1] & 7) > 4) JoyState &= ~JST_FIREL;
      }
    }
  }
}


// ----------------------------------------------------------------------------------------
// We steal 128K of the VRAM to hold the MSX BIOS flavors and 16K for one look-up table.
// ----------------------------------------------------------------------------------------
void useVRAM(void)
{
  vramSetBankD(VRAM_D_LCD );                 // Not using this for video but 128K of faster RAM always useful!  Mapped at 0x06860000 -   Currently unused (future expansion)
  vramSetBankE(VRAM_E_LCD );                 // Not using this for video but 64K of faster RAM always useful!   Mapped at 0x06880000 -   We use this block of 128K memory to store 4x MSX BIOS flavors
  vramSetBankF(VRAM_F_LCD );                 // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06890000 -   ..
  vramSetBankG(VRAM_G_LCD );                 // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x06894000 -   ..
  vramSetBankH(VRAM_H_LCD );                 // Not using this for video but 32K of faster RAM always useful!   Mapped at 0x06898000 -   ..
  vramSetBankI(VRAM_I_LCD );                 // Not using this for video but 16K of faster RAM always useful!   Mapped at 0x068A0000 -   16K Used for the VDP Look Up Table
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
  vramSetBankB(VRAM_B_MAIN_SPRITE);          // Once emulation of game starts, we steal this back for an additional 128K of VRAM at 0x6820000 which we will use as a snapshot buffer for taking screen pics
  vramSetBankC(VRAM_C_SUB_BG);

  //  Stop blending effect of intro
  REG_BLDCNT=0; REG_BLDCNT_SUB=0; REG_BLDY=0; REG_BLDY_SUB=0;

  //  Render the top screen
  bg0 = bgInit(0, BgType_Text8bpp,  BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp,  BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(topscreenTiles,  bgGetGfxPtr(bg0), LZ77Vram);
  decompress(topscreenMap,  (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) topscreenPal,(void*)  BG_PALETTE,256*2);
  unsigned  short dmaVal =*(bgGetMapPtr(bg0)+51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1),32*24*2);

  // Put up the options screen 
  BottomScreenOptions();

  //  Find the files
  colecoDSFindFiles();
}


void BottomScreenOptions(void)
{
    swiWaitForVBlank();
    
    // ---------------------------------------------------
    // Put up the options select screen background...
    // ---------------------------------------------------
    bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x256, 31,0);
    bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x256, 29,0);
    bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);
    decompress(optionsTiles, bgGetGfxPtr(bg0b), LZ77Vram);
    decompress(optionsMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
    dmaCopy((void*) optionsPal,(void*) BG_PALETTE_SUB,256*2);
    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);
}

// ---------------------------------------------------------------------------
// Setup the bottom screen - mostly for menu, high scores, options, etc.
// ---------------------------------------------------------------------------
void BottomScreenKeypad(void)
{
    if (myGlobalConfig.debugger == 3)  // Full Z80 Debug overrides things... put up the debugger overlay
    {
          //  Init bottom screen
          decompress(debug_ovlTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(debug_ovlMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) debug_ovlPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 1) // Full Keyboard (based on machine)
    {
        if (adam_mode)  // ADAM Keyboard
        {
          //  Init bottom screen
          decompress(adam_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(adam_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) adam_fullPal,(void*) BG_PALETTE_SUB,256*2);
        }
        else if (msx_mode)  //MSX Keyboard
        {
          //  Init bottom screen
          if (msx_mode && msx_japanese_matrix)
          {
              decompress(msx_japanTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
              decompress(msx_japanMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
              dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
              dmaCopy((void*) msx_japanPal,(void*) BG_PALETTE_SUB,256*2);
          }
          else
          {
              decompress(msx_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
              decompress(msx_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
              dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
              dmaCopy((void*) msx_fullPal,(void*) BG_PALETTE_SUB,256*2);
          }
        }
        else if (memotech_mode) // Memotech MTX
        {
          //  Init bottom screen
          decompress(mtx_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(mtx_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) mtx_fullPal,(void*) BG_PALETTE_SUB,256*2);
        }
        else if (creativision_mode) // CreatiVision Keypad
        {
          //  Init bottom screen
          decompress(cvision_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(cvision_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) cvision_kbdPal,(void*) BG_PALETTE_SUB,256*2);
        }
        else if (einstein_mode) // Einstein Keyboard
        {
          //  Init bottom screen
          decompress(einstein_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(einstein_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) einstein_kbdPal,(void*) BG_PALETTE_SUB,256*2);
        }
        else if (svi_mode) // Spectravideo SVI
        {
          //  Init bottom screen
          decompress(svi_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(svi_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) svi_fullPal,(void*) BG_PALETTE_SUB,256*2);
        }
        else if (sg1000_mode) // SC-3000 Keyboard
        {
          //  Init bottom screen
          decompress(sc3000_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(sc3000_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) sc3000_kbdPal,(void*) BG_PALETTE_SUB,256*2);
        }
        else if (sordm5_mode) // Sord M5 Keyboard
        {
          //  Init bottom screen
          decompress(m5_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(m5_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) m5_kbdPal,(void*) BG_PALETTE_SUB,256*2);
        }
        else // No custom keyboard for this machine... just use simplified alpha keyboard...
        {
          //  Init bottom screen
          decompress(alpha_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(alpha_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) alpha_kbdPal,(void*) BG_PALETTE_SUB,256*2);
        }
    }
    else if (myConfig.overlay == 2) // Alpha Simplified Keyboard
    {
      //  Init bottom screen
      decompress(alpha_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(alpha_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) alpha_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 3)  // Wargames
    {
      //  Init bottom screen
      decompress(wargamesTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(wargamesMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) wargamesPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 4)  // Mousetrap
    {
      //  Init bottom screen
      decompress(mousetrapTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(mousetrapMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) mousetrapPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 5)  // Gateway to Apshai
    {
      //  Init bottom screen
      decompress(gatewayTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(gatewayMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) gatewayPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 6)  // Spy Hunter
    {
      //  Init bottom screen
      decompress(spyhunterTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(spyhunterMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) spyhunterPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 7)  // Fix Up the Mix Up
    {
      //  Init bottom screen
      decompress(fixupmixupTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(fixupmixupMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) fixupmixupPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 8)  // Boulder Dash
    {
      //  Init bottom screen
      decompress(boulderTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(boulderMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) boulderPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 9)  // Quest for Quinta Roo
    {
      //  Init bottom screen
      decompress(questTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(questMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) questPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 10)  // 2010
    {
      //  Init bottom screen
      decompress(hal2010Tiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(hal2010Map, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) hal2010Pal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 11)  // Space Shuttle
    {
      //  Init bottom screen
      decompress(shuttleTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(shuttleMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) shuttlePal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 12)  // Utopia
    {
      //  Init bottom screen
      decompress(utopiaTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(utopiaMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) utopiaPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else // Generic Overlay (overlay == 0)
    {
      if (msx_mode)
      {
          //  Init bottom screen
          decompress(msx_smTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(msx_smMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) msx_smPal,(void*) BG_PALETTE_SUB,256*2);
      }
      else if (creativision_mode)
      {
          //  Init bottom screen
          decompress(cvisionTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(cvisionMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) cvisionPal,(void*) BG_PALETTE_SUB,256*2);
      }
      else if (adam_mode)
      {
          //  Init bottom screen
          decompress(adam_smTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(adam_smMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) adam_smPal,(void*) BG_PALETTE_SUB,256*2);
      }
      else if (pv2000_mode)
      {
          //  Init bottom screen
          decompress(pv2000_smTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(pv2000_smMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) pv2000_smPal,(void*) BG_PALETTE_SUB,256*2);
      }
      else
      {
          //  Init bottom screen with the standard colecovision overlay
          decompress(colecovisionTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
          decompress(colecovisionMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
          dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
          dmaCopy((void*) colecovisionPal,(void*) BG_PALETTE_SUB,256*2);
      }
    }

    unsigned  short dmaVal = *(bgGetMapPtr(bg1b)+24*32);
    dmaFillWords(dmaVal | (dmaVal<<16),(void*)  bgGetMapPtr(bg1b),32*24*2);

    last_pal_mode = 99;
    last_msx_scc_enable = 99;
    DisplayStatusLine(true);
}

/*********************************************************************************
 * Init CPU for the current game
 ********************************************************************************/
void colecoDSInitCPU(void)
{
  //  -----------------------------------------
  //  Init Main Memory and VDP Video Memory
  //  -----------------------------------------
  memset(RAM_Memory, 0x00, 0x20000);
  memset(pVDPVidMem, 0x00, 0x4000);

  // -----------------------------------------------
  // Init bottom screen do display correct overlay
  // -----------------------------------------------
  BottomScreenKeypad();

  // -----------------------------------------------------
  //  Load the correct Bios ROM for the given machine
  // -----------------------------------------------------
  if (sordm5_mode)
  {
      if (myConfig.isPAL)
        memcpy(RAM_Memory,SordM5BiosEU,0x2000);
      else
        memcpy(RAM_Memory,SordM5BiosJP,0x2000);
  }
  else if (pv2000_mode)
  {
      memcpy(RAM_Memory,PV2000Bios,0x4000);
  }
  else if (memotech_mode)
  {
      memotech_restore_bios();
  }
  else if (msx_mode)
  {
      msx_restore_bios();
  }
  else if (svi_mode)
  {
      svi_restore_bios();
  }
  else if (adam_mode)
  {
      adam_setup_bios();
  }
  else if (pencil2_mode)
  {
      memcpy(RAM_Memory,Pencil2Bios,0x2000);
  }
  else if (einstein_mode)
  {
      einstein_restore_bios();
  }
  else if (creativision_mode)
  {
      creativision_restore_bios();
  }
  else  // Finally we get to the Coleco BIOS
  {
      if (myConfig.cpuCore == 0)    // If DrZ80 core... put the BIOS into main RAM for now
      {
        memcpy(RAM_Memory,ColecoBios,0x2000);
      }
  }
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

    memset(BIOS_Memory, 0xFF, 0x10000); // All of BIOS area is FF until loaded up

    // --------------------------------------------------
    // We will look for all 3 BIOS files here but only
    // the Colecovision coleco.rom is critical.
    // --------------------------------------------------
    bColecoBiosFound = false;
    bSordBiosFound = false;
    bMSXBiosFound = false;
    bSVIBiosFound = false;
    bAdamBiosFound = false;
    bPV2000BiosFound = false;
    bPencilBiosFound = false;
    bEinsteinBiosFound = false;
    bCreativisionBiosFound = false;

    // -----------------------------------------------------------
    // First load Sord M5 bios - don't really care if this fails
    // -----------------------------------------------------------
    fp = fopen("sordm5.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/sordm5.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/sordm5.rom", "rb");
    if (fp != NULL)
    {
        bSordBiosFound = true;
        fread(SordM5BiosJP, 0x2000, 1, fp);
        fclose(fp);
    }

    fp = fopen("sordm5p.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/sordm5p.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/sordm5p.rom", "rb");
    if (fp != NULL)
    {
        bSordBiosFound = true;
        fread(SordM5BiosEU, 0x2000, 1, fp);
        fclose(fp);
    }
    else 
    {
        memcpy(SordM5BiosEU, SordM5BiosJP, 0x2000); // Otherwise the JP bios will have to do for both
    }
    
    // -----------------------------------------------------------
    // Try to load the Casio PV-2000 ROM BIOS
    // -----------------------------------------------------------
    fp = fopen("pv2000.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/pv2000.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/pv2000.rom", "rb");
    if (fp != NULL)
    {
        bPV2000BiosFound = true;
        fread(PV2000Bios, 0x4000, 1, fp);
        fclose(fp);
    }

    // -----------------------------------------------------------
    // Next try to load the MSX.ROM or reasonable equivilents.
    // If this fails we still have the C-BIOS as a backup.
    // -----------------------------------------------------------
    
    fp = NULL;
    
    // First try the Panasonic CF-2700 ROM which is a fairly generic MSX machine
    if (fp == NULL)
    {
        if (fp == NULL) fp = fopen("cf-2700.rom", "rb");
        if (fp == NULL) fp = fopen("/roms/bios/cf-2700.rom", "rb");
        if (fp == NULL) fp = fopen("/data/bios/cf-2700.rom", "rb");    
        if (fp != NULL) {strcpy(msx_rom_str, "CF2700.ROM 64K"); strcpy(msx_rom_str_short, "CF-2700");}
    }

    // First try the Panasonic CF-2700 ROM which is a fairly generic MSX machine
    if (fp == NULL)
    {
        if (fp == NULL) fp = fopen("cf2700.rom", "rb");
        if (fp == NULL) fp = fopen("/roms/bios/cf2700.rom", "rb");
        if (fp == NULL) fp = fopen("/data/bios/cf2700.rom", "rb");    
        if (fp != NULL) {strcpy(msx_rom_str, "CF2700.ROM 64K"); strcpy(msx_rom_str_short, "CF-2700");}
    }
    
    if (fp == NULL)
    {
        if (fp == NULL) fp = fopen("cf-2700_basic-bios1_gb.rom", "rb");
        if (fp == NULL) fp = fopen("/roms/bios/cf-2700_basic-bios1_gb.rom", "rb");
        if (fp == NULL) fp = fopen("/data/bios/cf-2700_basic-bios1_gb.rom", "rb");    
        if (fp != NULL) {strcpy(msx_rom_str, "CF2700.ROM 64K"); strcpy(msx_rom_str_short, "CF-2700");}
    }
    
    // Next try the Goldstar FC-200 ROM which is a fairly generic MSX machine
    if (fp == NULL)
    {
        if (fp == NULL) fp = fopen("fc-200.rom", "rb");
        if (fp == NULL) fp = fopen("/roms/bios/fc-200.rom", "rb");
        if (fp == NULL) fp = fopen("/data/bios/fc-200.rom", "rb");    
        if (fp != NULL) {strcpy(msx_rom_str, "FC-200.ROM 64K"); strcpy(msx_rom_str_short, "FC-200");}
    }

    // If msx.rom not found, try the Goldstar FC-200 ROM which is a fairly generic MSX machine
    if (fp == NULL)
    {
        if (fp == NULL) fp = fopen("fc-200_basic-bios1.rom", "rb");
        if (fp == NULL) fp = fopen("/roms/bios/fc-200_basic-bios1.rom", "rb");
        if (fp == NULL) fp = fopen("/data/bios/fc-200_basic-bios1.rom", "rb");    
        if (fp != NULL) {strcpy(msx_rom_str, "FC-200.ROM 64K"); strcpy(msx_rom_str_short, "FC-200");}
    }

    // If any of the above not found, try the Casio MX-15 ROM which is a fairly generic MSX machine though we "expand it" to 64K
    if (fp == NULL)
    {
        if (fp == NULL) fp = fopen("mx-15.rom", "rb");
        if (fp == NULL) fp = fopen("/roms/bios/mx-15.rom", "rb");
        if (fp == NULL) fp = fopen("/data/bios/mx-15.rom", "rb");    
        if (fp != NULL) {strcpy(msx_rom_str, "MX-15.ROM 64K"); strcpy(msx_rom_str_short, "MX-15");}
    }
    
    // Last we try to find the ubiquitous msx.rom which is some generic machine of unknown origin
    if (fp == NULL)
    {
        if (fp == NULL) fp = fopen("msx.rom", "rb");
        if (fp == NULL) fp = fopen("/roms/bios/msx.rom", "rb");
        if (fp == NULL) fp = fopen("/data/bios/msx.rom", "rb");
        if (fp != NULL) {strcpy(msx_rom_str, "MSX.ROM 64K"); strcpy(msx_rom_str_short, "MSX");}
    }
    
    if (fp != NULL)
    {
        bMSXBiosFound = true;
        fread(BIOS_Memory, 0x8000, 1, fp);
        fclose(fp);

        msx_patch_bios();   // Patch BIOS for cassette use

        // Now store this BIOS up into memory so we can use it later in msx_restore_bios()
        memcpy(MSX_Bios, BIOS_Memory, 0x8000);
    }

    // Yamaha CX5M
    fp = fopen("cx5m.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/cx5m.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/cx5m.rom", "rb");

    if (fp == NULL) fp = fopen("cx5m_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/cx5m_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/cx5m_basic-bios1.rom", "rb");
    
    if (fp != NULL)
    {
        bMSXBiosFound = true;
        fread(BIOS_Memory, 0x8000, 1, fp);
        fclose(fp);

        msx_patch_bios();   // Patch BIOS for cassette use

        // Now store this BIOS up into VRAM where we can use it later in msx_restore_bios()
        u8 *ptr = (u8*) (0x06880000 + 0x0000);
        memcpy(ptr, BIOS_Memory, 0x8000);
    }

    // Toshiba HX-10
    fp = fopen("hx-10.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/hx-10.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/hx-10.rom", "rb");
    
    if (fp == NULL) fp = fopen("hx-10_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/hx-10_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/hx-10_basic-bios1.rom", "rb");
    
    if (fp != NULL)
    {
        bMSXBiosFound = true;
        fread(BIOS_Memory, 0x8000, 1, fp);
        fclose(fp);

        msx_patch_bios();   // Patch BIOS for cassette use

        // Now store this BIOS up into VRAM where we can use it later in msx_restore_bios()
        u8 *ptr = (u8*) (0x06880000 + 0x8000);
        memcpy(ptr, BIOS_Memory, 0x8000);
    }

    // Sony Hit-Bit HB-10 (Japan)
    fp = fopen("hb-10.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/hb-10.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/hb-10.rom", "rb");
    
    if (fp == NULL) fp = fopen("hb-10_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/hb-10_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/hb-10_basic-bios1.rom", "rb");
    
    
    // Casio PV-7 and PV-16 are the exact same BIOS as the HB-10 so if we couldn't find that, we try the pv-7
    if (fp == NULL) fp = fopen("pv-7.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/pv-7.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/pv-7.rom", "rb");
    
    if (fp == NULL) fp = fopen("pv-7_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/pv-7_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/pv-7_basic-bios1.rom", "rb");
    
    if (fp == NULL) fp = fopen("pv-16.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/pv-16.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/pv-16.rom", "rb");        
    
    if (fp != NULL)
    {
        bMSXBiosFound = true;
        fread(BIOS_Memory, 0x8000, 1, fp);
        fclose(fp);

        msx_patch_bios();   // Patch BIOS for cassette use

        // Now store this BIOS up into VRAM where we can use it later in msx_restore_bios()
        u8 *ptr = (u8*) (0x06880000 + 0x10000);
        memcpy(ptr, BIOS_Memory, 0x8000);
    }

    // National FS-1300 (Japan)
    fp = fopen("fs-1300.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/fs-1300.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/fs-1300.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/fs1300.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/fs1300.rom", "rb");
    
    if (fp == NULL) fp = fopen("fs-1300_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/fs-1300_basic-bios1.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/fs-1300_basic-bios1.rom", "rb");
    
    if (fp != NULL)
    {
        bMSXBiosFound = true;
        fread(BIOS_Memory, 0x8000, 1, fp);
        fclose(fp);

        msx_patch_bios();   // Patch BIOS for cassette use

        // Now store this BIOS up into VRAM where we can use it later in msx_restore_bios()
        u8 *ptr = (u8*) (0x06880000 + 0x18000);
        memcpy(ptr, BIOS_Memory, 0x8000);
    }

    
    // -----------------------------------------------------------
    // Next try to load the SVI.ROM
    // -----------------------------------------------------------
    fp = fopen("svi.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/svi.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/svi.rom", "rb");
    if (fp != NULL)
    {
        bSVIBiosFound = true;
        fread(SVIBios, 0x8000, 1, fp);
        fclose(fp);
    }

    // -----------------------------------------------------------
    // Next try to load the PENCIL2.ROM
    // -----------------------------------------------------------
    fp = fopen("pencil2.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/pencil2.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/pencil2.rom", "rb");
    if (fp != NULL)
    {
        bPencilBiosFound = true;
        fread(Pencil2Bios, 0x2000, 1, fp);
        fclose(fp);
    }

    // -----------------------------------------------------------
    // Next try to load the EINSTIEN.ROM
    // -----------------------------------------------------------
    fp = fopen("einstein.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/einstein.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/einstein.rom", "rb");
    if (fp != NULL)
    {
        bEinsteinBiosFound = true;
        fread(EinsteinBios, 0x2000, 1, fp);
        fclose(fp);
    }

    // -----------------------------------------------------------
    // Next try to load the EINSTIEN2.ROM (diagnostcs, etc)
    // -----------------------------------------------------------
    fp = fopen("einstein2.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/einstein2.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/einstein2.rom", "rb");
    if (fp != NULL)
    {
        fread(EinsteinBios2, 0x2000, 1, fp);
        fclose(fp);
    } else memset(EinsteinBios2, 0xFF, 0x2000);
    
    // -----------------------------------------------------------
    // Next try to load the bioscv.rom (creativision)
    // -----------------------------------------------------------
    fp = fopen("bioscv.rom", "rb");
    if (fp == NULL) fp = fopen("/roms/bios/bioscv.rom", "rb");
    if (fp == NULL) fp = fopen("/data/bios/bioscv.rom", "rb");
    if (fp != NULL)
    {
        bCreativisionBiosFound = true;
        fread(CreativisionBios, 0x800, 1, fp);
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

  if  (!fatInitDefault()) {
     iprintf("Unable to initialize libfat!\n");
     return -1;
  }

  highscore_init();

  lcdMainOnTop();

  //  Init timer for frame management
  TIMER2_DATA=0;
  TIMER2_CR=TIMER_ENABLE|TIMER_DIV_1024;
  dsInstallSoundEmuFIFO();

  //  Show the fade-away intro logo...
  intro_logo();

  SetYtrigger(190); //trigger 2 lines before vsync

  irqSet(IRQ_VBLANK,  irqVBlank);
  irqEnable(IRQ_VBLANK);
    
  
  // -----------------------------------------------------------------
  // Grab the BIOS before we try to switch any directories around...
  // -----------------------------------------------------------------
  useVRAM();
  LoadBIOSFiles();

  // -----------------------------------------------------------------
  // And do an initial load of configuration... We'll match it up
  // with the game that was selected later...
  // -----------------------------------------------------------------
  LoadConfig();

  //  Handle command line argument... mostly for TWL++
  if  (argc > 1)
  {
      //  We want to start in the directory where the file is being launched...
      if  (strchr(argv[1], '/') != NULL)
      {
          static char  path[128];
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
        if (myGlobalConfig.showBiosInfo)
        {
            u8 idx = 6;
            DSPrint(2,idx++,0,"LOADING BIOS FILES ..."); idx++;
                                         DSPrint(2,idx++,0,"coleco.rom     BIOS FOUND");
            if (bMSXBiosFound)          {DSPrint(2,idx++,0,"msx.rom        BIOS FOUND"); }
            if (bSVIBiosFound)          {DSPrint(2,idx++,0,"svi.rom        BIOS FOUND"); }
            if (bSordBiosFound)         {DSPrint(2,idx++,0,"sordm5.rom     BIOS FOUND"); }
            if (bPV2000BiosFound)       {DSPrint(2,idx++,0,"pv2000.rom     BIOS FOUND"); }
            if (bPencilBiosFound)       {DSPrint(2,idx++,0,"pencil2.rom    BIOS FOUND"); }
            if (bEinsteinBiosFound)     {DSPrint(2,idx++,0,"einstein.rom   BIOS FOUND"); }
            if (bCreativisionBiosFound) {DSPrint(2,idx++,0,"bioscv.rom     BIOS FOUND"); }
            if (bAdamBiosFound)         {DSPrint(2,idx++,0,"eos.rom        BIOS FOUND"); }
            if (bAdamBiosFound)         {DSPrint(2,idx++,0,"writer.rom     BIOS FOUND"); }
            DSPrint(2,idx++,0,"SG-1000/3000 AND MTX BUILT-IN"); idx++;
            DSPrint(2,idx++,0,"TOUCH SCREEN / KEY TO BEGIN"); idx++;

            while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
            while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))==0);
            while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
        }
    }
    else
    {
        DSPrint(2,10,0,"ERROR: coleco.rom NOT FOUND");
        DSPrint(2,12,0,"ERROR: CANT RUN WITHOUT BIOS");
        DSPrint(2,14,0,"Put coleco.rom in same dir");
        DSPrint(2,15,0,"as EMULATOR or /ROMS/BIOS");
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

