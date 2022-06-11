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
#include <fcntl.h>

#include "colecoDS.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#define NORAM 0xFF

u16 einstein_ram_start = 0x8000;
u8 keyboard_w = 0x00;
u8 key_int_mask = 0xFF;
u8 myKeyData = 0xFF;
u8 adc_mux = 0x00;
u16 keyboard_interrupt=0;

#define KEYBOARD_VECTOR  0xF7

void scan_keyboard(void)
{
    if ((nds_key == 0) && (kbd_key == 0))
    {
        // Nothing pressed... short-circut    
        myKeyData = 0x00;
    }
    else
    {
      myKeyData = 0x00;
      if (!(keyboard_w & 0x01))
      {
          if (kbd_key == KBD_KEY_BRK)   myKeyData |= 0x01;
          if (kbd_key == KBD_KEY_F7)    myKeyData |= 0x04;
          if (kbd_key == KBD_KEY_HOME)  myKeyData |= 0x08;
          if (kbd_key == KBD_KEY_CAPS)  myKeyData |= 0x10;
          if (kbd_key == KBD_KEY_RET)   myKeyData |= 0x20;
          if (kbd_key == ' ')           myKeyData |= 0x40;
          if (kbd_key == KBD_KEY_ESC)   myKeyData |= 0x80;
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_RETURN)) myKeyData |= 0x20;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_SPACE))  myKeyData |= 0x40;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_ESC))    myKeyData |= 0x80;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_HOME))   myKeyData |= 0x08;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F7))     myKeyData |= 0x04;
          }
      }
      if (!(keyboard_w & 0x02))
      {
          if ((JoyState & JST_FIRER))   myKeyData |= 0x20;  // Same as DOWN

          if (kbd_key == 'I')           myKeyData |= 0x01;
          if (kbd_key == 'O')           myKeyData |= 0x02;
          if (kbd_key == 'P')           myKeyData |= 0x04;
          if (kbd_key == KBD_KEY_LEFT)  myKeyData |= 0x08;
          if (kbd_key == '-')           myKeyData |= 0x10;
          if (kbd_key == KBD_KEY_DOWN)  myKeyData |= 0x20;
          if (kbd_key == '0')           myKeyData |= 0x80;
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_I))      myKeyData |= 0x01;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_O))      myKeyData |= 0x02;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_P))      myKeyData |= 0x04;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_LEFT))   myKeyData |= 0x08;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_DOWN))   myKeyData |= 0x40;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_0))      myKeyData |= 0x80;
          }
      }
        
      if (!(keyboard_w & 0x04))
      {
          if (kbd_key == 'K')           myKeyData |= 0x01;
          if (kbd_key == 'L')           myKeyData |= 0x02;
          if (kbd_key == ';')           myKeyData |= 0x04;
          if (kbd_key == ':')           myKeyData |= 0x08;
          if (kbd_key == KBD_KEY_RIGHT) myKeyData |= 0x10;
          if (kbd_key == '9')           myKeyData |= 0x40;
          if (kbd_key == KBD_KEY_F8)    myKeyData |= 0x80;
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_K))      myKeyData |= 0x01;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_L))      myKeyData |= 0x02;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_COLON))  myKeyData |= 0x08;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_RIGHT))  myKeyData |= 0x10;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_9))      myKeyData |= 0x40;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F8))     myKeyData |= 0x80;
          }
      }
        
      if (!(keyboard_w & 0x08))
      {
          if ((JoyState&0x0F) == JST_PURPLE) myKeyData |= 0x40;  // Same as UP

          if (kbd_key == ',')           myKeyData |= 0x01;
          if (kbd_key == '.')           myKeyData |= 0x02;
          if (kbd_key == '/')           myKeyData |= 0x04;
          if (kbd_key == '8')           myKeyData |= 0x08;
          if (kbd_key == KBD_KEY_DEL)   myKeyData |= 0x10;
          if (kbd_key == '=')           myKeyData |= 0x20;
          if (kbd_key == KBD_KEY_UP)    myKeyData |= 0x40;
          if (kbd_key == KBD_KEY_F4)    myKeyData |= 0x80;
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_PERIOD)) myKeyData |= 0x02;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_SLASH))  myKeyData |= 0x04;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_8))      myKeyData |= 0x08;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_UP))     myKeyData |= 0x40;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F4))     myKeyData |= 0x80;
          }
      }

      if (!(keyboard_w & 0x10))
      {
          if (kbd_key == '7')           myKeyData |= 0x01;
          if (kbd_key == '6')           myKeyData |= 0x02;
          if (kbd_key == '5')           myKeyData |= 0x04;
          if (kbd_key == '4')           myKeyData |= 0x08;
          if (kbd_key == '3')           myKeyData |= 0x10;
          if (kbd_key == '2')           myKeyData |= 0x20;
          if (kbd_key == '1')           myKeyData |= 0x40;
          if (kbd_key == KBD_KEY_F3)    myKeyData |= 0x80;
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_7))  myKeyData |= 0x01;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_6))  myKeyData |= 0x02;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_5))  myKeyData |= 0x04;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_4))  myKeyData |= 0x08;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_3))  myKeyData |= 0x10;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_2))  myKeyData |= 0x20;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_1))  myKeyData |= 0x40;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F3)) myKeyData |= 0x80;
          }
      }
      if (!(keyboard_w & 0x20))
      {
          if (kbd_key == 'U')           myKeyData |= 0x01;
          if (kbd_key == 'Y')           myKeyData |= 0x02;
          if (kbd_key == 'T')           myKeyData |= 0x04;
          if (kbd_key == 'R')           myKeyData |= 0x08;
          if (kbd_key == 'E')           myKeyData |= 0x10;
          if (kbd_key == 'W')           myKeyData |= 0x20;
          if (kbd_key == 'Q')           myKeyData |= 0x40;
          if (kbd_key == KBD_KEY_F2)    myKeyData |= 0x80;
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_U))  myKeyData |= 0x01;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_Y))  myKeyData |= 0x02;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_T))  myKeyData |= 0x04;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_R))  myKeyData |= 0x08;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_E))  myKeyData |= 0x10;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_W))  myKeyData |= 0x20;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_Q))  myKeyData |= 0x40;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F2)) myKeyData |= 0x80;
          }
      }
      if (!(keyboard_w & 0x40))
      {
          if (kbd_key == 'J')           myKeyData |= 0x01;
          if (kbd_key == 'H')           myKeyData |= 0x02;
          if (kbd_key == 'G')           myKeyData |= 0x04;
          if (kbd_key == 'F')           myKeyData |= 0x08;
          if (kbd_key == 'D')           myKeyData |= 0x10;
          if (kbd_key == 'S')           myKeyData |= 0x20;
          if (kbd_key == 'A')           myKeyData |= 0x40;
          if (kbd_key == KBD_KEY_F1)    myKeyData |= 0x80;
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_J))  myKeyData |= 0x01;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_H))  myKeyData |= 0x02;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_G))  myKeyData |= 0x04;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F))  myKeyData |= 0x08;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_D))  myKeyData |= 0x10;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_S))  myKeyData |= 0x20;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_A))  myKeyData |= 0x40;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F1)) myKeyData |= 0x80;
          }
      }
      if (!(keyboard_w & 0x80))
      {
          if (kbd_key == 'M')           myKeyData |= 0x01;
          if (kbd_key == 'N')           myKeyData |= 0x02;
          if (kbd_key == 'B')           myKeyData |= 0x04;
          if (kbd_key == 'V')           myKeyData |= 0x08;
          if (kbd_key == 'C')           myKeyData |= 0x10;
          if (kbd_key == 'X')           myKeyData |= 0x20;
          if (kbd_key == 'Z')           myKeyData |= 0x40;
          if (kbd_key == KBD_KEY_F6)    myKeyData |= 0x80;
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_M))  myKeyData |= 0x01;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_N))  myKeyData |= 0x02;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_B))  myKeyData |= 0x04;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_V))  myKeyData |= 0x08;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_C))  myKeyData |= 0x10;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_X))  myKeyData |= 0x20;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_Z))  myKeyData |= 0x40;
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F6)) myKeyData |= 0x80;
          }
      }
    }
    myKeyData = ~myKeyData;
}

