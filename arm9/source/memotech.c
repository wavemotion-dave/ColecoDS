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

u8 memotech_magrom_present = 0;
u8 memotech_mtx_500_only = 0;
u8 memotech_lastMagROMPage = 0x00;

// ---------------------------------------------------------------------
// Memotech MTX IO Port Read - VDP, Joystick/Keyboard and Z80-CTC
// ---------------------------------------------------------------------
unsigned char cpu_readport_memotech(register unsigned short Port) 
{
  // MTX ports are 8-bit
  Port &= 0x00FF; 

  if (Port == 0x00) return IOBYTE;
  else if (Port >= 0x08 && Port <= 0x0B)      // Z80-CTC Area
  {
      return ctc_timer[Port-0x08];
  }
  else if ((Port == 0x01))  // VDP Area
  {
      return(RdData9918());
  }
  else if ((Port == 0x02))  // VDP Area
  {
      return(RdCtrl9918());
  }
  else if ((Port == 0x03))  // Per MEMO - drive 0x03
  {
      return(0x03);
  }
  else if ((Port == 0x05))
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
          // Player 1
          if (JoyState & JST_UP)    joy1 |= 0x01;
          if (JoyState & JST_DOWN)  joy1 |= 0x02;
          if (JoyState & JST_LEFT)  joy1 |= 0x04;
          if (JoyState & JST_RIGHT) joy1 |= 0x08;
          
          // Player 2
          if (JoyState & (JST_UP<<16))    joy1 |= 0x10;
          if (JoyState & (JST_DOWN<<16))  joy1 |= 0x20;
          if (JoyState & (JST_LEFT<<16))  joy1 |= 0x40;
          if (JoyState & (JST_RIGHT<<16)) joy1 |= 0x80;
      }          

      // -----------------------------------------------------------
      // We are at the top of the scan loop... if we have buffered 
      // keys, we insert them into the stream now...
      // -----------------------------------------------------------
      if (MTX_KBD_DRIVE == 0xFD)
      {
          if (key_shift_hold > 0) {key_shift = 1; key_shift_hold--;}
          if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
          {
              kbd_key = BufferedKeys[BufferedKeysReadIdx];
              BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
              if (kbd_key == KBD_KEY_SHIFT) key_shift_hold = 1;
          }
      }
      
      if ((JoyState == 0) && (kbd_key == 0) && (key_shift == 0) && (key_ctrl == 0) && (nds_key == 0)) return 0xFF;
      
      u8 scan_matrix = ~MTX_KBD_DRIVE & 0xFF;
      u8 key1 = 0x00;
      
      // For the full keyboard overlay... this is a bit of a hack for SHIFT and CTRL
      if ((last_special_key_dampen > 0) && (last_special_key_dampen != 20))
      {
          if (--last_special_key_dampen == 0)
          {
              last_special_key = 0;
              AffChaine(4,0,6, "    ");
          }
      }
      
      if (last_special_key == KBD_KEY_SHIFT) 
      { 
        AffChaine(4,0,6, "SHFT");
        key_shift = 1;
      }
      else if (last_special_key == KBD_KEY_CTRL)  
      {
        AffChaine(4,0,6, "CTRL");
        key_ctrl = 1;
      }
      
      if ((kbd_key != 0) && (kbd_key != KBD_KEY_SHIFT) && (kbd_key != KBD_KEY_CTRL) && (kbd_key != KBD_KEY_CODE) && (kbd_key != KBD_KEY_GRAPH))
      {
          if (last_special_key_dampen == 20) last_special_key_dampen = 19;    // Start the SHIFT/CONTROL countdown... this should be enough time for it to register
      }
      

      if (scan_matrix & 0x01)
      {
          if (kbd_key)
          {
              if (kbd_key == '1')           key1 |= 0x01;
              if (kbd_key == '3')           key1 |= 0x02;
              if (kbd_key == '5')           key1 |= 0x04;
              if (kbd_key == '7')           key1 |= 0x08;
              if (kbd_key == '9')           key1 |= 0x10;
              if (kbd_key == '-')           key1 |= 0x20;
              if (kbd_key == '\\')          key1 |= 0x40;
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_1))   key1 |= 0x01;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_3))   key1 |= 0x02;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_5))   key1 |= 0x04;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_7))   key1 |= 0x08;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_9))   key1 |= 0x10;
              }
          }
      }

      if (scan_matrix & 0x02)
      {
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_ESC)   key1 |= 0x01;
              if (kbd_key == '2')           key1 |= 0x02;
              if (kbd_key == '4')           key1 |= 0x04;
              if (kbd_key == '6')           key1 |= 0x08;
              if (kbd_key == '8')           key1 |= 0x10;
              if (kbd_key == '0')           key1 |= 0x20;
              if (kbd_key == '^')           key1 |= 0x40;
          }
          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_ESC)) key1 |= 0x01;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_2))   key1 |= 0x02;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_4))   key1 |= 0x04;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_6))   key1 |= 0x08;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_8))   key1 |= 0x10;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_0))   key1 |= 0x20;
              }
          }
      }
      
      if (scan_matrix & 0x04)
      {
          if (joy1 & 0x01)                  key1 |= 0x80;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_CTRL)  key1 |= 0x01;
              if (kbd_key == 'W')           key1 |= 0x02;
              if (kbd_key == 'R')           key1 |= 0x04;
              if (kbd_key == 'Y')           key1 |= 0x08;
              if (kbd_key == 'I')           key1 |= 0x10;
              if (kbd_key == 'P')           key1 |= 0x20;
              if (kbd_key == '[')           key1 |= 0x40;
              if (kbd_key == KBD_KEY_UP)    key1 |= 0x80;
          }          
          
          if (key_ctrl)                     key1 |= 0x01;
          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_CTRL)) key1 |= 0x01;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_W))   key1 |= 0x02;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_R))   key1 |= 0x04;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_Y))   key1 |= 0x08;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_I))   key1 |= 0x10;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_P))   key1 |= 0x20;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_UP))  key1 |= 0x80;
              }
          }
      }
      
      if (scan_matrix & 0x08)
      {
          if (joy1 & 0x04)                  key1 |= 0x80;
          if (kbd_key)
          {
              if (kbd_key == 'Q')           key1 |= 0x01;
              if (kbd_key == 'E')           key1 |= 0x02;
              if (kbd_key == 'T')           key1 |= 0x04;
              if (kbd_key == 'U')           key1 |= 0x08;
              if (kbd_key == 'O')           key1 |= 0x10;
              if (kbd_key == '@')           key1 |= 0x20;
              if (kbd_key == KBD_KEY_LEFT)  key1 |= 0x80;              
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_Q))   key1 |= 0x01;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_E))   key1 |= 0x02;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_T))   key1 |= 0x04;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_U))   key1 |= 0x08;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_O))   key1 |= 0x10;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_LEFT)) key1 |= 0x80;
              }
          }
      }
      
      
      if (scan_matrix & 0x10)
      {
          if (joy1 & 0x08)                  key1 |= 0x80;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_CAPS)  key1 |= 0x01;
              if (kbd_key == 'S')           key1 |= 0x02;
              if (kbd_key == 'F')           key1 |= 0x04;
              if (kbd_key == 'H')           key1 |= 0x08;
              if (kbd_key == 'K')           key1 |= 0x10;
              if (kbd_key == ';')           key1 |= 0x20;
              if (kbd_key == ']')           key1 |= 0x40;
              if (kbd_key == KBD_KEY_RIGHT) key1 |= 0x80;              
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_SHIFT))   key1 |= 0x01;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_S))       key1 |= 0x02;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F))       key1 |= 0x04;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_H))       key1 |= 0x08;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_K))       key1 |= 0x10;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_RIGHT))   key1 |= 0x80;
              }
          }
      }
      
      if (scan_matrix & 0x20)
      {
          if (JoyState & JST_FIRER)         key1 |= 0x80;    // HOME
          if (JoyState & JST_FIREL)         key1 |= 0x80;    // HOME
          if (kbd_key)
          {
              if (kbd_key == 'A')           key1 |= 0x01;
              if (kbd_key == 'D')           key1 |= 0x02;
              if (kbd_key == 'G')           key1 |= 0x04;
              if (kbd_key == 'J')           key1 |= 0x08;
              if (kbd_key == 'L')           key1 |= 0x10;
              if (kbd_key == ':')           key1 |= 0x20;
              if (kbd_key == KBD_KEY_RET)   key1 |= 0x40;
              if (kbd_key == KBD_KEY_HOME)  key1 |= 0x80;
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_A))   key1 |= 0x01;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_D))   key1 |= 0x02;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_G))   key1 |= 0x04;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_J))   key1 |= 0x08;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_L))   key1 |= 0x10;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_COLON))  key1 |= 0x20;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_RETURN)) key1 |= 0x40;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_HOME))   key1 |= 0x80;
              }
          }
      }

      if (scan_matrix & 0x40)
      {
          if (joy1 & 0x02)                  key1 |= 0x80;
          
          if (kbd_key)
          {
              if (kbd_key == 'X')           key1 |= 0x02;
              if (kbd_key == 'V')           key1 |= 0x04;
              if (kbd_key == 'N')           key1 |= 0x08;
              if (kbd_key == ',')           key1 |= 0x10;
              if (kbd_key == '/')           key1 |= 0x20;
              if (kbd_key == '-')           key1 |= 0x40;
              if (kbd_key == KBD_KEY_DOWN)  key1 |= 0x80;              
          }          
          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_X))     key1 |= 0x02;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_V))     key1 |= 0x04;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_N))     key1 |= 0x08;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_COMMA)) key1 |= 0x10;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_SLASH)) key1 |= 0x20;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_DOWN))  key1 |= 0x80;
              }
          }
          
          if (key_shift)                    key1 |= 0x01;    // SHIFT key
      }
      
      if (scan_matrix & 0x80)
      {
          //"B", "M", "Z", "C" and <space>  are the 2P "left" joystick
          if (joy1 & 0x10)                  key1 |= 0x04;
          if (joy1 & 0x20)                  key1 |= 0x08;
          if (joy1 & 0x40)                  key1 |= 0x01;
          if (joy1 & 0x80)                  key1 |= 0x02;
          
          if (kbd_key)
          {
              if (kbd_key == 'Z')           key1 |= 0x01;
              if (kbd_key == 'C')           key1 |= 0x02;
              if (kbd_key == 'B')           key1 |= 0x04;
              if (kbd_key == 'M')           key1 |= 0x08;
              if (kbd_key == '.')           key1 |= 0x10;
          }
          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_Z))   key1 |= 0x01;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_C))   key1 |= 0x02;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_B))   key1 |= 0x04;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_M))   key1 |= 0x08;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_PERIOD)) key1 |= 0x10;
              }
          }
      }
      
      return (~key1 & 0xFF);      
  }    
  else if ((Port == 0x06))
  {
      if (MTX_KBD_DRIVE == 0xFD)
      {
          u8 key1 = 0x00;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_BS)    key1 = 0x01;    // Backspace key on Memotech
              if (kbd_key == KBD_KEY_F5)    key1 = 0x02;    // F5
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F5))   key1 |= 0x02;
              }
          }
          return (~key1 & 0xFF);
      }
      else if (MTX_KBD_DRIVE == 0xFE)
      {
          u8 key1 = 0x00;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_STOP)  key1 = 0x01;    // BREAK key on Memotech
              if (kbd_key == KBD_KEY_F1)    key1 = 0x02;    // F1
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_CTRL))   key1 |= 0x01;
              }
              if (nds_key)
              {
                  for (u8 i=0; i<12; i++)
                  {
                      if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F1))   key1 |= 0x02;
                  }
              }
          }
          return (~key1 & 0xFF);
      }

      else if (MTX_KBD_DRIVE == 0xFB)
      {
          u8 key1 = 0x00;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_TAB)   key1 = 0x01;    // TAB
              if (kbd_key == KBD_KEY_F2)    key1 = 0x02;    // F2
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F2))   key1 |= 0x02;
              }
          }
          return (~key1 & 0xFF);
      }
      else if (MTX_KBD_DRIVE == 0xF7)
      {
          u8 key1 = 0x00;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_DEL)   key1 = 0x01;    // DEL
              if (kbd_key == KBD_KEY_F6)    key1 = 0x02;    // F6
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F6))   key1 |= 0x02;
              }
          }
          return (~key1 & 0xFF);
      }
      
      
      else if (MTX_KBD_DRIVE == 0xEF)
      {
          u8 key1 = 0x00;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_F7)   key1 = 0x02;    // F7
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F7))   key1 |= 0x02;
              }
          }
          return (~key1 & 0xFF);
      }
      else if (MTX_KBD_DRIVE == 0xDF)
      {
          u8 key1 = 0x00;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_F3)    key1 = 0x02;    // F3
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F3))   key1 |= 0x02;
              }
          }
          return (~key1 & 0xFF);
      }
      

      else if (MTX_KBD_DRIVE == 0xBF)
      {
          u8 key1 = 0x00;
          if (kbd_key)
          {
              if (kbd_key == KBD_KEY_F8)    key1 = 0x02;    // F8
          }          
          return (~key1 & 0xFF);
      }
      else if (MTX_KBD_DRIVE == 0x7F)
      {
          u8 key1 = 0x00;
          if (JoyState == JST_BLUE)          key1 = 0x01;    // Map the alternate 2 buttons to 'space' as some games make use of this as a 2nd button
          if (JoyState == JST_PURPLE)        key1 = 0x01;          
          if (kbd_key)
          {
              if (kbd_key == ' ')           key1 = 0x01;    // Space
              if (kbd_key == KBD_KEY_F4)    key1 = 0x02;    // F4
          }          
          if (nds_key)
          {
              for (u8 i=0; i<12; i++)
              {
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_SPACE))   key1 |= 0x01;
                  if ((nds_key & NDS_keyMap[i]) && (keyCoresp[myConfig.keymap[i]] == META_KBD_F4))      key1 |= 0x02;
              }
          }
          
          return (~key1 & 0xFF);
      }
  }

  // No such port
  return(NORAM);
}


