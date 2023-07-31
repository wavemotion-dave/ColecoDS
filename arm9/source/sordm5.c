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
#include "printf.h"

#define NORAM 0xFF

// ------------------------------------------------------------------
// Sord M5 IO Port Read - just VDP, Joystick/Keyboard and Z80-CTC
// ------------------------------------------------------------------
unsigned char cpu_readport_m5(register unsigned short Port) 
{
  // M5 ports are 8-bit but bit3 is not decoded
  Port &= 0x00F7; 

  if (Port < 0x10)      // Z80-CTC Area
  {
      Port &= 0x03;
      return ctc_timer[Port];
  }
  else if (Port < 0x20)  // VDP Area
  {
      return(Port&0x01 ? RdCtrl9918():RdData9918());      
  }
  else if (Port == 0x30)    // Y0
  {
      u8 joy1 = 0x00;

      if (key_ctrl)                 joy1 |= 0x01;
      if (key_shift)                joy1 |= 0x04;
      if (kbd_key == ' ')           joy1 |= 0x40;
      if (kbd_key == KBD_KEY_RET)   joy1 |= 0x80;
      
      return (joy1);      
  }
  else if (Port == 0x31)    // Y1
  {
      u8 joy1 = 0x00;
      if (JoyState & JST_FIREL) joy1 |= 0x01;  // '1' (joystick button 1)
      if (JoyState & JST_FIRER) joy1 |= 0x02;  // '2' (joystick button 2)
      if (JoyState == JST_1)   joy1 |= 0x01;  // '1'
      if (JoyState == JST_2)   joy1 |= 0x02;  // '2'
      if (JoyState == JST_3)   joy1 |= 0x04;  // '3'
      if (JoyState == JST_4)   joy1 |= 0x08;  // '4'
      if (JoyState == JST_5)   joy1 |= 0x10;  // '5'
      if (JoyState == JST_6)   joy1 |= 0x20;  // '6'
      if (JoyState == JST_7)   joy1 |= 0x40;  // '7'
      if (JoyState == JST_8)   joy1 |= 0x80;  // '8'
      
      if (kbd_key == '1')      joy1 |= 0x01;
      if (kbd_key == '2')      joy1 |= 0x02;
      if (kbd_key == '3')      joy1 |= 0x04;
      if (kbd_key == '4')      joy1 |= 0x08;
      if (kbd_key == '5')      joy1 |= 0x10;
      if (kbd_key == '6')      joy1 |= 0x20;
      if (kbd_key == '7')      joy1 |= 0x40;
      if (kbd_key == '8')      joy1 |= 0x80;
      
      return (joy1);
  }
  else if (Port == 0x32)    // Y2
  {
      u8 joy1 = 0x00;
      
      if (kbd_key == 'Q')      joy1 |= 0x01;
      if (kbd_key == 'W')      joy1 |= 0x02;
      if (kbd_key == 'E')      joy1 |= 0x04;
      if (kbd_key == 'R')      joy1 |= 0x08;
      if (kbd_key == 'T')      joy1 |= 0x10;
      if (kbd_key == 'Y')      joy1 |= 0x20;
      if (kbd_key == 'U')      joy1 |= 0x40;
      if (kbd_key == 'I')      joy1 |= 0x80;
      
      return joy1;
  }
  else if (Port == 0x33)    // Y3
  {
      u8 joy1 = 0x00;
      
      if (kbd_key == 'A')      joy1 |= 0x01;
      if (kbd_key == 'S')      joy1 |= 0x02;
      if (kbd_key == 'D')      joy1 |= 0x04;
      if (kbd_key == 'F')      joy1 |= 0x08;
      if (kbd_key == 'G')      joy1 |= 0x10;
      if (kbd_key == 'H')      joy1 |= 0x20;
      if (kbd_key == 'J')      joy1 |= 0x40;
      if (kbd_key == 'K')      joy1 |= 0x80;
      
      return joy1;
  }
  else if (Port == 0x34)    // Y4
  {
      u8 joy1 = 0x00;
      
      if (kbd_key == 'Z')      joy1 |= 0x01;
      if (kbd_key == 'X')      joy1 |= 0x02;
      if (kbd_key == 'C')      joy1 |= 0x04;
      if (kbd_key == 'V')      joy1 |= 0x08;
      if (kbd_key == 'B')      joy1 |= 0x10;
      if (kbd_key == 'N')      joy1 |= 0x20;
      if (kbd_key == 'M')      joy1 |= 0x40;
      if (kbd_key == ',')      joy1 |= 0x80;
      
      return joy1;
  }
  else if (Port == 0x35)    // Y5
  {
      u8 joy1 = 0x00;
      
      if (kbd_key == '9')      joy1 |= 0x01;
      if (kbd_key == '0')      joy1 |= 0x02;
      if (kbd_key == '-')      joy1 |= 0x04;
      if (kbd_key == '^')      joy1 |= 0x08;
      if (kbd_key == '.')      joy1 |= 0x10;
      if (kbd_key == '/')      joy1 |= 0x20;
      if (kbd_key == '-')      joy1 |= 0x40;
      if (kbd_key == '\\')     joy1 |= 0x80;
      return joy1;
  }
  else if (Port == 0x36)    // Y6
  {
      u8 joy1 = 0x00;
      
      if (kbd_key == 'O')      joy1 |= 0x01;
      if (kbd_key == 'P')      joy1 |= 0x02;
      if (kbd_key == '@')      joy1 |= 0x04;
      if (kbd_key == '[')      joy1 |= 0x08;
      if (kbd_key == 'L')      joy1 |= 0x10;
      if (kbd_key == ';')      joy1 |= 0x20;
      if (kbd_key == ':')      joy1 |= 0x40;
      if (kbd_key == ']')      joy1 |= 0x80;
      
      return joy1;
  }
  else if (Port == 0x37)    // Joystick Port 1 or 2
  {
      u8 joy1 = 0x00;
      
      // Player 1
      if (JoyState & JST_UP)     joy1 |= 0x02;
      if (JoyState & JST_DOWN)   joy1 |= 0x08;
      if (JoyState & JST_LEFT)   joy1 |= 0x04;
      if (JoyState & JST_RIGHT)  joy1 |= 0x01;      
      
      // Player 2
      if (JoyState & (JST_UP<<16))    joy1 |= 0x20;
      if (JoyState & (JST_DOWN<<16))  joy1 |= 0x80;
      if (JoyState & (JST_LEFT<<16))  joy1 |= 0x40;
      if (JoyState & (JST_RIGHT<<16)) joy1 |= 0x10;
        
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
    // M5 ports are 8-bit but bit3 is not decoded
    Port &= 0x00F7; 

    // ----------------------------------------------------------------------
    // Z80-CTC Area
    // This is only a partial implementation of the CTC logic - just enough
    // to handle the VDP and Sound Generation and very little else. This is
    // NOT accurate emulation - but it's good enough to render the Sord M5
    // games as playable in this emulator.
    // ----------------------------------------------------------------------
    if (Port < 0x10)
    {
        Port &= 0x03;
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
                    vdp_int_source = ctc_vector[3];         // When the VDP interrupts the CPU, it's channel 3 on the CTC
                }
            }
        }
    }
    else if (Port < 0x20)  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) CPU.IRequest=vdp_int_source;    // Sord M5 must get vector from the Z80-CTC. Only the CZ80 core works with this.
    }
    else if (Port < 0x30) sn76496W(Value, &sncol);
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
        vdp_int_source = INT_NONE;          // No IRQ set to start (part of CRC writes)
    }
}


// End of file
