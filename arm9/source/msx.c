// =====================================================================================
// Copyright (c) 2021-2024 Dave Bernazzani (wavemotion-dave)
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
#include <dirent.h>

#include "colecoDS.h"
#include "Adam.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "MSX_CBIOS.h"
#include "fdc.h"
#include "printf.h"

// ---------------------------------------
// Some MSX Mapper / Slot Handling stuff
// ---------------------------------------
u8 mapperType           __attribute__((section(".dtcm"))) = 0;
u8 mapperMask           __attribute__((section(".dtcm"))) = 0;
u8 bROMInSegment[4]     __attribute__((section(".dtcm"))) = {0,0,0,0};
u8 bRAMInSegment[4]     __attribute__((section(".dtcm"))) = {0,0,0,0};

u8 *MSXCartPtr[8]       __attribute__((section(".dtcm"))) = {0,0,0,0,0,0,0,0};

u16 beeperFreq          __attribute__((section(".dtcm"))) = 0;
u8 msx_beeper_process   __attribute__((section(".dtcm"))) = 0;
u8 beeperWasOn          __attribute__((section(".dtcm"))) = 0;
u8 msx_sram_enabled     __attribute__((section(".dtcm"))) = 0;

u16 msx_block_size      __attribute__((section(".dtcm"))) = 0x2000; // Either 8K or 16K based on Mapper Type

SCC mySCC               __attribute__((section(".dtcm")));          // Declare new SCC module for Konami MSX games that use it

// --------------------------------------------------------------------------
// These aren't used very often so we don't need them in fast .dtcm memory
// --------------------------------------------------------------------------
u16 msx_init            = 0x4000;
u16 msx_basic           = 0x0000;
u32 msx_last_rom_size   = 0;
u8  msx_japanese_matrix = 0;        // Default to International keyboard layout. Set to '1' to enable Japanese layout.

static u8 header_MSX[8] = { 0x1f,0xa6,0xde,0xba,0xcc,0x13,0x7d,0x74 };