// ------------------------------------------------------------------------------------
// Memotech MTX IO Port Write - Need to handle SN sound, KBD, VDP and the Z80-CTC chip
// ------------------------------------------------------------------------------------
void cpu_writeport_memotech(register unsigned short Port,register unsigned char Value) 
{
    // MTX ports are 8-bit
    Port &= 0x00FF;
    
    if (Port == 0x00)   // This is where the memory bank map magic happens for the MTX
    {
        IOBYTE = Value;
        if (lastIOBYTE != IOBYTE)
        {
            // -----------------------------------------------------------------------
            // We are using simplified logic for the MTX... this should provide
            // a simple 64K machine roughly the same as a Memotech MTX-512 
            // -----------------------------------------------------------------------
            if ((IOBYTE & 0x80) == 0)  // ROM Mode...
            {
                // -------------------------------------------------------------------
                // No matter the ROM paging, the basic mtx_os[] BIOS is present...
                // -------------------------------------------------------------------
                MemoryMap[0] = BIOS_Memory + 0x0000;    // Restore Memotech BIOS OS-A - this always lives in the bottom slot

                if ((IOBYTE & 0x70) == 0x00)            // Is BASIC ROM Enabled?
                {
                    MemoryMap[1] = BIOS_Memory + 0x2000;        // Restore Memotech BASIC
                }
                else if ((IOBYTE & 0x70) == 0x10)   // Is ASSEMBLY ROM Enabled?
                {
                    MemoryMap[1] = BIOS_Memory + 0x4000;         // Restore Memotech Assembly ROM
                }
                else if (((IOBYTE & 0x70) == 0x70) && memotech_magrom_present)   // ROM7 - MAGROM (if present)
                {
                    MemoryMap[1] = (u8 *)(ROM_Memory+0x2000);    // Copy MAGROM into memory
                }
                else                                // Nothing else is supported fill ROM above BIOS with 0xFF
                {
                    MemoryMap[1] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                }
                
                // ---------------------------------------------------
                // Now map the RAM based on the RAM Paging bits...
                // ---------------------------------------------------
                if ((IOBYTE & 0x0F) == 0x00)    // Page 0
                {
                    if (memotech_mtx_500_only)
                    {
                        MemoryMap[2] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
                        MemoryMap[3] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
                        memotech_RAM_start = 0x8000;                // Allow access to RAM above base memory
                }
                    else
                    {
                        MemoryMap[2] = (u8 *)RAM_Memory+0x4000; // The third   RAM block is mapped to 0x4000
                        MemoryMap[3] = (u8 *)RAM_Memory+0x6000; // The fourth  RAM block is mapped to 0x6000
                        memotech_RAM_start = 0x4000;                // Allow access to RAM above base memory
                    }
                    MemoryMap[4] = (u8 *)RAM_Memory+0x8000;     // The fifth   RAM block is mapped to 0x8000
                    MemoryMap[5] = (u8 *)RAM_Memory+0xA000;     // The sixth   RAM block is mapped to 0xA000
                    MemoryMap[6] = (u8 *)RAM_Memory+0xC000;     // The seventh RAM block is mapped to 0xC000 - Common Area
                    MemoryMap[7] = (u8 *)RAM_Memory+0xE000;     // The eighth  RAM block is mapped to 0xE000 - Common Area
                }
                else if ((IOBYTE & 0x0F) == 0x01)   // Page 1
                {
                    if (memotech_magrom_present)
                    {
                        MemoryMap[2] = (u8 *)(ROM_Memory+(0x4000 * memotech_lastMagROMPage));
                        MemoryMap[3] = (u8 *)(ROM_Memory+(0x4000 * memotech_lastMagROMPage))+0x2000;
                    }
                    else                
                    {
                        MemoryMap[2] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
                        MemoryMap[3] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
                    }
                    
                    if (memotech_mtx_500_only)
                    {
                        MemoryMap[4] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
                        MemoryMap[5] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
                        memotech_RAM_start = 0xC000;                // Allow access to RAM above base memory
                    }
                    else
                    {
                        MemoryMap[4] = (u8 *)RAM_Memory+0x0000;     // We map the first block here
                        MemoryMap[5] = (u8 *)RAM_Memory+0x2000;     // We map the second block here
                        memotech_RAM_start = 0x8000;                // Allow access to RAM above base memory
                    }
                    MemoryMap[6] = (u8 *)RAM_Memory+0xC000;     // The seventh RAM block is mapped to 0xC000 - Common Area
                    MemoryMap[7] = (u8 *)RAM_Memory+0xE000;     // The eighth  RAM block is mapped to 0xE000 - Common Area
                }
                else    // Page 2-15
                {
                    MemoryMap[0] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[1] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    if (memotech_magrom_present)
                    {
                        MemoryMap[2] = (u8 *)(ROM_Memory+(0x4000 * memotech_lastMagROMPage));
                        MemoryMap[3] = (u8 *)(ROM_Memory+(0x4000 * memotech_lastMagROMPage))+0x2000;
                    }
                    else                
                    {
                        MemoryMap[2] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
                        MemoryMap[3] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
                    }
                    MemoryMap[4] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[5] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[6] = (u8 *)RAM_Memory+0xC000;      // Common memory area
                    MemoryMap[7] = (u8 *)RAM_Memory+0xE000;      // Common memory area
                    memotech_RAM_start = 0xC000;                 // Just the common RAM enabled
                }
            }
            else  // RAM Mode
            {
                if ((IOBYTE & 0x0F) == 0x00)   // All 64K enabled
                {
                    if (memotech_mtx_500_only)
                    {
                        MemoryMap[0] = (u8 *)BIOS_Memory+0xC000;    // Just 0xFF out here...
                        MemoryMap[1] = (u8 *)BIOS_Memory+0xC000;    // Just 0xFF out here...
                        MemoryMap[2] = (u8 *)BIOS_Memory+0xC000;    // Just 0xFF out here...
                        MemoryMap[3] = (u8 *)BIOS_Memory+0xC000;    // Just 0xFF out here...
                        memotech_RAM_start = 0x0000;                // We're emulating a 64K machine
                    }
                    else
                    {
                        MemoryMap[0] = (u8 *)RAM_Memory+0x0000;     // The first   RAM block is mapped to 0x0000
                        MemoryMap[1] = (u8 *)RAM_Memory+0x2000;     // The second  RAM block is mapped to 0x2000
                        MemoryMap[2] = (u8 *)RAM_Memory+0x4000;     // The third   RAM block is mapped to 0x4000
                        MemoryMap[3] = (u8 *)RAM_Memory+0x6000;     // The fourth  RAM block is mapped to 0x6000
                        memotech_RAM_start = 0x0000;                // We're emulating a 64K machine
                    }
                    MemoryMap[4] = (u8 *)RAM_Memory+0x8000;     // The fifth   RAM block is mapped to 0x8000
                    MemoryMap[5] = (u8 *)RAM_Memory+0xA000;     // The sixth   RAM block is mapped to 0xA000
                    MemoryMap[6] = (u8 *)RAM_Memory+0xC000;     // The seventh RAM block is mapped to 0xC000 - Common Area
                    MemoryMap[7] = (u8 *)RAM_Memory+0xE000;     // The eighth  RAM block is mapped to 0xE000 - Common Area
                }
                else    // Only the common RAM is available
                {
                    MemoryMap[0] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[1] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[2] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[3] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[4] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[5] = (u8 *)(BIOS_Memory+0xC000);   // Just 0xFF out here...
                    MemoryMap[6] = (u8 *)RAM_Memory+0xC000;      // Common memory area
                    MemoryMap[7] = (u8 *)RAM_Memory+0xE000;      // Common memory area
                    memotech_RAM_start = 0xC000;                 // Just the common RAM enabled
                }
            }
            
            lastIOBYTE = IOBYTE;
        }
    }
    // ----------------------------------------------------------------------
    // Z80-CTC Area
    // This is only a partial implementation of the CTC logic - just enough
    // to handle the VDP and Sound Generation and very little else. This is
    // NOT accurate emulation - but it's good enough to render the Memotech
    // games as playable in this emulator.
    // ----------------------------------------------------------------------
    else if (Port >= 0x08 && Port <= 0x0B)
    {
        Port &= 0x03;
        if (ctc_latch[Port])    // If latched, we now have the countdown timer value
        {
            ctc_time[Port] = Value;     // Latch the time constant and compute the countdown timer directly below.
            ctc_timer[Port] = ((((ctc_control[Port] & 0x20) ? 256 : 16) * (ctc_time[Port] ? ctc_time[Port]:256)) / MTX_CTC_SOUND_DIV) + 1;
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
                    ctc_vector[0] = (Value & 0xf8) | 0;     // VDP Interrupt
                    ctc_vector[1] = (Value & 0xf8) | 2;     // 
                    ctc_vector[2] = (Value & 0xf8) | 4;     // 
                    ctc_vector[3] = (Value & 0xf8) | 6;     // 
                    vdp_int_source = ctc_vector[0];         // When the VDP interrupts the CPU, it's channel 0 on the CTC
                }
            }
        }
    }
    else if ((Port == 0x01) || (Port == 0x02))  // VDP Area
    {
        if ((Port & 1) != 0) WrData9918(Value);
        else if (WrCtrl9918(Value)) CPU.IRequest=vdp_int_source;    // Memotech MTX must get vector from the Z80-CTC. Only the CZ80 core works with this.
    }
    else if (Port == 0x05) MTX_KBD_DRIVE = Value;
    else if (Port == 0x06) sn76496W(Value, &sncol);
    else if (Port == 0xFB || Port == 0xFF) // MAGROM paging
    {
        if (memotech_RAM_start >= 0x8000)
        {
            memotech_lastMagROMPage = Value;
            MemoryMap[2] = (u8 *)(ROM_Memory+(0x4000 * memotech_lastMagROMPage));
            MemoryMap[3] = (u8 *)(ROM_Memory+(0x4000 * memotech_lastMagROMPage))+0x2000;
        }
    }
}


