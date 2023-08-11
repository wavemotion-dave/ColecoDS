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
#include "ecranBas.h"
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
#include "pv2000_sm.h"
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
#include "cvision.h"

#include "soundbank.h"
#include "soundbank_bin.h"
#include "MSX_CBIOS.h"
#include "MTX_BIOS.h"
#include "C24XX.h"
#include "screenshot.h"

#include "cpu/sn76496/SN76496.h"
#include "cpu/sn76496/Fake_AY.h"
#include "cpu/z80/Z80_interface.h"

#include "printf.h"

extern Z80 CPU;
u32 debug1=0;
u32 debug2=0;
u32 debug3=0;
u32 debug4=0;
u32 debug5=0;
u32 debug6=0;

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
u8 BIOS_Memory[0x10000]               ALIGN(32) = {0};        // To hold our BIOS and related OS memory (64K)
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
u8 SordM5Bios[0x2000]     = {0};  // We keep the Sord M5 8K BIOS around to swap in/out
u8 PV2000Bios[0x4000]     = {0};  // We keep the Casio PV-2000 16K BIOS around to swap in/out
u8 AdamEOS[0x2000]        = {0};  // We keep the ADAM EOS.ROM bios around to swap in/out
u8 AdamWRITER[0x8000]     = {0};  // We keep the ADAM WRITER.ROM bios around to swap in/out
u8 SVIBios[0x8000]        = {0};  // We keep the SVI 32K BIOS around to swap in/out
u8 Pencil2Bios[0x2000]    = {0};  // We keep the 8K Pencil 2 BIOS around to swap in/out
u8 EinsteinBios[0x2000]   = {0};  // We keep the 8k Einstein BIOS around
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
u8 adam_unsaved_data    = 0;
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

// ------------------------------------------------------------------------------------------
// Various sound chips in the system. We emulate the SN and AY sound chips but both of
// these really still use the underlying SN76496 sound chip driver for siplicity and speed.
// ------------------------------------------------------------------------------------------
extern SN76496 sncol;       // The SN sound chip is the main Colecovision sound
extern SN76496 aycol;       // The AY sound chip is for the Super Game Moudle
extern SN76496 sccABC;      // The SCC sound chip for MSX games that suport it (5 channels so we use these 3 and "steal" two more from sncol which is otherwise unused on MSX
extern SN76496 sccDE;       // The SCC sound chip for MSX games that suport it (5 channels so we use these 3 and "steal" two more from sncol which is otherwise unused on MSX
       SN76496 snmute;      // We keep this handy as a simple way to mute the sound

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
u8 pencil2_mode      __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .pii Pencil 2 ROM is loaded (only one known to exist!)
u8 einstein_mode     __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .com Einstien ROM is loaded
u8 creativision_mode __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a .cv ROM is loaded (Creativision)
u8 coleco_mode       __attribute__((section(".dtcm"))) = 0;       // Set to 1 when a Colecovision ROM is loaded

u16 machine_mode     __attribute__((section(".dtcm"))) = 0x0001;  // A faster way to know what type of machine we are

u8 kbd_key           __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the ASCII key (e.g. 'A', 'B', '3', etc)
u16 nds_key          __attribute__((section(".dtcm"))) = 0;       // 0 if no key pressed, othewise the NDS keys from keysCurrent() or similar
u8 last_mapped_key   __attribute__((section(".dtcm"))) = 0;       // The last mapped key which has been pressed - used for key click feedback
u8 kbd_keys_pressed  __attribute__((section(".dtcm"))) = 0;       // Each frame we check for keys pressed - since we can map keyboard keys to the NDS, there may be several pressed at once
u8 kbd_keys[12]      __attribute__((section(".dtcm")));           // Up to 12 possible keys pressed at the same time (we have 12 NDS physical buttons though it's unlikely that more than 2 or maybe 3 would be pressed)

u8 bStartSoundEngine = false;  // Set to true to unmute sound after 1 frame of rendering...
int bg0, bg1, bg0b, bg1b;      // Some vars for NDS background screen handling
volatile u16 vusCptVBL = 0;    // We use this as a basic timer for the Mario sprite... could be removed if another timer can be utilized

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
    META_KBD_STOP,
    META_KBD_F1,
    META_KBD_F2,
    META_KBD_F3,
    META_KBD_F4,
    META_KBD_F5,
    META_KBD_F6,
    META_KBD_F7,
    META_KBD_F8
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

// --------------------------------------------------------------------------------------------
// MAXMOD streaming setup and handling...
// We were using the normal ARM7 sound core but it sounded "scratchy" and so with the help
// of FluBBa, we've swiched over to the maxmod sound core which performs much better.
// --------------------------------------------------------------------------------------------
#define sample_rate  27965      // To match the driver in sn76496 - this is good enough quality for the DS
#define buffer_size  (512+12)   // Enough buffer that we don't have to fill it too often

mm_ds_system sys  __attribute__((section(".dtcm")));
mm_stream myStream __attribute__((section(".dtcm")));
u16 mixbuf1[2048];      // When we have SN and AY sound we have to mix 3+3 channels
u16 mixbuf2[2048];      // into a single output so we render to mix buffers first.
u16 mixbuf3[2048];      // into a single output so we render to mix buffers first.
u16 mixbuf4[2048];      // into a single output so we render to mix buffers first.