// ------------------------------------------------------------------
// MSX IO Port Read - just VDP and Joystick to contend with...
// ------------------------------------------------------------------
unsigned char cpu_readport_msx(register unsigned short Port) 
{
  // MSX ports are 8-bit
  Port &= 0x00FF; 

  //98h~9Bh   Access to the VDP I/O ports.    
  if      (Port == 0x98) return RdData9918();
  else if (Port == 0x99) return RdCtrl9918(); 
  else if (Port == 0xA2)  // PSG Read... might be joypad data
  {
      // -------------------------------------------
      // Only port 1 is used for the first Joystick
      // -------------------------------------------
      if (myAY.ayRegIndex == 14)
      {
          u8 joy1 = 0x00;

          // -------------------------------------------------------------
          // Only port 1... not port 2. AY register 15 (PortB) bit 6 is 
          // set to 0 for the port 1 joystick and that's the only one 
          // this emulator will respond to...
          // -------------------------------------------------------------
          if ((myAY.ayPortBOut & 0x40) == 0)
          {
              if (myConfig.dpad == DPAD_NORMAL)
              {
                  if (JoyState & JST_UP)    joy1 |= 0x01;
                  if (JoyState & JST_DOWN)  joy1 |= 0x02;
                  if (JoyState & JST_LEFT)  joy1 |= 0x04;
                  if (JoyState & JST_RIGHT) joy1 |= 0x08;

                  if (JoyState & JST_FIREL) joy1 |= 0x10;
                  if (JoyState & JST_FIRER) joy1 |= 0x20;
              }
              else if (myConfig.dpad == DPAD_DIAGONALS)
              {
                  if (JoyState & JST_UP)    joy1 |= (0x01 | 0x08);
                  if (JoyState & JST_DOWN)  joy1 |= (0x02 | 0x04);
                  if (JoyState & JST_LEFT)  joy1 |= (0x04 | 0x01);
                  if (JoyState & JST_RIGHT) joy1 |= (0x08 | 0x02);

                  if (JoyState & JST_FIREL) joy1 |= 0x10;
                  if (JoyState & JST_FIRER) joy1 |= 0x20;
              }
          }

          myAY.ayPortAIn = ~joy1;
      }
      return ay38910DataR(&myAY);
  }
  else if (Port == 0xA8) return Port_PPI_A;
  else if (Port == 0xA9)
  {
      // ----------------------------------------------------------
      // Keyboard Port (international and US)
      //  Row   Bit_7 Bit_6 Bit_5 Bit_4 Bit_3 Bit_2 Bit_1 Bit_0
      //   0     "7"   "6"   "5"   "4"   "3"   "2"   "1"   "0"
      //   1     ";"   "]"   "["   "\"   "="   "-"   "9"   "8"
      //   2     "B"   "A"  DEAD   "/"   "."   ","   "'"   "`"
      //   3     "J"   "I"   "H"   "G"   "F"   "E"   "D"   "C"
      //   4     "R"   "Q"   "P"   "O"   "N"   "M"   "L"   "K"
      //   5     "Z"   "Y"   "X"   "W"   "V"   "U"   "T"   "S"
      //   6     F3    F2    F1   CODE   CAP  GRAPH CTRL  SHIFT
      //   7     RET   SEL   BS   STOP   TAB   ESC   F5    F4
      //   8    RIGHT DOWN   UP   LEFT   DEL   INS  HOME  SPACE
      // ----------------------------------------------------------
      
      // For the full keyboard overlay... this is a bit of a hack for SHIFT and CTRL
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
            DSPrint(4,0,6, (msx_japanese_matrix ? "KANA":"CODE"));
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
      
      u8 key1 = 0x00;   // Accumulate keys here...

      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i< (kbd_keys_pressed ? kbd_keys_pressed:1); i++) // Always one pass at least for joysticks...
      {
          kbd_key = kbd_keys[i];

          if ((Port_PPI_C & 0x0F) == 0)      // Row 0
          {
              if (JoyState == JST_0)   key1 |= 0x01;  // '0'
              if (JoyState == JST_1)   key1 |= 0x02;  // '1'
              if (JoyState == JST_2)   key1 |= 0x04;  // '2'
              if (JoyState == JST_3)   key1 |= 0x08;  // '3'
              if (JoyState == JST_4)   key1 |= 0x10;  // '4'
              if (JoyState == JST_5)   // This one can be user-defined
              {
                  if (myConfig.msxKey5 == 0) key1 |= 0x20;  // '5'
                  if (myConfig.msxKey5 == 6) key1 |= 0x40;  // '6'
                  if (myConfig.msxKey5 == 7) key1 |= 0x80;  // '7'
              }
              if (kbd_key)
              {
                  if (kbd_key == '0')           key1  |= 0x01;
                  if (kbd_key == '1')           key1  |= 0x02;
                  if (kbd_key == '2')           key1  |= 0x04;
                  if (kbd_key == '3')           key1  |= 0x08;
                  if (kbd_key == '4')           key1  |= 0x10;
                  if (kbd_key == '5')           key1  |= 0x20;
                  if (kbd_key == '6')           key1  |= 0x40;
                  if (kbd_key == '7')           key1  |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 1)  // Row 1
          {
              if (JoyState == JST_5)
              {
                  if (myConfig.msxKey5 == 8) key1 |= 0x01;  // '8'
                  if (myConfig.msxKey5 == 9) key1 |= 0x02;  // '9'
              }
              if (kbd_key)
              {
                  if (kbd_key == '8')           key1 |= 0x01;
                  if (kbd_key == '9')           key1 |= 0x02;
                  if (kbd_key == '-')           key1 |= 0x04;
                  if (kbd_key == '=')           key1 |= 0x08;
                  if (kbd_key == '\\')          key1 |= 0x10;
                  if (kbd_key == '[')           key1 |= 0x20;
                  if (kbd_key == ']')           key1 |= 0x40;
                  if (kbd_key == ';')           key1 |= 0x80;
                  if (kbd_key == ':')           key1 |= 0x80;
              }
          }
          else if ((Port_PPI_C & 0x0F) == 2)  // Row 2
          {
              if (JoyState == JST_5)
              {
                  if (myConfig.msxKey5 == 10) key1 |= 0x40;  // 'A'
                  if (myConfig.msxKey5 == 11) key1 |= 0x80;  // 'B'
              }
              if (kbd_key)
              {
                  if (kbd_key == KBD_KEY_QUOTE) key1 |= 0x01;
                  if (kbd_key == '`')           key1 |= 0x02;
                  if (kbd_key == ',')           key1 |= 0x04;
                  if (kbd_key == '.')           key1 |= 0x08;
                  if (kbd_key == '/')           key1 |= 0x10;
                  if (kbd_key == KBD_KEY_DEAD)  key1 |= 0x20;
                  if (kbd_key == 'A')           key1 |= 0x40;
                  if (kbd_key == 'B')           key1 |= 0x80;
              }          
          }
          else if ((Port_PPI_C & 0x0F) == 3)  // Row 3
          {
              if (JoyState == JST_5)
              {
                  if (myConfig.msxKey5 == 12) key1 |= 0x01;  // 'C'
                  if (myConfig.msxKey5 == 13) key1 |= 0x02;  // 'D'
                  if (myConfig.msxKey5 == 14) key1 |= 0x04;  // 'E'
                  if (myConfig.msxKey5 == 15) key1 |= 0x08;  // 'F'
                  if (myConfig.msxKey5 == 16) key1 |= 0x10;  // 'G'
                  if (myConfig.msxKey5 == 17) key1 |= 0x20;  // 'H'
                  if (myConfig.msxKey5 == 18) key1 |= 0x40;  // 'I'
                  if (myConfig.msxKey5 == 19) key1 |= 0x80;  // 'J'
              }
              if (kbd_key)
              {
                  if (kbd_key == 'C')           key1 |= 0x01;
                  if (kbd_key == 'D')           key1 |= 0x02;
                  if (kbd_key == 'E')           key1 |= 0x04;
                  if (kbd_key == 'F')           key1 |= 0x08;
                  if (kbd_key == 'G')           key1 |= 0x10;
                  if (kbd_key == 'H')           key1 |= 0x20;
                  if (kbd_key == 'I')           key1 |= 0x40;
                  if (kbd_key == 'J')           key1 |= 0x80;
              }          
          }
          else if ((Port_PPI_C & 0x0F) == 4)  // Row 4
          {
              if (JoyState == JST_5)
              {
                  if (myConfig.msxKey5 == 20) key1 |= 0x01;  // 'K'
                  if (myConfig.msxKey5 == 21) key1 |= 0x02;  // 'L'
                  if (myConfig.msxKey5 == 22) key1 |= 0x04;  // 'M'
                  if (myConfig.msxKey5 == 23) key1 |= 0x08;  // 'N'
                  if (myConfig.msxKey5 == 24) key1 |= 0x10;  // 'O'
                  if (myConfig.msxKey5 == 25) key1 |= 0x20;  // 'P'
                  if (myConfig.msxKey5 == 26) key1 |= 0x40;  // 'Q'
                  if (myConfig.msxKey5 == 27) key1 |= 0x80;  // 'R'
              }
              if (kbd_key)
              {
                  if (kbd_key == 'K')           key1 |= 0x01;
                  if (kbd_key == 'L')           key1 |= 0x02;
                  if (kbd_key == 'M')           key1 |= 0x04;
                  if (kbd_key == 'N')           key1 |= 0x08;
                  if (kbd_key == 'O')           key1 |= 0x10;
                  if (kbd_key == 'P')           key1 |= 0x20;
                  if (kbd_key == 'Q')           key1 |= 0x40;
                  if (kbd_key == 'R')           key1 |= 0x80;
              }          
          }
          else if ((Port_PPI_C & 0x0F) == 5)  // Row 5
          {
              if (JoyState == JST_5)
              {
                  if (myConfig.msxKey5 == 28) key1 |= 0x01;  // 'S'
                  if (myConfig.msxKey5 == 29) key1 |= 0x02;  // 'T'
                  if (myConfig.msxKey5 == 30) key1 |= 0x04;  // 'U'
                  if (myConfig.msxKey5 == 31) key1 |= 0x08;  // 'V'
                  if (myConfig.msxKey5 == 32) key1 |= 0x10;  // 'W'
                  if (myConfig.msxKey5 == 33) key1 |= 0x20;  // 'X'
                  if (myConfig.msxKey5 == 34) key1 |= 0x40;  // 'Y'
                  if (myConfig.msxKey5 == 35) key1 |= 0x80;  // 'Z'
              }
              if (kbd_key)
              {
                  if (kbd_key == 'S')           key1 |= 0x01;
                  if (kbd_key == 'T')           key1 |= 0x02;
                  if (kbd_key == 'U')           key1 |= 0x04;
                  if (kbd_key == 'V')           key1 |= 0x08;
                  if (kbd_key == 'W')           key1 |= 0x10;
                  if (kbd_key == 'X')           key1 |= 0x20;
                  if (kbd_key == 'Y')           key1 |= 0x40;
                  if (kbd_key == 'Z')           key1 |= 0x80;
              }          
          }      
          else if ((Port_PPI_C & 0x0F) == 6) // Row 6
          {
              if (JoyState == JST_7) key1 |= 0x20;    // F1
              if (JoyState == JST_8) key1 |= 0x40;    // F2
              if (JoyState == JST_9) key1 |= 0x80;    // F3
              if (JoyState == JST_5)
              {
                if (myConfig.msxKey5 == 1) key1 |= 0x01;  // SHIFT
                if (myConfig.msxKey5 == 2) key1 |= 0x02;  // CTRL
              }
              if (kbd_key)
              {
                  if (kbd_key == KBD_KEY_SHIFT) key1 |= 0x01;
                  if (kbd_key == KBD_KEY_CTRL)  key1 |= 0x02;
                  if (kbd_key == KBD_KEY_GRAPH) key1 |= 0x04;
                  if (kbd_key == KBD_KEY_CAPS)  key1 |= 0x08;
                  if (kbd_key == KBD_KEY_CODE)  key1 |= 0x10;
                  if (kbd_key == KBD_KEY_F1)    key1 |= 0x20;
                  if (kbd_key == KBD_KEY_F2)    key1 |= 0x40;
                  if (kbd_key == KBD_KEY_F3)    key1 |= 0x80;
              }          
              if (key_shift)  key1 |= 0x01;  // SHIFT
              if (key_ctrl)   key1 |= 0x02;  // CTRL
              if (key_graph)  key1 |= 0x04;  // GRAPH
              if (key_code)   key1 |= 0x10;  // CODE/KANA
          }
          else if ((Port_PPI_C & 0x0F) == 7) // Row 7
          {
              if (JoyState == JST_6)     key1 |= 0x10;  // STOP
              if (JoyState == JST_POUND) key1 |= 0x80;  // RETURN
              if (JoyState == JST_5)
              {
                if (myConfig.msxKey5 == 4) key1 |= 0x01;  // F4
                if (myConfig.msxKey5 == 5) key1 |= 0x02;  // F5
                if (myConfig.msxKey5 == 3) key1 |= 0x04;  // ESC
              }
              if (kbd_key)
              {
                  if (kbd_key == KBD_KEY_F4)    key1 |= 0x01;
                  if (kbd_key == KBD_KEY_F5)    key1 |= 0x02;
                  if (kbd_key == KBD_KEY_ESC)   key1 |= 0x04;
                  if (kbd_key == KBD_KEY_TAB)   key1 |= 0x08;
                  if (kbd_key == KBD_KEY_STOP)  key1 |= 0x10;
                  if (kbd_key == KBD_KEY_BS)    key1 |= 0x20;
                  if (kbd_key == KBD_KEY_SEL)   key1 |= 0x40;
                  if (kbd_key == KBD_KEY_RET)   key1 |= 0x80;

              }          
          }
          else if ((Port_PPI_C & 0x0F) == 8) // Row 8  RIGHT DOWN   UP   LEFT   DEL   INS  HOME  SPACE          
          {
              if (JoyState == JST_STAR) key1 |= 0x01;  // SPACE

              if (kbd_key)
              {
                  if (kbd_key == ' ')           key1 |= 0x01;
                  if (kbd_key == KBD_KEY_HOME)  key1 |= 0x02;
                  if (kbd_key == KBD_KEY_INS)   key1 |= 0x04;
                  if (kbd_key == KBD_KEY_DEL)   key1 |= 0x08;
                  if (kbd_key == KBD_KEY_LEFT)  key1 |= 0x10;
                  if (kbd_key == KBD_KEY_UP)    key1 |= 0x20;
                  if (kbd_key == KBD_KEY_DOWN)  key1 |= 0x40;
                  if (kbd_key == KBD_KEY_RIGHT) key1 |= 0x80;
              }          
          }
      }
      return ~key1;
  }
  else if (Port == 0xAA)
  {
      return Port_PPI_C;
  }
  else if (Port >= 0xD0 && Port <= 0xD7)  // Floppy Drive Controller
  {
      return fdc_read(Port & 0x07);
  }
    
  // No such port
  return(NORAM);
}


