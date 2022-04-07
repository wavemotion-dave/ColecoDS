#ifndef _COLECOMNGT_H
#define _COLECOMNGT_H

#include <nds.h>
#include "colecoDS.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/sn76496/SN76496.h"
#include "cpu/sn76496/Fake_AY.h"

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

#define CTC_SOUND_DIV     280
#define MTX_CTC_SOUND_DIV 260

extern u8 romBuffer[];

extern u8 bROMInSlot[4];
extern u8 bRAMInSlot[4];
extern u8 *Slot1ROMPtr[8];

extern u8 AdamRAM[0x20000];
extern u8 adam_128k_mode;

extern SN76496 sncol;
extern SN76496 aycol;

extern u8 VR;                           // Sound and VDP register storage

extern u8 JoyMode;                      // Joystick / Paddle management
extern u32 JoyState;                    // Joystick / Paddle management

extern u8 bMagicMegaCart;
extern u8 bActivisionPCB;
extern u8 msx_sram_enabled;
extern u8 sgm_enable;
extern u8 AY_Enable;
extern u8 lastBank; 
extern u8 romBankMask;
extern u32 file_crc;
extern u8 AY_EnvelopeOn;
extern u8 msx_auto_clear_irq;

extern u8 adam_ram_lo;
extern u8 adam_ram_hi;
extern u8 adam_ram_lo_exp;
extern u8 adam_ram_hi_exp;

extern u8 sgm_low_mem[8192];

extern u8 romBankMask;
extern u8 sgm_enable;
extern u8 ay_reg_idx;
extern u8 ay_reg[16];
extern u16 sgm_low_addr;

extern u8 Port53;
extern u8 Port60;
extern u8 Port20;

extern u8 bFirstSGMEnable;
extern u8 AY_Enable;
extern u8 AY_NeverEnable;
extern u8 SGM_NeverEnable;
extern u8 AY_EnvelopeOn;

// -------------------------------
// A few misc externs needed...
// -------------------------------
extern u32* lutTablehh;
extern s32  cycle_deficit;
extern u8 msx_sram_at_8000;
extern u16 svi_RAM_start;
extern u16 memotech_RAM_start;

extern u8 IOBYTE;
extern u8 MTX_KBD_DRIVE;
extern u8 lastIOBYTE;
extern u32 tape_pos;
extern u32 tape_len;
extern u8 key_shift_hold;
extern u8 msx_beeper_enabled;

extern u8 Port_PPI_A;
extern u8 Port_PPI_B;
extern u8 Port_PPI_C;
extern u8 msx_auto_clear_irq;


extern u8 ctc_control[4];
extern u8 ctc_time[4];
extern u16 ctc_timer[4]; 
extern u8 ctc_vector[4]; 
extern u8 ctc_latch[4];  
extern u8 sordm5_irq;    

extern u8 BufferedKeys[32];
extern u8 BufferedKeysWriteIdx;
extern u8 BufferedKeysReadIdx;


// --------------------------------------------------
// Some CPU and VDP and SGM stuff that we need
// --------------------------------------------------
extern void SetupAdam(bool bResetAdamNet);
extern byte Loop9918(void);
extern void DrZ80_InitHandlers(void);
extern u8 ColecoBios[];
extern u8 lastBank;
extern u8 MSXBios[];
extern u8 CBios[];
extern u8 SVIBios[];
extern u8 AdamEOS[];
extern u8 AdamWRITER[];
extern u8 Slot3RAM[0x10000];
extern u8 msx_SRAM[0x4000];
extern u8 key_shift;
extern u32 last_tape_pos;
extern u8* Slot3RAMPtr;
extern u8* Slot0BIOSPtr;
extern u32 LastROMSize;
extern u8 Port_PPI_CTRL;
extern u8 OldPortC;

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
extern void sg1000_reset(void);
extern void sordm5_reset(void);
extern void memotech_reset(void);
extern void svi_reset(void);
extern void msx_reset(void);
extern void MSX_HandleBeeper(void);

extern u8 loadrom(const char *path,u8 * ptr, int nmemb);

extern u32 LoopZ80();
extern void BankSwitch(u8 bank);
extern void CheckMSXHeaders(char *szGame);
extern void BufferKey(u8 key);

extern ITCM_CODE void FastMemCopy(u8* dest, u8* src, u16 numBytes);
extern ITCM_CODE void SaveRAM(u8 slot);
extern ITCM_CODE void RestoreRAM(u8 slot);
extern void MSX_HandleCassette(register Z80 *r);
extern void MTX_HandleCassette(register Z80 *r);
extern void SVI_HandleCassette(register Z80 *r);
extern void SVI_PatchBIOS(void);

#endif