void memotech_restore_bios(void)
{
    memset(BIOS_Memory, 0xFF, 0x10000);
    memcpy(BIOS_Memory+0x0000, mtx_os,    0x2000);
    memcpy(BIOS_Memory+0x2000, mtx_basic, 0x2000);
    memcpy(BIOS_Memory+0x4000, mtx_assem, 0x2000);
    MemoryMap[0] = BIOS_Memory+0x0000;          // OS-A
    MemoryMap[1] = BIOS_Memory+0x2000;          // BASIC

    if (memotech_mtx_500_only)
    {
        MemoryMap[2] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
        MemoryMap[3] = (u8 *)(BIOS_Memory+0xC000);  // Just 0xFF out here...
        memotech_RAM_start = 0x8000;                // Allow access to RAM above base memory
    }
    else
    {
        MemoryMap[2] = (u8 *)RAM_Memory+0x4000;     // The third   RAM block is mapped to 0x4000
        MemoryMap[3] = (u8 *)RAM_Memory+0x6000;     // The fourth  RAM block is mapped to 0x6000
        memotech_RAM_start = 0x4000;                // Allow access to RAM above base memory
    }
        
    MemoryMap[4] = (u8 *)RAM_Memory+0x8000;     // The fifth   RAM block is mapped to 0x8000
    MemoryMap[5] = (u8 *)RAM_Memory+0xA000;     // The sixth   RAM block is mapped to 0xA000
    MemoryMap[6] = (u8 *)RAM_Memory+0xC000;     // The seventh RAM block is mapped to 0xC000 - Common Area
    MemoryMap[7] = (u8 *)RAM_Memory+0xE000;     // The eighth  RAM block is mapped to 0xE000 - Common Area
    memotech_lastMagROMPage = 0x00;
}