//------------------------------------------------------------------------------
// National FS-1300, and generic C-BIOS or MSX.ROM 64K Slot 3
//------------------------------------------------------------------------------
// Memory          Slot 0       Slot 1      Slot 2      Slot 3
// C000h~FFFFh      ---       Cartridge      ---       16K RAM
// 8000h~BFFFh      ---       Cartridge      ---       16K RAM
// 4000h~7FFFh    Main-ROM    Cartridge      ---       16K RAM
// 0000h~3FFFh    Main-ROM    Cartridge      ---       16K RAM
//---------------------------------------------------------------
void msx_slot_map_generic(unsigned char Value)
{
    // ---------------------------------------------------------------------
    // Slot 0 holds the 32K of MSX BIOS (0xFF above 32K)
    // Slot 1 is where the Game Cartridge Lives (up to 64K)
    // Slot 2 is empty (0xFF always)
    // Slot 3 is our main RAM. We emulate 64K of RAM
    // ---------------------------------------------------------------------
    if (((Value>>0) & 0x03) != ((Port_PPI_A>>0) & 0x03))
    switch ((Value>>0) & 0x03)  // [0x0000~0x3FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = BIOS_Memory + 0x0000;
            MemoryMap[1] = BIOS_Memory + 0x2000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[0] = 1;
                bRAMInSegment[0] = 0;
                MemoryMap[0] = (u8 *)(MSXCartPtr[0]);
                MemoryMap[1] = (u8 *)(MSXCartPtr[1]);
                break;
            }
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to our 64K of RAM
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 1;
            MemoryMap[0] = RAM_Memory+0x0000;
            MemoryMap[1] = RAM_Memory+0x2000;
            break;
    }

    if (((Value>>2) & 0x03) != ((Port_PPI_A>>2) & 0x03))
    switch ((Value>>2) & 0x03)  // [0x4000~0x7FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = BIOS_Memory + 0x4000;
            MemoryMap[3] = BIOS_Memory + 0x6000;                    
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 3)  // .dsk based MSX1 game so slide in the FDC Controller ROM
            {
                bROMInSegment[1] = 1;
                bRAMInSegment[1] = 0;
                
                MemoryMap[2] = fastdrom_cdx2 + 0x0000;
                MemoryMap[3] = fastdrom_cdx2 + 0x2000;                
                
                break;
            }
            else if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[1] = 1;
                bRAMInSegment[1] = 0;
                MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
                MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to our 64K of RAM
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 1;
            MemoryMap[2] = RAM_Memory+0x4000;
            MemoryMap[3] = RAM_Memory+0x6000;
            break;
    }

    if (((Value>>4) & 0x03) != ((Port_PPI_A>>4) & 0x03))
    switch ((Value>>4) & 0x03)  // [0x8000~0xBFFF]
    {
        case 0x00:  // Slot 0:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[2] = 1;
                bRAMInSegment[2] = 0;
                MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
                MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to our 64K of RAM
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 1;
            MemoryMap[4] = RAM_Memory+0x8000;
            MemoryMap[5] = RAM_Memory+0xA000;
            break;
    }

    if (((Value>>6) & 0x03) != ((Port_PPI_A>>6) & 0x03))
    switch ((Value>>6) & 0x03)  // [0xC000~0xFFFF]
    {
        case 0x00:  // Slot 0:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[3] = 1;
                bRAMInSegment[3] = 0;
                MemoryMap[6] = (u8 *)(MSXCartPtr[6]);
                MemoryMap[7] = (u8 *)(MSXCartPtr[7]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3 is RAM so we allow RAM writes now
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 1;
            MemoryMap[6] = RAM_Memory+0xC000;
            MemoryMap[7] = RAM_Memory+0xE000;
            break;
    }
}

//------------------------------------------------------------------------------
// Panasonic CF-2700 with RAM in Slot 1 and Cart in Slot 2 (nothing in Slot 3)
//------------------------------------------------------------------------------
// Memory          Slot 0       Slot 1      Slot 2      Slot 3
// C000h~FFFFh      ---        16K RAM     Cartridge     ---
// 8000h~BFFFh      ---        16K RAM     Cartridge     ---
// 4000h~7FFFh    Main-ROM     16K RAM     Cartridge     ---
// 0000h~3FFFh    Main-ROM     16K RAM     Cartridge     ---
//---------------------------------------------------------------
void msx_slot_map_fs2700(unsigned char Value)
{
    if (((Value>>0) & 0x03) != ((Port_PPI_A>>0) & 0x03))
    switch ((Value>>0) & 0x03)  // [0x0000~0x3FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = BIOS_Memory + 0x0000;
            MemoryMap[1] = BIOS_Memory + 0x2000;
            break;
        case 0x01:  // Slot 1:  Maps to our 64K of RAM
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 1;
            MemoryMap[0] = RAM_Memory+0x0000;
            MemoryMap[1] = RAM_Memory+0x2000;
            break;
        case 0x02:  // Slot 2:  Maps to our Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[0] = 1;
                bRAMInSegment[0] = 0;
                MemoryMap[0] = (u8 *)(MSXCartPtr[0]);
                MemoryMap[1] = (u8 *)(MSXCartPtr[1]);
                break;
            } // else fall through
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>2) & 0x03) != ((Port_PPI_A>>2) & 0x03))
    switch ((Value>>2) & 0x03)  // [0x4000~0x7FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = BIOS_Memory + 0x4000;
            MemoryMap[3] = BIOS_Memory + 0x6000;                    
            break;
        case 0x01:  // Slot 1:  Maps to our 64K of RAM
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 1;
            MemoryMap[2] = RAM_Memory+0x4000;
            MemoryMap[3] = RAM_Memory+0x6000;
            break;
        case 0x02:  // Slot 2:  Maps to Game Cart
            if (msx_mode == 3)  // .dsk based MSX1 game so slide in the FDC Controller ROM
            {
                bROMInSegment[1] = 1;
                bRAMInSegment[1] = 0;
                
                MemoryMap[2] = fastdrom_cdx2 + 0x0000;
                MemoryMap[3] = fastdrom_cdx2 + 0x2000;                
                
                break;
            }
            else if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[1] = 1;
                bRAMInSegment[1] = 0;
                MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
                MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>4) & 0x03) != ((Port_PPI_A>>4) & 0x03))
    switch ((Value>>4) & 0x03)  // [0x8000~0xBFFF]
    {
        case 0x00:  // Slot 0:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x01:  // Slot 1:  Maps to our 64K of RAM
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 1;
            MemoryMap[4] = RAM_Memory+0x8000;
            MemoryMap[5] = RAM_Memory+0xA000;
            break;
        case 0x02:  // Slot 2:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[2] = 1;
                bRAMInSegment[2] = 0;
                MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
                MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>6) & 0x03) != ((Port_PPI_A>>6) & 0x03))
    switch ((Value>>6) & 0x03)  // [0xC000~0xFFFF]
    {
        case 0x00:  // Slot 0:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
        case 0x01:  // Slot 1 is RAM so we allow RAM writes now
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 1;
            MemoryMap[6] = RAM_Memory+0xC000;
            MemoryMap[7] = RAM_Memory+0xE000;
            break;
        case 0x02:  // Slot 2:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[3] = 1;
                bRAMInSegment[3] = 0;
                MemoryMap[6] = (u8 *)(MSXCartPtr[6]);
                MemoryMap[7] = (u8 *)(MSXCartPtr[7]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
    }
}

//---------------------------------------------------------------
// Yamaha CX5M with 32K of Memory in Slot 1
//---------------------------------------------------------------
// Memory          Slot 0       Slot 1      Slot 2      Slot 3
// C000h~FFFFh    16K RAM     Cartridge      ---         ---
// 8000h~BFFFh    16K RAM     Cartridge      ---         ---
// 4000h~7FFFh    Main-ROM    Cartridge      ---         ---
// 0000h~3FFFh    Main-ROM    Cartridge      ---         ---
//---------------------------------------------------------------
void msx_slot_map_cx5m(unsigned char Value)
{
    if (((Value>>0) & 0x03) != ((Port_PPI_A>>0) & 0x03))
    switch ((Value>>0) & 0x03)  // [0x0000~0x3FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = BIOS_Memory + 0x0000;
            MemoryMap[1] = BIOS_Memory + 0x2000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[0] = 1;
                bRAMInSegment[0] = 0;
                MemoryMap[0] = (u8 *)(MSXCartPtr[0]);
                MemoryMap[1] = (u8 *)(MSXCartPtr[1]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>2) & 0x03) != ((Port_PPI_A>>2) & 0x03))
    switch ((Value>>2) & 0x03)  // [0x4000~0x7FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = BIOS_Memory + 0x4000;
            MemoryMap[3] = BIOS_Memory + 0x6000;                    
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[1] = 1;
                bRAMInSegment[1] = 0;
                MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
                MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>4) & 0x03) != ((Port_PPI_A>>4) & 0x03))
    switch ((Value>>4) & 0x03)  // [0x8000~0xBFFF]
    {
        case 0x00:  // Slot 0:  Maps to our 32K of Emulated RAM
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 1;
            MemoryMap[4] = RAM_Memory+0x8000;
            MemoryMap[5] = RAM_Memory+0xA000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[2] = 1;
                bRAMInSegment[2] = 0;
                MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
                MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>6) & 0x03) != ((Port_PPI_A>>6) & 0x03))
    switch ((Value>>6) & 0x03)  // [0xC000~0xFFFF]
    {
        case 0x00:  // Slot 0:  Maps to our 32K of Emulated RAM
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 1;
            MemoryMap[6] = RAM_Memory+0xC000;
            MemoryMap[7] = RAM_Memory+0xE000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[3] = 1;
                bRAMInSegment[3] = 0;
                MemoryMap[6] = (u8 *)(MSXCartPtr[6]);
                MemoryMap[7] = (u8 *)(MSXCartPtr[7]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3: Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
    }
}

