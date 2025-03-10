// =====================================================================================
// Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>

#include "colecoDS.h"
#include "Adam.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "printf.h"

static u8 header_SVI[17] = { 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x7F};

// ------------------------------------------------------------------
// SPECTRAVIDEO SVI 318/328 Port Handling...
//80    98  W   VDP TMS9918A Writes a byte to VRAM, increases VRAM pointer
//81    99  W   VDP TMS9918A Address / Register output
//84    98  R   VDP TMS9918A Reads a byte from VRAM, increases VRAM pointer
//85    99  R   VDP TMS9918A Status register
//88    A0  W   PSG AY-3-8910 Adress latch
//8C    A1  W   PSG AY-3-8910 Data write
//90    A2  R   PSG AY-3-8910 Data read
//98    A8  R   PPI 8255 Port A
//99    A9  R   PPI 8255 Port B
//9A    AA  R   PPI 8255 Port C
//9B    AB  R   PPI 8255 Mode select and I/O setup of A,B,C
// ------------------------------------------------------------------
unsigned char cpu_readport_svi(register unsigned short Port)
{
  // SVI ports are 8-bit
  Port &= 0x00FF;

  // Access to the VDP I/O ports.
  if      (Port == 0x84) return RdData9918();
  else if (Port == 0x85) return RdCtrl9918();
  else if (Port == 0x90)  // PSG Read... might be joypad data
  {
      // -------------------------------------------------------
      // Port A is used for both P1 and P2 Joystick directions
      // -------------------------------------------------------
      if (myAY.ayRegIndex == 14)
      {
          u8 joy1 = 0x00;

          if (myConfig.dpad == DPAD_NORMAL)
          {
              if (JoyState & JST_UP)    joy1 |= 0x01;
              if (JoyState & JST_DOWN)  joy1 |= 0x02;
              if (JoyState & JST_LEFT)  joy1 |= 0x04;
              if (JoyState & JST_RIGHT) joy1 |= 0x08;

              if (JoyState & (JST_UP << 16))    joy1 |= 0x10;
              if (JoyState & (JST_DOWN << 16))  joy1 |= 0x20;
              if (JoyState & (JST_LEFT << 16))  joy1 |= 0x40;
              if (JoyState & (JST_RIGHT << 16)) joy1 |= 0x80;
          }
          else if (myConfig.dpad == DPAD_DIAGONALS)
          {
              if (JoyState & JST_UP)    joy1 |= (0x01 | 0x08);
              if (JoyState & JST_DOWN)  joy1 |= (0x02 | 0x04);
              if (JoyState & JST_LEFT)  joy1 |= (0x04 | 0x01);
              if (JoyState & JST_RIGHT) joy1 |= (0x08 | 0x02);
          }

          myAY.ayPortAIn = ~joy1;
      }
      return ay38910DataR(&myAY);
  }
  else if (Port == 0x98)
  {
      Port_PPI_A |= 0x3F;
      if (JoyState & JST_FIREL)   Port_PPI_A &= ~0x10;
      if (JoyState & JST_FIRER)   Port_PPI_A &= ~0x10;

      if (JoyState & (JST_FIREL<<16))   Port_PPI_A &= ~0x20;
      if (JoyState & (JST_FIRER<<16))   Port_PPI_A &= ~0x20;

      return Port_PPI_A;
  }
  else if (Port == 0x99)
  {
      // ------------------------------------------------------------------------
      // SVI Keyboard Port:
      //        bit7    bit6    bit5    bit4    bit3    bit2    bit1    bit0
      // row 0  7&      6^      5%      4$      3#      2@      1!      0)
      // row 1  /?      .>      =+      ,<      '"      ;:      9(      8*
      // row 2  G       F       E       D       C       B       A       -_
      // row 3  O       N       M       L       K       J       I       H
      // row 4  W       V       U       T       S       R       Q       P
      // row 5  ↑       BS      ]}      \~      [{      Z       Y       X
      // row 6  ←       ENTER   STOP    ESC     GRAPR   GRAPL   CTRL    SHIFT
      // row 7  ↓       INS     CLS     F5      F4      F3      F2      F1
      // row 8  →       n/a     PRINT   SEL     CAPS    DEL     TAB     SPACE
      // row 9  NUM7    NUM6    NUM5    NUM4    NUM3    NUM2    NUM1    NUM0
      // row 10 NUM,    NUM.    NUM/    NUM*    NUM-    NUM+    NUM9    NUM8
      // ------------------------------------------------------------------------

      u8 key1 = 0x00;   // Accumulate keys here...

      // For the full keyboard overlay... this is a bit of a hack for SHIFT, CTRL, CODE and GRAPH
      if (last_special_key != 0)
      {
          if ((last_special_key_dampen > 0) && (last_special_key_dampen != 20))
          {
              if (--last_special_key_dampen == 0)
              {
                  last_special_key = 0;
                  DSPrint(4,0,6, "    ");
              }
          }

          if (last_special_key == KBD_KEY_SHIFT)
          {
            DSPrint(4,0,6, "SHFT");
            key_shift = 1;
          }
          else if (last_special_key == KBD_KEY_CTRL)
          {
            DSPrint(4,0,6, "CTRL");
            key_ctrl = 1;
          }
          else if (last_special_key == KBD_KEY_CODE)
          {
            DSPrint(4,0,6, "CODE");
            key_code = 1;
          }
          else if (last_special_key == KBD_KEY_GRAPH)
          {
            DSPrint(4,0,6, "GRPH");
            key_graph = 1;
          }

          if ((kbd_key != 0) && (kbd_key != KBD_KEY_SHIFT) && (kbd_key != KBD_KEY_CTRL) && (kbd_key != KBD_KEY_CODE) && (kbd_key != KBD_KEY_GRAPH))
          {
              if (last_special_key_dampen == 20) last_special_key_dampen = 19;    // Start the SHIFT/CONTROL countdown... this should be enough time for it to register
          }
      }

      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i< (kbd_keys_pressed ? kbd_keys_pressed:1); i++) // Always one pass at least for joysticks...
      {
          kbd_key = kbd_keys[i];

          if ((Port_PPI_C & 0x0F) == 0)      // Row 0
          {
              if (kbd_key)
              {
                  if (kbd_key == '0')           key1 |= 0x01;
                  if (kbd_key == '1')           key1 |= 0x02;
                  if (kbd_key == '2')           key1 |= 0x04;
                  if (kbd_key == '3')           key1 |= 0x08;
                  if (kbd_key == '4')           key1 |= 0x10;
                  if (kbd_key == '5')           key1 |= 0x20;
                  if (kbd_key == '6')           key1 |= 0x40;
                  if (kbd_key == '7')           key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 1) // Row 1
          {
              if (kbd_key)
              {
                  if (kbd_key == '8')           key1 |= 0x01;
                  if (kbd_key == '9')           key1 |= 0x02;
                  if (kbd_key == ':')           key1 |= 0x04;
                  if (kbd_key == KBD_KEY_QUOTE) key1 |= 0x08;
                  if (kbd_key == ',')           key1 |= 0x10;
                  if (kbd_key == '=')           key1 |= 0x20;
                  if (kbd_key == '.')           key1 |= 0x40;
                  if (kbd_key == '/')           key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 2)  // Row 2
          {
              if (kbd_key)
              {
                  if (kbd_key == '-')           key1 |= 0x01;
                  if (kbd_key == 'A')           key1 |= 0x02;
                  if (kbd_key == 'B')           key1 |= 0x04;
                  if (kbd_key == 'C')           key1 |= 0x08;
                  if (kbd_key == 'D')           key1 |= 0x10;
                  if (kbd_key == 'E')           key1 |= 0x20;
                  if (kbd_key == 'F')           key1 |= 0x40;
                  if (kbd_key == 'G')           key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 3)  // Row 3
          {
              if (kbd_key)
              {
                  if (kbd_key == 'H')           key1 |= 0x01;
                  if (kbd_key == 'I')           key1 |= 0x02;
                  if (kbd_key == 'J')           key1 |= 0x04;
                  if (kbd_key == 'K')           key1 |= 0x08;
                  if (kbd_key == 'L')           key1 |= 0x10;
                  if (kbd_key == 'M')           key1 |= 0x20;
                  if (kbd_key == 'N')           key1 |= 0x40;
                  if (kbd_key == 'O')           key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 4)  // Row 4
          {
              if (kbd_key)
              {
                  if (kbd_key == 'P')           key1 |= 0x01;
                  if (kbd_key == 'Q')           key1 |= 0x02;
                  if (kbd_key == 'R')           key1 |= 0x04;
                  if (kbd_key == 'S')           key1 |= 0x08;
                  if (kbd_key == 'T')           key1 |= 0x10;
                  if (kbd_key == 'U')           key1 |= 0x20;
                  if (kbd_key == 'V')           key1 |= 0x40;
                  if (kbd_key == 'W')           key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 5)  // Row 5
          {
              if (kbd_key)
              {
                  if (kbd_key == 'X')           key1 |= 0x01;
                  if (kbd_key == 'Y')           key1 |= 0x02;
                  if (kbd_key == 'Z')           key1 |= 0x04;
                  if (kbd_key == '[')           key1 |= 0x08;
                  if (kbd_key == '`')           key1 |= 0x10;
                  if (kbd_key == ']')           key1 |= 0x20;
                  if (kbd_key == KBD_KEY_DEL)   key1 |= 0x40;
                  if (kbd_key == KBD_KEY_UP)    key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 6) // Row 6
          {
              if (kbd_key)
              {
                  if (kbd_key == KBD_KEY_SHIFT) key1 |= 0x01;
                  if (kbd_key == KBD_KEY_CTRL)  key1 |= 0x02;
                  if (kbd_key == KBD_KEY_GRAPH) key1 |= 0x04;
                  if (kbd_key == KBD_KEY_CODE)  key1 |= 0x08;
                  if (kbd_key == KBD_KEY_ESC)   key1 |= 0x10;
                  if (kbd_key == KBD_KEY_BREAK) key1 |= 0x20;
                  if (kbd_key == KBD_KEY_RET)   key1 |= 0x40;
                  if (kbd_key == KBD_KEY_LEFT)  key1 |= 0x80;
              }

              // Handle the Shift Key and Control Key
              if (key_shift)  key1 |= 0x01;
              if (key_ctrl)   key1 |= 0x02;
              if (key_graph)  key1 |= 0x04;
              if (key_code)   key1 |= 0x08;
          }
          else if ((Port_PPI_C & 0x0F) == 7) // Row 7
          {
              if (kbd_key)
              {
                  if (kbd_key == KBD_KEY_F1)    key1 |= 0x01;
                  if (kbd_key == KBD_KEY_F2)    key1 |= 0x02;
                  if (kbd_key == KBD_KEY_F3)    key1 |= 0x04;
                  if (kbd_key == KBD_KEY_F4)    key1 |= 0x08;
                  if (kbd_key == KBD_KEY_F5)    key1 |= 0x10;
                  if (kbd_key == KBD_KEY_HOME)  key1 |= 0x20;
                  if (kbd_key == KBD_KEY_INS)   key1 |= 0x40;
                  if (kbd_key == KBD_KEY_DOWN)  key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 8) // Row 8
          {
              if ((JoyState & 0x0F) == JST_STAR)  key1 |= 0x01;  // SPACE
              if (JoyState == JST_PURPLE)         key1 |= 0x01;  // SPACE

              if (kbd_key)
              {
                  if (kbd_key == ' ')           key1 |= 0x01;
                  if (kbd_key == KBD_KEY_TAB)   key1 |= 0x02;
                  if (kbd_key == KBD_KEY_DEL)   key1 |= 0x04;
                  if (kbd_key == KBD_KEY_CAPS)  key1 |= 0x08;
                  if (kbd_key == KBD_KEY_SEL)   key1 |= 0x10;
                  if (kbd_key == KBD_KEY_RIGHT) key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 9) // Row 9
          {
              if (JoyState == JST_BLUE)   key1 |= 0x08;  // NUM3
          }
      }

      return ~key1;
  }
  else if (Port == 0x9A) return Port_PPI_C;

  // No such port
  return(NORAM);
}


// ------------------------------------------------------------------------
// Patch SVI BIOS for Cassette use and point memory map to 32K BIOS in 
// lower memory and RAM in upper memory ... the running program can 
// alter this and swap in RAM or ROM in either segment of 32K memory map.
// ------------------------------------------------------------------------
void svi_restore_bios(void)
{
    memcpy(BIOS_Memory, SVIBios, 0x8000);       // Restore SVI BIOS

    // And patch it for cassette use...
    BIOS_Memory[0x210A] = 0xed; BIOS_Memory[0x210B] = 0xfe; BIOS_Memory[0x210C] = 0xc9;
    BIOS_Memory[0x21A9] = 0xed; BIOS_Memory[0x21AA] = 0xfe; BIOS_Memory[0x21AB] = 0xc9;
    BIOS_Memory[0x0069] = 0xed; BIOS_Memory[0x006A] = 0xfe; BIOS_Memory[0x006B] = 0xc9;
    BIOS_Memory[0x006C] = 0xed; BIOS_Memory[0x006D] = 0xfe; BIOS_Memory[0x006E] = 0xc9;
    BIOS_Memory[0x006F] = 0xed; BIOS_Memory[0x0070] = 0xfe; BIOS_Memory[0x0071] = 0xc9;
    BIOS_Memory[0x0072] = 0xed; BIOS_Memory[0x0073] = 0xfe; BIOS_Memory[0x0074] = 0xc9;
    BIOS_Memory[0x0075] = 0xed; BIOS_Memory[0x0076] = 0xfe; BIOS_Memory[0x0077] = 0xc9;
    BIOS_Memory[0x0078] = 0xed; BIOS_Memory[0x0079] = 0xfe; BIOS_Memory[0x007A] = 0xc9;
    //BIOS_Memory[0x2073] = 0x01;  // Remove Delay
    BIOS_Memory[0x20D0] = 0x10; BIOS_Memory[0x20D1] = 0x00;   // Only write 0x10 header bytes (instead of 190!)
    BIOS_Memory[0x20E3]=0x00; BIOS_Memory[0x20E4]=0x00; BIOS_Memory[0x20E5]=0x00; BIOS_Memory[0x20E6]=0xed; BIOS_Memory[0x20E7]=0xfe;

    MemoryMap[0] = (u8 *)(BIOS_Memory + 0x0000);      // Restore SVI BIOS
    MemoryMap[1] = (u8 *)(BIOS_Memory + 0x2000);      // Restore SVI BIOS
    MemoryMap[2] = (u8 *)(BIOS_Memory + 0x4000);      // Restore SVI BIOS
    MemoryMap[3] = (u8 *)(BIOS_Memory + 0x6000);      // Restore SVI BIOS
    
    MemoryMap[4] = (u8 *)(RAM_Memory  + 0x8000);      // RAM here by default
    MemoryMap[5] = (u8 *)(RAM_Memory  + 0xA000);      // RAM here by default
    MemoryMap[6] = (u8 *)(RAM_Memory  + 0xC000);      // RAM here by default
    MemoryMap[7] = (u8 *)(RAM_Memory  + 0xE000);      // RAM here by default
    
    svi_RAMinSegment[0]  = 0;    // ROM in lower 32K segment by default
    svi_RAMinSegment[1]  = 1;    // RAM in upper 32K segment by default
}


//94    A8  W   PPI 8255 Port A
//95    A9  W   PPI 8255 Port B
//96    AA  W   PPI 8255 Port C
//
// #0000-#7FFF  Description #8000-#FFFF Description
// BANK 01  BASIC ROM                           BANK 02 MAIN RAM
// BANK 11  Game cartridge ROM                  BANK 12 ROM0 + ROM1 (optional game cartridge ROMs)
// BANK 21  Standard SVI-328 extended RAM       BANK 22 SV-807 RAM expansion
// BANK 31  SV-807 RAM expansion                BANK 32 SV-807 RAM expansion
//
// The BANK selection is handled trough PSG Register 15 bits:
// 0 CART ROM Bank 11 (#0000-#7FFF) Game cartridge
// 1 BK21 RAM Bank 21 (#0000-#7FFF) RAM on SVI-328
// 2 BK22 RAM Bank 22 (#8000-#FFFF) Expansion RAM
// 3 BK31 RAM Bank 31 (#0000-#7FFF) Expansion RAM
// 4 BK32 RAM Bank 32 (#8000-#FFFF) Expansion RAM
// 5 (CAPS Caps Lock LED on/off)
// 6 ROMEN0 ROM "Bank 12/L" (#8000-#BFFF) Game cartridge
// 7 ROMEN1 ROM "Bank 12/H" (#C000-#FFFF) Game cartridge
// 0 = Enabled, 1 = Disabled
void cpu_writeport_svi(register unsigned short Port,register unsigned char Value)
{
    // SVI ports are 8-bit
    Port &= 0x00FF;

    if      (Port == 0x80) WrData9918(Value);
    else if (Port == 0x81) {if (WrCtrl9918(Value)) { CPU.IRequest=INT_RST38; }}
    else if (Port == 0x88)    // PSG Area
    {
        ay38910IndexW(Value, &myAY);
    }
    else if (Port == 0x8C)
    {
        ay38910DataW(Value, &myAY);
        if (myAY.ayRegIndex == 14)
        {
            myAY.ayPortAIn = Value;
        }
        else if (myAY.ayRegIndex == 15)
        {
            myAY.ayPortBIn = Value;
            IOBYTE = Value;

            if (lastIOBYTE != IOBYTE)
            {
                u8 lowerBankEnable = (~IOBYTE) & 0x0B;          // Positive logic

                if (lowerBankEnable == 0x00)  // Normal BIOS ROM in lower bank (fairly common configuration)
                {
                    MemoryMap[0] = (u8 *)(BIOS_Memory + 0x0000);      // Restore SVI BIOS
                    MemoryMap[1] = (u8 *)(BIOS_Memory + 0x2000);      // Restore SVI BIOS
                    MemoryMap[2] = (u8 *)(BIOS_Memory + 0x4000);      // Restore SVI BIOS
                    MemoryMap[3] = (u8 *)(BIOS_Memory + 0x6000);      // Restore SVI BIOS
                    svi_RAMinSegment[0]  = 0;
                }
                else if (lowerBankEnable & 0x01)   // No Game Cart in lower slot
                {
                    if (svi_mode == 2)
                    {
                        MemoryMap[0] = (u8 *)(ROM_Memory + 0x0000);       // Cartridge here
                        MemoryMap[1] = (u8 *)(ROM_Memory + 0x2000);       // Cartridge here
                        MemoryMap[2] = (u8 *)(ROM_Memory + 0x4000);       // Cartridge here
                        MemoryMap[3] = (u8 *)(ROM_Memory + 0x6000);       // Cartridge here
                    }
                    else
                    {
                        MemoryMap[0] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                        MemoryMap[1] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                        MemoryMap[2] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                        MemoryMap[3] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    }
                    svi_RAMinSegment[0]  = 0;
                }
                else if (lowerBankEnable & 0x02)   // SVI-328 has the other 32K of RAM in lower slot
                {
                    MemoryMap[0] = (u8 *)(RAM_Memory + 0x0000);       // Normal RAM in lower slot
                    MemoryMap[1] = (u8 *)(RAM_Memory + 0x2000);       // Normal RAM in lower slot
                    MemoryMap[2] = (u8 *)(RAM_Memory + 0x4000);       // Normal RAM in lower slot
                    MemoryMap[3] = (u8 *)(RAM_Memory + 0x6000);       // Normal RAM in lower slot
                    svi_RAMinSegment[0]  = 1;
                }
                else if (lowerBankEnable & 0x08)   // No Expansion RAM in lower slot
                {
                    MemoryMap[0] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    MemoryMap[1] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    MemoryMap[2] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    MemoryMap[3] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    svi_RAMinSegment[0]  = 0;
                }


                u8 upperBankEnable = (~IOBYTE) & 0xD4;          // Positive logic

                if (upperBankEnable == 0x00)  // Normal 32K RAM in upper bank (fairly common)
                {
                      MemoryMap[4] = (u8 *)(RAM_Memory + 0x8000);      // Restore RAM
                      MemoryMap[5] = (u8 *)(RAM_Memory + 0xA000);      // Restore RAM
                      MemoryMap[6] = (u8 *)(RAM_Memory + 0xC000);      // Restore RAM
                      MemoryMap[7] = (u8 *)(RAM_Memory + 0xE000);      // Restore RAM
                      svi_RAMinSegment[1] = 1;
                }
                else if (((upperBankEnable & 0xC0)==0x40) && (svi_mode == 2))   // Cart in upper slot
                {
                    MemoryMap[4] = (u8 *)(ROM_Memory + 0x8000);       // Cartridge here
                    MemoryMap[5] = (u8 *)(ROM_Memory + 0xA000);       // Cartridge here
                    svi_RAMinSegment[1]  = 1;
                }
                else if (((upperBankEnable & 0xC0)==0x80) && (svi_mode == 2))   // Cart in upper slot
                {
                    MemoryMap[6] = (u8 *)(ROM_Memory + 0xC000);       // Cartridge here
                    MemoryMap[7] = (u8 *)(ROM_Memory + 0xE000);       // Cartridge here
                    svi_RAMinSegment[1]  = 1;
                }
                else if (((upperBankEnable & 0xC0)==0xC0) && (svi_mode == 2))   // Cart in upper slot
                {
                    MemoryMap[4] = (u8 *)(ROM_Memory + 0x8000);       // Cartridge here
                    MemoryMap[5] = (u8 *)(ROM_Memory + 0xA000);       // Cartridge here
                    MemoryMap[6] = (u8 *)(ROM_Memory + 0xC000);       // Cartridge here
                    MemoryMap[7] = (u8 *)(ROM_Memory + 0xE000);       // Cartridge here
                    svi_RAMinSegment[1]  = 0;
                }
                else // Nothing lives here...
                {
                    MemoryMap[4] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    MemoryMap[5] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    MemoryMap[6] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    MemoryMap[7] = (u8 *)(BIOS_Memory + 0xC000);       // Nothing here... 0xFF
                    svi_RAMinSegment[1]  = 0;
                }

                lastIOBYTE = IOBYTE;
            }
        }
    }
    else if (Port == 0x95)  // PPI - Register B
    {
        Port_PPI_B = Value;
    }
    else if (Port == 0x96)  // PPI - Register C
    {
        Port_PPI_C = Value;
    }
    else if (Port == 0x97)  // PPI - Mode/Control
    {
        Port_PPI_CTRL = Value;
        if ((Value & 0x80) == 0)    // Set or Clear Bits
        {
            if (Value & 0x01)
            {
                Port_PPI_C |= (0x01 << ((Value >> 1) & 0x07));
            }
            else
            {
                Port_PPI_C &= ~(0x01 << ((Value >> 1) & 0x07));
            }
        }
        else
        {
        }
    }
  else if (Port > 0x80)
  {

  }

}

// ---------------------------------------------------------
// The SVI has some tape related stuff and memory banking
// ---------------------------------------------------------
void svi_reset(void)
{
    if (svi_mode)
    {
        IOBYTE = 0x00;
        lastIOBYTE = 99;
        tape_pos = 0;

        svi_RAMinSegment[0] = 0;
        svi_RAMinSegment[1] = 1;

        Port_PPI_A = 0x00;
        Port_PPI_B = 0x00;
        Port_PPI_C = 0x00;
    }
}

// Spectravideo SVI Cassette patch:
// BIOS Area:
//case 0x0069: tapion(ref, cpu); break; // CSRDON
//case 0x006C: tapin(ref, cpu);  break; // CASIN
//case 0x006F: tapiof(ref, cpu); break; // CTOFF
//case 0x0072: tapoon(ref, cpu); break; // CWRTON
//case 0x0075: tapout(ref, cpu); break; // CASOUT
//case 0x0078: tapoof(ref, cpu); break; // CTWOFF
// SVI-328 BASIC:
//case 0x20E6: casout(ref, cpu); break; // CASOUT
//case 0x210A: tapion(ref, cpu); break; // CSRDON
//case 0x21A9: tapin(ref, cpu); break;  // CASIN
void SVI_HandleCassette(register Z80 *r)
{
    if ( r->PC.W-2 == 0x210A || r->PC.W-2 == 0x0069)
    {
        if (tape_pos >= tape_len) {r->AF.B.l |= C_FLAG;return;}
        u8 done = false;
        // Find Header/Program
        while (!done)
        {
            if ((ROM_Memory[tape_pos] == 0x55) && (ROM_Memory[tape_pos+1] == 0x55) && (ROM_Memory[tape_pos+2] == 0x55) && (ROM_Memory[tape_pos+3] == 0x55) && (ROM_Memory[tape_pos+4] == 0x7F))
            {
                tape_pos++; tape_pos++; tape_pos++; tape_pos++;
                break;
            }
            tape_pos++;
            if (tape_pos >= tape_len)
                break;
        }

        if (tape_pos < tape_len)
        {
            r->AF.B.h = ROM_Memory[tape_pos++];
            r->AF.B.l &= ~C_FLAG;
        }
        else
        {
            r->AF.B.l |= C_FLAG;
        }
    }
    else if ( r->PC.W-2 == 0x21A9 || r->PC.W-2 == 0x006C)
    {
        if (tape_pos >= tape_len) {r->AF.B.l |= C_FLAG;return;}
        // Read Data Byte from Cassette
        r->AF.B.h = ROM_Memory[tape_pos++];
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x006F)   // Stop Tape
    {
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x0072)   // CWRTON
    {
        for (u8 i=0; i<17; i++)
        {
            ROM_Memory[tape_pos++] = header_SVI[i];
        }
        if (tape_pos > tape_len)  tape_len=tape_pos;
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x0075)   // TAPOUT
    {
        ROM_Memory[tape_pos++] = r->AF.B.h;
        if (tape_pos > tape_len)  tape_len=tape_pos;
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x20E6)   // CASOUT
    {
        ROM_Memory[tape_pos++] = r->AF.B.h;
        if (tape_pos > tape_len)  tape_len=tape_pos;
        r->AF.B.l &= ~C_FLAG;
        r->PC.W = 0x20ED;
    }
    else if (r->PC.W-2 == 0x0078)   // CTWOFF
    {
        r->AF.B.l |= C_FLAG;
    }
    else {debug[15] = r->PC.W-2;} // Debug breadcrumbs in case we get here...
}

// End of file