// -------------------------------------------------------------------------------------------
// maxmod will call this routine when the buffer is half-empty and requests that
// we fill the sound buffer with more samples. They will request 'len' samples and
// we will fill exactly that many. If the sound is paused, we fill with 'mute' samples.
// -------------------------------------------------------------------------------------------
u16 last_sample = 0;
mm_word OurSoundMixer(mm_word len, mm_addr dest, mm_stream_formats format)
{
    if (soundEmuPause)  // If paused, just "mix" in mute sound chip... all channels are OFF
    {
        u16 *p = (u16*)dest;
        for (int i=0; i<len*2; i++)
        {
           *p++ = last_sample;      // To prevent pops and clicks... just keep outputting the last sample
        }
    }
    else
    {
        if (machine_mode & (MODE_MSX | MODE_SVI | MODE_EINSTEIN))
        {
          if (msx_scc_enable)   // If SCC is enabled, we need to mix in 3 sound chips worth of channels... whew!
          {
              ay76496Mixer(len*4, mixbuf1, &aycol);
              ay76496Mixer(len*4, mixbuf3, &sccABC);
              ay76496Mixer(len*4, mixbuf4, &sccDE);
              u16 *p = (u16*)dest;
              for (int i=0; i<len*2; i++)
              {
                  // ------------------------------------------------------------------------
                  // We normalize the samples and mix them carefully to minimize clipping...
                  // ------------------------------------------------------------------------
                  s32 combined = mixbuf1[i];
                  combined += (s16)(mixbuf3[i] - 32768);
                  combined += (s16)(mixbuf4[i] - 32768);
                  if (combined > 65535) combined = 65535;
                  *p++ = (u16)combined;
              }
              p--; last_sample = *p;
          }
          else  // Pretty simple... just AY
          {
              ay76496Mixer(len*4, dest, &aycol);
              last_sample = ((u16*)dest)[len*2 - 1];
          }
        }
        else if (AY_Enable)  // If AY is enabled we mix the normal SN chip with the AY chip sound
        {
            ay76496Mixer(len*4, mixbuf1, &aycol);
            sn76496Mixer(len*4, mixbuf2, &sncol);
            u16 *p = (u16*)dest;
            for (int i=0; i<len*2; i++)
            {
                // ------------------------------------------------------------------------
                // We normalize the samples and mix them carefully to minimize clipping...
                // ------------------------------------------------------------------------
                s32 combined = mixbuf1[i];
                combined += (s16)(mixbuf2[i] - 32768);
                if (combined > 65535) combined = 65535;
                *p++ = (u16)combined;
            }
            p--; last_sample = *p;
        }
        else  // This is the 'normal' case of just Colecovision SN sound chip output
        {
            sn76496Mixer(len*4, dest, &sncol);
            last_sample = ((u16*)dest)[len*2 - 1];
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
  //  initialize maxmod with our small 3-effect soundbank
  //----------------------------------------------------------------
  mmInitDefaultMem((mm_addr)soundbank_bin);

  mmLoadEffect(SFX_CLICKNOQUIT);
  mmLoadEffect(SFX_KEYCLICK);
  mmLoadEffect(SFX_MUS_INTRO);

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


  //  -----------------------------------------------------------------------
  //  The "Fake SCC" sound chip channels ABC for games using this sound chip
  //  -----------------------------------------------------------------------
  ay76496Reset(2, &sccABC);         // Reset the "AY" sound chip

  ay76496W(0x80 | 0x00,&sccABC);    // Write new Frequency for Channel A
  ay76496W(0x00 | 0x00,&sccABC);    // Write new Frequency for Channel A
  ay76496W(0x90 | 0x0F,&sccABC);    // Write new Volume for Channel A

  ay76496W(0xA0 | 0x00,&sccABC);    // Write new Frequency for Channel B
  ay76496W(0x00 | 0x00,&sccABC);    // Write new Frequency for Channel B
  ay76496W(0xB0 | 0x0F,&sccABC);    // Write new Volume for Channel B

  ay76496W(0xC0 | 0x00,&sccABC);    // Write new Frequency for Channel C
  ay76496W(0x00 | 0x00,&sccABC);    // Write new Frequency for Channel C
  ay76496W(0xD0 | 0x0F,&sccABC);    // Write new Volume for Channel C

  ay76496W(0xFF,  &sccABC);         // Disable Noise Channel  (not used in SCC)

  sn76496Mixer(8, mixbuf3, &sccABC);  // Do an initial mix conversion to clear the output

  //  -----------------------------------------------------------------------
  //  The "Fake SCC" sound chip channels ABC for games using this sound chip
  //  -----------------------------------------------------------------------
  ay76496Reset(2, &sccDE);         // Reset the "AY" sound chip

  ay76496W(0x80 | 0x00,&sccDE);    // Write new Frequency for Channel D
  ay76496W(0x00 | 0x00,&sccDE);    // Write new Frequency for Channel D
  ay76496W(0x90 | 0x0F,&sccDE);    // Write new Volume for Channel D

  ay76496W(0xA0 | 0x00,&sccDE);    // Write new Frequency for Channel E
  ay76496W(0x00 | 0x00,&sccDE);    // Write new Frequency for Channel E
  ay76496W(0xB0 | 0x0F,&sccDE);    // Write new Volume for Channel E

  ay76496W(0xC0 | 0x00,&sccDE);    // Write new Frequency for Channel F (not used)
  ay76496W(0x00 | 0x00,&sccDE);    // Write new Frequency for Channel  F (not used)
  ay76496W(0xD0 | 0x0F,&sccDE);    // Write new Volume for Channel  F (not used)

  ay76496W(0xFF,  &sccDE);         // Disable Noise Channel (not used in SCC)

  sn76496Mixer(8, mixbuf4, &sccDE);  // Do an initial mix conversion to clear the output

  setupStream();    // Setup maxmod stream...

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
static u8 last_scc_mode = 0;


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
  last_scc_mode = 0;
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

  sn76496Reset(1, &sncol);              // Reset the SN sound chip
  sn76496W(0x90 | 0x0F  ,&sncol);       //  Write new Volume for Channel A (off)
  sn76496W(0xB0 | 0x0F  ,&sncol);       //  Write new Volume for Channel B (off)
  sn76496W(0xD0 | 0x0F  ,&sncol);       //  Write new Volume for Channel C (off)

  ay76496Reset(2, &aycol);              // Reset the SN sound chip
  ay76496W(0x90 | 0x0F  ,&aycol);       //  Write new Volume for Channel A (off)
  ay76496W(0xB0 | 0x0F  ,&aycol);       //  Write new Volume for Channel B (off)
  ay76496W(0xD0 | 0x0F  ,&aycol);       //  Write new Volume for Channel C (off)

  DrZ80_Reset();                        // Reset the DrZ80 CPU core
  ResetZ80(&CPU);                       // Reset the Z80 CPU core

  sordm5_reset();                       // Reset the Sord M5 specific vars
  memotech_reset();                     // Reset the memotech MTX specific vars
  svi_reset();                          // Reset the SVI specific vars
  msx_reset();                          // Reset the MSX specific vars
  pv2000_reset();                       // Reset the PV2000 specific vars
  einstein_reset();                     // Reset the Tatung Einstein specific vars

  adam_CapsLock = 0;
  adam_unsaved_data = 0;

  write_EE_counter=0;

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
      memcpy(RAM_Memory,SordM5Bios,0x2000);     // Restore Sord M5 BIOS
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
  else if (adam_mode)
  {
      colecoWipeRAM();
      adam_128k_mode = 0;                       // Normal 64K ADAM to start
      SetupAdam(false);
  }
  else if (pencil2_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      memcpy(RAM_Memory,Pencil2Bios,0x2000);
  }
  else if (einstein_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      einstien_restore_bios();                  // Put the BIOS back in place and point to it
  }
  else if (creativision_mode)
  {
      colecoWipeRAM();                          // Wipe main RAM area
      creativision_restore_bios();
      creativision_reset();                     // Reset the Creativision and 6502 CPU - must be done after BIOS is loaded to get reset vector properly loaded
  }
  else
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

  XBuf = XBuf_A;                      // Set the initial screen ping-pong buffer to A

  ResetStatusFlags();   // Some static status flags for the UI mostly

  debug1 = 0;  debug2 = 0; debug3 = 0;  debug4 = 0; debug5 = 0; debug6 = 0;
}

//*********************************************************************************
// A mini Z80 debugger of sorts. Put out some Z80, VDP and SGM/Bank info on
// screen every frame to help us debug some of the problem games. This is enabled
// via a compile switch in colecoDS.h - uncomment the define for DEBUG_Z80 line.
//*********************************************************************************
const char *VModeNames[] =
{
    "VDP_0  ",
    "VDP_3  ",
    "VDP_2  ",
    "VDP_2+3",
    "VDP_1  ",
    "VDP_?5 ",
    "VDP_1+2",
    "VDP_?7 ",
};

void ShowDebugZ80(void)
{
    char tmp[33];
    u8 idx=1;
#ifdef FULL_DEBUG
    extern u8 a_idx, b_idx, c_idx;
    extern u8 lastBank;
    extern u8 romBankMask;
    extern u8 Port20, Port53, Port60;
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
    idx++;

    if (AY_Enable)
    {
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
        idx++;
    }
    else
    {
        siprintf(tmp, "SN0 %04X %04X %2d", sncol.ch0Frq, sncol.ch0Reg, sncol.ch0Att);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "SN1 %04X %04X %2d", sncol.ch1Frq, sncol.ch1Reg, sncol.ch1Att);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "SN2 %04X %04X %2d", sncol.ch2Frq, sncol.ch2Reg, sncol.ch2Att);
        AffChaine(0,idx++,7, tmp);
        siprintf(tmp, "SN3 %04X %04X %2d", sncol.ch3Frq, sncol.ch3Reg, sncol.ch3Att);
        AffChaine(0,idx++,7, tmp);
        idx++;
    }

    siprintf(tmp, "Bank  %02X [%02X]", (lastBank != 199 ? lastBank:0), romBankMask);    AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "PORTS P23=%02X P53=%02X P60=%02X", Port20, Port53, Port60);          AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "VMode %02X %s", TMS9918_Mode, VModeNames[TMS9918_Mode]);             AffChaine(0,idx++,7, tmp);
    siprintf(tmp, "VSize %s", ((TMS9918_VRAMMask == 0x3FFF) ? "16K":" 4K"));            AffChaine(0,idx++,7, tmp);

    idx = 1;
    siprintf(tmp, "D1 %-5lu %04X", debug1, (u16)debug1); AffChaine(19,idx++,7, tmp);
    siprintf(tmp, "D2 %-5lu %04X", debug2, (u16)debug2); AffChaine(19,idx++,7, tmp);
    siprintf(tmp, "D3 %-5lu %04X", debug3, (u16)debug3); AffChaine(19,idx++,7, tmp);
    siprintf(tmp, "D4 %-5lu %04X", debug4, (u16)debug4); AffChaine(19,idx++,7, tmp);
    idx++;
    siprintf(tmp, "SVI %s %s", (svi_RAM[0] ? "RAM":"ROM"), (svi_RAM[1] ? "RAM":"ROM")); AffChaine(19,idx++,7, tmp);
    siprintf(tmp, "PPI A=%02X B=%02X",Port_PPI_A,Port_PPI_B);    AffChaine(19,idx++,7, tmp);
    siprintf(tmp, "PPI C=%02X M=%02X",Port_PPI_C,Port_PPI_CTRL); AffChaine(19,idx++,7, tmp);
