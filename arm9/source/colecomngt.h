#ifndef _COLECOMNGT_H
#define _COLECOMNGT_H

#include <nds.h>
#include "colecoDS.h"
#include "cpu/z80/Z80_interface.h"

#define IMAGE_VERIFY_FAIL 0X01
#define IMAGE_VERIFY_PASS 0x02

extern u8 mapperType;
extern u8 mapperMask;

#define GUESS       0
#define KON8        1
#define ASC8        2
#define SCC8        3
#define ASC16       4
#define ZEN8        5
#define ZEN16       6
#define RES1        7
#define RES2        8
#define AT0K        9
#define AT4K        10
#define AT8K        11
#define LIN64       12


#define MAX_MAPPERS 7

extern u8 bROMInSlot[];
extern u8 bRAMInSlot[];
extern u8 PortA8;
extern u8 *Slot1ROMPtr[];

extern u8 VR;                           // Sound and VDP register storage

extern u8 JoyMode;                      // Joystick / Paddle management
extern u32 JoyState;                    // Joystick / Paddle management

extern u8 bMagicMegaCart;
extern u8 bActivisionPCB;
extern u8 sgm_enable;
extern u8 AY_Enable;
extern u8 lastBank; 
extern u8 romBankMask;
extern u32 file_crc;
extern u8 AY_EnvelopeOn;

extern u8 colecoInit(char *szGame);
extern void colecoSetPal(void);
extern void colecoUpdateScreen(void);
extern void colecoKeyProc(void);
extern void colecoRun(void);
extern void getfile_crc(const char *path);

extern void colecoLoadState();
extern void colecoSaveState();

extern void colecoWipeRAM(void);

extern u8 colecoCartVerify(const u8 *cartData);
extern void sgm_reset(void);
extern void sordm5_reset(void);
extern void msx_reset(void);

extern u8 loadrom(const char *path,u8 * ptr, int nmemb);

extern u32 LoopZ80();
extern void BankSwitch(u8 bank);
extern void CheckMSXHeaders(char *szGame);
#endif
