#ifndef _COLECODS_H_
#define _COLECODS_H_

#include <nds.h>
#include <string.h>

#define VERSIONCLDS "0.5"

#define DEBUGNO
#define USEDRZ80

#ifdef USECOLEMZ80C
#include "cpu/z80/z80/Z80.h"
#endif
#ifdef USEDRZ80
#include "cpu/z80/drz80/Z80_interface.h"
#endif

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

extern u16 emuFps;
extern u16 emuActFrames;

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

extern volatile u16 vusCptVBL;                   // Video Management
extern u8 XBuf[256*256] ALIGN(32);

extern bool isFATSystem;
extern char szFATDir[256];
extern char szDrv[15];

extern char szKeyName[18][15];
extern u16 keyCoresp[18];
extern u16 keyboard_JoyNDS[12];

extern u32 lgeEmul;       // Langue emul : 0 = FR / 1 = UK

extern u8 pColecoMem[0x10000];                   // Coleco Memory

extern unsigned int soundEmuPause;

extern int bg0, bg1, bg0b,bg1b;

extern u16 *pVidFlipBuf;                         // Video flipping buffer

typedef union {
  struct { unsigned char l,h; } B;
  unsigned short W;
} pair;

extern char szLang[3][62][35];

extern void showMainMenu(void);
extern void InitBottomScreen(void);

#endif