// --------------------------------------------------------------------------
// The Einstein is interesting in that it will always respond to writes to
// RAM even if the ROM is swapped in (since you can't write to ROM anyway).
// This is handled in the memory write interface in Z80_interface.c
// --------------------------------------------------------------------------
void einstein_swap_memory(void)
{
    extern u8 EinsteinBios[];
    
    if (einstein_ram_start == 0x8000)  
    {
        extern u8 einstein_ram_dirty;
        if (einstein_ram_dirty) // If we have some dirty bytes, copy over all of the RAM shadow copy
        {
            einstein_ram_dirty=0;
            memcpy(pColecoMem, Slot3RAM, 0x8000);
        } 
        else    // Otherwise just copy over the RAM into where the BIOS was... saves time.
        {
            memcpy(pColecoMem, Slot3RAM, 0x2000);
        }
        einstein_ram_start = 0x0000;
    }
    else
    {
        einstein_ram_start = 0x8000;
        memcpy(pColecoMem,EinsteinBios,0x2000);
        //memset(pColecoMem+0x2000,0xFF, 0x6000);  - In theory we should blank this area but to save memory swap time we don't bother
        if (tape_len == 1626) // A bit of a hack... the size of the Diagnostic ROM
        {
            memcpy(pColecoMem+0x4000, romBuffer, tape_len);   // only for Diagnostics ROM
        }        
    }
}

