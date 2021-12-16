#ifndef _COLECODS_H_
#define _COLECODS_H_

#include <nds.h>
#include <string.h>
#include "cpu/z80/drz80/Z80_interface.h"

#define VERSIONCLDS "4.3"

#define JST_NONE      0x0000
#define JST_KEYPAD    0x000F
#define JST_UP        0x0100
#define JST_RIGHT     0x0200
#define JST_DOWN      0x0400
#define JST_LEFT      0x0800
#define JST_FIRER     0x0040
#define JST_FIREL     0x4000
#define JST_0         0x0005
#define JST_1         0x0002
#define JST_2         0x0008
#define JST_3         0x0003
#define JST_4         0x000D
#define JST_5         0x000C
#define JST_6         0x0001
#define JST_7         0x000A
#define JST_8         0x000E
#define JST_9         0x0004
#define JST_STAR      0x0006
#define JST_POUND     0x0009
#define JST_PURPLE    0x0007
#define JST_BLUE      0x000B
#define JST_RED       JST_FIRER
#define JST_YELLOW    JST_FIREL


#define JOYMODE_JOYSTICK  0
#define JOYMODE_KEYPAD    1

extern u16 emuFps;
extern u16 emuActFrames;
extern u16 timingFrames;


#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

extern volatile u16 vusCptVBL;                   // Video Management

extern u16 keyCoresp[20];

extern u8 pColecoMem[0x10000];                   // Coleco Memory

extern u8 soundEmuPause;

extern int bg0, bg1, bg0b,bg1b;

extern u16 *pVidFlipBuf;                         // Video flipping buffer

typedef union {
  struct { unsigned char l,h; } B;
  unsigned short W;
} pair;


extern void showMainMenu(void);
extern void InitBottomScreen(void);
extern void PauseSound(void);
extern void UnPauseSound(void);

#endif
