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
#include "AdamNet.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/z80/ctc.h"
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
          DSPrint(4,0,6, "FUNC");
          key_code = 1;
      }

      if ((kbd_key != 0) && (kbd_key != KBD_KEY_SHIFT) && (kbd_key != KBD_KEY_CTRL) && (kbd_key != KBD_KEY_CODE))
      {
          if (last_special_key_dampen == 20) last_special_key_dampen = 19;    // Start the SHIFT/CONTROL countdown... this should be enough time for it to register
      }
  }
    
    
  if (Port < 0x10)      // Z80-CTC Area
  {
      return CTC_Read(Port & 0x03);
  }
  else if (Port < 0x20)  // VDP Area
  {
      return(Port&0x01 ? RdCtrl9918():RdData9918());      
  }
  else if (Port == 0x30)    // Y0
  {
      u8 joy1 = 0x00;

      if (key_ctrl)                 joy1 |= 0x01;
      if (key_code)                 joy1 |= 0x02;
      if (key_shift)                joy1 |= 0x04;
      if (kbd_key == ' ')           joy1 |= 0x40;
      if (kbd_key == KBD_KEY_RET)   joy1 |= 0x80;
      
      return (joy1);      
  }
  else if (Port == 0x31)    // Y1
  {
      
      u8 joy1 = 0x00;
      // -------------------------------------------------
      // Check joystick fire buttons or keypad area
      // -------------------------------------------------
      if (JoyState)
      {
          if (JoyState & JST_FIREL)     joy1 |= 0x01;  // '1' (P1 joystick button 1)
          if (JoyState & JST_FIRER)     joy1 |= 0x02;  // '2' (P1 joystick button 2)
          if (JoyState & JST_FIREL<<16) joy1 |= 0x10;  // '5' (P2 joystick button 1)
          if (JoyState & JST_FIRER<<16) joy1 |= 0x20;  // '6' (P2 joystick button 2)

          if (JoyState == JST_1)   joy1 |= 0x01;  // '1'
          if (JoyState == JST_2)   joy1 |= 0x02;  // '2'
          if (JoyState == JST_3)   joy1 |= 0x04;  // '3'
          if (JoyState == JST_4)   joy1 |= 0x08;  // '4'
          if (JoyState == JST_5)   joy1 |= 0x10;  // '5'
          if (JoyState == JST_6)   joy1 |= 0x20;  // '6'
          if (JoyState == JST_7)   joy1 |= 0x40;  // '7'
          if (JoyState == JST_8)   joy1 |= 0x80;  // '8'
      }
      
      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i<kbd_keys_pressed; i++)
      {
          kbd_key = kbd_keys[i];

          if (kbd_key == '1')      joy1 |= 0x01;
          if (kbd_key == '2')      joy1 |= 0x02;
          if (kbd_key == '3')      joy1 |= 0x04;
          if (kbd_key == '4')      joy1 |= 0x08;
          if (kbd_key == '5')      joy1 |= 0x10;
          if (kbd_key == '6')      joy1 |= 0x20;
          if (kbd_key == '7')      joy1 |= 0x40;
          if (kbd_key == '8')      joy1 |= 0x80;
      }
      
      return (joy1);
  }
  else if (Port == 0x32)    // Y2
  {
      u8 joy1 = 0x00;
      
      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i<kbd_keys_pressed; i++)
      {
          kbd_key = kbd_keys[i];
          
          if (kbd_key == 'Q')      joy1 |= 0x01;
          if (kbd_key == 'W')      joy1 |= 0x02;
          if (kbd_key == 'E')      joy1 |= 0x04;
          if (kbd_key == 'R')      joy1 |= 0x08;
          if (kbd_key == 'T')      joy1 |= 0x10;
          if (kbd_key == 'Y')      joy1 |= 0x20;
          if (kbd_key == 'U')      joy1 |= 0x40;
          if (kbd_key == 'I')      joy1 |= 0x80;
      }
      
      return joy1;
  }
  else if (Port == 0x33)    // Y3
  {
      u8 joy1 = 0x00;
      
      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i<kbd_keys_pressed; i++)
      {
          kbd_key = kbd_keys[i];
          
          if (kbd_key == 'A')      joy1 |= 0x01;
          if (kbd_key == 'S')      joy1 |= 0x02;
          if (kbd_key == 'D')      joy1 |= 0x04;
          if (kbd_key == 'F')      joy1 |= 0x08;
          if (kbd_key == 'G')      joy1 |= 0x10;
          if (kbd_key == 'H')      joy1 |= 0x20;
          if (kbd_key == 'J')      joy1 |= 0x40;
          if (kbd_key == 'K')      joy1 |= 0x80;
      }
      
      return joy1;
  }
  else if (Port == 0x34)    // Y4
  {
      u8 joy1 = 0x00;
      
      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i<kbd_keys_pressed; i++)
      {
          kbd_key = kbd_keys[i];
          
          if (kbd_key == 'Z')      joy1 |= 0x01;
          if (kbd_key == 'X')      joy1 |= 0x02;
          if (kbd_key == 'C')      joy1 |= 0x04;
          if (kbd_key == 'V')      joy1 |= 0x08;
          if (kbd_key == 'B')      joy1 |= 0x10;
          if (kbd_key == 'N')      joy1 |= 0x20;
          if (kbd_key == 'M')      joy1 |= 0x40;
          if (kbd_key == ',')      joy1 |= 0x80;
      }
      
      return joy1;
  }
  else if (Port == 0x35)    // Y5
  {
      u8 joy1 = 0x00;
      
      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i<kbd_keys_pressed; i++)
      {
          kbd_key = kbd_keys[i];
          
          if (kbd_key == '9')      joy1 |= 0x01;
          if (kbd_key == '0')      joy1 |= 0x02;
          if (kbd_key == '-')      joy1 |= 0x04;
          if (kbd_key == '^')      joy1 |= 0x08;
          if (kbd_key == '.')      joy1 |= 0x10;
          if (kbd_key == '/')      joy1 |= 0x20;
          if (kbd_key == '_')      joy1 |= 0x40;
          if (kbd_key == '\\')     joy1 |= 0x80;
      }
      return joy1;
  }
  else if (Port == 0x36)    // Y6
  {
      u8 joy1 = 0x00;
      
      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i<kbd_keys_pressed; i++)
      {
          kbd_key = kbd_keys[i];
          
          if (kbd_key == 'O')      joy1 |= 0x01;
          if (kbd_key == 'P')      joy1 |= 0x02;
          if (kbd_key == '@')      joy1 |= 0x04;
          if (kbd_key == '[')      joy1 |= 0x08;
          if (kbd_key == 'L')      joy1 |= 0x10;
          if (kbd_key == ';')      joy1 |= 0x20;
          if (kbd_key == ':')      joy1 |= 0x40;
          if (kbd_key == ']')      joy1 |= 0x80;
      }
      
      return joy1;
  }
  else if (Port == 0x37)    // Joystick Port 1 or 2
  {
      u8 joy1 = 0x00;
      
      if (JoyState)
      {
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
      }
        
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
        CTC_Write(Port & 0x03, Value);  // Write the CTC data
    }
    else if (Port < 0x20)  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) CPU.IRequest=vdp_int_source;    // Sord M5 must get vector from the Z80-CTC. Only the CZ80 core works with this.
    }
    else if (Port < 0x30) sn76496W(Value, &sncol);
}

// ---------------------------------------------------------
// The Sord M5 generates interrupts on key press. 
// ---------------------------------------------------------
void sordm5_check_keyboard_interrupt(void)
{
    static u8 last_m5_kbd=0; 
    static u32 last_m5_joy=0;
    if (ctc_enabled)
    {
      if (kbd_key || JoyState) 
      {
          if ((kbd_key != last_m5_kbd) || (JoyState != last_m5_joy)) keyboard_interrupt = vdp_int_source; // Sord M5 cascades interupts for keyboard onto the VDP
      }
      last_m5_kbd = kbd_key;
      last_m5_joy = JoyState;
    }
}

// ---------------------------------------------------------
// The Sord M5 has Z80-CTC vars that need to be reset.
// ---------------------------------------------------------
void sordm5_reset(void)
{
    if (sordm5_mode)
    {
        // Reset the Z80-CTC stuff...
        CTC_Init(CTC_CHAN3);                // CTC channel 3 is the VDP interrupt
        vdp_int_source = INT_NONE;          // No IRQ set to start (part of CTC writes)
        keyboard_interrupt = INT_NONE;      // No keyboard int to start
    }
}


// End of file
