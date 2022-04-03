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

#include "colecoDS.h"
#include "AdamNet.h"
#include "FDIDisk.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "MTX_BIOS.h"
#define NORAM 0xFF

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
      // -------------------------------------------
      // Only port 1 is used for the first Joystick
      // -------------------------------------------
      if (ay_reg_idx == 14)
      {
          u8 joy1 = 0x00;

          if (myConfig.dpad == DPAD_JOYSTICK)
          {
              if (JoyState & JST_UP)    joy1 |= 0x01;
              if (JoyState & JST_DOWN)  joy1 |= 0x02;
              if (JoyState & JST_LEFT)  joy1 |= 0x04;
              if (JoyState & JST_RIGHT) joy1 |= 0x08;
          }
          else if (myConfig.dpad == DPAD_DIAGONALS)
          {
              if (JoyState & JST_UP)    joy1 |= (0x01 | 0x08);
              if (JoyState & JST_DOWN)  joy1 |= (0x02 | 0x04);
              if (JoyState & JST_LEFT)  joy1 |= (0x04 | 0x01);
              if (JoyState & JST_RIGHT) joy1 |= (0x08 | 0x02);
          }

          ay_reg[14] = ~joy1;
      }      
      return FakeAY_ReadData();
  }    
  else if (Port == 0x98) 
  {
      PortA8 |= 0x30;
      if (JoyState & JST_FIREL) PortA8 &= ~0x10;
      if (JoyState & JST_FIRER) PortA8 &= ~0x10;
      
      return PortA8;
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
      
      u8 key1 = 0x00;
      if ((PortAA & 0x0F) == 0)      // Row 0
      {
          // -----------------------------------------------------------
          // We are at the top of the scan loop... if we have buffered 
          // keys, we insert them into the stream now...
          // -----------------------------------------------------------
          if (key_shift_hold > 0) {key_shift = 1; key_shift_hold--;}
          if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
          {
              msx_key = BufferedKeys[BufferedKeysReadIdx];
              BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
              if (msx_key == KBD_KEY_SHIFT) key_shift_hold = 1;
          }
          
          if (msx_key)
          {
              if (msx_key == '0')           key1 = 0x01;
              if (msx_key == '1')           key1 = 0x02;
              if (msx_key == '2')           key1 = 0x04;
              if (msx_key == '3')           key1 = 0x08;
              if (msx_key == '4')           key1 = 0x10;
              if (msx_key == '5')           key1 = 0x20;
              if (msx_key == '6')           key1 = 0x40;
              if (msx_key == '7')           key1 = 0x80;
          }
      }      
      else if ((PortAA & 0x0F) == 1)
      {
          if (msx_key)
          {
              if (msx_key == '8')           key1 = 0x01;
              if (msx_key == '9')           key1 = 0x02;
              if (msx_key == ':')           key1 = 0x04;
              if (msx_key == KBD_KEY_QUOTE) key1 = 0x08;
              if (msx_key == ',')           key1 = 0x10;
              if (msx_key == '=')           key1 = 0x20;
              if (msx_key == '.')           key1 = 0x40;
              if (msx_key == '/')           key1 = 0x80;
          }
      }
      else if ((PortAA & 0x0F) == 2)  // Row 2
      {
          if (msx_key)
          {
              if (msx_key == '-')           key1 = 0x01;
              if (msx_key == 'A')           key1 = 0x02;
              if (msx_key == 'B')           key1 = 0x04;
              if (msx_key == 'C')           key1 = 0x08;
              if (msx_key == 'D')           key1 = 0x10;
              if (msx_key == 'E')           key1 = 0x20;
              if (msx_key == 'F')           key1 = 0x40;
              if (msx_key == 'G')           key1 = 0x80;
          }          
      }
      else if ((PortAA & 0x0F) == 3)  // Row 3
      {
          if (msx_key)
          {
              if (msx_key == 'H')           key1 = 0x01;
              if (msx_key == 'I')           key1 = 0x02;
              if (msx_key == 'J')           key1 = 0x04;
              if (msx_key == 'K')           key1 = 0x08;
              if (msx_key == 'L')           key1 = 0x10;
              if (msx_key == 'M')           key1 = 0x20;
              if (msx_key == 'N')           key1 = 0x40;
              if (msx_key == 'O')           key1 = 0x80;
          }          
      }
      else if ((PortAA & 0x0F) == 4)  // Row 4
      {
          if (msx_key)
          {
              if (msx_key == 'P')           key1 = 0x01;
              if (msx_key == 'Q')           key1 = 0x02;
              if (msx_key == 'R')           key1 = 0x04;
              if (msx_key == 'S')           key1 = 0x08;
              if (msx_key == 'T')           key1 = 0x10;
              if (msx_key == 'U')           key1 = 0x20;
              if (msx_key == 'V')           key1 = 0x40;
              if (msx_key == 'W')           key1 = 0x80;
          }          
      }
      else if ((PortAA & 0x0F) == 5)  // Row 5
      {
          if (myConfig.dpad == DPAD_MSX_KEYS)
          {
              if (JoyState & JST_UP)    key1 |= 0x80;  // KEY UP
          }
          if (msx_key)
          {
              if (msx_key == 'X')           key1 = 0x01;
              if (msx_key == 'Y')           key1 = 0x02;
              if (msx_key == 'Z')           key1 = 0x04;
              if (msx_key == '[')           key1 = 0x08;
              if (msx_key == ']')           key1 = 0x10;
              if (msx_key == KBD_KEY_DEL)   key1 = 0x40;              
              if (msx_key == KBD_KEY_UP)    key1 = 0x80;
          }          
      }      
      else if ((PortAA & 0x0F) == 6) // Row 6
      {
          // Handle the Shift Key ... two ways
          if (key_shift || (msx_key == KBD_KEY_SHIFT))  key1 = 0x01;
          
          if (myConfig.dpad == DPAD_MSX_KEYS)
          {
              if (JoyState & JST_LEFT)  key1 |= 0x80;  // KEY LEFT
          }
          if (msx_key)
          {
              if (msx_key == KBD_KEY_CTRL)  key1 = 0x02;
              if (msx_key == KBD_KEY_ESC)   key1 = 0x10;
              if (msx_key == KBD_KEY_STOP)  key1 = 0x20;
              if (msx_key == KBD_KEY_RET)   key1 = 0x40;
              if (msx_key == KBD_KEY_LEFT)  key1 = 0x80;
          }          
      }
      else if ((PortAA & 0x0F) == 7) // Row 7
      {
          if (myConfig.dpad == DPAD_MSX_KEYS)
          {
              if (JoyState & JST_DOWN)  key1 |= 0x80;  // KEY DOWN
          }
          if (msx_key)
          {
              if (msx_key == KBD_KEY_F1)    key1 = 0x01;
              if (msx_key == KBD_KEY_F2)    key1 = 0x02;
              if (msx_key == KBD_KEY_F3)    key1 = 0x04;
              if (msx_key == KBD_KEY_F4)    key1 = 0x08;
              if (msx_key == KBD_KEY_F5)    key1 = 0x10;
              if (msx_key == KBD_KEY_DOWN)  key1 = 0x80;              
          }          
      }
      else if ((PortAA & 0x0F) == 8) // Row 8
      {
          if (JoyState == JST_STAR)  key1 |= 0x01;  // SPACE
          if (JoyState & JST_PURPLE) key1 |= 0x01;  // SPACE
          if (JoyState & JST_BLUE)   key1 |= 0x01;  // SPACE
          
          if (myConfig.dpad == DPAD_MSX_KEYS)
          {
              if (JoyState & JST_RIGHT) key1 |= 0x80;  // KEY RIGHT          
              if (JoyState & JST_FIREL) key1 |= 0x01;  // SPACE
              if (JoyState & JST_FIRER) key1 |= 0x01;  // SPACE
          }
          if (msx_key)
          {
              if (msx_key == ' ')           key1 = 0x01;
              if (msx_key == KBD_KEY_RIGHT) key1 = 0x80;
              if (msx_key == KBD_KEY_CAPS)  key1 = 0x08;              
          }          
      }
      return ~key1;
  }
  else if (Port == 0x9A) return PortAA;    
    
  // No such port
  return(NORAM);
}