// ---------------------------------------------------------
// The Memotech MTX has CTC plus some memory handling stuff
// ---------------------------------------------------------
void memotech_reset(void)
{
    if (memotech_mode)
    {
        // Reset the Z80-CTC stuff...
        memset(ctc_control, 0x00, 4);       // Set Software Reset Bit (freeze)
        memset(ctc_time, 0x00, 4);          // No time value set
        memset(ctc_timer, 0x00, 8);         // No timer value set
        memset(ctc_vector, 0x00, 4);        // No vectors set
        memset(ctc_latch, 0x00, 4);         // No latch set
        vdp_int_source = INT_NONE;          // No IRQ set to start (CRC writes this)

        IOBYTE = 0x00;                      // Used for ROM-RAM bankswitch
        MTX_KBD_DRIVE = 0x00;               // Used to determine which Keybaord scanrow to use
        lastIOBYTE = 99;                    // To save time
        tape_pos = 0;                       // Start at the front of a cassette

        mtx_os[0x0aae] = 0xed;              // Patch BIOS for .MTX tape access
        mtx_os[0x0aaf] = 0xfe;              // ..
        mtx_os[0x0ab0] = 0xc9;              // ..
        
        memotech_magrom_present = (((file_crc == 0xe3f495c4) || (file_crc == 0x98240ee9) || (file_crc == 0xcbc13a32)) ? 1:0);       // The MAGROM 1.05 and 1.05a and Magrom v2
        memotech_mtx_500_only = (((file_crc == 0x9a0461db) ||               // Duckybod
                                  (file_crc == 0xd1cd3e62) ||               // Soldier Sam
                                  (file_crc == 0x93556570) ||               // TNT Tim                                  
                                  (file_crc == 0xa1d594fb)) ? 1:0);         // Dragon's Ring won't run on MTX512

        // Get the Memotech BIOS files ready...
        memotech_restore_bios();
    }
}

