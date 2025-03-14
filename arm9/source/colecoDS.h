// =====================================================================================
// Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, it's source code and associated 
// readme files, with or without modification, are permitted in any medium without 
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty.
// =====================================================================================
#ifndef _COLECODS_H_
#define _COLECODS_H_

#include <nds.h>
#include <string.h>
#include "C24XX.h"

#define VERSIONCLDS "V10.5"

extern u32 debug[0x10];

// These are the various special icons/menu operations
#define MENU_CHOICE_NONE        0x00
#define MENU_CHOICE_RESET_GAME  0x01
#define MENU_CHOICE_END_GAME    0x02
#define MENU_CHOICE_SAVE_GAME   0x03
#define MENU_CHOICE_LOAD_GAME   0x04
#define MENU_CHOICE_HI_SCORE    0x05
#define MENU_CHOICE_CASSETTE    0x06
#define MENU_CHOICE_SWAP_KBD    0x07
#define MENU_CHOICE_MENU        0xFF        // Special brings up a menu of choices

#define JST_NONE              0x0000
#define JST_KEYPAD            0x000F

// ------------------------------------------------------------------------------
// Joystick UP, RIGHT, LEFT, DOWN and the two FIRE buttons are independent...
// but all other keypad buttons (including the PURPLE and BLUE butotns on
// the super action controller) are shared on the bottom of this 16-bit word.
// This is how a real Colecovision deals with it.
// ------------------------------------------------------------------------------
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

// -------------------------------------------------------------------------------
// These 4 are not actual Colecovision key maps... they trigger the spinner logic.
// -------------------------------------------------------------------------------
#define META_SPINX_LEFT     0xFFFE0001
#define META_SPINX_RIGHT    0xFFFE0002
#define META_SPINY_LEFT     0xFFFE0003
#define META_SPINY_RIGHT    0xFFFE0004

// -----------------------------------------------------------------------------------
// And these are meta keys for mapping NDS keys to keyboard keys (many of the computer
// games don't use joystick inputs and so need to map to keyboard keys...)
// -----------------------------------------------------------------------------------
#define META_KBD_A          0xFFFF0001
#define META_KBD_B          0xFFFF0002
#define META_KBD_C          0xFFFF0003
#define META_KBD_D          0xFFFF0004
#define META_KBD_E          0xFFFF0005
#define META_KBD_F          0xFFFF0006
#define META_KBD_G          0xFFFF0007
#define META_KBD_H          0xFFFF0008
#define META_KBD_I          0xFFFF0009
#define META_KBD_J          0xFFFF000A
#define META_KBD_K          0xFFFF000B
#define META_KBD_L          0xFFFF000C
#define META_KBD_M          0xFFFF000D
#define META_KBD_N          0xFFFF000E
#define META_KBD_O          0xFFFF000F
#define META_KBD_P          0xFFFF0010
#define META_KBD_Q          0xFFFF0011
#define META_KBD_R          0xFFFF0012
#define META_KBD_S          0xFFFF0013
#define META_KBD_T          0xFFFF0014
#define META_KBD_U          0xFFFF0015
#define META_KBD_V          0xFFFF0016
#define META_KBD_W          0xFFFF0017
#define META_KBD_X          0xFFFF0018
#define META_KBD_Y          0xFFFF0019
#define META_KBD_Z          0xFFFF001A

#define META_KBD_0          0xFFFF001B
#define META_KBD_1          0xFFFF001C
#define META_KBD_2          0xFFFF001D
#define META_KBD_3          0xFFFF001E
#define META_KBD_4          0xFFFF001F
#define META_KBD_5          0xFFFF0020
#define META_KBD_6          0xFFFF0021
#define META_KBD_7          0xFFFF0022
#define META_KBD_8          0xFFFF0023
#define META_KBD_9          0xFFFF0024

#define META_KBD_SHIFT      0xFFFF0025
#define META_KBD_CTRL       0xFFFF0026
#define META_KBD_CODE       0xFFFF0027
#define META_KBD_GRAPH      0xFFFF0028

#define META_KBD_SPACE      0xFFFF0029
#define META_KBD_RETURN     0xFFFF002A
#define META_KBD_ESC        0xFFFF002B
#define META_KBD_HOME       0xFFFF002C
#define META_KBD_UP         0xFFFF002D
#define META_KBD_DOWN       0xFFFF002E
#define META_KBD_LEFT       0xFFFF002F
#define META_KBD_RIGHT      0xFFFF0030
#define META_KBD_PERIOD     0xFFFF0031
#define META_KBD_COMMA      0xFFFF0032
#define META_KBD_COLON      0xFFFF0033
#define META_KBD_SEMI       0xFFFF0034
#define META_KBD_QUOTE      0xFFFF0035
#define META_KBD_SLASH      0xFFFF0036
#define META_KBD_BACKSLASH  0xFFFF0037
#define META_KBD_PLUS       0xFFFF0038
#define META_KBD_MINUS      0xFFFF0039
#define META_KBD_LBRACKET   0xFFFF003A
#define META_KBD_RBRACKET   0xFFFF003B
#define META_KBD_CARET      0xFFFF003C
#define META_KBD_ASTERISK   0xFFFF003D
#define META_KBD_ATSIGN     0xFFFF003E
#define META_KBD_BS         0xFFFF003F
#define META_KBD_TAB        0xFFFF0040
#define META_KBD_INS        0xFFFF0041
#define META_KBD_DEL        0xFFFF0042
#define META_KBD_CLR        0xFFFF0043
#define META_KBD_UNDO       0xFFFF0044
#define META_KBD_MOVE       0xFFFF0045
#define META_KBD_WILDCARD   0xFFFF0046
#define META_KBD_STORE      0xFFFF0047
#define META_KBD_PRINT      0xFFFF0048
#define META_KBD_STOP_BRK   0xFFFF0049
#define META_KBD_F1         0xFFFF004A
#define META_KBD_F2         0xFFFF004B
#define META_KBD_F3         0xFFFF004C
#define META_KBD_F4         0xFFFF004D
#define META_KBD_F5         0xFFFF004E
#define META_KBD_F6         0xFFFF004F
#define META_KBD_F7         0xFFFF0050
#define META_KBD_F8         0xFFFF0051