//---------------------------------------------------------------
// Toshiba HX-10 with 64K of memory in Slot 2
//---------------------------------------------------------------
// Memory          Slot 0       Slot 1      Slot 2      Slot 3
// C000h~FFFFh      ---       Cartridge     16K RAM      ---
// 8000h~BFFFh      ---       Cartridge     16K RAM      ---
// 4000h~7FFFh    Main-ROM    Cartridge     16K RAM      ---
// 0000h~3FFFh    Main-ROM    Cartridge     16K RAM      ---
//---------------------------------------------------------------
void msx_slot_map_hx10(unsigned char Value)
{
    if (((Value>>0) & 0x03) != ((Port_PPI_A>>0) & 0x03))
    switch ((Value>>0) & 0x03)  // [0x0000~0x3FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = BIOS_Memory + 0x0000;
            MemoryMap[1] = BIOS_Memory + 0x2000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[0] = 1;
                bRAMInSegment[0] = 0;
                MemoryMap[0] = (u8 *)(MSXCartPtr[0]);
                MemoryMap[1] = (u8 *)(MSXCartPtr[1]);
            }
            else    // Maps to nothing
            {
                bROMInSegment[0] = 0;
                bRAMInSegment[0] = 0;
                MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
                MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            }
            break;
        case 0x02:  // Slot 2:  Maps to our 64K of RAM
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 1;
            MemoryMap[0] = RAM_Memory+0x0000;
            MemoryMap[1] = RAM_Memory+0x2000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>2) & 0x03) != ((Port_PPI_A>>2) & 0x03))
    switch ((Value>>2) & 0x03)  // [0x4000~0x7FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = BIOS_Memory + 0x4000;
            MemoryMap[3] = BIOS_Memory + 0x6000;                    
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[1] = 1;
                bRAMInSegment[1] = 0;
                MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
                MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
            }
            else    // Maps to nothing
            {
                bROMInSegment[1] = 0;
                bRAMInSegment[1] = 0;
                MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
                MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            }
            break;
        case 0x02:  // Slot 2:  Maps to our 64K of RAM
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 1;
            MemoryMap[2] = RAM_Memory+0x4000;
            MemoryMap[3] = RAM_Memory+0x6000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>4) & 0x03) != ((Port_PPI_A>>4) & 0x03))
    switch ((Value>>4) & 0x03)  // [0x8000~0xBFFF]
    {
        case 0x00:  // Slot 0:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[2] = 1;
                bRAMInSegment[2] = 0;
                MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
                MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
            }
            else    // Maps to nothing
            {
                bROMInSegment[2] = 0;
                bRAMInSegment[2] = 0;
                MemoryMap[4] = (u8 *)BIOS_Memory+0x8000;
                MemoryMap[5] = (u8 *)BIOS_Memory+0x8000;
            }
            break;
        case 0x02:  // Slot 2:  Maps to our 64K of RAM
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 1;
            MemoryMap[4] = RAM_Memory+0x8000;
            MemoryMap[5] = RAM_Memory+0xA000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>6) & 0x03) != ((Port_PPI_A>>6) & 0x03))
    switch ((Value>>6) & 0x03)  // [0xC000~0xFFFF]
    {
        case 0x00:  // Slot 0:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[3] = 1;
                bRAMInSegment[3] = 0;
                MemoryMap[6] = (u8 *)(MSXCartPtr[6]);
                MemoryMap[7] = (u8 *)(MSXCartPtr[7]);
            }
            else    // Maps to nothing
            {
                bROMInSegment[3] = 0;
                bRAMInSegment[3] = 0;
                MemoryMap[6] = (u8 *)BIOS_Memory+0x8000;
                MemoryMap[7] = (u8 *)BIOS_Memory+0x8000;
            }
            break;
        case 0x02:  // Slot 2 is RAM so we allow RAM writes now
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 1;
            MemoryMap[6] = RAM_Memory+0xC000;
            MemoryMap[7] = RAM_Memory+0xE000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
    }
}

//---------------------------------------------------------------
// Sony Hit-Bit HB-10 has 16K of RAM in slot 0
//---------------------------------------------------------------
// Memory          Slot 0       Slot 1      Slot 2      Slot 3
// C000h~FFFFh    16K RAM     Cartridge      ---         ---
// 8000h~BFFFh       ---      Cartridge      ---         ---
// 4000h~7FFFh    Main-ROM    Cartridge      ---         ---
// 0000h~3FFFh    Main-ROM    Cartridge      ---         ---
//---------------------------------------------------------------
void msx_slot_map_hb10(unsigned char Value)
{
    if (((Value>>0) & 0x03) != ((Port_PPI_A>>0) & 0x03))
    switch ((Value>>0) & 0x03)  // [0x0000~0x3FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = BIOS_Memory + 0x0000;
            MemoryMap[1] = BIOS_Memory + 0x2000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[0] = 1;
                bRAMInSegment[0] = 0;
                MemoryMap[0] = (u8 *)(MSXCartPtr[0]);
                MemoryMap[1] = (u8 *)(MSXCartPtr[1]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>2) & 0x03) != ((Port_PPI_A>>2) & 0x03))
    switch ((Value>>2) & 0x03)  // [0x4000~0x7FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = BIOS_Memory + 0x4000;
            MemoryMap[3] = BIOS_Memory + 0x6000;                    
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[1] = 1;
                bRAMInSegment[1] = 0;
                MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
                MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>4) & 0x03) != ((Port_PPI_A>>4) & 0x03))
    switch ((Value>>4) & 0x03)  // [0x8000~0xBFFF]
    {
        case 0x00:  // Slot 0:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[2] = 1;
                bRAMInSegment[2] = 0;
                MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
                MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>6) & 0x03) != ((Port_PPI_A>>6) & 0x03))
    switch ((Value>>6) & 0x03)  // [0xC000~0xFFFF]
    {
        case 0x00:  // Slot 0:  Maps to our 16K of Emulated RAM
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 1;
            MemoryMap[6] = RAM_Memory+0xC000;
            MemoryMap[7] = RAM_Memory+0xE000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[3] = 1;
                bRAMInSegment[3] = 0;
                MemoryMap[6] = (u8 *)(MSXCartPtr[6]);
                MemoryMap[7] = (u8 *)(MSXCartPtr[7]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3: Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
    }
}

//---------------------------------------------------------------
// Casio PV-7 has a paultry 8K of RAM at the top of slot 0 (E000-FFFF)
//---------------------------------------------------------------
// Memory          Slot 0       Slot 1      Slot 2      Slot 3
// C000h~FFFFh     8K RAM     Cartridge      ---         ---
// 8000h~BFFFh       ---      Cartridge      ---         ---
// 4000h~7FFFh    Main-ROM    Cartridge      ---         ---
// 0000h~3FFFh    Main-ROM    Cartridge      ---         ---
//---------------------------------------------------------------
void msx_slot_map_pv7(unsigned char Value)
{
    if (((Value>>0) & 0x03) != ((Port_PPI_A>>0) & 0x03))
    switch ((Value>>0) & 0x03)  // [0x0000~0x3FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = BIOS_Memory + 0x0000;
            MemoryMap[1] = BIOS_Memory + 0x2000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[0] = 1;
                bRAMInSegment[0] = 0;
                MemoryMap[0] = (u8 *)(MSXCartPtr[0]);
                MemoryMap[1] = (u8 *)(MSXCartPtr[1]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[0] = 0;
            bRAMInSegment[0] = 0;
            MemoryMap[0] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[1] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>2) & 0x03) != ((Port_PPI_A>>2) & 0x03))
    switch ((Value>>2) & 0x03)  // [0x4000~0x7FFF]
    {
        case 0x00:  // Slot 0:  Maps to BIOS Rom
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = BIOS_Memory + 0x4000;
            MemoryMap[3] = BIOS_Memory + 0x6000;                    
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[1] = 1;
                bRAMInSegment[1] = 0;
                MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
                MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[1] = 0;
            bRAMInSegment[1] = 0;
            MemoryMap[2] = (u8 *)BIOS_Memory+0x8000;
            MemoryMap[3] = (u8 *)BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>4) & 0x03) != ((Port_PPI_A>>4) & 0x03))
    switch ((Value>>4) & 0x03)  // [0x8000~0xBFFF]
    {
        case 0x00:  // Slot 0:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[2] = 1;
                bRAMInSegment[2] = 0;
                MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
                MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3:  Maps to nothing... 0xFF
            bROMInSegment[2] = 0;
            bRAMInSegment[2] = 0;
            MemoryMap[4] = BIOS_Memory+0x8000;
            MemoryMap[5] = BIOS_Memory+0x8000;
            break;
    }

    if (((Value>>6) & 0x03) != ((Port_PPI_A>>6) & 0x03))
    switch ((Value>>6) & 0x03)  // [0xC000~0xFFFF]
    {
        case 0x00:  // Slot 0:  Maps to our 16K of Emulated RAM
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 2;  // Special value means only 8K mapped here...
            MemoryMap[6] = BIOS_Memory+0xC000; // Just BIOS here... 0xFF
            MemoryMap[7] = RAM_Memory+0xE000;
            break;
        case 0x01:  // Slot 1:  Maps to Game Cart
            if (msx_mode == 1)  // msx_mode of 1 is a .ROM cart
            {
                bROMInSegment[3] = 1;
                bRAMInSegment[3] = 0;
                MemoryMap[6] = (u8 *)(MSXCartPtr[6]);
                MemoryMap[7] = (u8 *)(MSXCartPtr[7]);
                break;
            }
            // Else fall through to the next case and map nothing...
        case 0x02:  // Slot 2:  Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
        case 0x03:  // Slot 3: Maps to nothing... 0xFF
            bROMInSegment[3] = 0;
            bRAMInSegment[3] = 0;
            MemoryMap[6] = BIOS_Memory+0x8000;
            MemoryMap[7] = BIOS_Memory+0x8000;
            break;
    }
}


