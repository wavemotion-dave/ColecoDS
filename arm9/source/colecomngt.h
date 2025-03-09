// =====================================================================================
// Copyright (c) 2021-2024 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, it's source code and associated 
// readme files, with or without modification, are permitted in any medium without 
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty.
// =====================================================================================
#ifndef _COLECOMNGT_H
#define _COLECOMNGT_H

#include <nds.h>
#include "colecoDS.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/sn76496/SN76496.h"
#include "cpu/ay38910/AY38910.h"
#include "cpu/scc/SCC.h"

#define NORAM       0xFF

extern u8 mapperType;
extern u8 mapperMask;

extern u8 msx_caps_lock;
extern u8 msx_kana_lock;

#define GUESS       0
#define KON8        1
#define ASC8        2
#define SCC8        3
#define ASC16       4
#define ZEN8        5
#define ZEN16       6
#define XBLAM       7
#define RES2        8
#define AT0K        9
#define AT4K        10
#define AT8K        11
#define LIN64       12

#define MAX_MAPPERS 7   // The most we can guess when examining ROM data

extern u32 MAX_CART_SIZE;

extern u8 bSuperSimplifiedMemory;

extern u8 *ROM_Memory;
extern u8 RAM_Memory[0x10000];
extern u8 BIOS_Memory[0x10000];
extern u8 SRAM_Memory[0x4000];
extern u8 fastdrom_cdx2[0x4000];

extern u8 bROMInSegment[4];
extern u8 bRAMInSegment[4];
extern u8 *MSXCartPtr[8];
extern u8 *MemoryMap[8];
extern u8 msx_slot_dirty[4];

extern u8 adam_ext_ram_used;
extern u8  msx_last_block[4];

extern SN76496 mySN;
extern AY38910 myAY;
extern SCC     mySCC;

extern u8 msx_scc_enable;
extern u8 sg1000_sms_mapper;

extern u8 JoyMode;                      // Joystick / Paddle management
extern u32 JoyState;                    // Joystick / Paddle management

extern u8 IssueCtrlBreak;
extern u8 ein_alpha_lock;

extern u8 sRamAtE000_OK;
extern u8 bMagicMegaCart;
extern u8 bActivisionPCB;
extern u8 bSuperGameCart;
extern u8 b31_in_1;
extern u8 msx_sram_enabled;
extern u8 sgm_enable;
extern u8 AY_Enable;
extern u8 last_mega_bank; 
extern u16 msx_block_size;
extern u32 file_crc;
extern u8 ctc_enabled;

extern u8 adam_ram_lo;
extern u8 adam_ram_hi;
extern u8 adam_ram_lo_exp;
extern u8 adam_ram_hi_exp;

extern u8 romBankMask;
extern u8 sgm_enable;
extern u16 sgm_low_addr;

extern u8 bMSXBiosFound;

extern u8 Port53;
extern u8 Port60;
extern u8 Port20;
extern u8 Port42;

extern u8 bFirstSGMEnable;
extern u16 einstein_ram_start;
extern u8 keyboard_w;
extern u8 key_int_mask;

extern u8 SGC_Bank[4];
extern u8 SGC_SST_State;
extern u8 SGC_SST_CmdPos;

// -------------------------------
// A few misc externs needed...
// -------------------------------
extern u32* lutTablehh;
extern u8 msx_sram_at_8000;
extern u8 svi_RAMinSegment[2];
extern u16 memotech_RAM_start;

extern u8 memotech_magrom_present;
extern u8 memotech_mtx_500_only;
extern u8 memotech_lastMagROMPage;

extern u8 IOBYTE;
extern u8 MTX_KBD_DRIVE;
extern u8 lastIOBYTE;
extern u32 tape_pos;
extern u32 tape_len;
extern u8 key_shift_hold;

extern u8 Port_PPI_A;
extern u8 Port_PPI_B;
extern u8 Port_PPI_C;
extern u8 Port_PPI_CTRL;

extern u16 vdp_int_source;    

extern void ProcessBufferedKeys(void);
extern u8 BufferedKeys[32];
extern u8 BufferedKeysWriteIdx;
extern u8 BufferedKeysReadIdx;
extern u16 keyboard_interrupt;
extern u16 joystick_interrupt;

// --------------------------------------------------
// Some CPU and VDP and SGM stuff that we need
// --------------------------------------------------
extern void SetupAdam(bool bResetAdamNet);
extern byte Loop9918(void);
extern u8 ColecoBios[];
extern u8 lastBank;
extern u8 CBios[];
extern u8 Pencil2Bios[];
extern u8 AdamEOS[];
extern u8 AdamWRITER[];
extern u8 key_shift;
extern u8 key_ctrl;
extern u8 key_code;
extern u8 key_graph;
extern u8 key_dia;
extern u32 last_tape_pos;
extern u32 msx_last_rom_size;
extern u8 OldPortC;
extern u8 myKeyData;
extern u8 adc_mux;

// And the various SVI and MSX bios flavors...
extern u8 SVIBios[];
extern u8 MSXBios_Generic[];
extern u8 MSXBios_PanasonicCF2700[];
extern u8 MSXBios_YamahaCX5M[];
extern u8 MSXBios_ToshibaHX10[];
extern u8 MSXBios_SonyHB10[];
extern u8 MSXBios_NationalFS1300[];
extern u8 MSXBios_CasioPV7[];

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
extern void creativision_reset(void);
extern void sg1000_reset(void);
extern void sordm5_reset(void);
extern void memotech_reset(void);
extern void einstein_reset(void);
extern void svi_reset(void);
extern void pv2000_reset(void);
extern void msx_reset(void);
extern void msx_restore_bios(void);
extern void MSX_HandleBeeper(void);
extern void einstein_HandleBeeper(void);
extern void einstein_handle_interrupts(void);
extern void einstein_load_com_file(void);
extern void einstien_load_dsk_file(void);
extern void einstein_restore_bios(void);
extern void memotech_launch_run_file(void);
extern void sordm5_check_keyboard_interrupt(void);
extern void einstein_save_disk(u8 disk);
extern void einstein_load_disk(u8 disk);
extern void einstein_swap_disk(u8 disk, char *szFileName);
extern void einstein_init_ramdisk(void);
extern void einstein_install_ramdisk(void);

extern u8 loadrom(const char *path,u8 * ptr);

extern u32 LoopZ80();
extern void MegaCartBankSwitch(u8 bank);
extern void MegaCartBankSwap(u8 bank);
extern void BufferKey(u8 key);
extern void BufferKeys(char *str);

extern void MSX_InitialMemoryLayout(u32 romSize);
extern void MSX_HandleCassette(register Z80 *r);
extern void MTX_HandleCassette(register Z80 *r);
extern void SVI_HandleCassette(register Z80 *r);
extern void svi_restore_bios(void);
extern void memotech_restore_bios(void);
extern void adam_setup_bios(void);
extern void creativision_loadrom(int romSize);
extern void creativision_restore_bios(void);
extern u8  *creativision_get_savestate(u16 *cv_cpu_size);
extern void creativision_put_savestate(u8 *mem);
extern void creativision_loadBAS(void);

extern void msxSaveEEPROM(void);
extern void msxLoadEEPROM(void);

extern void SuperGameCartSetup(int romSize);
extern void SuperGameCartWrite(u16 address, u8 value);
extern u8   SuperGameCartRead(u16 address);
extern void SuperGameCartSaveFlash(void);

extern void BeeperON(u16 beeper_freq);
extern void BeeperOFF(void);

extern void Z80_Interface_Reset(void);

#endif