// ---------------------------------------------------------------------
// The Einstein IO map is broken into chunks of 8 ports that are 
// semi-related. The map looks like:
// 00h to 01h   - RESET
// 02h to 07h   - PSG (AY Sound Chip + PortA/PortB for keyboard)
// 08h to 0Fh   - VDP
// 10h to 17h   - PCI (not emulated)
// 18h to 1Fh   - FDC (not emulated)
// 20h          - /KBDINT_MSK
// 21h          - /ADCINT_MSK
// 22h          - /ALPHA
// 23h          - /DRSEL (Drive Select)
// 24h          - /ROM (select ROM vs RAM)
// 25h          - /FIREINT_MSK
// 26h to 27h   - Unused
// 28h to 2Fh   - CTC
// 30h to 37h   - PIO (not emulated)
// 38h to 3Fh   - ADC (partially emulated for Joystick Only)
// ---------------------------------------------------------------------
unsigned char cpu_readport_einstein(register unsigned short Port) 
{
  // MTX ports are 8-bit
  Port &= 0x003F; 

  if (Port == 0x00 || Port == 0x01 || Port==0x04 || Port== 0x05) // Reset port
  {
      memset(ay_reg, 0x00, 16);    // Clear the AY registers...
  }
  else if (Port == 0x24)
  {
      einstein_swap_memory();
  }    
  else if ((Port >= 0x18) && (Port <= 0x1B)) // Floppy Disk
  {
      return 0xFF;
  }    
  else if ((Port >= 0x30) && (Port <= 0x33)) // PIO
  {
      return 0xFF;
  }    
  else if ((Port >= 0x38) && (Port <= 0x3F))    // ADC
  {
      u8 adc_port = 0xFF;
      
      if (adc_mux & 0x02) // Player 2 Joystick
      {
          if ((adc_mux & 5) == 4) 
          {
              adc_port = 0x7F;
          }
          if ((adc_mux & 5) == 5) 
          {
              adc_port = 0x7F;
          }
      }
      else              // Player 1 Joystick
      {
          if (myConfig.dpad == DPAD_DIAGONALS)
          {
              if      (JoyState & JST_UP)    JoyState = (JST_UP   | JST_RIGHT);
              else if (JoyState & JST_DOWN)  JoyState = (JST_DOWN | JST_LEFT);
              else if (JoyState & JST_LEFT)  JoyState = (JST_LEFT | JST_UP);
              else if (JoyState & JST_RIGHT) JoyState = (JST_RIGHT | JST_DOWN);
          }
          
          if ((adc_mux & 5) == 4) 
          {
              adc_port = 0x7F;
              if (JoyState & JST_RIGHT) adc_port = 0xFF;
              if (JoyState & JST_LEFT)  adc_port = 0x00;
          }
          if ((adc_mux & 5) == 5) 
          {
              adc_port = 0x7F;
              if (JoyState & JST_UP)    adc_port = 0xFF;
              if (JoyState & JST_DOWN)  adc_port = 0x00;
          }
      }
      
      return adc_port;
  }
  else if (Port >= 0x28 && Port <= 0x2F)      // Z80-CTC Area
  {
      return ctc_timer[Port & 0x03];
  }
  else if ((Port == 0x08) || (Port == 0x09) || (Port == 0x0E) || (Port == 0x0F))  // VDP Area
  {
      if ((Port & 1)==0) return(RdData9918());
      return(RdCtrl9918());
  }
  else if (Port == 0x02 || Port == 0x06)  // PSG Read... might be joypad data
  {
      // --------------
      // Port A Read
      // --------------
      if (ay_reg_idx == 14)
      {
          return 0xFF;
      }
      // --------------
      // Port B Read
      // --------------
      if (ay_reg_idx == 15)
      {
          scan_keyboard();
          return myKeyData;
      }            
      return FakeAY_ReadData();
  }        
  else if ((Port == 0x20)) // Keyboard Read Port
  {
      u8 key_port = 0xFF;
      
      if (keyboard_interrupt)
      {
          CPU.IRequest=INT_NONE;
          keyboard_interrupt = 0;
      }
      
      if (JoyState & JST_FIREL) key_port &= ~0x01;
      
      if (key_ctrl)  key_port &= ~0x40;  // CTRL KEY
      if (key_shift) key_port &= ~0x80;  // SHIFT KEY
      
          for (u8 i=0; i<12; i++)
          {
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_SHIFT))  key_port &= ~0x80;  // SHIFT KEY
              if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_CTRL))   key_port &= ~0x40;  // CTRL KEY
          }
      
      return key_port;
  }
  else
  {
  }
    
  // No such port
  return(NORAM);
}