// --------------------------------------------------------------------
// Patch the SVI bios so that we can intercept cassette read/writes...
// --------------------------------------------------------------------
void SVI_PatchBIOS(void)
{
    //patchAddressSVI[] = {0x006C,0x006F,0x0072,0x0075,0x0078,0x210A,0x21A9,0}; // 0x0069
    pColecoMem[0x210A] = 0xed; pColecoMem[0x210B] = 0xfe; pColecoMem[0x210C] = 0xc9; 
    pColecoMem[0x21A9] = 0xed; pColecoMem[0x21AA] = 0xfe; pColecoMem[0x21AB] = 0xc9; 
    pColecoMem[0x0069] = 0xed; pColecoMem[0x006A] = 0xfe; pColecoMem[0x006B] = 0xc9; 
    pColecoMem[0x006C] = 0xed; pColecoMem[0x006D] = 0xfe; pColecoMem[0x006E] = 0xc9; 
    pColecoMem[0x006F] = 0xed; pColecoMem[0x0070] = 0xfe; pColecoMem[0x0071] = 0xc9; 
    pColecoMem[0x0072] = 0xed; pColecoMem[0x0073] = 0xfe; pColecoMem[0x0074] = 0xc9; 
    pColecoMem[0x0075] = 0xed; pColecoMem[0x0076] = 0xfe; pColecoMem[0x0077] = 0xc9; 
    pColecoMem[0x0078] = 0xed; pColecoMem[0x0079] = 0xfe; pColecoMem[0x007A] = 0xc9; 
    pColecoMem[0x2073] = 0x01;
    pColecoMem[0x20D0] = 0x10; pColecoMem[0x20D1] = 0x00;  
    pColecoMem[0x20E3]=0x00; pColecoMem[0x20E4]=0x00; pColecoMem[0x20E5]=0x00; pColecoMem[0x20E6]=0xed; pColecoMem[0x20E7]=0xfe;
}