#define MAX_KEY_OPTIONS     125


#define JOYMODE_JOYSTICK    0
#define JOYMODE_KEYPAD      1

// -----------------------------
// For the Full Keyboard...
// -----------------------------
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
#define KBD_KEY_STOP        18  // Some machines call it 'STOP' and others 'BREAK' but reasonably similar functdionality    
#define KBD_KEY_BREAK       18  // Some machines call it 'STOP' and others 'BREAK' but reasonably similar functdionality
#define KBD_KEY_SEL         19
#define KBD_KEY_RET         20
#define KBD_KEY_DEL         21
#define KBD_KEY_INS         22
#define KBD_KEY_UNUSED      23
#define KBD_KEY_HOME        24
#define KBD_KEY_QUOTE       25
#define KBD_KEY_CAPS        26
#define KBD_KEY_TAB         27
#define KBD_KEY_BS          28
#define KBD_KEY_CODE        29
#define KBD_KEY_GRAPH       30
#define KBD_KEY_DEAD        31
#define KBD_KEY_WILDCARD    32
#define KBD_KEY_STORE       33
#define KBD_KEY_PRINT       34
#define KBD_KEY_CLEAR       35
#define KBD_KEY_MOVE        36
#define KBD_KEY_UNDO        37
#define KBD_KEY_LF          38
#define KBD_KEY_DIA         39
#define KBD_KEY_YEN         40
#define KBD_KEY_CAS         255

extern u16 emuFps;
extern u16 emuActFrames;
extern u16 timingFrames;

extern u8 msx_scc_enable;
extern u8 sg1000_double_reset;

extern char initial_file[];
extern char initial_path[];

extern u8 spinX_left;
extern u8 spinX_right;
extern u8 spinY_left;
extern u8 spinY_right;

extern u16 nds_key;
extern u8  kbd_key;

extern u8 sg1000_mode;
extern u8 sordm5_mode;
extern u8 einstein_mode;
extern u8 pv2000_mode;
extern u8 memotech_mode;
extern u8 pencil2_mode;
extern u8 msx_mode;
extern u8 svi_mode;
extern u8 adam_mode;
extern u8 coleco_mode;
extern u8 adam_CapsLock;
extern u8 creativision_mode;

extern u16 machine_mode;

extern u8 kbd_keys_pressed;
extern u8 kbd_keys[12];

extern char disk_last_file[3][256];
extern char disk_last_path[3][256];
extern u32  disk_last_size[3];
extern u8   disk_unsaved_data[3];

extern u32 tape_pos, tape_len;

#define MODE_COLECO         0x0000  // No bits set! Fastest way to check the machine_mode
#define MODE_ADAM           0x0002
#define MODE_SG_1000        0x0004
#define MODE_SORDM5         0x0008
#define MODE_PV2000         0x0010
#define MODE_MEMOTECH       0x0020
#define MODE_EINSTEIN       0x0040
#define MODE_SVI            0x0080
#define MODE_MSX            0x0100
#define MODE_PENCIL2        0x0200
#define MODE_CREATIVISION   0x0400

#define WAITVBL swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank(); swiWaitForVBlank();

extern u8 *DSI_RAM_Buffer;

extern volatile u16 vusCptVBL;                   // Video Management

extern u32 keyCoresp[MAX_KEY_OPTIONS];
extern u16 NDS_keyMap[];

extern u8 soundEmuPause;
extern u8 write_NV_counter;
extern u8 msx_japanese_matrix;

extern int bg0, bg1, bg0b, bg1b;

extern u16 *pVidFlipBuf;                         // Video flipping buffer

extern C24XX EEPROM;

extern u8 adam_ram_lo, adam_ram_hi;
extern u8 io_show_status;

extern void BottomScreenOptions(void);
extern void BottomScreenOptionsAdam(void);
extern void BottomScreenOptionsEinstein(void);
extern void BottomScreenKeypad(void);
extern void PauseSound(void);
extern void UnPauseSound(void);
extern void ResetStatusFlags(void);
extern void ReadFileCRCAndConfig(void);
extern void SetupAdam(bool);
extern void DisplayStatusLine(bool bForce);
extern void colecoSaveEEPROM(void);    
extern void colecoLoadEEPROM(void);    
extern void ResetColecovision(void);
extern u32  creativision_run(void);
extern void msx_patch_bios(void);
extern bool isAdamDDP(u8 disk);
extern void processDirectAudio(void);

extern void debug_init();
extern void debug_save();
extern void debug_printf(const char * str, ...);


#endif