void MTX_HandleCassette(register Z80 *r)
{
    // Memotech MTX Tape Patch
    if ( r->PC.W-2 == 0x0AAE )
    {
        word base   = r->HL.W;
        word length = r->DE.W;

        if ( RAM_Memory[0xfd68] == 0 )
        /* SAVE */
        {
            // Not supported yet...
        }
        else if ( RAM_Memory[0xfd67] != 0 )
        /* VERIFY.
           Normally, if verification fails, the MTX BASIC ROM
           stops the tape, cleans up and does rst 0x28.
           That rst instruction is at 0x0adb. */
        {
            if ( base == 0xc011 && length == 18 )
            {
                tape_pos = 0;
            }
            tape_pos += length;
        }
        else
        {
            /* LOAD */
            if ( base == 0xc011 && length == 18 )
            /* Load header, so read whole file */
            {
                // File is in ROM_Memory[]
                tape_pos = 0;
            }

            /* Then return chunks as requested */
            for (u16 i=0; i<length; i++)
            {
                extern void cpu_writemem16(u8, u16);
                cpu_writemem16(ROM_Memory[tape_pos++], base+i);
            }
        }

        cpu_writeport_memotech(0x08, 0x00);
        cpu_writeport_memotech(0x08, 0xf0);
        cpu_writeport_memotech(0x08, 0x03);
        cpu_writeport_memotech(0x09, 0x03);
        cpu_writeport_memotech(0x0A, 0x03);
        cpu_writeport_memotech(0x0B, 0x03);

        RdCtrl9918();

        /* Then re-enables the video interrupt */
        cpu_writeport_memotech(0x08, 0xa5);
        cpu_writeport_memotech(0x08, 0x7d);    
    }    
}