// ----------------------------------------------------------------------
// MSX IO Port Write - VDP and AY Sound Chip plus Slot Mapper $A8
// ----------------------------------------------------------------------
void cpu_writeport_msx(register unsigned short Port,register unsigned char Value) 
{
    // MSX ports are 8-bit
    Port &= 0x00FF;

    if      (Port == 0x98) WrData9918(Value);
    else if (Port == 0x99) {if (WrCtrl9918(Value)) { CPU.IRequest=INT_RST38; }}
    else if (Port == 0xA0) {ay38910IndexW(Value&0xF, &myAY);}   // PSG Area
    else if (Port == 0xA1) {ay38910DataW(Value, &myAY);}
    else if (Port == 0xA8) // Slot system for MSX
    {
        if (Port_PPI_A != Value)
        {
            // ---------------------------------------------------------------------
            // bits 7-6     bits 5-4     bits 3-2      bits 1-0
            // C000h~FFFF   8000h~BFFF   4000h~7FFF    0000h~3FFF
            // ---------------------------------------------------------------------
            
            // Based on the machine we are emulating, we map memory slightly differently.
            switch (myConfig.msxBios)
            {
                case 1:  msx_slot_map_generic(Value); break;     // Generic MSX.ROM (64K mapped in slot 3)
                case 2:  msx_slot_map_fs2700(Value);  break;     // Panasonic CF-2700 (64K mapped in slot 1, Cart in slot 2)
                case 3:  msx_slot_map_cx5m(Value);    break;     // Yamaha CX5M (32K mapped in slot 0)
                case 4:  msx_slot_map_hx10(Value);    break;     // Toshiba HX-10 (64K mapped in slot 2)
                case 5:  msx_slot_map_hb10(Value);    break;     // Sony HB-10 (16K mapped in slot 0)
                case 6:  msx_slot_map_generic(Value); break;     // National FS-1300 (64K mapped in slot 3)
                case 7:  msx_slot_map_pv7(Value);     break;     // Casio PV-7 (8K mapped in slot 0)
                default: msx_slot_map_generic(Value); break;     // C-BIOS as a fall-back (64K mapped in slot 3)
            }
            
            Port_PPI_A = Value;             // Useful when read back
        }
    }
    else if (Port == 0xA9)  // PPI - Register B
    {
        Port_PPI_B = Value;
    }
    else if (Port == 0xAA)  // PPI - Register C
    {
        if (Value & 0x80)  // Beeper ON
        {
            if ((Port_PPI_C & 0x80) == 0) beeperFreq++;
        }
        Port_PPI_C = Value;
        msx_caps_lock = ((Port_PPI_C & 0x40) ? 0:1);
    }
    else if (Port == 0xAB)  // PPI - Register C Fast Modify
    {
        if ((Value & 0x0E) == 0x0E) // Are we hitting the Beeper ON/OFF bit?
        {
            if ((Value & 1) && ((Port_PPI_C & 0x80) == 0)) beeperFreq++;   // Beeper ON
        }
        
        // Set or clear the proper bit in PORTC
        u8 bit =  (Value & 0x0E) >> 1;
        if (Value & 1) Port_PPI_C |= (1 << bit);
        else Port_PPI_C &= ~(1 << bit);

        msx_caps_lock = ((Port_PPI_C & 0x40) ? 0:1);
    }
    else if (Port >= 0xD0 && Port <= 0xD7)  // Floppy Drive Controller
    {
      //2 sides * 80 tracks * 9 sectors per track * 512 bytes per sector = 737280 Bytes (720kB)
      fdc_write(Port & 0x07, Value);
    }
}



// --------------------------------------------------------------------------
// Try to guess the ROM type from the loaded binary... basically we are
// counting the number of load addresses that would access a mapper hot-spot.
// --------------------------------------------------------------------------
u8 MSX_GuessROMType(u32 size)
{
    u8 type = KON8;  // Default to Konami 8K mapper
    u16 guess[MAX_MAPPERS] = {0,0,0,0};
    
    for (int i=0; i<size - 3; i++)
    {
        if (ROM_Memory[i] == 0x32)   // LD,A instruction
        {
            u16 value = ROM_Memory[i+1] + (ROM_Memory[i+2] << 8);
            switch (value)
            {
                case 0x5000:
                case 0x9000:
                case 0xb000:
                    guess[SCC8]++;
                    break;
                case 0x4000:
                case 0x8000:
                case 0xa000:
                    guess[KON8]++;
                    break;
                case 0x6800:
                case 0x7800:
                    guess[ASC8]++;guess[ASC8]++;
                    break;
                case 0x6000:
                    guess[KON8]++;
                    guess[ASC8]++;
                    guess[ASC16]++;
                    break;
                case 0x7000:
                    guess[SCC8]++;
                    guess[ASC8]++;
                    guess[ASC16]++;
                    break;
                case 0x77FF:
                    guess[ASC16]++;guess[ASC16]++;
                    break;
            }
        }
    }

    // Now pick the mapper that had the most Load addresses above...
    if      ((guess[ASC16] > guess[KON8]) && (guess[ASC16] > guess[SCC8]) && (guess[ASC16] > guess[ASC8]))    type = ASC16;
    else if ((guess[ASC8]  > guess[KON8]) && (guess[ASC8]  > guess[SCC8]) && (guess[ASC8] >= guess[ASC16]))   type = ASC8;      // ASC8 wins "ties" over ASC16
    else if ((guess[SCC8]  > guess[KON8]) && (guess[SCC8]  > guess[ASC8]) && (guess[SCC8]  > guess[ASC16]))   type = SCC8;
    else type = KON8; 
    
    if (size == (64 * 1024)) type = ASC16;      // Big percentage of 64K mapper ROMs are ASCII16 so default to that and override below
    
    
    // ----------------------------------------------------------------------
    // Since mappers are hard to detect reliably, check a few special CRCs
    // ----------------------------------------------------------------------
    if (file_crc == 0x5dc45624) type = ASC8;   // Super Laydock
    if (file_crc == 0xb885a464) type = ZEN8;   // Super Laydock    
    if (file_crc == 0x7454ad5b) type = ASC16;  // Sorcery
    if (file_crc == 0x3891bc0f) type = ASC16;  // Govellious
    if (file_crc == 0x1d1ec602) type = ASC16;  // Eggerland 2
    if (file_crc == 0x704ec575) type = ASC16;  // Toobin
    if (file_crc == 0x885773f9) type = ASC16;  // Dragon Slayer 3
    if (file_crc == 0x0521ca7a) type = ASC16;  // Dynamite Dan
    if (file_crc == 0xab6cd62c) type = ASC16;  // King's Knight    
    if (file_crc == 0x00c5d5b5) type = ASC16;  // Hydlyde III
    if (file_crc == 0x2a019191) type = ASC8;   // R-Type 512k
    if (file_crc == 0xa3a51fbb) type = ASC16;  // R-Type 512k
    if (file_crc == 0x952bfaa4) type = SCC8;   // R-Type 512k
    if (file_crc == 0xfbd3f05b) type = ASC16;  // Alien Attack 3.5
    if (file_crc == 0xa6e924ab) type = ASC16;  // Aliens aka Alien 2
    if (file_crc == 0xa6e924ab) type = ASC16;  // Aliens aka Alien 2
    if (file_crc == 0xc6fc7bd7) type = ASC16;  // Aliens aka Alien 2
    if (file_crc == 0x3ddcb524) type = ASC16;  // Aliens aka Alien 2    
    if (file_crc == 0x1306ccca) type = ASC8;   // Auf Wiedershen Monty [1.7]
    if (file_crc == 0xec036e37) type = ASC16;  // Gall Force
    if (file_crc == 0xa29176e3) type = ASC16;  // Mecha 9    
    if (file_crc == 0x03379ef8) type = ASC16;  // MSXDev Step Up 1.2
    if (file_crc == 0x2f9bed70) type = ZEN16;  // Adventure Kid
    if (file_crc == 0xab9fb4a4) type = ASC16;  // Astro Marine Corps
    if (file_crc == 0x8cf0e6c0) type = ASC8;   // Bomber King
    if (file_crc == 0xf2f2f0c8) type = ASC16;  // Borfestu (English Translation)
    if (file_crc == 0x8801b31e) type = LIN64;  // Break In
    if (file_crc == 0x79cf953d) type = ASC8;   // Death Wish III    
    if (file_crc == 0x387c1de7) type = ASC16;  // Dragon Slayer III - Romantica
    if (file_crc == 0x2526e568) type = ASC16;  // Dungeon Hunter
    if (file_crc == 0x1d75c0b4) type = ASC16;  // Freedom Fighter (newest)
    if (file_crc == 0xc570ea63) type = ASC16;  // Freedom Fighter
    if (file_crc == 0xa5b0e901) type = ASC16;  // Game Over (Parts I and II)
    if (file_crc == 0xb2e57a45) type = ASC16;  // Gremlins II
    if (file_crc == 0x96b7faca) type = ASC16;  // MSX Harry Fox Special (JP)
    if (file_crc == 0x92943e5b) type = ASC16;  // Hydlide 2 - Shrine of Darkness
    if (file_crc == 0xb29edaec) type = ASC16;  // Hydlide 2 - Shrine of Darkness
    if (file_crc == 0xa0fd57cf) type = ASC16;  // Hydlide 2 - Shrine of Darkness
    if (file_crc == 0x41c82156) type = ZEN8;   // Hydlide III (Zenmia)
    if (file_crc == 0xcb9eebfb) type = SCC8;   // Lex Flics (translated)
    if (file_crc == 0xf5eb7cca) type = SCC8;   // Lazy Jones    
    if (file_crc == 0xd6c395f8) type = SCC8;   // Monster Hunter
    if (file_crc == 0x8183bae1) type = LIN64;  // Mutants from the Deep   
    if (file_crc == 0xa78fdb26) type = ASC8;   // No Back Down   
    if (file_crc == 0x454381e4) type = SCC8;   // PuzzyBox 1.0
    if (file_crc == 0x31e3f039) type = LIN64;  // Stupid Martians
    if (file_crc == 0x9195c34c) type = ZEN8;   // Super Boy III
    if (file_crc == 0x73efe840) type = ASC16;  // Super Pierrot
    if (file_crc == 0x41676177) type = LIN64;  // Tina's Island Adventure
    if (file_crc == 0xc5c14bbd) type = KON8;   // The Hobbit
    if (file_crc == 0xfea70207) type = ASC16;  // Vaxol
    if (file_crc == 0x35899655) type = ASC8;   // Venom Strikes Back    
    if (file_crc == 0x47273220) type = XBLAM;  // Cross Blaim
    
    return type;
}



