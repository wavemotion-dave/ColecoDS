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

u8 Port_PPI_CTRL = 0xFF;
u8 OldPortC=0xFF;

// ------------------------------------------------------------------
// SG-1000 IO Port Read - just VDP and Joystick to contend with...
// ------------------------------------------------------------------
void sg1000_reset(void)
{
    Port_PPI_C = 0xFF;
    Port_PPI_CTRL = 0xFF;
    OldPortC=0xFF;
    memcpy(pColecoMem, romBuffer, 0x8000);
}


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
  else if ((Port == 0xDC) || (Port == 0xC0)) // Port A
  {
      u8 joy1 = 0x00;

      if (sg1000_mode == 2) // SC-3000 mode
      {
          if (myConfig.dpad == DPAD_kbd_keyS)
          {
              if (JoyState & JST_UP)    kbd_key = KBD_KEY_UP;
              if (JoyState & JST_DOWN)  kbd_key = KBD_KEY_DOWN;
              if (JoyState & JST_LEFT)  kbd_key = KBD_KEY_LEFT;
              if (JoyState & JST_RIGHT) kbd_key = KBD_KEY_RIGHT;
              if (JoyState & JST_FIREL) kbd_key = ' ';
              if (JoyState & JST_FIRER) kbd_key = ' ';
          }
          
        //      Port A                          Port B
        // Row  D0  D1  D2  D3  D4  D5  D6  D7  D0  D1  D2  D3
        //  -   ------------------------------- ---------------
        //  0   '1' 'Q' 'A' 'Z' ED  ',' 'K' 'I' '8' --- --- ---
        //  1   '2' 'W' 'S' 'X' SPC '.' 'L' 'O' '9' --- --- ---
        //  2   '3' 'E' 'D' 'C' HC  '/' ';' 'P' '0' --- --- ---
        //  3   '4' 'R' 'F' 'V' ID  PI  ':' '@' '-' --- --- ---
        //  4   '5' 'T' 'G' 'B' --- DA  ']' '[' '^' --- --- ---
        //  5   '6' 'Y' 'H' 'N' --- LA  CR  --- YEN --- --- FNC
        //  6   '7' 'U' 'J' 'M' --- RA  UA  --- BRK GRP CTL SHF
        //  7   1U  1D  1L  1R  1TL 1TR 2U  2D  2L  2R  2TL 2TR
        if ((Port_PPI_C & 0x07) == 0x00)  // Row 0
        {
          if (kbd_key == '1')       joy1 |= 0x01;
          if (kbd_key == 'Q')       joy1 |= 0x02;
          if (kbd_key == 'A')       joy1 |= 0x04;
          if (kbd_key == 'Z')       joy1 |= 0x08;
          if (kbd_key == KBD_KEY_ESC) joy1 |= 0x10;
          if (kbd_key == ',')       joy1 |= 0x20;
          if (kbd_key == 'K')       joy1 |= 0x40;
          if (kbd_key == 'I')       joy1 |= 0x80;
        }

        if ((Port_PPI_C & 0x07) == 0x01)  // Row 1
        {
          if (kbd_key == '2')       joy1 |= 0x01;
          if (kbd_key == 'W')       joy1 |= 0x02;
          if (kbd_key == 'S')       joy1 |= 0x04;
          if (kbd_key == 'X')       joy1 |= 0x08;

          if (kbd_key == ' ')       joy1 |= 0x10;
          if (kbd_key == '.')       joy1 |= 0x20;
          if (kbd_key == 'L')       joy1 |= 0x40;
          if (kbd_key == 'O')       joy1 |= 0x80;
        }

        if ((Port_PPI_C & 0x07) == 0x02)  // Row 2
        {
          if (kbd_key == '3')       joy1 |= 0x01;
          if (kbd_key == 'E')       joy1 |= 0x02;
          if (kbd_key == 'D')       joy1 |= 0x04;
          if (kbd_key == 'C')       joy1 |= 0x08;
          if (kbd_key == KBD_KEY_HOME) joy1 |= 0x10;            
          if (kbd_key == '/')       joy1 |= 0x20;
          if (kbd_key == ';')       joy1 |= 0x40;
          if (kbd_key == 'P')       joy1 |= 0x80;
        }

        if ((Port_PPI_C & 0x07) == 0x03)  // Row 3
        {
          if (kbd_key == '4')       joy1 |= 0x01;
          if (kbd_key == 'R')       joy1 |= 0x02;
          if (kbd_key == 'F')       joy1 |= 0x04;
          if (kbd_key == 'V')       joy1 |= 0x08;
          if (kbd_key == KBD_KEY_DEL) joy1 |= 0x10;            
          if (kbd_key == ':')       joy1 |= 0x40;
          if (kbd_key == 'O')       joy1 |= 0x80;
        }

        //  4   '5' 'T' 'G' 'B' --- DA  ']' '[' '^' --- --- ---
        //  5   '6' 'Y' 'H' 'N' --- LA  CR  --- YEN --- --- FNC
        //  6   '7' 'U' 'J' 'M' --- RA  UA  --- BRK GRP CTL SHF
          
        if ((Port_PPI_C & 0x07) == 0x04)  // Row 4
        {
          if (kbd_key == '5')       joy1 |= 0x01;
          if (kbd_key == 'T')       joy1 |= 0x02;
          if (kbd_key == 'G')       joy1 |= 0x04;
          if (kbd_key == 'B')       joy1 |= 0x08;

          if (kbd_key == KBD_KEY_DOWN) joy1 |= 0x20;
          if (kbd_key == ']')       joy1 |= 0x40;
          if (kbd_key == '[')       joy1 |= 0x80;
        }

        if ((Port_PPI_C & 0x07) == 0x05)  // Row 5
        {
          if (kbd_key == '6')       joy1 |= 0x01;
          if (kbd_key == 'Y')       joy1 |= 0x02;
          if (kbd_key == 'H')       joy1 |= 0x04;
          if (kbd_key == 'N')       joy1 |= 0x08;
          if (kbd_key == KBD_KEY_LEFT) joy1 |= 0x20;
          if (kbd_key == KBD_KEY_RET) joy1 |= 0x40;
        }

        if ((Port_PPI_C & 0x07) == 0x06)  // Row 6
        {
          if (kbd_key == '7')       joy1 |= 0x01;
          if (kbd_key == 'U')       joy1 |= 0x02;
          if (kbd_key == 'J')       joy1 |= 0x04;
          if (kbd_key == 'M')       joy1 |= 0x08;
          if (kbd_key == KBD_KEY_RIGHT) joy1 |= 0x20;
          if (kbd_key == KBD_KEY_UP) joy1 |= 0x40;
        }

        if ((Port_PPI_C & 0x07) == 0x07)  // Row 7 (joystick)
        {
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
        }
      }
      else // SG-1000 Mode... just joystick
      {
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
      }
      
      return (~joy1);
  }
  else if ((Port == 0xDD) || (Port == 0xC1)) // Port B
  {
      u8 joy2 = 0x00;
      if ((Port_PPI_C & 0x07) == 0x00)  // Row 0
      {
          if (kbd_key == '8')             joy2 |= 0x01;
      }

      if ((Port_PPI_C & 0x07) == 0x01)  // Row 1
      {
          if (kbd_key == '9')             joy2 |= 0x01;
      }
      
      if ((Port_PPI_C & 0x07) == 0x02)  // Row 2
      {
          if (kbd_key == '0')             joy2 |= 0x01;
      }

      if ((Port_PPI_C & 0x07) == 0x03)  // Row 3
      {
          if (kbd_key == '-')             joy2 |= 0x01;
      }

      if ((Port_PPI_C & 0x07) == 0x04)  // Row 4
      {
          if (kbd_key == '#')             joy2 |= 0x01;
      }

      if ((Port_PPI_C & 0x07) == 0x06)  // Row 6
      {
          if (kbd_key == KBD_KEY_BRK)     joy2 |= 0x01;
          if (key_shift)                  joy2 |= 0x04;
          if (kbd_key == KBD_KEY_SHIFT)   joy2 |= 0x04;
          if (kbd_key == KBD_KEY_CTRL)    joy2 |= 0x08;
      }
      if ((Port_PPI_C & 0x07) == 0x07)  // Row 7 (joystick)
      {
        //if (JoyState & JST_BLUE) joy2 |= 0x10;    // Reset (not sure this is used)
      }
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
    
    // ------------------------
    // Megacart Handling
    // ------------------------
    if (Port >= 0xE0) 
    {
        int game_no;
        
        if (romBankMask == 0x3F)
            game_no = (Value & 0x80) ? ((Value & 0x1f) | ((Value & 0x40) ? 0x20 : 0x00)) : 0x3F;
        else
            game_no = (Value & 0x1f) | (Value & 0xc0) >> 1;
        if (game_no == romBankMask)
        {
            memcpy(pColecoMem, romBuffer, 0x8000);        // And place it into the bottom ROM area of our SG-1000 / SC-3000
        }
        else
        {
            FILE* handle = fopen(lastAdamDataPath, "rb");
            if (handle != NULL) 
            {
                fseek(handle, (0x8000 * (u32)game_no), SEEK_SET);   // Seek to the 32K chunk we want to read in
                fread((void*) pColecoMem, 0x8000, 1, handle);       // Read 32K from that paged block
                fclose(handle);
            }
        }
        return;
    }

    if ((Port & 0xE0) == 0xA0)  // VDP Area
    {
        if ((Port & 1) == 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) { CPU.IRequest=INT_RST38; cpuirequest=Z80_IRQ_INT; }    // SG-1000 does not use NMI like Colecovision does...
    }
    else if (Port == 0x7E) sn76496W(Value, &sncol);
    else if (Port == 0x7F) sn76496W(Value, &sncol);
    else if ((Port == 0xDC) || (Port == 0xC0)) Port_PPI_A = Value;
    else if ((Port == 0xDD) || (Port == 0xC1)) Port_PPI_B = Value;
    else if ((Port == 0xDE) || (Port == 0xC2)) 
    {
        Port_PPI_C = Value;
    }
    else if ((Port == 0xDF) || (Port == 0xC3)) 
    {
        if (Value & 0x01)   // If PortC is input...
        {
            // If switching from output to input... save
            if ((Port_PPI_CTRL & 0x01) == 0) OldPortC = Port_PPI_C;
            Port_PPI_C |= 0x07;
        }
        else
        {
            // If switching from input to output... restore
            if ((Port_PPI_CTRL & 0x01) == 1) Port_PPI_C = OldPortC;
        }
        Port_PPI_CTRL = Value;  // Control Port
    }
}


// End of file