#else
    idx = 1;
    siprintf(tmp, "D1 %-12lu %08lX", debug1, debug1); AffChaine(5,idx++,7, tmp);
    siprintf(tmp, "D2 %-12lu %08lX", debug2, debug2); AffChaine(5,idx++,7, tmp);
    siprintf(tmp, "D3 %-12lu %08lX", debug3, debug3); AffChaine(5,idx++,7, tmp);
    siprintf(tmp, "D4 %-12lu %08lX", debug4, debug4); AffChaine(5,idx++,7, tmp);
    siprintf(tmp, "D5 %-12lu %08lX", debug5, debug5); AffChaine(5,idx++,7, tmp);
    siprintf(tmp, "D6 %-12lu %08lX", debug6, debug6); AffChaine(5,idx++,7, tmp);
    
    for (int chan=0; chan<=3; chan++)
    {
        siprintf(tmp, "CTC%d control  = 0x%02X    ", chan, CTC[chan].control); AffChaine(5,idx++,7, tmp);
        siprintf(tmp, "CTC%d constant = %-8lu", chan, (u32)CTC[chan].constant); AffChaine(5,idx++,7, tmp);
        siprintf(tmp, "CTC%d counter  = %-8lu", chan, (u32)CTC[chan].counter); AffChaine(5,idx++,7, tmp);
    }
    
#endif
    idx++;
}