// -------------------------------------------------------------------------
// Setup the initial MSX memory layout based on the size of the ROM loaded.
// -------------------------------------------------------------------------
void MSX_InitialMemoryLayout(u32 romSize)
{
    msx_last_rom_size = romSize;
    
    // -------------------------------------
    // Make sure the MSX ports are clear
    // -------------------------------------
    Port_PPI_A = 0x00;
    Port_PPI_B = 0x00;
    Port_PPI_C = 0x00;      
    
    // ---------------------------------------------
    // Start with reset memory - fill in MSX slots
    // ---------------------------------------------
    memset(RAM_Memory,  0x00, 0x10000);
    memset(SRAM_Memory, 0xFF, 0x4000);
    
    // -----------------------------------------
    // Setup RAM/ROM pointers back to defaults
    // -----------------------------------------
    memset(bRAMInSegment, 0, 4);   // Default to no RAM in slot until told so
    memset(bROMInSegment, 0, 4);   // Default to no ROM in slot until told so
    for (u8 i=0; i<8; i++)
    {
        MSXCartPtr[i] = 0;     // All pages normal until told otherwise by A8 writes
    }
    
    // ---------------------------------------------
    // Restore the MSX BIOS and point to it
    // ---------------------------------------------
    msx_restore_bios();
    
    // ---------------------------------------------
    // If we are .cas or .dsk we can point to 
    // nothing for the cart and return here... 
    // ---------------------------------------------
    if (msx_mode >=2) 
    {
        MSXCartPtr[0] = (u8*)BIOS_Memory+0xC000;       // Segment 0
        MSXCartPtr[1] = (u8*)BIOS_Memory+0xC000;       // Segment 1
        MSXCartPtr[2] = (u8*)BIOS_Memory+0xC000;       // Segment 2
        MSXCartPtr[3] = (u8*)BIOS_Memory+0xC000;       // Segment 3
        MSXCartPtr[4] = (u8*)BIOS_Memory+0xC000;       // Segment 4
        MSXCartPtr[5] = (u8*)BIOS_Memory+0xC000;       // Segment 5
        MSXCartPtr[6] = (u8*)BIOS_Memory+0xC000;       // Segment 6
        MSXCartPtr[7] = (u8*)BIOS_Memory+0xC000;       // Segment 7
        
        mapperType = 0;
        return;
    }
    
    // ------------------------------------------------------------
    // Setup the Z80 memory based on the MSX game ROM size loaded
    // ------------------------------------------------------------
    if (romSize == (8 * 1024))
    {
        if (msx_basic)  // Basic Game loads at 0x8000 ONLY
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x8000;        // Segment 0
            MSXCartPtr[1] = (u8*)ROM_Memory+0x8000;        // Segment 1
            MSXCartPtr[2] = (u8*)ROM_Memory+0x8000;        // Segment 2
            MSXCartPtr[3] = (u8*)ROM_Memory+0x8000;        // Segment 3
            MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 4 - Actual ROM is here
            MSXCartPtr[5] = (u8*)ROM_Memory+0x8000;        // Segment 5
            MSXCartPtr[6] = (u8*)ROM_Memory+0x8000;        // Segment 6
            MSXCartPtr[7] = (u8*)ROM_Memory+0x8000;        // Segment 7
        }
        else            // Mirrors at every 8K
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0
            MSXCartPtr[1] = (u8*)ROM_Memory+0x0000;        // Segment 1
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 2
            MSXCartPtr[3] = (u8*)ROM_Memory+0x0000;        // Segment 3
            MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 4
            MSXCartPtr[5] = (u8*)ROM_Memory+0x0000;        // Segment 5
            MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 6
            MSXCartPtr[7] = (u8*)ROM_Memory+0x0000;        // Segment 7
        }
    }
    else if (romSize <= (16 * 1024) && (myConfig.msxMapper != SCC8))
    {
        if (myConfig.msxMapper == AT4K)  // Load the 16K rom at 0x4000 without Mirrors
        {
                MSXCartPtr[0] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[1] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0
                MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 1
                MSXCartPtr[4] = (u8*)ROM_Memory+0xC000;        // Segment NA 
                MSXCartPtr[5] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[6] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[7] = (u8*)ROM_Memory+0xC000;        // Segment NA              
        }
        else if (myConfig.msxMapper == AT8K) // Load the 16K rom at 0x8000 without Mirrors
        {
                MSXCartPtr[0] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[1] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[2] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[3] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 
                MSXCartPtr[5] = (u8*)ROM_Memory+0x2000;        // Segment 1 
                MSXCartPtr[6] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[7] = (u8*)ROM_Memory+0xC000;        // Segment NA              
        }
        else
        {
            if (msx_basic)  // Basic Game loads at 0x8000 without Mirrors
            {
                MSXCartPtr[0] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[1] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[2] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[3] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 
                MSXCartPtr[5] = (u8*)ROM_Memory+0x2000;        // Segment 1 
                MSXCartPtr[6] = (u8*)ROM_Memory+0xC000;        // Segment NA
                MSXCartPtr[7] = (u8*)ROM_Memory+0xC000;        // Segment NA              
            }
            else    // Mirrors every 16K
            {
                MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0 
                MSXCartPtr[1] = (u8*)ROM_Memory+0x2000;        // Segment 1 
                MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0 
                MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 1 
                MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 
                MSXCartPtr[5] = (u8*)ROM_Memory+0x2000;        // Segment 1 
                MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 
                MSXCartPtr[7] = (u8*)ROM_Memory+0x2000;        // Segment 1               
            }
        }
    }
    else if (romSize <= (32 * 1024) && (myConfig.msxMapper != SCC8))
    {
        // ------------------------------------------------------------------------------------------------------
        // For 32K roms, we need more information to determine exactly where to load it... however
        // this simple algorithm handles at least 90% of all real-world games... basically the header
        // of the .ROM file has a INIT load address that we can use as a clue as to what banks the actual
        // code should be loaded... if the INIT is address 0x4000 or higher (this is fairly common) then we
        // load the 32K rom into banks 1+2 and we mirror the first 16K on page 0 and the upper 16K on page 3.
        // ------------------------------------------------------------------------------------------------------
        if (myConfig.msxMapper == AT0K)  // Then the full 32K ROM is mapped here
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0
            MSXCartPtr[1] = (u8*)ROM_Memory+0x2000;        // Segment 1
            MSXCartPtr[2] = (u8*)ROM_Memory+0x4000;        // Segment 2
            MSXCartPtr[3] = (u8*)ROM_Memory+0x6000;        // Segment 3
            MSXCartPtr[4] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[5] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[6] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[7] = (u8*)ROM_Memory+0xC000;        // Segment NA
        }
        else  if (myConfig.msxMapper == AT4K)  // Then the full 32K ROM is mapped here
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[1] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0
            MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 1
            MSXCartPtr[4] = (u8*)ROM_Memory+0x4000;        // Segment 2
            MSXCartPtr[5] = (u8*)ROM_Memory+0x6000;        // Segment 3
            MSXCartPtr[6] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[7] = (u8*)ROM_Memory+0xC000;        // Segment NA
        }
        else if (myConfig.msxMapper == AT8K)  // Then the full 32K ROM is mapped here
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[1] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[2] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[3] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0
            MSXCartPtr[5] = (u8*)ROM_Memory+0x2000;        // Segment 1
            MSXCartPtr[6] = (u8*)ROM_Memory+0x4000;        // Segment 2
            MSXCartPtr[7] = (u8*)ROM_Memory+0x6000;        // Segment 3
        }
        else
        {
            if (msx_init >= 0x4000 || msx_basic) // This comes from the .ROM header - if the init address is 0x4000 or higher, we load in bank 1+2
            {
                MSXCartPtr[0] = (u8*)ROM_Memory+0x4000;        // Segment 2 Mirror
                MSXCartPtr[1] = (u8*)ROM_Memory+0x6000;        // Segment 3 Mirror
                MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0
                MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 1
                MSXCartPtr[4] = (u8*)ROM_Memory+0x4000;        // Segment 2
                MSXCartPtr[5] = (u8*)ROM_Memory+0x6000;        // Segment 3
                MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 Mirror
                MSXCartPtr[7] = (u8*)ROM_Memory+0x2000;        // Segment 1 Mirror
            }
            else  // Otherwise we load in bank 0+1 and mirrors on 2+3
            {
                MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0
                MSXCartPtr[1] = (u8*)ROM_Memory+0x2000;        // Segment 1
                MSXCartPtr[2] = (u8*)ROM_Memory+0x4000;        // Segment 2
                MSXCartPtr[3] = (u8*)ROM_Memory+0x6000;        // Segment 3
                MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 Mirror
                MSXCartPtr[5] = (u8*)ROM_Memory+0x2000;        // Segment 1 Mirror
                MSXCartPtr[6] = (u8*)ROM_Memory+0x4000;        // Segment 2 Mirror
                MSXCartPtr[7] = (u8*)ROM_Memory+0x8000;        // Segment 3 Mirror
            }
        }
    }
    else if (romSize == (48 * 1024) && (myConfig.msxMapper != SCC8))
    {
        if ((myConfig.msxMapper == KON8) || (myConfig.msxMapper == ZEN8))
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x4000;        // Segment 2 Mirror
            MSXCartPtr[1] = (u8*)ROM_Memory+0x6000;        // Segment 3 Mirror
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 1 
            MSXCartPtr[4] = (u8*)ROM_Memory+0x4000;        // Segment 2 
            MSXCartPtr[5] = (u8*)ROM_Memory+0x6000;        // Segment 3 
            MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 Mirror
            MSXCartPtr[7] = (u8*)ROM_Memory+0x2000;        // Segment 1 Mirror
            mapperMask = 0x07;
        }
        else if (myConfig.msxMapper == ASC8)
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[1] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[3] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[5] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[7] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            mapperMask = 0x07;
        }
        else if ((myConfig.msxMapper == ASC16) || (myConfig.msxMapper == ZEN16))
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[1] = (u8*)ROM_Memory+0x2000;        // Segment 1 
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 1 
            MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[5] = (u8*)ROM_Memory+0x2000;        // Segment 1 
            MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[7] = (u8*)ROM_Memory+0x2000;        // Segment 1 
            mapperMask = 0x03;
        }
        else if (myConfig.msxMapper == AT4K)
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0xC000;        // Segment NA 
            MSXCartPtr[1] = (u8*)ROM_Memory+0xC000;        // Segment NA
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0
            MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 1 
            MSXCartPtr[4] = (u8*)ROM_Memory+0x4000;        // Segment 2 
            MSXCartPtr[5] = (u8*)ROM_Memory+0x6000;        // Segment 3 
            MSXCartPtr[6] = (u8*)ROM_Memory+0x8000;        // Segment 4 
            MSXCartPtr[7] = (u8*)ROM_Memory+0xA000;        // Segment 5
        }
        else // Load the rom at AT0K
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0 
            MSXCartPtr[1] = (u8*)ROM_Memory+0x2000;        // Segment 1
            MSXCartPtr[2] = (u8*)ROM_Memory+0x4000;        // Segment 2 
            MSXCartPtr[3] = (u8*)ROM_Memory+0x6000;        // Segment 3 
            MSXCartPtr[4] = (u8*)ROM_Memory+0x8000;        // Segment 4 
            MSXCartPtr[5] = (u8*)ROM_Memory+0xA000;        // Segment 5 
            MSXCartPtr[6] = (u8*)ROM_Memory+0xC000;        // Segment NA 
            MSXCartPtr[7] = (u8*)ROM_Memory+0xE000;        // Segment NA
        }
    }
    else if ((romSize == (64 * 1024)) && (myConfig.msxMapper == LIN64))   // 64K Linear ROM
    {
        MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0
        MSXCartPtr[1] = (u8*)ROM_Memory+0x2000;        // Segment 1
        MSXCartPtr[2] = (u8*)ROM_Memory+0x4000;        // Segment 2
        MSXCartPtr[3] = (u8*)ROM_Memory+0x6000;        // Segment 3
        MSXCartPtr[4] = (u8*)ROM_Memory+0x8000;        // Segment 4
        MSXCartPtr[5] = (u8*)ROM_Memory+0xA000;        // Segment 5
        MSXCartPtr[6] = (u8*)ROM_Memory+0xC000;        // Segment 6
        MSXCartPtr[7] = (u8*)ROM_Memory+0xE000;        // Segment 7
        
    }
    else if ((romSize >= (16 * 1024)) && (romSize <= (MAX_CART_SIZE * 1024)))   // We'll take anything between these two...
    {
        if (myConfig.msxMapper == GUESS)
        {
            mapperType = MSX_GuessROMType(romSize);
        }
        else
        {
            mapperType = myConfig.msxMapper;   
        }

        if ((mapperType == KON8) || (mapperType == SCC8) || (mapperType == ZEN8))
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x4000;        // Segment 2 Mirror
            MSXCartPtr[1] = (u8*)ROM_Memory+0x6000;        // Segment 3 Mirror
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 1 default
            MSXCartPtr[4] = (u8*)ROM_Memory+0x4000;        // Segment 2 default
            MSXCartPtr[5] = (u8*)ROM_Memory+0x6000;        // Segment 3 default
            MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 Mirror
            MSXCartPtr[7] = (u8*)ROM_Memory+0x2000;        // Segment 1 Mirror
        }
        else if (mapperType == ASC8)
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[1] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[3] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[5] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[7] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
        }                
        else if (mapperType == ASC16 || mapperType == ZEN16)
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[1] = (u8*)ROM_Memory+0x2000;        // Segment 0 default
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 0 default
            MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[5] = (u8*)ROM_Memory+0x2000;        // Segment 0 default
            MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[7] = (u8*)ROM_Memory+0x2000;        // Segment 0 default
        }
        else if (mapperType == XBLAM)        // Just for Cross Blaim
        {
            MSXCartPtr[0] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[1] = (u8*)ROM_Memory+0x2000;        // Segment 0 default
            MSXCartPtr[2] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[3] = (u8*)ROM_Memory+0x2000;        // Segment 0 default
            MSXCartPtr[4] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[5] = (u8*)ROM_Memory+0x2000;        // Segment 0 default
            MSXCartPtr[6] = (u8*)ROM_Memory+0x0000;        // Segment 0 default
            MSXCartPtr[7] = (u8*)ROM_Memory+0x2000;        // Segment 0 default
        }

        // ---------------------------------------------------------------------
        // We now set our memory masks such that we page memory in properly...
        // ---------------------------------------------------------------------
        if (romSize <= (128 * 1024))
        {
            if (mapperType == ASC16 || mapperType == ZEN16 || mapperType == XBLAM)
                mapperMask = (romSize <= (64 * 1024)) ? 0x03:0x07;
            else
                mapperMask = (romSize <= (64 * 1024)) ? 0x07:0x0F;
        }
        else if (romSize <= (512 * 1024))
        {
            if (mapperType == ASC16 || mapperType == ZEN16)
                mapperMask = (romSize <= (256 * 1024)) ? 0x0F:0x1F;
            else
                mapperMask = (romSize <= (256 * 1024)) ? 0x1F:0x3F;
        }        
        else // Must be 1024k... 
        {
            if (mapperType == ASC16 || mapperType == ZEN16)
                mapperMask = 0x3F;
            else 
                mapperMask = 0x7F;
        }

        if (msx_sram_enabled) mapperMask = 0x3F;        // Override for SRAM which uses upper bits (4 or 5) to select
    }
    else    
    {
        // Size not right for MSX support... we've already pre-filled 0xFF so nothing more to do here...
    }
    
    msx_block_size = ((mapperType == ASC16 || mapperType == ZEN16 || mapperType == XBLAM) ? 0x4000:0x2000);
}

