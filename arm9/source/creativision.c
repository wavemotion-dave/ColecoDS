// =====================================================================================
// Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, it's source code and associated 
// readme files, with or without modification, are permitted in any medium without 
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty.
// =====================================================================================
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>

#include "cpu/m6502/M6502.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/sn76496/SN76496.h"
#define NORAM 0xFF

M6502 m6502 __attribute__((section(".dtcm")));        // Our core 6502 CPU

extern u8 RAM_Memory[];
extern byte Loop9918(void);
extern int debug1, debug2, debug3, debug4;
extern SN76496 sncol;

byte PIAAIO      = 0;
byte PIAADDR     = 0;
byte PIAACTL     = 0;
byte PIABIO      = 0;
byte PIABDDR     = 0;
byte PIABCTL     = 0;

byte KEYTBL[16] =  { 255, 255, 255, 255, 255, 255, 255, 255,   /* KEYTBL[7]  is piaa3=lo */
                     255, 255, 255, 255, 255, 255, 255, 255 }; /* KEYTBL[11] is piaa2=lo */
                                                               /* KEYTBL[13] is piaa1=lo */
                                                               /* KEYTBL[14] is piaa0=lo */

u8 *creativision_get_cpu(u16 *cv_cpu_size)
{
    *cv_cpu_size = (u16)sizeof(m6502);
    return (u8*)&m6502;
}

void creativision_put_cpu(u8 *mem)
{
    memcpy(&m6502, mem, sizeof(m6502));
}

void creativision_reset(void)
{
    Reset6502(&m6502);
}

u32 creativision_run(void)
{
    Exec6502(&m6502);   // Run 1 scanline
    return 0;
}


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


void creativision_input(void)
{
    extern u32 JoyState;
    
    KEYTBL[14] = 0xFF;
    KEYTBL[13] = 0xFF;
    KEYTBL[11] = 0xFF;
    KEYTBL[7]  = 0xFF;
    
    if (JoyState & JST_FIREL)   KEYTBL[14] &= 0x7f;  // P1 Right Button
    if (JoyState & JST_FIRER)   KEYTBL[13] &= 0x7f;  // P1 Left Button

    if (JoyState & JST_UP)      KEYTBL[14] &= 0xf7;  // P1 up
    if (JoyState & JST_DOWN)    KEYTBL[14] &= 0xfd;  // P1 down
    if (JoyState & JST_LEFT)    KEYTBL[14] &= 0xdf;  // P1 left
    if (JoyState & JST_RIGHT)   KEYTBL[14] &= 0xfb;  // P1 right

    if (JoyState == JST_1)      KEYTBL[14] &= 0xf3;  // 1      
    if (JoyState == JST_2)      KEYTBL[13] &= 0xcf;  // 2
    if (JoyState == JST_3)      KEYTBL[13] &= 0x9f;  // 3
    if (JoyState == JST_4)      KEYTBL[13] &= 0xd7;  // 4
    if (JoyState == JST_5)      KEYTBL[13] &= 0xb7;  // 5
    if (JoyState == JST_6)      KEYTBL[13] &= 0xaf;  // 6

    if (JoyState == JST_7)      KEYTBL[11] &= 0xf3;  // SPACE
    if (JoyState == JST_8)      KEYTBL[7]  &= 0xfa;  // Y
    if (JoyState == JST_9)      KEYTBL[7]  &= 0xaf;  // N
    if (JoyState == JST_0)      KEYTBL[7]  &= 0xf6;  // RETURN
    
    if (JoyState == JST_STAR)   Int6502(&m6502, INT_NMI);   // Game Reset (note, this is needed to start games)
    if (JoyState == JST_POUND)  KEYTBL[7] &= 0xed;          // 0 but graphic shows ST=START
}

// ========================================================================================
// Memory Map:
// 
// $0000 - $03FF: 1K RAM (mirrored thrice at $0400 - $07FF, $0800 - $0BFF, $0C00 - $0FFF)
// $1000 - $1FFF: PIA (joysticks, sound)
// $2000 - $2FFF: VDP read
// $3000 - $3FFF: VDP write
// $4000 - $7FFF: 16K ROM2 (BASIC in upper 4K)
// $8000 - $BFFF: 16K ROM1 (BASIC in upper 8K)
// $C000 - $E7FF: 10K memory expansion?
// $E800 - $EFFF: I/O interface (2K)
// $F000 - $F7FF: 2K ???
// $F800 - $FFFF: 2K ROM0 (BIOS)
// ========================================================================================
void Wr6502(register word Addr,register byte Value)
{
    switch (Addr & 0xF000)
    {
        case 0x0000:
            RAM_Memory[(Addr & 0x3FF) + 0x000] = Value;
            RAM_Memory[(Addr & 0x3FF) + 0x400] = Value;
            RAM_Memory[(Addr & 0x3FF) + 0x800] = Value;
            RAM_Memory[(Addr & 0x3FF) + 0xC00] = Value;
            break;

        case 0x1000:            // PIA
          switch(Addr & 0x03) 
          {
            case 0:
              if ((PIAACTL&0x04)==0) 
              {
                PIAADDR=Value;
                return;
              }
              else 
              {
                PIAAIO=Value;
                return;
              }
            case 1:
              PIAACTL=Value; return;
            case 2:
              if ((PIABCTL&0x04)==0) 
              {
                PIABDDR=Value; return;
              }
              else 
              {
                PIABIO=Value;
                if (Value != 0x90) 
                {
                  extern u32 file_crc;                    
                  sn76496W(Value, &sncol);
                  if ((file_crc == 0x767a1f38) && (sncol.ch1Frq < 5000))    // Sonic Invaders... high pitch "fix"
                  {
                      sncol.ch1Frq = 0;
                      sncol.ch1Att = 15;
                      sncol.ch1Reg = 0xFF;
                  }
                }
                return;
              }
            case 3:
              PIABCTL=Value; return;
          }
          break;
            
        case 0x3000:    // VDP Writes
            if ((Addr & 1)==0) WrData9918(Value);
            else if (WrCtrl9918(Value)) Int6502(&m6502, INT_IRQ);
            break;
    }
}

byte Rd6502(register word Addr)
{
    byte x, y;
    
    switch (Addr & 0xF000)
    {
        case 0x1000:                // PIA
          if ((Addr & 0x03)==2) 
          {
            /* read from PIAB */
            if ((PIABCTL&0x04)==0) {
              return(PIABDDR);
            }
            else {
              if (PIABDDR==0xff) {
                return(0x9f);
              }
              else {
                if ((PIAAIO&0x0f)==0) {
                  x=KEYTBL[7]^KEYTBL[11]^0xff;
                  y=KEYTBL[13]^KEYTBL[14]^0xff;
                  return (x^y^0xff);
                }
                else {
                  return(KEYTBL[PIAAIO&0x0f]);
                }
              }
            }
          }
          if ((Addr & 0x03)==3) 
          {
            /* read from PIABCTL */
            return (PIABCTL | 0x80);
          }
          break;
            
        case 0x2000:  // VDP read 0x2000 to 0x2FFF
          if ((Addr & 1)==0) return(RdData9918());
          return(RdCtrl9918());
          break;
    }

    return RAM_Memory[Addr];
}

// End of file
