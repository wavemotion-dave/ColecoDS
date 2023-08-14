// =====================================================================================
// Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)
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
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "printf.h"

#define NORAM 0xFF

// ------------------------------------------------------------------
// Casio PV-2000 reset 
// ------------------------------------------------------------------
void pv2000_reset(void)
{
    Port_PPI_CTRL = 0x00;
}


// --------------------------------------------------------------------------
// The Casio PV-2000 uses memory address 0x4000 + 0x4001 as the VDP iterface
// --------------------------------------------------------------------------
u8 cpu_readmem_pv2000 (u16 address) 
{
    if ((address == 0x4000) || (address == 0x4001))
    {
          if (address & 1) return(RdCtrl9918()); 
          else return(RdData9918());
    }
    return (RAM_Memory[address]);
}


// ---------------------------------------------------------------------------
// The Caio PV-2000 is unusual in that it will generate an interrupt if
// a key is pressed. This is checked once per frame when the NMI interrupt
// fires to indicate the end of the VDP period.
// ---------------------------------------------------------------------------
void pv2000_check_kbd(void)
{
    static u32 lastJoyState = 0;
    static u8 lastKbdState = 0;
    
    if (JoyState != 0 && (Port_PPI_CTRL == 0x0F))
    {
        if (lastJoyState != JoyState)
        {
            CPU.IRequest=INT_RST38;  // Change NMI into IRQ for keyboard press
        }
    }    
    lastJoyState = JoyState;
    
    if (kbd_key != 0 && (Port_PPI_CTRL == 0x0F))
    {
        if (lastKbdState != kbd_key)
        {
            CPU.IRequest=INT_RST38;  // Change NMI into IRQ for keyboard press
        }
    }    
    lastKbdState = kbd_key;
}

// --------------------------------------------------------------------------
// The Casio PV-2000 uses memory address 0x4000 + 0x4001 as the VDP iterface
// --------------------------------------------------------------------------
void cpu_writemem_pv2000 (u8 value,u16 address) 
{
    if ((address == 0x4000) || (address == 0x4001))
    {
        if ((address & 1) == 0) WrData9918(value);
        else if (WrCtrl9918(value)) { CPU.IRequest=INT_NMI; cpuirequest=Z80_NMI_INT; }
    }
    else if ((address >= 0x7000) && (address < 0x8000)) // PV-2000 RAM area
    {
        RAM_Memory[address] = value;
    }
}