// ------------------------------------------------------------
// The status line shows the status of the Super Game Moudle,
// AY sound chip support and MegaCart support.  Game players
// probably don't care, but it's really helpful for devs.
// ------------------------------------------------------------
u8 last_pal_mode = 99;
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
            AffChaine(23,0,6, (sg1000_mode == 2 ? "SC-3000":"SG-1000"));
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL  && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            AffChaine(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
    }
    else if (pv2000_mode)
    {
        if ((last_pv2000_mode != pv2000_mode) || bForce)
        {
            last_pv2000_mode = pv2000_mode;
            AffChaine(23,0,6, "PV-2000");
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
    else if (memotech_mode)
    {
        if ((last_memotech_mode != memotech_mode) || bForce)
        {
            last_memotech_mode = memotech_mode;
            AffChaine(20,0,6, "MEMOTECH MTX");
            last_pal_mode = 99;
        }
        if ((memotech_mode == 2) && (last_tape_pos != tape_pos) && (!memotech_magrom_present))
        {
            last_tape_pos = tape_pos;
            char tmp[15];
            siprintf(tmp, "CAS %d%%  ", (int)(100 * (int)tape_pos)/(int)tape_len);
            AffChaine(9,0,6, tmp);
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            AffChaine(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
    }
    else if (msx_mode)
    {
        if ((last_msx_mode != msx_mode) || bForce)
        {
            char tmp[16];
            last_msx_mode = msx_mode;
            switch (myConfig.msxBios)
            {
                case 1: siprintf(tmp, "%-7s %3dK", msx_rom_str_short, (int)(LastROMSize/1024));    break;     // MSX (64K machine... use variable name)
                case 2: siprintf(tmp, "CX5M    %3dK",                  (int)(LastROMSize/1024));    break;     // Yamaha CX5M (32K mapped in slot 0)
                case 3: siprintf(tmp, "HX-10   %3dK",                  (int)(LastROMSize/1024));    break;     // Toshiba HX-10 (64K mapped in slot 2)
                case 4: siprintf(tmp, "HB-10   %3dK",                  (int)(LastROMSize/1024));    break;     // Sony HB-10 (16K mapped in slot 0)
                case 5: siprintf(tmp, "FS-1300 %3dK",                  (int)(LastROMSize/1024));    break;     // National FS-1300 (64K mapped in slot 3)
                case 6: siprintf(tmp, "PV-7    %3dK",                  (int)(LastROMSize/1024));    break;     // Casio PV-7 (just 8K at the top of slot 0)
                default:siprintf(tmp, "MSX     %3dK",                  (int)(LastROMSize/1024));    break;     // C-BIOS as a fall-back (64K mapped in slot 3)
            }            
            AffChaine(20,0,6, tmp);
            last_pal_mode = 99;
        }
        if (last_msx_scc_enable != msx_scc_enable)
        {   
            // SCC and CAS are mutually exclusive so we can reuse the same area on screen...
            AffChaine(9,0,6, "SCC");
            last_msx_scc_enable = msx_scc_enable;
        }
        if ((last_tape_pos != tape_pos) && (msx_mode == 2))
        {
            last_tape_pos = tape_pos;
            char tmp[15];
            siprintf(tmp, "CAS %d%%  ", (int)(100 * (int)tape_pos)/(int)tape_len);
            AffChaine(9,0,6, tmp);
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL  && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            AffChaine(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
        if (write_EE_counter > 0)
        {
            --write_EE_counter;
            if (write_EE_counter == 0)
            {
                // Save EE now!
                msxSaveEEPROM();
            }
            AffChaine(5,0,6, (write_EE_counter ? "EE":"  "));
        }

    }
    else if (svi_mode)
    {
        if ((last_svi_mode != svi_mode) || bForce)
        {
            last_svi_mode = svi_mode;
            AffChaine(20,0,6, "SPECTRAVIDEO");
            last_pal_mode = 99;
        }
        if ((last_tape_pos != tape_pos) && (svi_mode == 1))
        {
            last_tape_pos = tape_pos;
            char tmp[15];
            siprintf(tmp, "CAS %d%%  ", (int)(100 * (int)tape_pos)/(int)tape_len);
            AffChaine(9,0,6, tmp);
        }
        if (last_pal_mode != myConfig.isPAL)
        {
            last_pal_mode = myConfig.isPAL;
            AffChaine(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
    }
    else if (adam_mode)
    {
        if ((last_adam_mode != adam_mode) || bForce)
        {
            last_adam_mode = adam_mode;
            AffChaine(25,0,6, "ADAM");
        }

        AffChaine(20,0,6, (adam_CapsLock ? "CAP":"   "));

        if (io_show_status)
        {
            AffChaine(30,0,6, (io_show_status == 2 ? "WR":"RD"));
            io_show_status = 0;
        }
        else
            AffChaine(30,0,6, "  ");
    }
    else if (pencil2_mode)
    {
        if ((pencil2_mode != last_pencil_mode) || bForce)
        {
            last_pencil_mode = pencil2_mode;
            AffChaine(22,0,6, "PENCIL II");
        }
    }
    else if (creativision_mode)
    {
        if ((creativision_mode != last_pencil_mode) || bForce)
        {
            last_pencil_mode = creativision_mode;
            AffChaine(20,0,6, "CREATIVISION");
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL  && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            AffChaine(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
    }
    else if (einstein_mode)
    {
        if ((einstein_mode != last_einstein_mode) || bForce)
        {
            AffChaine(22,0,6, "EINSTEIN");
            last_pal_mode = 99;
        }
        if (last_pal_mode != myConfig.isPAL  && !myGlobalConfig.showFPS)
        {
            last_pal_mode = myConfig.isPAL;
            AffChaine(0,0,6, myConfig.isPAL ? "PAL":"   ");
        }
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

        if (write_EE_counter > 0)
        {
            --write_EE_counter;
            if (write_EE_counter == 0)
            {
                // Save EE now!
                colecoSaveEEPROM();
            }
            AffChaine(30,0,6, (write_EE_counter ? "EE":"  "));
        }
    }
}



// ------------------------------------------------------------------------
// Save out the ADAM .ddp or .dsk file and show 'SAVING' on screen
// ------------------------------------------------------------------------
void SaveAdamTapeOrDisk(void)
{
    if (io_show_status) return; // Don't save while io status

    AffChaine(12,0,6, "SAVING");
    if (strstr(lastAdamDataPath, ".ddp") != 0)
        SaveFDI(&Tapes[0], lastAdamDataPath, FMT_DDP);
    else
        SaveFDI(&Disks[0], lastAdamDataPath, FMT_ADMDSK);
    AffChaine(12,0,6, "      ");
    DisplayStatusLine(true);
    adam_unsaved_data = 0;
}


void DigitalDataInsert(char *filename)
{
    FILE *fp;

    // --------------------------------------------
    // Read the .DDP or .DSK into the ROM_Memory[]
    // --------------------------------------------
    fp = fopen(filename, "rb");
    fseek(fp, 0, SEEK_END);
    LastROMSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    memset(ROM_Memory, 0xFF, (MAX_CART_SIZE * 1024));
    fread(ROM_Memory, tape_len, 1, fp);
    fclose(fp);

    // --------------------------------------------
    // And set it as the active ddp or dsk...
    // --------------------------------------------
    strcpy(lastAdamDataPath, filename);
    if (strstr(lastAdamDataPath, ".ddp") != 0)
    {
        ChangeTape(0, lastAdamDataPath);
    }
    else
    {
        ChangeDisk(0, lastAdamDataPath);
    }
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


// ------------------------------------------------------------------------
// Show the Cassette Menu text - highlight the selected row.
// ------------------------------------------------------------------------
u8 cassete_menu_items = 0;
void CassetteMenuShow(bool bClearScreen, u8 sel)
{
    cassete_menu_items = 0;
    if (bClearScreen)
    {
      // ---------------------------------------------------
      // Put up a generic background for this mini-menu...
      // ---------------------------------------------------
      dmaCopy((void*) bgGetMapPtr(bg0b)+30*32*2,(void*) bgGetMapPtr(bg0b),32*24*2);
      unsigned short dmaVal = *(bgGetMapPtr(bg0b)+24*32);
      dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
      swiWaitForVBlank();
    }

    if (adam_mode)
    {
        AffChaine(8,8,6,                    "DIGITAL DATA MENU");
        AffChaine(8,10+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " SAVE DDP/DSK  ");  cassete_menu_items++;
        AffChaine(8,10+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " SWAP DDP/DSK  ");  cassete_menu_items++;
        AffChaine(8,10+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " EXIT MENU     ");  cassete_menu_items++;
        if (adam_unsaved_data)
        {
            AffChaine(3, 15, 0, "DDP/DSK HAS UNSAVED DATA!");
        }
    }
    else
    {
        AffChaine(9,7,6,                    "CASSETTE MENU");
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " SAVE CASSETTE    ");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " SWAP CASSETTE    ");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " REWIND CASSETTE  ");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " CLOAD  RUN       ");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " BLOAD 'CAS:',R   ");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " RUN   'CAS:'     ");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " LOAD  ''         ");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " RUN              ");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " RUN EINSTEIN .COM");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " RUN MEMOTECH .RUN");  cassete_menu_items++;
        AffChaine(8,9+cassete_menu_items,(sel==cassete_menu_items)?2:0,  " EXIT MENU        ");  cassete_menu_items++;
    }
    DisplayFileName();
}

