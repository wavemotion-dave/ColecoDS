#ifndef _COLECODS_H_
#define _COLECODS_H_

#include <nds.h>
#include <string.h>
#include "cpu/z80/Z80_interface.h"

#define VERSIONCLDS "6.5"

//#define DEBUG_Z80   YES
//#define FULL_DEBUG
extern u32 debug1;
extern u32 debug2;
extern u32 debug3;
extern u32 debug4;

#define JST_NONE            0x0000
#define JST_KEYPAD          0x000F
#define JST_UP              0x0100
#define JST_RIGHT           0x0200
#define JST_DOWN            0x0400
#define JST_LEFT            0x0800
#define JST_FIRER           0x0040
#define JST_FIREL           0x4000
#define JST_0               0x0005
#define JST_1               0x0002
#define JST_2               0x0008
#define JST_3               0x0003
#define JST_4               0x000D
#define JST_5               0x000C
#define JST_6               0x0001
#define JST_7               0x000A
#define JST_8               0x000E
#define JST_9               0x0004
#define JST_STAR            0x0006
#define JST_POUND           0x0009
#define JST_PURPLE          0x0007
#define JST_BLUE            0x000B
#define JST_RED             JST_FIRER
#define JST_YELLOW          JST_FIREL

// These 4 are not actual Colecovision key maps... they trigger the spinner logic.
#define META_SPINX_LEFT     0xFFFF0001
#define META_SPINX_RIGHT    0xFFFF0002
#define META_SPINY_LEFT     0xFFFF0003
#define META_SPINY_RIGHT    0xFFFF0004

#define JOYMODE_JOYSTICK    0
#define JOYMODE_KEYPAD      1


// For the MSX Full Keyboard...
#define KBD_KEY_UP          1
#define KBD_KEY_DOWN        2
#define KBD_KEY_RIGHT       3
#define KBD_KEY_LEFT        4
#define KBD_KEY_F1          5
#define KBD_KEY_F2          6
#define KBD_KEY_F3          7
#define KBD_KEY_F4          8
#define KBD_KEY_F5          9
#define KBD_KEY_F6          10
#define KBD_KEY_F7          11
#define KBD_KEY_F8          12
#define KBD_KEY_F9          13
#define KBD_KEY_F10         14
#define KBD_KEY_CTRL        15
#define KBD_KEY_SHIFT       16
#define KBD_KEY_ESC         17
#define KBD_KEY_STOP        18
#define KBD_KEY_SEL         19
#define KBD_KEY_RET         20
#define KBD_KEY_DEL         21
#define KBD_KEY_BRK         21
#define KBD_KEY_HOME        21
#define KBD_KEY_QUOTE       22
#define KBD_KEY_CAPS        23
#define KBD_KEY_CAS         255

extern u16 emuFps;
extern u16 emuActFrames;
extern u16 timingFrames;

extern u8 spinX_left;
extern u8 spinX_right;
extern u8 spinY_left;
extern u8 spinY_right;

extern u8 sg1000_mode;
extern u8 sordm5_mode;
extern u8 memotech_mode;
extern u8 msx_mode;
extern u8 svi_mode;
extern u8 adam_mode;
extern u8 msx_key;

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

#define MAX_KEY_OPTIONS  44

extern volatile u16 vusCptVBL;                   // Video Management

extern u32 keyCoresp[MAX_KEY_OPTIONS];

extern u8 pColecoMem[0x10000];                   // Coleco Memory

extern u8 soundEmuPause;

extern int bg0, bg1, bg0b,bg1b;

extern u16 *pVidFlipBuf;                         // Video flipping buffer


extern void showMainMenu(void);
extern void InitBottomScreen(void);
extern void PauseSound(void);
extern void UnPauseSound(void);
extern void ResetStatusFlags(void);
extern void ReadFileCRCAndConfig(void);
extern void SetupAdam(bool);

#endif