// ------------------------------------------------------------------------------------
// Memotech MTX IO Port Write - Need to handle SN sound, VDP and the Z80-CTC chip
// ------------------------------------------------------------------------------------
void cpu_writeport_einstein(register unsigned short Port,register unsigned char Value) 
{
    // MTX ports are 8-bit
    Port &= 0x003F;
    
    if (Port == 0x00 || Port == 0x01 || Port==0x04 || Port== 0x05) // Reset port
    {
        memset(ay_reg, 0x00, 16);    // Clear the AY registers...
    }
    else if (Port == 0x20)  // KEYBOARD INT MASK
    {
        debug3++;
        key_int_mask = Value;   
    }
    else if (Port == 0x21)  // ADC INT MASK
    {
        //key_int_mask = Value;   
    }
    else if (Port == 0x25)  // JOYSTICK INT MASK
    {
        //key_int_mask = Value;   
    }
    else if (Port == 0x25)  // Drive Select
    {
        //if (Value & 1) einstein_ram_dirty();
    }
    else if (Port == 0x24)  // ROM vs RAM bank port
    {
        einstein_swap_memory();
    }
    else if ((Port >= 0x38) && (Port <= 0x3F))    // ADC
    {
        adc_mux = Value;
    }    
    else if ((Port >= 0x18) && (Port <= 0x1B)) // Floppy Disk
    {
    }    
    // ----------------------------------------------------------------------
    // Z80-CTC Area
    // This is only a partial implementation of the CTC logic - just enough
    // to handle the VDP and Sound Generation and very little else. This is
    // NOT accurate emulation - but it's good enough to render the Memotech
    // games as playable in this emulator.
    // ----------------------------------------------------------------------
    else if (Port >= 0x28 && Port <= 0x2F)
    {
        Port &= 0x03;
        if (ctc_latch[Port])    // If latched, we now have the countdown timer value
        {
            ctc_time[Port] = Value;     // Latch the time constant and compute the countdown timer directly below.
            ctc_latch[Port] = 0x00;     // Reset the latch - we're back to looking for control words
            
            if (Port < 3)
                ctc_timer[Port] = ((((ctc_control[Port] & 0x20) ? 256 : 16) * (ctc_time[Port] ? ctc_time[Port]:256)) / 170) + 1;
            else
                ctc_timer[3] = ((((ctc_control[3] & 0x20) ? 256 : 16) * (ctc_time[3] ? ctc_time[3]:256)) / 60) + 1;
            
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
                    ctc_vector[0] = (Value & 0xf8) | 0;     // 
                    ctc_vector[1] = (Value & 0xf8) | 2;     // 
                    ctc_vector[2] = (Value & 0xf8) | 4;     // 
                    ctc_vector[3] = (Value & 0xf8) | 6;     // 
                }
            }
        }
    }
    else if ((Port == 0x08) || (Port == 0x09) || (Port == 0x0E) || (Port == 0x0F))  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) CPU.IRequest=vdp_int_source;
    }
    else if (Port == 0x02 || Port == 0x06) 
    {
        FakeAY_WriteIndex(Value & 0x0F);
    }
    else if (Port == 0x03 || Port == 0x07) 
    {
        FakeAY_WriteData(Value);
        if (ay_reg_idx == 14) 
        {
            keyboard_w = Value;
            scan_keyboard();
        }
    }
    else if ((Port >= 0x10) && (Port <= 0x17))  // PCI
    {
    }
    else if ((Port >= 0x30) && (Port <= 0x37))  // PIO
    {
    }
    else
    {
    }
}