/*********************************************************************************
 * A few ZX Speccy ports utilize the MSX beeper to "simulate" the sound...
 ********************************************************************************/

// ------------------------------------------------------------------------------------
// If the MSX Beeper is being used (rare but a few of the ZX Spectrum ports use it), 
// then we need to service it here. We basically track the frequency at which the
// game has hit the beeper and approximate that by using AY Channel A to produce the 
// tone.  This is crude and doesn't sound quite right... but good enough.
// ------------------------------------------------------------------------------------
void BeeperOFF(void)
{
    sn76496W(0x80 | 0x00,&mySN);    // Write new Frequency for Channel A
    sn76496W(0x00 | 0x00,&mySN);    // Write new Frequency for Channel A
    sn76496W(0x90 | 0x0F,&mySN);    // Write new Volume for Channel A

    sn76496W(0xA0 | 0x00,&mySN);    // Write new Frequency for Channel B
    sn76496W(0x00 | 0x00,&mySN);    // Write new Frequency for Channel B
    sn76496W(0xB0 | 0x0F,&mySN);    // Write new Volume for Channel B

    sn76496W(0xC0 | 0x00,&mySN);    // Write new Frequency for Channel C
    sn76496W(0x00 | 0x00,&mySN);    // Write new Frequency for Channel C
    sn76496W(0xD0 | 0x0F,&mySN);    // Write new Volume for Channel C

    sn76496W(0xFF,  &mySN);         // Disable Noise Channel
}

