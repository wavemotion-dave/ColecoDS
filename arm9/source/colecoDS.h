#ifndef _COLECODS_H_
#define _COLECODS_H_

#include <nds.h>
#include <string.h>
#include "cpu/z80/Z80_interface.h"

#define VERSIONCLDS "6.2"

//#define DEBUG_Z80   YES
extern u32 debug1;
extern u32 debug2;

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
#define MSX_KEY_UP          1
#define MSX_KEY_DOWN        2
#define MSX_KEY_RIGHT       3
#define MSX_KEY_LEFT        4
#define MSX_KEY_M1          5
#define MSX_KEY_M2          6
#define MSX_KEY_M3          7
#define MSX_KEY_M4          8
#define MSX_KEY_M5          9
#define MSX_KEY_CTRL        10
#define MSX_KEY_SHIFT       11
#define MSX_KEY_ESC         12
#define MSX_KEY_STOP        13
#define MSX_KEY_SEL         14
#define MSX_KEY_RET         15
#define MSX_KEY_DEL         16

extern u16 emuFps;
extern u16 emuActFrames;
extern u16 timingFrames;

extern u8 spinX_left;
extern u8 spinX_right;
extern u8 spinY_left;
extern u8 spinY_right;

extern u8 sg1000_mode;
extern u8 sordm5_mode;
extern u8 msx_mode;
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

#endif