// ------------------------------------------------------------------------
// Handle Cassette mini-menu interface...
// ------------------------------------------------------------------------
void CassetteMenu(void)
{
  u8 menuSelection = 0;

  SoundPause();
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  if (creativision_mode)
  {
      AffChaine(5,0,0, "BAS LOADING");
      creativision_loadBAS();
      WAITVBL;WAITVBL;WAITVBL;WAITVBL;
      BufferKey('R');
      BufferKey('U');
      BufferKey('N');
      BufferKey(KBD_KEY_RET);
      AffChaine(5,0,0, "           ");
      SoundUnPause();
      return;
  }

  CassetteMenuShow(true, menuSelection);

  while (true)
  {
    nds_key = keysCurrent();
    if (nds_key)
    {
        if (nds_key & KEY_UP)
        {
            menuSelection = (menuSelection > 0) ? (menuSelection-1):(cassete_menu_items-1);
            CassetteMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_DOWN)
        {
            menuSelection = (menuSelection+1) % cassete_menu_items;
            CassetteMenuShow(false, menuSelection);
        }
        if (nds_key & KEY_A)
        {
            if (menuSelection == 0) // SAVE CASSETTE
            {
                if  (showMessage("DO YOU REALLY WANT TO","WRITE CASSETTE DATA?") == ID_SHM_YES)
                {
                    if (adam_mode)
                    {
                        SaveAdamTapeOrDisk();
                    }
                    else
                    {
                        if (msx_mode || svi_mode)   // Not supporting Memotech MTX yet...
                        {
                            AffChaine(12,0,6, "SAVING");
                            FILE *fp;
                            fp = fopen(gpFic[ucGameChoice].szName, "wb");
                            fwrite(ROM_Memory, tape_len, 1, fp);
                            fclose(fp);
                            WAITVBL;WAITVBL;
                            AffChaine(12,0,6, "      ");
                            DisplayStatusLine(true);
                        }
                    }
                }
                CassetteMenuShow(true, menuSelection);
            }
            if (menuSelection == 1) // SWAP CASSETTE
            {
                colecoDSLoadFile();
                if (ucGameChoice >= 0)
                {
                    if (adam_mode)
                    {
                        DigitalDataInsert(gpFic[ucGameChoice].szName);
                    }
                    else
                    {
                        CassetteInsert(gpFic[ucGameChoice].szName);
                    }
                    break;
                }
                else
                {
                    CassetteMenuShow(true, menuSelection);
                }
            }
            if (menuSelection == 2) // REWIND (ADAM = EXIT)
            {
                  if (!adam_mode && (tape_pos>0))
                  {
                      tape_pos = 0;
                      AffChaine(12,0,6, "REWOUND");
                      WAITVBL;WAITVBL;WAITVBL;WAITVBL;
                      AffChaine(12,0,6, "       ");
                      DisplayStatusLine(true);
                      CassetteMenuShow(true, menuSelection);
                  }
                  if (adam_mode) break;
            }
            if (menuSelection == 3)
            {
                  BufferKey('C');
                  BufferKey('L');
                  BufferKey('O');
                  BufferKey('A');
                  BufferKey('D');
                  BufferKey(KBD_KEY_RET);
                  BufferKey(255);
                  BufferKey('R');
                  BufferKey('U');
                  BufferKey('N');
                  BufferKey(KBD_KEY_RET);
                  break;
            }
            if (menuSelection == 4)
            {
                  BufferKey('B');
                  BufferKey('L');
                  BufferKey('O');
                  BufferKey('A');
                  BufferKey('D');
                  BufferKey(KBD_KEY_SHIFT);
                  BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                  BufferKey('C');
                  BufferKey('A');
                  BufferKey('S');
                  if (msx_mode && !msx_japanese_matrix) BufferKey(KBD_KEY_SHIFT);
                  BufferKey(msx_japanese_matrix ? KBD_KEY_QUOTE : ':');
                  BufferKey(KBD_KEY_SHIFT);
                  BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                  BufferKey(',');
                  BufferKey('R');
                  BufferKey(KBD_KEY_RET);
                  break;
            }
            if (menuSelection == 5)
            {
                  BufferKey('R');
                  BufferKey('U');
                  BufferKey('N');
                  BufferKey(KBD_KEY_SHIFT);
                  BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                  BufferKey('C');
                  BufferKey('A');
                  BufferKey('S');
                  if (msx_mode && !msx_japanese_matrix) BufferKey(KBD_KEY_SHIFT);
                  BufferKey(msx_japanese_matrix ? KBD_KEY_QUOTE : ':');
                  BufferKey(KBD_KEY_SHIFT);
                  BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                  BufferKey(KBD_KEY_RET);
                  break;
            }
            if (menuSelection == 6)
            {
                  BufferKey('L');
                  BufferKey('O');
                  BufferKey('A');
                  BufferKey('D');
                  BufferKey(KBD_KEY_SHIFT);
                  BufferKey('2');
                  BufferKey(KBD_KEY_SHIFT);
                  BufferKey('2');
                  BufferKey(KBD_KEY_RET);
                  break;
            }
            if (menuSelection == 7)
            {
                  BufferKey('R');
                  BufferKey('U');
                  BufferKey('N');
                  BufferKey(KBD_KEY_RET);
                  break;
            }
            if (menuSelection == 8)
            {
                  einstein_load_com_file();
                  break;
            }
            if (menuSelection == 9)
            {
                memotech_launch_run_file();
                break;
            }
            if (menuSelection == 10)
            {
                break;
            }
        }
        if (nds_key & KEY_B)
        {
            break;
        }

        while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
        WAITVBL;WAITVBL;
    }
  }

  while ((keysCurrent() & (KEY_UP | KEY_DOWN | KEY_A ))!=0);
  WAITVBL;WAITVBL;

  InitBottomScreen();  // Could be generic or overlay...

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
      dmaCopy((void*) bgGetMapPtr(bg0b)+30*32*2,(void*) bgGetMapPtr(bg0b),32*24*2);
      unsigned short dmaVal = *(bgGetMapPtr(bg0b)+24*32);
      dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
      swiWaitForVBlank();
    }

    AffChaine(8,7,6,                                           " CV MINI MENU  ");
    AffChaine(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " RESET  GAME   ");  mini_menu_items++;
    AffChaine(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " QUIT   GAME   ");  mini_menu_items++;
    AffChaine(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " HIGH   SCORE  ");  mini_menu_items++;
    AffChaine(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " SAVE   STATE  ");  mini_menu_items++;
    AffChaine(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " LOAD   STATE  ");  mini_menu_items++;
    AffChaine(8,9+mini_menu_items,(sel==mini_menu_items)?2:0,  " EXIT   MENU   ");  mini_menu_items++;
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

  InitBottomScreen();  // Could be generic or overlay...

  SoundUnPause();

  return retVal;
}