//94    A8  W   PPI 8255 Port A
//95    A9  W   PPI 8255 Port B
//96    AA  W   PPI 8255 Port C
// #0000-#7FFF	Description	#8000-#FFFF	Description
// BANK 01	BASIC ROM	                        BANK 02	MAIN RAM
// BANK 11	Game cartridge ROM	                BANK 12	ROM0 + ROM1 (optional game cartridge ROMs)
// BANK 21	Standard SVI-328 extended RAM	    BANK 22	SV-807 RAM expansion
// BANK 31	SV-807 RAM expansion	            BANK 32	SV-807 RAM expansion
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
    else if (Port == 0x81) {if (WrCtrl9918(Value)) { CPU.IRequest=INT_RST38; cpuirequest=Z80_IRQ_INT;}}
    else if (Port == 0x88)    // PSG Area
    {
        FakeAY_WriteIndex(Value & 0x0F);
    }
    else if (Port == 0x8C) 
    {
        FakeAY_WriteData(Value);
        if (ay_reg_idx == 15)
        {
            IOBYTE = ay_reg[ay_reg_idx] & 0x1F;

            if (lastIOBYTE != IOBYTE)
            {
                lastIOBYTE = IOBYTE;
                
                if (IOBYTE == 0x1F)   // Normal ROM + 32K Upper RAM
                {
                      memcpy(pColecoMem,SVIBios,0x8000);        // Restore SVI BIOS (ram is already saved in Slot3RAM[])
                      SVI_PatchBIOS();
                      if (svi_RAM_start == 0xFFFF)
                      {
                        memcpy(pColecoMem+0x8000, Slot3RAM+0x8000, 0x8000);     // Restore RAM in upper slot
                      }
                      svi_RAM_start = 0x8000;
                    
                }
                else if (IOBYTE == 0x1D) // All 64K RAM
                {
                    if (svi_RAM_start == 0x8000)
                    {
                      memcpy(pColecoMem, Slot3RAM, 0x8000);     // Restore RAM in lower slot
                    }
                    else if (svi_RAM_start == 0xFFFF)
                    {
                      memcpy(pColecoMem, Slot3RAM, 0x10000);     // Restore RAM in both slots
                    }
                    svi_RAM_start = 0x0000;
                }
                else if (IOBYTE == 0x1E)   // Cart ROM (not present)
                {
                  memset(pColecoMem, 0xFF, 0x8000);     // Nothing in lower slot
                  svi_RAM_start = 0x8000;
                }
                else    // No RAM avaialble for any other combinations...
                {
                    debug1++;
                    debug3 = IOBYTE;
                    svi_RAM_start = 0xFFFF;
                    memset(pColecoMem+0x8000, 0xFF, 0x8000);    // No RAM in upper slot
                }                
            }
        }        
    }
    else if (Port == 0x95)  // PPI - Register B
    {
        PortA9 = Value;
    }
    else if (Port == 0x96)  // PPI - Register C
    {
        PortAA = Value;
    }
}

// ---------------------------------------------------------
// The SVI has some tape related stuff
// ---------------------------------------------------------
void svi_reset(void)
{
    if (svi_mode)
    {
        IOBYTE = 0x00;
        MTX_KBD_DRIVE = 0x00;
        lastIOBYTE = 99;
        tape_pos = 0;

        svi_RAM_start = 0x8000;
        
        PortA8 = 0x00;
        PortA9 = 0x00;
        PortAA = 0x00;       
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
            if ((romBuffer[tape_pos] == 0x55) && (romBuffer[tape_pos+1] == 0x55) && (romBuffer[tape_pos+2] == 0x7F))
            {
                tape_pos++; tape_pos++;
                break;
            }
            tape_pos++;
            if (tape_pos >= tape_len)
                break;
        }

        if (tape_pos < tape_len)
        {
            r->AF.B.h = romBuffer[tape_pos++];
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
        r->AF.B.h = romBuffer[tape_pos++];
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
            romBuffer[tape_pos++] = header_SVI[i];
        }
        if (tape_pos > tape_len)  tape_len=tape_pos;
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x0075)   // TAPOUT
    {
        romBuffer[tape_pos++] = r->AF.B.h;
        if (tape_pos > tape_len)  tape_len=tape_pos;
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x20E6)   // CASOUT
    {
        romBuffer[tape_pos++] = r->AF.B.h;
        if (tape_pos > tape_len)  tape_len=tape_pos;
        r->AF.B.l &= ~C_FLAG;
        r->PC.W = 0x20ED;
    }    
    else if (r->PC.W-2 == 0x0078)   // CTWOFF
    {
        r->AF.B.l |= C_FLAG;
    }
    else {debug4 = r->PC.W-2;}
}

// End of file