// ------------------------------------------------------------------
// Casio PV-2000 IO Port Read
// ------------------------------------------------------------------
unsigned char cpu_readport_pv2000(register unsigned short Port) 
{
  // PV-2000 ports are 8-bit
  Port &= 0x00FF; 
 
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

      if ((kbd_key != 0) && (kbd_key != KBD_KEY_SHIFT) && (kbd_key != KBD_KEY_CTRL) && (kbd_key != KBD_KEY_CODE) && (kbd_key != KBD_KEY_GRAPH))
      {
          if (last_special_key_dampen == 20) last_special_key_dampen = 19;    // Start the SHIFT/CONTROL countdown... this should be enough time for it to register
      }
  }
    
  if (Port == 0x10)
  {
      u8 joy1 = 0x00;
      if ((Port_PPI_CTRL & 0x0F) == 0x00)    // 5,6,7,8
      {
          if (JoyState == JST_5)    joy1 |= 0x08;
          if (JoyState == JST_6)    joy1 |= 0x04;
          if (JoyState == JST_7)    joy1 |= 0x02;

          if (kbd_key == '5')       joy1 |= 0x08;
          if (kbd_key == '6')       joy1 |= 0x04;
          if (kbd_key == '7')       joy1 |= 0x02;
          if (kbd_key == '8')       joy1 |= 0x01;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x01)    // TYUI
      {
          if (JoyState == JST_8)    joy1 |= 0x04; // Y
          
          if (kbd_key == 'T')       joy1 |= 0x08;
          if (kbd_key == 'Y')       joy1 |= 0x04;
          if (kbd_key == 'U')       joy1 |= 0x02;
          if (kbd_key == 'I')       joy1 |= 0x01;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x02)    // KJHG
      {
          if (kbd_key == 'G')       joy1 |= 0x08;
          if (kbd_key == 'H')       joy1 |= 0x04;
          if (kbd_key == 'J')       joy1 |= 0x02;
          if (kbd_key == 'K')       joy1 |= 0x01;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x03)    // VBN SPACE
      {
          if (JoyState == JST_9)    joy1 |= 0x02; // N
          
          if (kbd_key == 'V')       joy1 |= 0x08;
          if (kbd_key == 'B')       joy1 |= 0x04;
          if (kbd_key == 'N')       joy1 |= 0x02;
          if (kbd_key == ' ')       joy1 |= 0x01;
      }
      
      if ((Port_PPI_CTRL & 0x0F) == 0x04)    // 9-^0
      {

          if (kbd_key == '0')       joy1 |= 0x08;
          if (kbd_key == '^')       joy1 |= 0x04;
          if (kbd_key == '-')       joy1 |= 0x02;
          if (kbd_key == '9')       joy1 |= 0x01;
      }
      
      if ((Port_PPI_CTRL & 0x0F) == 0x05)    // P[@O
      {

          if (kbd_key == 'P')       joy1 |= 0x08;
          if (kbd_key == '[')       joy1 |= 0x04;
          if (kbd_key == '@')       joy1 |= 0x02;
          if (kbd_key == 'O')       joy1 |= 0x01;
      }
      
      if ((Port_PPI_CTRL & 0x0F) == 0x06)    // L:];
      {

          if (kbd_key == ';')       joy1 |= 0x08;
          if (kbd_key == ']')       joy1 |= 0x04;
          if (kbd_key == ':')       joy1 |= 0x02;
          if (kbd_key == 'L')       joy1 |= 0x01;
      }

      if ((Port_PPI_CTRL & 0x0F) == 0x07)    // ,/.M
      {

          if (kbd_key == ',')       joy1 |= 0x08;
          if (kbd_key == '/')       joy1 |= 0x04;
          if (kbd_key == '.')       joy1 |= 0x02;
          if (kbd_key == 'M')       joy1 |= 0x01;
      }      
        
      if ((Port_PPI_CTRL & 0x0F) == 0x08)    // Keyboard RETURN
      {
          if (JoyState == JST_POUND)    joy1 |= 0x01;
          if (kbd_key == KBD_KEY_RET)   joy1 |= 0x01;
          if (kbd_key == KBD_KEY_HOME)  joy1 |= 0x02;
          if (kbd_key == KBD_KEY_DEL)   joy1 |= 0x04;
      }
      
      if ((Port_PPI_CTRL & 0x0F) == 0x09)    // STOP
      {
          if (kbd_key == KBD_KEY_STOP)  joy1 |= 0x08;
      }
      
      return joy1;
  }
  else if (Port == 0x20)
  {
      u8 joy1 = 0x00;
      
      if ((Port_PPI_CTRL & 0x0F) == 0x00)    // 1,2,3,4
      {
          if (JoyState == JST_1)    joy1 |= 0x08;
          if (JoyState == JST_2)    joy1 |= 0x04;
          if (JoyState == JST_3)    joy1 |= 0x02;
          if (JoyState == JST_4)    joy1 |= 0x01;
          
          if (kbd_key == '1')       joy1 |= 0x08;
          if (kbd_key == '2')       joy1 |= 0x04;
          if (kbd_key == '3')       joy1 |= 0x02;
          if (kbd_key == '4')       joy1 |= 0x01;
          
      }
      
      if ((Port_PPI_CTRL & 0x0F) == 0x01)    // REWQ
      {
          if (kbd_key == 'Q')       joy1 |= 0x08;
          if (kbd_key == 'W')       joy1 |= 0x04;
          if (kbd_key == 'E')       joy1 |= 0x02;
          if (kbd_key == 'R')       joy1 |= 0x01;
      }

      if ((Port_PPI_CTRL & 0x0F) == 0x02)    // ASDF
      {
          if (kbd_key == 'A')       joy1 |= 0x08;
          if (kbd_key == 'S')       joy1 |= 0x04;
          if (kbd_key == 'D')       joy1 |= 0x02;
          if (kbd_key == 'F')       joy1 |= 0x01;
      }

      if ((Port_PPI_CTRL & 0x0F) == 0x03)    // ZXC CODE
      {
          if (kbd_key == KBD_KEY_ESC)   joy1 |= 0x08;
          if (kbd_key == 'Z')           joy1 |= 0x04;
          if (kbd_key == 'X')           joy1 |= 0x02;
          if (kbd_key == 'C')           joy1 |= 0x01;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x04)    // Player 1 START and SELECT
      {
          if (JoyState == JST_0)     joy1 |= 0x02;    // START
          if (JoyState == JST_STAR)  joy1 |= 0x04;    // SELECT
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x05)
      {

      }
      if ((Port_PPI_CTRL & 0x0F) == 0x06)    // Joystick Left Right
      {
          if (JoyState & JST_DOWN)      joy1 |= 0x01;
          if (JoyState & JST_RIGHT)     joy1 |= 0x02;
          if (kbd_key == KBD_KEY_DOWN)  joy1 |= 0x01;
          if (kbd_key == KBD_KEY_RIGHT) joy1 |= 0x02;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x07)    // Joystick Up Down
      {
          if (JoyState & JST_LEFT)      joy1 |= 0x01;
          if (JoyState & JST_UP)        joy1 |= 0x02;
          if (kbd_key == KBD_KEY_LEFT)  joy1 |= 0x01;
          if (kbd_key == KBD_KEY_UP)    joy1 |= 0x02;
      }
      
      if ((Port_PPI_CTRL & 0x0F) == 0x08)    // Joystick Attack Buttons
      {
          if (JoyState & JST_FIREL)   joy1 |= 0x01;
          if (JoyState & JST_FIRER)   joy1 |= 0x02;
      }
      
      return joy1;
  }
  else if (Port == 0x40)
  {
      u8 joy1 = 0x00;
      if (JoyState == JST_BLUE)     joy1 |= 0x01;   // COLOR
      if (JoyState == JST_PURPLE)   joy1 |= 0x02;   // FUNC
      if (key_graph)                joy1 |= 0x01;   // COLOR
      if (key_ctrl)                 joy1 |= 0x02;   // FUNC
      if (key_shift)                joy1 |= 0x04;   // SHIFT
      return joy1;
  }

  // No such port
  return(NORAM);
}

// ----------------------------------------------------------------------
// Casio PV-2000 IO Port Write
// ----------------------------------------------------------------------
void cpu_writeport_pv2000(register unsigned short Port,register unsigned char Value) 
{
    // SG-1000 ports are 8-bit
    Port &= 0x00FF;
    
    if (Port == 0x20)
    {
        Port_PPI_CTRL = Value & 0x0F;
    }
    if (Port == 0x40) sn76496W(Value, &sncol);
}


// End of file