// ------------------------------------------------------------------------
// Return 1 if we are showing full keyboard... otherwise 0
// ------------------------------------------------------------------------
inline u8 IsFullKeyboard(void) {return ((myConfig.overlay >= 9) ? 1:0);}

u8 last_special_key = 0;
u8 last_special_key_dampen = 0;
u8 last_kbd_key = 0;

u8 handle_adam_keyboard_press(u16 iTx, u16 iTy)
{
    if ((iTy >= 12) && (iTy < 42))        // Row 1 (top row with I-VI Smartkeys)
    {
        if      ((iTx >= 0)   && (iTx < 25))   kbd_key = ADAM_KEY_ESC;
        else if ((iTx >= 25)  && (iTx < 59))   kbd_key = ADAM_KEY_F1;
        else if ((iTx >= 59)  && (iTx < 88))   kbd_key = ADAM_KEY_F2;
        else if ((iTx >= 88)  && (iTx < 117))  kbd_key = ADAM_KEY_F3;
        else if ((iTx >= 117) && (iTx < 146))  kbd_key = ADAM_KEY_F4;
        else if ((iTx >= 146) && (iTx < 176))  kbd_key = ADAM_KEY_F5;
        else if ((iTx >= 176) && (iTx < 210))  kbd_key = ADAM_KEY_F6;
        else if ((iTx >= 210) && (iTx < 235))  kbd_key = ADAM_KEY_BS;
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
        if      ((iTx >= 0)   && (iTx < 27))   {kbd_key = 0; last_kbd_key = 0; last_special_key = KBD_KEY_CTRL; AffChaine(4,0,6, "CTRL");}
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
        if      ((iTx >= 0)   && (iTx < 33))   {kbd_key = 0;  last_kbd_key = 0; last_special_key = KBD_KEY_SHIFT; AffChaine(4,0,6, "SHFT");}
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
        else if ((iTx >= 33)  && (iTx < 190))  kbd_key = ' ';
        else if ((iTx >= 190) && (iTx < 235))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 235) && (iTx < 255))  return MENU_CHOICE_MENU;
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
        mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
        AffChaine(4,0,6, "    ");
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
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_STOP;
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
        else if ((iTx >= 163) && (iTx < 191))  {kbd_key = KBD_KEY_CODE; if (msx_japanese_matrix) {AffChaine(4,0,6,"KANA"); bKanaShown=1;} else {last_special_key = KBD_KEY_CODE; last_special_key_dampen = 20;}}
        else if ((iTx >= 191) && (iTx < 235))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 235) && (iTx < 255))  return MENU_CHOICE_MENU;
    }
    
    if ((kbd_key != 0) && (kbd_key != KBD_KEY_CODE) && bKanaShown)
    {
        AffChaine(4,0,6,"    ");
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
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_STOP;
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
        else if ((iTx >= 53)  && (iTx < 163))  kbd_key = ' ';
        else if ((iTx >= 163) && (iTx < 181))  {kbd_key = KBD_KEY_CODE; last_special_key = KBD_KEY_CODE; last_special_key_dampen = 20;}
        else if ((iTx >= 181) && (iTx < 225))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 225) && (iTx < 255))  return MENU_CHOICE_MENU;
    }
    
    if ((kbd_key != 0) && (kbd_key != KBD_KEY_CODE) && bKanaShown)
    {
        AffChaine(4,0,6,"    ");
        bKanaShown = 0;
    }

    return MENU_CHOICE_NONE;
}

u8 handle_mtx_keyboard_press(u16 iTx, u16 iTy)  // MTX/Etc Keyboard
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
        else if ((iTx >= 210) && (iTx < 255))  kbd_key = KBD_KEY_STOP;
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
        else if ((iTx >= 190) && (iTx < 235))  return MENU_CHOICE_CASSETTE;
        else if ((iTx >= 235) && (iTx < 255))  return MENU_CHOICE_MENU;
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
        if      ((iTx >= 0)   && (iTx < 25))   {kbd_key = KBD_KEY_CTRL; last_special_key = KBD_KEY_CTRL; last_special_key_dampen = 20;}
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
        if      ((iTx >= 0)   && (iTx < 27))   {kbd_key = KBD_KEY_SHIFT; last_special_key = KBD_KEY_SHIFT; last_special_key_dampen = 20;}
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
        else if ((iTx >= 214) && (iTx < 255))  kbd_key = KBD_KEY_STOP;
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
        if      ((iTx >= 1)   && (iTx < 34))   {kbd_key = KBD_KEY_GRAPH; last_special_key = KBD_KEY_GRAPH; last_special_key_dampen = 20;}
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
        else if ((iTx >= 226) && (iTx < 255))  kbd_key = (adam_mode ? ADAM_KEY_BS : KBD_KEY_BS);
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
            mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
            last_kbd_key = kbd_key;
        }
    }
    
    return MENU_CHOICE_NONE;
}


