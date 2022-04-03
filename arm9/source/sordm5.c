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
// Sord M5 IO Port Read - just VDP, Joystick/Keyboard and Z80-CTC
// ------------------------------------------------------------------
unsigned char cpu_readport_m5(register unsigned short Port) 
{
  // M5 ports are 8-bit
  Port &= 0x00FF; 

  if (Port >= 0x00 && Port <= 0x03)      // Z80-CTC Area
  {
      return ctc_timer[Port];
  }
  else if ((Port == 0x10) || (Port == 0x11))  // VDP Area
  {
      return(Port&0x01 ? RdCtrl9918():RdData9918());      
  }
  else if (Port == 0x30)    // Y0
  {
      u8 joy1 = 0x00;
      return (joy1);
  }
  else if (Port == 0x31)    // Y1
  {
      u8 joy1 = 0x00;
      if (JoyState & JST_FIREL) joy1 |= 0x01;  // '1' (joystick button 1)
      if (JoyState & JST_FIRER) joy1 |= 0x02;  // '2' (joystick button 2)
      if (JoyState & JST_1)   joy1 |= 0x01;  // '1'
      if (JoyState & JST_2)   joy1 |= 0x02;  // '2'
      if (JoyState & JST_3)   joy1 |= 0x04;  // '3'
      if (JoyState & JST_4)   joy1 |= 0x08;  // '4'
      if (JoyState & JST_5)   joy1 |= 0x10;  // '5'
      if (JoyState & JST_6)   joy1 |= 0x20;  // '6'
      if (JoyState & JST_7)   joy1 |= 0x40;  // '7'
      if (JoyState & JST_8)   joy1 |= 0x80;  // '8'
      return (joy1);
  }
  else if (Port >= 0x32 && Port < 0x37)
  {
      return 0x00;
  }
  else if (Port == 0x37)    // Joystick Port 1
  {
      u8 joy1 = 0x00;
      if (JoyState & JST_UP)     joy1 |= 0x02;
      if (JoyState & JST_DOWN)   joy1 |= 0x08;
      if (JoyState & JST_LEFT)   joy1 |= 0x04;
      if (JoyState & JST_RIGHT)  joy1 |= 0x01;      
      return (joy1);
  }
  else if (Port == 0x50)
  {
      return 0x00;
  }
    
  // No such port
  return(NORAM);
}

// --------------------------------------------------------------------------
// Sord M5 IO Port Write - Need to handle SN sound, VDP and the Z80-CTC chip
// --------------------------------------------------------------------------
void cpu_writeport_m5(register unsigned short Port,register unsigned char Value) 
{
    // M5 ports are 8-bit
    Port &= 0x00FF;

    // ----------------------------------------------------------------------
    // Z80-CTC Area
    // This is only a partial implementation of the CTC logic - just enough
    // to handle the VDP and Sound Generation and very little else. This is
    // NOT accurate emulation - but it's good enough to render the Sord M5
    // games as playable in this emulator.
    // ----------------------------------------------------------------------
    if (Port >= 0x00 && Port <= 0x03)      
    {
        if (ctc_latch[Port])    // If latched, we now have the countdown timer value
        {
            ctc_time[Port] = Value;     // Latch the time constant and compute the countdown timer directly below.
            ctc_timer[Port] = ((((ctc_control[Port] & 0x20) ? 256 : 16) * (ctc_time[Port] ? ctc_time[Port]:256)) / CTC_SOUND_DIV) + 1;
            ctc_latch[Port] = 0x00;     // Reset the latch - we're back to looking for control words
        }
        else
        {
            if (Value & 1) // Control Word
            {
                ctc_control[Port] = Value;      // Keep track of the control port 
                ctc_latch[Port] = Value & 0x04; // If the caller wants to set a countdown timer, the next value read will latch the timer
            }
            else
            {
                if (Port == 0x00) // Channel 0, bit0 clear is special - this is where the 4 CTC vector addresses are setup
                {
                    ctc_vector[0] = (Value & 0xf8) | 0;     // Usually used for SIO which is not emulated here
                    ctc_vector[1] = (Value & 0xf8) | 2;     // Usually used for PSG sound generation which we must deal with
                    ctc_vector[2] = (Value & 0xf8) | 4;     // Usually used for SIO which is not emulated here
                    ctc_vector[3] = (Value & 0xf8) | 6;     // Used for the VDP interrupt - this one is crucial!
                    sordm5_irq = ctc_vector[3];             // When the VDP interrupts the CPU, it's channel 3 on the CTC
                }
            }
        }
    }
    else if ((Port == 0x10) || (Port == 0x11))  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) CPU.IRequest=sordm5_irq;    // Sord M5 must get vector from the Z80-CTC. Only the CZ80 core works with this.
    }
    else if (Port == 0x20) sn76496W(Value, &sncol);
}


// ---------------------------------------------------------
// The Sord M5 has Z80-CTC vars that need to be reset.
// ---------------------------------------------------------
void sordm5_reset(void)
{
    if (sordm5_mode)
    {
        // Reset the Z80-CTC stuff...
        memset(ctc_control, 0x00, 4);       // Set Software Reset Bit (freeze)
        memset(ctc_time, 0x00, 4);          // No time value set
        memset(ctc_vector, 0x00, 4);        // No vectors set
        memset(ctc_latch, 0x00, 4);         // No latch set
        memset(ctc_timer, 0x00, 8);         // No timer value set
        sordm5_irq = 0xFF;                  // No IRQ set
    }
}


// End of file