void BeeperON(u16 beeper_freq)
{
    if (beeper_freq > 0)
    {
        if (beeper_freq > 0x1FF) beeper_freq = 0x1FF;
        beeper_freq = 0x3FF - beeper_freq;
        
        sn76496W(0x80 | (beeper_freq & 0xF), &mySN);
        sn76496W(0x00 | ((beeper_freq >> 4) & 0x1F), &mySN);
        sn76496W(0x97, &mySN); // Turn on sound at fixed "mid" volume 
    }
    else 
    {
        BeeperOFF();
    }
}

void MSX_HandleBeeper(void)
{
    if (++msx_beeper_process & 1)
    {
      if (beeperFreq > 0)
      {
          BeeperON(30 * beeperFreq); // Frequency in Hz
          beeperFreq = 0;            // Gather new Beeper freq
          beeperWasOn=1;
      } else {if (beeperWasOn) {BeeperOFF(); beeperWasOn=0;}}
    }
}


// ---------------------------------------------------------
// Restore the BIOS and point to it...
// ---------------------------------------------------------
void msx_restore_bios(void)
{
    memset(BIOS_Memory, 0xFF, 0x10000);

    msx_japanese_matrix = 0;
    // --------------------------------------------------------------
    // Based on config, load up the C-BIOS or the real MSX.ROM BIOS
    // --------------------------------------------------------------
    if (myConfig.msxBios)
    {
        // Determine which of the  MSX BIOS / machine flavors we should load...
        switch (myConfig.msxBios)
        {
            case 1: memcpy(BIOS_Memory, MSXBios_Generic, 0x8000); 
                    msx_japanese_matrix = ((BIOS_Memory[0x2c]&0xf) ? 0:1); break;                              // Generic MSX.ROM - use the BIOS to determine matrix layout
            case 2: memcpy(BIOS_Memory, MSXBios_PanasonicCF2700, 0x8000);  break;                              // Panasonic CF-2700
            case 3: memcpy(BIOS_Memory, MSXBios_YamahaCX5M,      0x8000);  break;                              // Yamaha CX5M
            case 4: memcpy(BIOS_Memory, MSXBios_ToshibaHX10,     0x8000);  break;                              // Toshiba HX-10
            case 5: memcpy(BIOS_Memory, MSXBios_SonyHB10,        0x8000); msx_japanese_matrix = 1; break;      // Sony HB-10 (uses the Japanese matrix)
            case 6: memcpy(BIOS_Memory, MSXBios_NationalFS1300,  0x8000); msx_japanese_matrix = 1; break;      // National FS-1300 (uses the Japanese matrix)
            case 7: memcpy(BIOS_Memory, MSXBios_CasioPV7,        0x8000); msx_japanese_matrix = 1; break;      // Casio PV-7 with a paultry 8K (uses the Japanese matrix) - same BIOS as HB-10
            default: memcpy(BIOS_Memory, CBios, 0x8000); break;                                                // C-BIOS as a fall-back
        }
    }
    else
    {
        memcpy(BIOS_Memory, CBios, 0x8000);
    }
    
    MemoryMap[0] = BIOS_Memory + 0x0000;
    MemoryMap[1] = BIOS_Memory + 0x2000;
    MemoryMap[2] = BIOS_Memory + 0x4000;
    MemoryMap[3] = BIOS_Memory + 0x6000;

    MemoryMap[4] = BIOS_Memory + 0x8000;
    MemoryMap[5] = BIOS_Memory + 0xA000;
    MemoryMap[6] = BIOS_Memory + 0xC000;
    MemoryMap[7] = BIOS_Memory + 0xE000;
}


// ----------------------------
// 0x00e1: tapion(ref, cpu);
// 0x00e4: tapin(ref, cpu); 
// 0x00e7: tapiof(ref, cpu);
// 0x00ea: tapoon(ref, cpu);
// 0x00ed: tapout(ref, cpu);
// 0x00f0: tapoof(ref, cpu);
// 0x00f3: stmotr(ref, cpu);
// ----------------------------
void MSX_HandleCassette(register Z80 *r)
{
    if (r->PC.W-2 == 0x00e1)
    {
        if (tape_pos >= tape_len) {r->AF.B.l |= C_FLAG; return;}
        u8 done = false;
        // Find Header/Program
        while (!done)
        {
            if ((ROM_Memory[tape_pos] == 0xcc) && (ROM_Memory[tape_pos+1] == 0x13) && (ROM_Memory[tape_pos+2] == 0x7d) && (ROM_Memory[tape_pos+3] == 0x74))
            {
                tape_pos++; tape_pos++; tape_pos++;
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
    else if (r->PC.W-2 == 0x00e4)
    {
        if (tape_pos >= tape_len) {r->AF.B.l |= C_FLAG; return;}
        r->AF.B.l |= C_FLAG;

        // Read Data Byte from Cassette
        if (tape_pos < tape_len)
        {
            r->AF.B.h = ROM_Memory[tape_pos++];
            r->AF.B.l &= ~C_FLAG;
        }        
    }
    else if (r->PC.W-2 == 0x00e7)   // Stop Tape
    {
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x00ea)   // Tape Out - Start (create header)
    {
        for (u8 i=0; i<8; i++)
        {
            ROM_Memory[tape_pos++] = header_MSX[i];
        }
        if (tape_pos > tape_len)  tape_len=tape_pos;
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x00ed)   // Tape Out - Data
    {
        ROM_Memory[tape_pos++] = r->AF.B.h;
        if (tape_pos > tape_len)  tape_len=tape_pos;
        r->AF.B.l &= ~C_FLAG;
    }
    else if (r->PC.W-2 == 0x00f0)   // Tape Out - Stop
    {
        r->AF.B.l |= C_FLAG;
    }
    else if (r->PC.W-2 == 0x00f3)   // Tape Out - Stop Motor
    {
        r->AF.B.l &= ~C_FLAG;
    }
}

// ---------------------------------------------------------
// The MSX has a few ports and special memory mapping
// ---------------------------------------------------------
void msx_reset(void)
{
    msx_sram_at_8000 = false;
    if (msx_mode)
    {
        tape_pos = 0;
        MSX_InitialMemoryLayout(msx_last_rom_size);
        if (msx_mode == 3) // .dsk based MSX 
        {
            fdc_init(WD2793, 1, (tape_len/1024 == 360) ? 1:2, 80, 9, 512, 1, ROM_Memory, NULL);
            fdc_reset(true);
        }
    }
    else
    {
        msx_init = 0x4000;
        msx_basic = 0x0000;
    }
}

void msx_patch_bios(void)
{
    // Patch the BIOS for Cassette Access...
    BIOS_Memory[0x00e1] = 0xed; BIOS_Memory[0x00e2] = 0xfe; BIOS_Memory[0x00e3] = 0xc9;
    BIOS_Memory[0x00e4] = 0xed; BIOS_Memory[0x00e5] = 0xfe; BIOS_Memory[0x00e6] = 0xc9;
    BIOS_Memory[0x00e7] = 0xed; BIOS_Memory[0x00e8] = 0xfe; BIOS_Memory[0x00e9] = 0xc9;
    BIOS_Memory[0x00ea] = 0xed; BIOS_Memory[0x00eb] = 0xfe; BIOS_Memory[0x00ec] = 0xc9;
    BIOS_Memory[0x00ed] = 0xed; BIOS_Memory[0x00ee] = 0xfe; BIOS_Memory[0x00ef] = 0xc9;
    BIOS_Memory[0x00f0] = 0xed; BIOS_Memory[0x00f1] = 0xfe; BIOS_Memory[0x00f2] = 0xc9;
    BIOS_Memory[0x00f3] = 0xed; BIOS_Memory[0x00f4] = 0xfe; BIOS_Memory[0x00f5] = 0xc9;
}


extern char szName[];
void msxSaveEEPROM(void)
{
    // Return to the original path
    chdir(initial_path);    

    // Init filename = romname and SRM (SRAM) in place of ROM
    DIR* dir = opendir("sav");
    if (dir) closedir(dir);    // Directory exists... close it out and move on.
    else mkdir("sav", 0777);   // Otherwise create the directory...
    sprintf(szName,"sav/%s", initial_file);

    int len = strlen(szName);
    szName[len-3] = 's';
    szName[len-2] = 'r';
    szName[len-1] = 'm';
    szName[len-0] = 0;

    FILE *handle = fopen(szName, "wb+");  
    if (handle != NULL) 
    {
      fwrite(SRAM_Memory, 0x4000, 1, handle);
      fclose(handle);
    }
}

void msxLoadEEPROM(void)
{
    // Return to the original path
    chdir(initial_path);    

    // Init filename = romname and SRM (SRAM) in place of ROM
    DIR* dir = opendir("sav");
    if (dir) closedir(dir);    // Directory exists... close it out and move on.
    else mkdir("sav", 0777);   // Otherwise create the directory...
    sprintf(szName,"sav/%s", initial_file);

    int len = strlen(szName);
    szName[len-3] = 's';
    szName[len-2] = 'r';
    szName[len-1] = 'm';
    szName[len-0] = 0;

    FILE *handle = fopen(szName, "rb+");
    if (handle != NULL) 
    {
      fread(SRAM_Memory, 0x4000, 1, handle);
      fclose(handle);
    }
    else
    {
      memset(SRAM_Memory, 0xFF, 0x4000);
    }
}


// End of file