u8 handle_normal_virtual_keypad(u16 iTx, u16 iTy)  // All other normal overlays with keypad on the right and menu choices on the left
{
    // For ADAM, the standard overlay has a CASSETTE icon to save data...
    if (adam_mode && (myConfig.overlay == 0))
    {
        if ((iTy >= 9) && (iTy < 30) && (iTx >= 120) && (iTx <= 155))
        {
            return MENU_CHOICE_CASSETTE;
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
  u32 ucUN, ucDEUX;
  static u32 lastUN = 0;
  static u8 dampenClick = 0;
  u8 meta_key = 0;

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
            if (myGlobalConfig.showFPS)
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

            // A bit of a hack for the SC-3000 Survivors Multi-Cart
            extern u8 sg1000_double_reset;
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

            // --------------------------------------------
            // Time 1 frame... 546 ticks of Timer2
            // This is how we time frame-to frame
            // to keep the game running at 60FPS
            // --------------------------------------------
            while(TIMER2_DATA < ((myConfig.isPAL ? 656:546)*(timingFrames+1)))
            {
                if (myGlobalConfig.showFPS == 2) break;   // If Full Speed, break out...
            }
        }

      // If the Z80 Debugger is enabled, call it
#ifdef DEBUG_Z80
      ShowDebugZ80();
#endif


      // -----------------------------------------------------------
      // This is where we accumualte the keys pressed... up to 12!
      // -----------------------------------------------------------
      kbd_keys_pressed = 0;
      memset(kbd_keys, 0x00, sizeof(kbd_keys));
      kbd_key = 0;
        
      // ------------------------------------------
      // Handle any screen touch events
      // ------------------------------------------
      ucUN  = 0;
      if  (keysCurrent() & KEY_TOUCH)
      {
        touchPosition touch;
        touchRead(&touch);
        iTx = touch.px;
        iTy = touch.py;

        // --------------------------------------------------------------------------
        // Test the touchscreen rendering of the ADAM/MSX/SVI/CreatiVision keybaords
        // --------------------------------------------------------------------------
        if (myConfig.overlay == 9) // ADAM Keyboard
        {
            meta_key = handle_adam_keyboard_press(iTx, iTy);
        }
        else if (myConfig.overlay == 10) // MSX Keyboard
        {
            meta_key = handle_msx_keyboard_press(iTx, iTy);
        }
        else if (myConfig.overlay == 11) // Memotech MTX Keyboard
        {
            meta_key = handle_mtx_keyboard_press(iTx, iTy);
        }
        else if (myConfig.overlay == 12) // Creativision Keyboard
        {
            meta_key = handle_cvision_keyboard_press(iTx, iTy);
        }
        else if (myConfig.overlay == 13) // Simplified Alpha Keyboard
        {
            meta_key = handle_alpha_keyboard_press(iTx, iTy);
        }
        else if (myConfig.overlay == 14) // Tatung Einstein Keyboard
        {
            meta_key = handle_einstein_keyboard_press(iTx, iTy);
        }
        else if (myConfig.overlay == 15) // SVI Keyboard
        {
            meta_key = handle_svi_keyboard_press(iTx, iTy);
        }
        else    // Normal 12 button virtual keypad
        {
            meta_key = handle_normal_virtual_keypad(iTx, iTy);

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
          
        if (kbd_key != 0)
        {
            kbd_keys[kbd_keys_pressed++] = kbd_key;
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
                showMainMenu();
                SoundUnPause();
                break;

            case MENU_CHOICE_END_GAME:
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
                    SoundUnPause();
                }
                break;

            case MENU_CHOICE_LOAD_GAME:
                if  (!LoadNow)
                {
                    SoundPause();
                    if (IsFullKeyboard())
                    {
                        if  (showMessage("DO YOU REALLY WANT TO","LOAD GAME STATE ?") == ID_SHM_YES)
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
                   SoundUnPause();
                }
                break;

            case MENU_CHOICE_CASSETTE:
                CassetteMenu();
                break;

            default:
                SaveNow = 0;
                LoadNow = 0;
        }

        // ---------------------------------------------------------------------
        // If we are mapping the touch-screen keypad to P2, we shift these up.
        // ---------------------------------------------------------------------
        if (myConfig.touchPad) ucUN = ucUN << 16;

        if (++dampenClick > 1)  // Make sure the key is pressed for an appreciable amount of time...
        {
            if (((ucUN != 0) || (kbd_key != 0)) && (lastUN == 0))
            {
                if (!adam_mode) mmEffect(SFX_KEYCLICK);  // Play short key click for feedback... ADAM handers do this for us
            }
            lastUN = (ucUN ? ucUN:kbd_key);
        }
      } //  SCR_TOUCH
      else
      {
        SaveNow=LoadNow = 0;
        lastUN = 0;  dampenClick = 0;
        last_kbd_key = 0;
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

      ucDEUX  = 0;
      nds_key  = keysCurrent();     // Get any current keys pressed on the NDS
        
      if ((nds_key & KEY_L) && (nds_key & KEY_R) && (nds_key & KEY_X))
      {
            lcdSwap();
            WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
      }
      else if ((nds_key & KEY_L) && (nds_key & KEY_R))
      {
            AffChaine(5,0,0,"SNAPSHOT");
            screenshot();
            WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
            AffChaine(5,0,0,"        ");
      }
      else if  (nds_key & (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT | KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_R | KEY_L | KEY_X | KEY_Y))
      {
          if (einstein_mode && (nds_key & KEY_START)) // Load .COM file directly
          {
              einstein_load_com_file();
              WAITVBL;WAITVBL;WAITVBL;
          }
          else if (memotech_mode && (nds_key & KEY_START))
          {
              extern u8 memotech_magrom_present;

              if (memotech_magrom_present)
              {
                  BufferKey('R');
                  BufferKey('O');
                  BufferKey('M');
                  BufferKey(' ');
                  BufferKey('7');
                  BufferKey(KBD_KEY_RET);
              }
              else
              if (memotech_mode == 2)   // .MTX file: enter LOAD "" into the keyboard buffer
              {
                  if (memotech_mtx_500_only)
                  {
                      RAM_Memory[0xFA7A] = 0x00;

                      BufferKey('N');
                      BufferKey('E');
                      BufferKey('W');
                      BufferKey(KBD_KEY_RET);
                      BufferKey(255);
                  }
                  
                  BufferKey('L');
                  BufferKey('O');
                  BufferKey('A');
                  BufferKey('D');
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
                  BufferKey('C');
                  BufferKey('L');
                  BufferKey('O');
                  BufferKey('A');
                  BufferKey('D');
                  BufferKey(KBD_KEY_RET);
                  BufferKey(255);
                  BufferKey('R');
                  BufferKey('U');
                  BufferKey('N');
                  BufferKey(KBD_KEY_RET);
              }
              else
              {
                  BufferKey('B');
                  BufferKey('L');
                  BufferKey('O');
                  BufferKey('A');
                  BufferKey('D');
                  BufferKey(KBD_KEY_SHIFT);
                  BufferKey(msx_japanese_matrix ? '2': KBD_KEY_QUOTE);
                  BufferKey('C');
                  BufferKey('A');
                  BufferKey('S');
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
                      else if (keyCoresp[myConfig.keymap[i]] == META_KBD_STOP)      kbd_key = (adam_mode ? ADAM_KEY_CLEAR : KBD_KEY_STOP);
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
                              mmEffect(SFX_KEYCLICK);  // Play short key click for feedback...
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
      
      // ------------------------------------------------------------------------------------------
      // Finally, check if there are any buffered keys that need to go into the keyboard handling.
      // ------------------------------------------------------------------------------------------
      ProcessBufferedKeys();

      // ---------------------------------------------------------
      // Accumulate all bits above into the Joystick State var...
      // ---------------------------------------------------------
      JoyState = ucUN | ucDEUX;

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
  vramSetBankE(VRAM_E_LCD );                 // Not using this  for video but 64K of faster RAM always useful!  Mapped  at 0x06880000 -   We use this block of 128K memory to store 4x MSX BIOS flavors
  vramSetBankF(VRAM_F_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x06890000 -   ..
  vramSetBankG(VRAM_G_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x06894000 -   ..
  vramSetBankH(VRAM_H_LCD );                 // Not using this  for video but 32K of faster RAM always useful!  Mapped  at 0x06898000 -   ..
  vramSetBankI(VRAM_I_LCD );                 // Not using this  for video but 16K of faster RAM always useful!  Mapped  at 0x068A0000 -   16K Used for the Look Up Table
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
    else if (myConfig.overlay == 9)  // ADAM Keyboard
    {
      //  Init bottom screen
      decompress(adam_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(adam_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) adam_fullPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 10)  //MSX/SVI Keyboard
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
    else if (myConfig.overlay == 11) // Memotech MTX
    {
      //  Init bottom screen
      decompress(mtx_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(mtx_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) mtx_fullPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 12) // CreatiVision Keypad
    {
      //  Init bottom screen
      decompress(cvision_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(cvision_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) cvision_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 13) // Alpha Simplified Keyboard
    {
      //  Init bottom screen
      decompress(alpha_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(alpha_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) alpha_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 14) // Einstein Keyboard
    {
      //  Init bottom screen
      decompress(einstein_kbdTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(einstein_kbdMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) einstein_kbdPal,(void*) BG_PALETTE_SUB,256*2);
    }
    else if (myConfig.overlay == 15) // Spectravideo SVI
    {
      //  Init bottom screen
      decompress(svi_fullTiles, bgGetGfxPtr(bg0b),  LZ77Vram);
      decompress(svi_fullMap, (void*) bgGetMapPtr(bg0b),  LZ77Vram);
      dmaCopy((void*) bgGetMapPtr(bg0b)+32*30*2,(void*) bgGetMapPtr(bg1b),32*24*2);
      dmaCopy((void*) svi_fullPal,(void*) BG_PALETTE_SUB,256*2);
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
  memset(RAM_Memory, 0xFF, 0x10000);
  memset(pVDPVidMem, 0x00, 0x4000);

  // -----------------------------------------------
  // Init bottom screen do display correct overlay
  // -----------------------------------------------
  InitBottomScreen();

  // -----------------------------------------------------
  //  Load the correct Bios ROM for the given machine
  // -----------------------------------------------------
  if (sordm5_mode)
  {
      memcpy(RAM_Memory,SordM5Bios,0x2000);
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
      einstien_restore_bios();
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
        fread(SordM5Bios, 0x2000, 1, fp);
        fclose(fp);
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
    
    
    // Casio PV-7 and PV-16 are the exact same BIOS as the HB-10
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
        if (myGlobalConfig.showBiosInfo)
        {
            u8 idx = 6;
            AffChaine(2,idx++,0,"LOADING BIOS FILES ..."); idx++;
                                         AffChaine(2,idx++,0,"coleco.rom     BIOS FOUND");
            if (bMSXBiosFound)          {AffChaine(2,idx++,0,"msx.rom        BIOS FOUND"); }
            if (bSVIBiosFound)          {AffChaine(2,idx++,0,"svi.rom        BIOS FOUND"); }
            if (bSordBiosFound)         {AffChaine(2,idx++,0,"sordm5.rom     BIOS FOUND"); }
            if (bPV2000BiosFound)       {AffChaine(2,idx++,0,"pv2000.rom     BIOS FOUND"); }
            if (bPencilBiosFound)       {AffChaine(2,idx++,0,"pencil2.rom    BIOS FOUND"); }
            if (bEinsteinBiosFound)     {AffChaine(2,idx++,0,"einstein.rom   BIOS FOUND"); }
            if (bCreativisionBiosFound) {AffChaine(2,idx++,0,"bioscv.rom     BIOS FOUND"); }
            if (bAdamBiosFound)         {AffChaine(2,idx++,0,"eos.rom        BIOS FOUND"); }
            if (bAdamBiosFound)         {AffChaine(2,idx++,0,"writer.rom     BIOS FOUND"); }
            AffChaine(2,idx++,0,"SG-1000/3000 AND MTX BUILT-IN"); idx++;
            AffChaine(2,idx++,0,"TOUCH SCREEN / KEY TO BEGIN"); idx++;

            while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
            while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))==0);
            while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_DOWN | KEY_UP | KEY_A | KEY_B | KEY_L | KEY_R))!=0);
        }
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

