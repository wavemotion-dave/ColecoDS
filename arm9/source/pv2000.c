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
    if (JoyState != 0 && (Port_PPI_CTRL == 0x0F))
    {
        if (lastJoyState != JoyState)
        {
            CPU.IRequest=INT_RST38;  // Change NMI into IRQ for keyboard press
        }
    }    
    lastJoyState = JoyState;
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
    
  if (Port == 0x10)
  {
      u8 joy1 = 0x00;
      if ((Port_PPI_CTRL & 0x0F) == 0x00)    // 5,6,7
      {
          if (JoyState == JST_5)    joy1 |= 0x08;
          if (JoyState == JST_6)    joy1 |= 0x04;
          if (JoyState == JST_7)    joy1 |= 0x02;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x01)    // Keyboard 'Y'
      {
          if (JoyState == JST_8)    joy1 |= 0x04;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x03)    // Keyboard 'N'
      {
          if (JoyState == JST_9)    joy1 |= 0x02;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x08)    // Keyboard RETURN
      {
          if (JoyState == JST_POUND) joy1 |= 0x01;
      }
      return joy1;
  }
  if (Port == 0x20)
  {
      u8 joy1 = 0x00;
      
      if ((Port_PPI_CTRL & 0x0F) == 0x00)    // 1,2,3,4
      {
          if (JoyState == JST_1)    joy1 |= 0x01;
          if (JoyState == JST_2)    joy1 |= 0x02;
          if (JoyState == JST_3)    joy1 |= 0x04;
          if (JoyState == JST_4)    joy1 |= 0x08;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x04)    // Player 1 START and SELECT
      {
          if (JoyState == JST_0)     joy1 |= 0x02;    // START
          if (JoyState == JST_STAR)  joy1 |= 0x04;    // SELECT
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x06)    // Joystick Left Right
      {
          if (JoyState & JST_DOWN)   joy1 |= 0x01;
          if (JoyState & JST_RIGHT)  joy1 |= 0x02;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x07)    // Joystick Up Down
      {
          if (JoyState & JST_LEFT)  joy1 |= 0x01;
          if (JoyState & JST_UP)    joy1 |= 0x02;
      }
      if ((Port_PPI_CTRL & 0x0F) == 0x08)    // Joystick Attack Buttons
      {
          if (JoyState & JST_FIREL)   joy1 |= 0x01;
          if (JoyState & JST_FIRER)   joy1 |= 0x02;
      }
      
      return joy1;
  }
  if (Port == 0x40)
  {
      u8 joy1 = 0x00;
      if (JoyState == JST_BLUE)     joy1 |= 0x01;
      if (JoyState == JST_PURPLE)   joy1 |= 0x02;
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
