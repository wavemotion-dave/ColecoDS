#ifndef _COLECOMNGT_H
#define _COLECOMNGT_H

#include <nds.h>

#include "colecoDS.h"

#ifdef USEDRZ80
#include "cpu/z80/drz80/Z80_interface.h"
#endif

#define IMAGE_VERIFY_FAIL 0X01
#define IMAGE_VERIFY_PASS 0x02

extern u8 VR;                           // Sound and VDP register storage

extern u16 JoyMode;                     // Joystick / Paddle management
extern u16 JoyStat[2];                  // Joystick / Paddle management

extern u8 bMagicMegaCart;
extern u8 bActivisionPCB;
extern u8 sgm_enable;
extern u8 AY_Enable;

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