// ---------------------------------------------------------
// The Memotech MTX has CTC plus some memory handling stuff
// ---------------------------------------------------------
void einstein_reset(void)
{
    if (einstein_mode)
    {
        // Reset the Z80-CTC stuff...
        memset(ctc_control, 0x00, 4);       // Set Software Reset Bit (freeze)
        memset(ctc_time, 0x00, 4);          // No time value set
        memset(ctc_timer, 0x00, 8);         // No timer value set
        memset(ctc_vector, 0x00, 4);        // No vectors set
        memset(ctc_latch, 0x00, 4);         // No latch set
        
        einstein_ram_start = 0x8000;
        keyboard_w = 0x00;
        myKeyData = 0xFF;
        keyboard_interrupt=0;
        key_int_mask = 0xFF;
        
        memset(ay_reg, 0x00, 16);    // Clear the AY registers...
        
        memset(Slot3RAM, 0x00, 0x10000);
    }
}

void einstein_handle_interrupts(void)
{
  static u8 ein_key_dampen=0;
    
  if (++ein_key_dampen < 100) return;
  ein_key_dampen=0;
  if ((CPU.IRequest == INT_NONE) && (keyboard_interrupt != KEYBOARD_VECTOR))
  {
      if ((key_int_mask&1) == 0)
      {
        scan_keyboard();
        if (myKeyData != 0xFF)  
        {
            keyboard_interrupt = KEYBOARD_VECTOR;
        }
      }
  }
}

void einstein_load_com_file(void)
{
    memcpy(pColecoMem, Slot3RAM, 0x8000);
    einstein_ram_start = 0x0000;
    memcpy(pColecoMem+0x100, romBuffer, tape_len);
    memcpy(Slot3RAM+0x100, romBuffer, tape_len);
    CPU.PC.W = 0x100;
    JumpZ80(CPU.PC.W);
}

// End of file

