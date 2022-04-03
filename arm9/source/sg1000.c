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



// ------------------------------------------------------------------
// SG-1000 IO Port Read - just VDP and Joystick to contend with...
// ------------------------------------------------------------------
unsigned char cpu_readport_sg(register unsigned short Port) 
{
  // SG-1000 ports are 8-bit
  Port &= 0x00FF; 

  if ((Port & 0xE0) == 0xA0)  // VDP Area
  {
      if (Port & 1) return(RdCtrl9918()); 
      else return(RdData9918());
  }
  else if ((Port == 0xDC) || (Port == 0xC0)) // Joystick Port 1
  {
      u8 joy1 = 0x00;

      if (myConfig.dpad == DPAD_DIAGONALS)
      {
          if (JoyState & JST_UP)    joy1 |= (0x01 | 0x08);
          if (JoyState & JST_DOWN)  joy1 |= (0x02 | 0x04);
          if (JoyState & JST_LEFT)  joy1 |= (0x04 | 0x01);
          if (JoyState & JST_RIGHT) joy1 |= (0x08 | 0x02);
      }
      else
      {
          if (JoyState & JST_UP)    joy1 |= 0x01;
          if (JoyState & JST_DOWN)  joy1 |= 0x02;
          if (JoyState & JST_LEFT)  joy1 |= 0x04;
          if (JoyState & JST_RIGHT) joy1 |= 0x08;
      }     
      
      if (JoyState & JST_FIREL) joy1 |= 0x10;
      if (JoyState & JST_FIRER) joy1 |= 0x20;
      return (~joy1);
  }
  else if ((Port == 0xDD) || (Port == 0xC1)) // Joystick Port 2
  {
      u8 joy2 = 0x00;
      if (JoyState & JST_BLUE) joy2 |= 0x10;    // Reset (not sure this is used)
      return (~joy2);
  }
    
  // No such port
  return(NORAM);
}

// ----------------------------------------------------------------------
// SG-1000 IO Port Write - just VDP and SN Sound Chip to contend with...
// ----------------------------------------------------------------------
void cpu_writeport_sg(register unsigned short Port,register unsigned char Value) 
{
    // SG-1000 ports are 8-bit
    Port &= 0x00FF;

    if ((Port & 0xE0) == 0xA0)  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) { CPU.IRequest=INT_RST38; cpuirequest=Z80_IRQ_INT; }    // SG-1000 does not use NMI like Colecovision does...
    }
    else if (Port == 0x7E) sn76496W(Value, &sncol);
    else if (Port == 0x7F) sn76496W(Value, &sncol);
}


// End of file
