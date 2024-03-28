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
#include "FDIDisk.h"

#ifndef ADAMNET_H
#define ADAMNET_H
#ifdef __cplusplus
extern "C" {
#endif

/** Adam Key Codes *******************************************/
#define ADAM_KEY_CONTROL    CON_CONTROL
#define ADAM_KEY_SHIFT      CON_SHIFT
#define ADAM_KEY_CAPS       CON_CAPS
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
#define ADAM_KEY_DIAG_NE    168
#define ADAM_KEY_DIAG_SE    169
#define ADAM_KEY_DIAG_SW    170
#define ADAM_KEY_DIAG_NW    171
    
    
/** Special Key Codes ****************************************/
/** Modifiers returned by GetKey() and WaitKey().           **/
/*************************************************************/
#define CON_KEYCODE  0x03FFFFFF /* Key code                  */
#define CON_MODES    0xFC000000 /* Mode bits, as follows:    */
#define CON_CLICK    0x04000000 /* Key click (LiteS60 only)  */
#define CON_CAPS     0x08000000 /* CapsLock held             */
#define CON_SHIFT    0x10000000 /* SHIFT held                */
#define CON_CONTROL  0x20000000 /* CONTROL held              */
#define CON_ALT      0x40000000 /* ALT held                  */
#define CON_RELEASE  0x80000000 /* Key released (going up)   */

#define CON_F1       0xEE
#define CON_F2       0xEF
#define CON_F3       0xF0
#define CON_F4       0xF1
#define CON_F5       0xF2
#define CON_F6       0xF3
#define CON_F7       0xF4
#define CON_F8       0xF5
#define CON_F9       0xF6
#define CON_F10      0xF7
#define CON_F11      0xF8
#define CON_F12      0xF9
#define CON_LEFT     0xFA
#define CON_RIGHT    0xFB
#define CON_UP       0xFC
#define CON_DOWN     0xFD
#define CON_OK       0xFE
#define CON_EXIT     0xFF

#define BAY_DISK1    0
#define BAY_DISK2    1
#define BAY_TAPE     2
    
#ifndef BYTE_TYPE_DEFINED
#define BYTE_TYPE_DEFINED
typedef unsigned char byte;
#endif

#ifndef WORD_TYPE_DEFINED
#define WORD_TYPE_DEFINED
typedef unsigned short word;
#endif
    
extern u16 *PCBTable;
extern u8 HoldingBuf[];
extern u8 KBDStatus, LastKey, DiskID;
extern u16 savedBUF, savedLEN, PCBAddr;

typedef struct
{
    byte status;             // Current status byte for this drive
    byte newstatus;          // Next status byte for this drive
    byte timeout;            // The current disk busy timeout - decremented once each VDP interrupt
    byte io_status;          // Used to produce the RD/WR and floppy sound under emulation
    int  lastblock;          // The last block read from the drive
} DevStatus_t;

extern DevStatus_t DiskStatus[MAX_DISKS];
extern DevStatus_t TapeStatus[MAX_TAPES];

extern u8 adam_ram_present[8];

/** ReadPCB() ************************************************/
/** Read value from a given PCB or DCB address.             **/
/*************************************************************/
void ReadPCB(word A);

/** WritePCB() ***********************************************/
/** Write value to a given PCB or DCB address.              **/
/*************************************************************/
void WritePCB(word A,byte V);

/** ResetPCB() ***********************************************/
/** Reset PCB and attached hardware.                        **/
/*************************************************************/
void ResetPCB(void);

/** PutKBD() *************************************************/
/** Add a new key to the keyboard buffer.                   **/
/*************************************************************/
void PutKBD(unsigned int Key);

byte ChangeTape(byte N,const char *FileName);
byte ChangeDisk(byte N,const char *FileName);

void adam_disk_tape_cache_check(void);
   
#ifdef __cplusplus
}
#endif
#endif /* ADAMNET_H */