void memotech_launch_run_file(void)
{
      vdp_int_source = INT_NONE;    // Needed when we wipe and run in this mode

      if (myConfig.memWipe == 2)    // Full MTX Memory Wipe and RAM mode enable
      {
        memset(RAM_Memory, 0x00, 0x10000);
        cpu_writeport_memotech(0x00, 0x80);                    
      }
      else if (myConfig.memWipe == 3)    // Random MSX Memory Wipe
      {
          for (int i=0; i< 0xC000; i++) RAM_Memory[0x4000+i] = (rand() & 0xFF);   // This pattern tends to make most things start up properly...
      }
  
      if (memotech_mode == 3)   // .COM file
      {
            MemoryMap[0] = (u8 *)RAM_Memory+0x0000;     // The first   RAM block is mapped to 0x0000
            MemoryMap[1] = (u8 *)RAM_Memory+0x2000;     // The second  RAM block is mapped to 0x2000
            MemoryMap[2] = (u8 *)RAM_Memory+0x4000;     // The third   RAM block is mapped to 0x4000
            MemoryMap[3] = (u8 *)RAM_Memory+0x6000;     // The fourth  RAM block is mapped to 0x6000
            MemoryMap[4] = (u8 *)RAM_Memory+0x8000;     // The fifth   RAM block is mapped to 0x8000
            MemoryMap[5] = (u8 *)RAM_Memory+0xA000;     // The sixth   RAM block is mapped to 0xA000
            MemoryMap[6] = (u8 *)RAM_Memory+0xC000;     // The seventh RAM block is mapped to 0xC000 - Common Area
            MemoryMap[7] = (u8 *)RAM_Memory+0xE000;     // The eighth  RAM block is mapped to 0xE000 - Common Area
            memotech_RAM_start = 0x0000;                // We're emulating a 64K machine
            memcpy(RAM_Memory+0x100, ROM_Memory, 0xFF00);          
            CPU.PC.W = 0x100;
      }
      else  // Must be .RUN fule
      {
          RAM_Memory[0x3627] = 0xd3;
          RAM_Memory[0x3628] = 0x05;
          CPU.IFF &= 0xFE;   // Disable Interrupts
          u16 mtx_start = (ROM_Memory[1] << 8) | ROM_Memory[0];
          u16 mtx_len   = (ROM_Memory[3] << 8) | ROM_Memory[2];
          u16 idx=4;
          for (int i=mtx_start; i < (mtx_start+mtx_len); i++)
          {
              RAM_Memory[i] = ROM_Memory[idx++];
          }
          CPU.PC.W = mtx_start;
      }
      RdCtrl9918();
}
// End of file
