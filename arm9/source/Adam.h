// ===============================================================
// Parts of this file were taken from ColEM with a number of 
// disk/tape caching issues fixed using the algorithms from
// ADAMEm with a liberal amount of glue and scaffolding from me.
//
// I do not claim copyright on much of this code... I've left
// both the ADAMEm and ColEm copyright statements below.
//
// Marcel and Marat are pioneers for Coleco/ADAM emulation and
// they have my heartfelt thanks for providing a blueprint.
// ===============================================================

/** ADAMEm: Coleco ADAM emulator ********************************************/
/**                                                                        **/
/**                                Coleco.c                                **/
/**                                                                        **/
/** This file contains the Coleco-specific emulation code                  **/
/**                                                                        **/
/** Copyright (C) Marcel de Kogel 1996,1997,1998,1999                      **/
/**     You are not allowed to distribute this software commercially       **/
/**     Please, notify me, if you make any changes to this file            **/
/****************************************************************************/

/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                       AdamNet.h                         **/
/**                                                         **/
/** This file contains implementation for the AdamNet I/O   **/
/** interface found in Coleco Adam home computer.           **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#ifndef ADAMNET_H
#define ADAMNET_H

/** Adam Key Codes *******************************************/
#define ADAM_KEY_CONTROL    CON_CONTROL
#define ADAM_KEY_SHIFT      CON_SHIFT
#define ADAM_KEY_ESC        27
#define ADAM_KEY_BS         8   // + SHIFT = 184
#define ADAM_KEY_TAB        9   // + SHIFT = 185
#define ADAM_KEY_ENTER      13 
#define ADAM_KEY_QUOTE      '\''
#define ADAM_KEY_BQUOTE     '`'
#define ADAM_KEY_BSLASH     '\\'
#define ADAM_KEY_COMMA      ','
#define ADAM_KEY_DOT        '.'
#define ADAM_KEY_SLASH      '/'
#define ADAM_KEY_ASTERISK   '*'
#define ADAM_KEY_HOME       128
#define ADAM_KEY_F1         129 // + SHIFT = 137 
#define ADAM_KEY_F2         130 // + SHIFT = 138
#define ADAM_KEY_F3         131 // + SHIFT = 139
#define ADAM_KEY_F4         132 // + SHIFT = 140
#define ADAM_KEY_F5         133 // + SHIFT = 141
#define ADAM_KEY_F6         134 // + SHIFT = 142
#define ADAM_KEY_WILDCARD   144 // + SHIFT = 152
#define ADAM_KEY_UNDO       145 // + SHIFT = 153
#define ADAM_KEY_MOVE       146 // + SHIFT = 154 (COPY)
#define ADAM_KEY_STORE      147 // + SHIFT = 155 (FETCH)
#define ADAM_KEY_INS        148 // + SHIFT = 156
#define ADAM_KEY_PRINT      149 // + SHIFT = 157
#define ADAM_KEY_CLEAR      150 // + SHIFT = 158
#define ADAM_KEY_DEL        151 // + SHIFT = 159, + CTRL = 127
#define ADAM_KEY_UP         160 // + CTRL = 164, + HOME = 172
#define ADAM_KEY_RIGHT      161 // + CTRL = 165, + HOME = 173
#define ADAM_KEY_DOWN       162 // + CTRL = 166, + HOME = 174
#define ADAM_KEY_LEFT       163 // + CTRL = 167, + HOME = 175
    
    
// --------------------------------------------
// Key modifiers augment normal keypresses...
// --------------------------------------------
#define CON_SHIFT    0x10000000 /* SHIFT held                */
#define CON_CONTROL  0x20000000 /* CONTROL held              */

extern u16 *PCBTable;
extern u8 KBDStatus, LastKey;
extern u16 PCBAddr;

// -----------------------------------------------------
// For the various disk/tape drives in the ADAM system
// -----------------------------------------------------
#define BAY_DISK1       0
#define BAY_DISK2       1
#define BAY_TAPE        2

#define MAX_DRIVES      4

#define DRIVE_TYPE_NONE 0
#define DRIVE_TYPE_DISK 1
#define DRIVE_TYPE_TAPE 2
    
typedef struct
{
    u8 *image;
    u32 imageSize;
    u32 imageSizeMax;
    u16 secSize;
    u8  skew;
    u8  driveType;
} AdamDrive_t;

typedef struct
{
    u8  status;             // Current status byte for this drive
    u8  newstatus;          // Next status byte for this drive
    u8  timeout;            // The current disk busy timeout - decremented once each VDP interrupt
    u8  io_status;          // Used to produce the RD/WR and floppy sound under emulation
    u32 lastblock;          // The last block read from the drive
} DriveStatus_t;

extern AdamDrive_t   AdamDrive[MAX_DRIVES];
extern DriveStatus_t AdamDriveStatus[MAX_DRIVES];

extern u8 adam_ram_present[8];

void ReadPCB(word A);
void WritePCB(word A,byte V);
void ResetPCB(void);
void PutKBD(unsigned int Key);

void adam_drive_insert(u8 drive, char *filename);
void adam_drive_eject(u8 drive);
u8  *adam_drive_sector(u8 drive, u32 sector);
void adam_drive_save(u8 drive);
void adam_drive_init(void);
void adam_drive_cache_check(void);
void adam_drive_update(u8 drive, u8 device, int cmd);

#endif /* ADAMNET_H */
