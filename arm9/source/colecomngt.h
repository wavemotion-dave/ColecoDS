#ifndef _COLECOMNGT_H
#define _COLECOMNGT_H

#include <nds.h>

#include "colecoDS.h"

#ifdef USECOLEMZ80C
#include "cpu/z80/z80/Z80.h"
#endif
#ifdef USEDRZ80
#include "cpu/z80/drz80/Z80_interface.h"
#endif

#define IMAGE_VERIFY_FAIL 0X01
#define IMAGE_VERIFY_PASS 0x02

#define SN76FREQ  15840

extern u8 VR;                           // Sound and VDP register storage

extern u32 JoyMode;                     // Joystick / Paddle management
extern u32 JoyStat[2];                  // Joystick / Paddle management

extern u32 ExitNow;

extern u8 VDPInit[8];

extern u8 colecoInit(char *szGame);
extern void colecoSetPal(void);
extern void colecoUpdateScreen(void);
extern void colecoKeyProc(void);
extern void colecoRun(void);

extern void colecoLoadState();
extern void colecoSaveState();

extern u8 colecoCartVerify(const u8 *cartData);
extern void sgm_reset(void);

extern u8 loadrom(const char *path,u8 * ptr, int nmemb);

extern u32 LoopZ80();

#endif
