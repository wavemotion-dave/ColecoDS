// =====================================================================================
// Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
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
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "printf.h"

u8 portFE           __attribute__((section(".dtcm"))) = 0x00;
u8 zx_AY_enabled    __attribute__((section(".dtcm"))) = 0;
int speccy_line     __attribute__((section(".dtcm"))) = 0;

u8 zx_special_key = 0;
int last_z80_size = 0;

// ---------------------------------------
// Speculator - Simple translation layer
// ---------------------------------------

unsigned char cpu_readport_speccy(register unsigned short Port) 
{
    static u8 bNonSpecialKeyWasPressed = 0;
    if ((Port & 0xFF) == 0xFE)
    {
        word inv = ~Port;
        u8 key = (portFE & 0x18) ? 0x00:0x40;
        if (inv & 0x0800) // 12345 row
        {
            if (JoyState == JST_1)      key |= 0x01; // 1
            if (JoyState == JST_2)      key |= 0x02; // 2
            if (JoyState == JST_3)      key |= 0x04; // 3
            if (JoyState == JST_4)      key |= 0x08; // 4
            if (JoyState == JST_5)      key |= 0x10; // 5
            
            if (keysCurrent() & KEY_SELECT) key |= 0x01; // 1
            
            if (kbd_key)
            {
                if (kbd_key == '1')           key  |= 0x01;
                if (kbd_key == '2')           key  |= 0x02;
                if (kbd_key == '3')           key  |= 0x04;
                if (kbd_key == '4')           key  |= 0x08;
                if (kbd_key == '5')           key  |= 0x10;
            }            
            
        }
        
        if (inv & 0x1000) // 09876 row
        {   
            if (keysCurrent() & KEY_START) key |= 0x01; // 0
            
            if (JoyState == JST_0)      key |= 0x01; // 0
            if (JoyState == JST_6)      key |= 0x10; // 6
            
            if (kbd_key)
            {
                if (kbd_key == '0')           key  |= 0x01;
                if (kbd_key == KBD_KEY_BS)    key  |= 0x01; // Backspace is SHIFT-0
                if (kbd_key == '9')           key  |= 0x02;
                if (kbd_key == '8')           key  |= 0x04;
                if (kbd_key == '7')           key  |= 0x08;
                if (kbd_key == '6')           key  |= 0x10;
            }            
        }

        if (inv & 0x4000) // ENTER LKJH row
        {
            if (JoyState == JST_POUND)  key |= 0x01; // ENTER
            if (JoyState == JST_7)      key |= 0x08; // J
            if (JoyState == JST_8)      key |= 0x04; // K
            if (kbd_key)
            {
                if (kbd_key == KBD_KEY_RET)   key  |= 0x01;
                if (kbd_key == 'L')           key  |= 0x02;
                if (kbd_key == 'K')           key  |= 0x04;
                if (kbd_key == 'J')           key  |= 0x08;
                if (kbd_key == 'H')           key  |= 0x10;
            }            
        }

        if (inv & 0x8000) // SPACE SYMBOL MNB
        {
            if (keysCurrent() & KEY_R) key |= 0x08;  // N
            if (zx_special_key == 2)   key |= 0x02;  // Symbol was previously pressed
            
            if (kbd_key)
            {
                if (kbd_key == ' ')           key  |= 0x01;
                if (kbd_key == ',')          {key  |= 0x02; zx_special_key = 2;}
                if (kbd_key == 'M')           key  |= 0x04;
                if (kbd_key == 'N')           key  |= 0x08;
                if (kbd_key == 'B')           key  |= 0x10;
            }            
        }

        if (inv & 0x0200) // ASDFG
        {
            if (JoyState == JST_STAR)         key |= 0x02; // S
            if (kbd_key)
            {
                if (kbd_key == 'A')           key  |= 0x01;
                if (kbd_key == 'S')           key  |= 0x02;
                if (kbd_key == 'D')           key  |= 0x04;
                if (kbd_key == 'F')           key  |= 0x08;
                if (kbd_key == 'G')           key  |= 0x10;
            }            
        }
        
        if (inv & 0x2000) // POIUY
        {
            if (keysCurrent() & KEY_L) key |= 0x10; // Y

            if (kbd_key)
            {
                if (kbd_key == 'P')           key  |= 0x01;
                if (kbd_key == 'O')           key  |= 0x02;
                if (kbd_key == 'I')           key  |= 0x04;
                if (kbd_key == 'U')           key  |= 0x08;
                if (kbd_key == 'Y')           key  |= 0x10;
            }            
        }
        
        if (inv & 0x0100) // Shift, ZXCV row
        {
            if (JoyState == JST_9)       key |= 0x02; // Z
            if (JoyState & JST_FIRER)    key |= 0x02; // NDS B button is Z
            if (JoyState == JST_BLUE)    key |= 0x04; // NDS Y button is X
            if (JoyState == JST_PURPLE)  key |= 0x08; // NDS X button is C
            if (zx_special_key == 1)     key |= 0x01; // Shift was previously pressed
            if (kbd_key)
            {
                if (kbd_key == KBD_KEY_BS)    key  |= 0x01; // Backspace is SHIFT-9
                if (kbd_key == KBD_KEY_SHIFT){key  |= 0x01; zx_special_key = 1;}
                if (kbd_key == '.')          {key  |= 0x01; zx_special_key = 1;}
                if (kbd_key == 'Z')           key  |= 0x02;
                if (kbd_key == 'X')           key  |= 0x04;
                if (kbd_key == 'C')           key  |= 0x08;
                if (kbd_key == 'V')           key  |= 0x10;
            }            
        }
        
        if (inv & 0x0400) // QWERT row
        {
            if (kbd_key)
            {
                if (kbd_key == 'Q')           key  |= 0x01;
                if (kbd_key == 'W')           key  |= 0x02;
                if (kbd_key == 'E')           key  |= 0x04;
                if (kbd_key == 'R')           key  |= 0x08;
                if (kbd_key == 'T')           key  |= 0x10;
            }            
        }
        
        if (((kbd_key >= 'A') && (kbd_key <= 'Z')) || ((kbd_key >= '0') && (kbd_key <= '9')))
        {
            bNonSpecialKeyWasPressed = 1;
        }
        else
        {
            if (bNonSpecialKeyWasPressed)
            {
                zx_special_key = 0;
                bNonSpecialKeyWasPressed = 0;
            }        
        }
        
        return (u8)~key;
    }
    else if ((Port & 0x3F) == 0x1F)  // Kempston Joystick interface... (only A5 driven low)
    {
        u8 joy1 = 0x00;
        if (keysCurrent() & KEY_X) joy1 |= 0x08; // X button is also UP
        
        if (JoyState & JST_FIREL) joy1 |= 0x10;
        if (JoyState & JST_UP)    joy1 |= 0x08;
        if (JoyState & JST_DOWN)  joy1 |= 0x04;
        if (JoyState & JST_LEFT)  joy1 |= 0x02;
        if (JoyState & JST_RIGHT) joy1 |= 0x01;
        return joy1;
    }
    else if (Port == 0xfffd) // AY input
    {
        return ay38910DataR(&myAY);
    }
    else debug[6] = Port;

    return 0xFF;  // Not emulating the 'floating bus' - only a few games are known to need it
    
}

//OUT (0xfffd) - Select a register 0-14.
//IN  (0xfffd) - Read the value of the selected register.
//OUT (0xbffd) - Write to the selected register.
void cpu_writeport_speccy(register unsigned short Port,register unsigned char Value) 
{
    // Port FE is our beeper value...
    if ((Port & 0xFF) == 0xFE)
    {
        portFE = Value;
    } 
    else if (Port == 0xfffd) // AY Register Select
    {
        ay38910IndexW(Value&0xF, &myAY);
        zx_AY_enabled = 1;
    }    
    else if (Port == 0xbffd) // AY Data Write
    {
        ay38910DataW(Value, &myAY);
    }    
    else
    {
        debug[7] = Port;
    }
}

u32 flash_timer=0;
u8 bFlash = 0;

ITCM_CODE void speccy_render_screen(void)
{
    u8 pixelOut[8];
    u32 *vidBuf = (u32*) (0x06000000);    // Video buffer... write 32-bits at a time
    
    if (++flash_timer > 16) {flash_timer=0; bFlash ^= 1;} // Same timing as real ULA - 16 frames on and 16 frames off

    if (!isDSiMode() && (flash_timer & 1)) return; // For DS-Lite/Phat, we draw every other frame...
    
    for (int y=0; y<192; y++)
    {
        u8 *attrPtr = &RAM_Memory[0x5800 + ((y/8)*32)];
        word offset = 0 | ((y&0x07) << 8) | ((y&0x38) << 2) | ((y&0xC0) << 5);
        for (int x=0; x<32; x++)
        {
            u8 attr = *attrPtr++;
            u8 pixels = RAM_Memory[0x4000 + offset++];
            
            u8 *pixelPtr = pixelOut;
            for (int j=0x80; j != 0x00; j=j>>1)
            {
                if ((attr & 0x80) && bFlash)
                {
                    if (pixels & j) *pixelPtr = (((attr>>3) & 0x07) << 1) | ((attr & 0x40) ? 1:0); else *pixelPtr = ((attr & 0x07) << 1) | ((attr & 0x40) ? 1:0); pixelPtr++;
                }
                else
                {
                    if (pixels & j) *pixelPtr = ((attr & 0x07) << 1) | ((attr & 0x40) ? 1:0); else *pixelPtr = (((attr>>3) & 0x07) << 1) | ((attr & 0x40) ? 1:0); pixelPtr++;
                }
            }
            u32 *ptr32 = (u32*) pixelOut;
            *vidBuf++ = *ptr32++;
            *vidBuf++ = *ptr32;
        }
    }
}

// Assumes .z80 file is in ROM_Memory[]
void speccy_decompress_z80(int romSize)
{
    u8 m_isCompressed = 1;
    int offset = 0; // Current offset into memory
    
    last_z80_size = romSize;
    
    if (romSize == (16*1024)) // Assume this is a diagnostic ROM of some kind
    {
        memcpy(RAM_Memory, ROM_Memory, romSize);   // Load diagnostics ROM into place
        speccy_mode = 3;                           // Force PC to 0x0000
        return;
    }
    
    memcpy(RAM_Memory, SpectrumBios, 0x4000);   // Load BIOS into place

    if (speccy_mode == 2) // SNA snapshot 
    {
        memcpy(RAM_Memory + 0x4000, ROM_Memory+27, 0xC000);
        return;
    }
    
    for (int i = 30; i < romSize; i++)
    {
        if (offset > 0xC000)
        {
            break;
        }
        if (ROM_Memory[i] == 0x00 && ROM_Memory[i + 1] == 0xED && ROM_Memory[i + 2] == 0xED && ROM_Memory[i + 3] == 0x00)
        {
            break;
        }
 
        if (i < romSize - 4)
        {
            if (ROM_Memory[i] == 0xED && ROM_Memory[i + 1] == 0xED && m_isCompressed)
            {
                i += 2;
                int repeat = ROM_Memory[i++];
                byte value = ROM_Memory[i];
                for (int j = 0; j < repeat; j++)
                {
                    RAM_Memory[0x4000 + offset] = value;
                    offset++;
                }
            }
            else
            {
                RAM_Memory[0x4000 + offset] = ROM_Memory[i];
                offset++;
            }
        }
        else
        {
            RAM_Memory[0x4000 + offset] = ROM_Memory[i];
            offset++;
        }
    }
    
    
}

//    0       1       A register
//    1       1       F register
//    2       2       BC register pair (LSB, i.e.  C, first)
//    4       2       HL register pair
//    6       2       Program counter
//    8       2       Stack pointer
void speccy_reset(void)
{
    if (speccy_mode)
    {
        portFE = 0x00;
        speccy_line = 0;
        zx_AY_enabled = 0;
        zx_special_key = 0;
        
        speccy_decompress_z80(last_z80_size);
        
        if (speccy_mode == 2)
        {
            CPU.I = ROM_Memory[0];
            
            CPU.HL1.B.l = ROM_Memory[1];
            CPU.HL1.B.h = ROM_Memory[2];

            CPU.DE1.B.l = ROM_Memory[3];
            CPU.DE1.B.h = ROM_Memory[4];

            CPU.BC1.B.l = ROM_Memory[5];
            CPU.BC1.B.h = ROM_Memory[6];

            CPU.AF1.B.l = ROM_Memory[7];
            CPU.AF1.B.h = ROM_Memory[8];
            
            CPU.HL.B.l = ROM_Memory[9];
            CPU.HL.B.h = ROM_Memory[10];

            CPU.DE.B.l = ROM_Memory[11];
            CPU.DE.B.h = ROM_Memory[12];

            CPU.BC.B.l = ROM_Memory[13];
            CPU.BC.B.h = ROM_Memory[14];

            CPU.IY.B.l = ROM_Memory[15];
            CPU.IY.B.h = ROM_Memory[16];

            CPU.IX.B.l = ROM_Memory[17];
            CPU.IX.B.h = ROM_Memory[18];
            
            CPU.IFF     = (ROM_Memory[19] ? (IFF_2|IFF_EI) : 0x00);
            CPU.IFF    |= ((ROM_Memory[25] & 3) == 1 ? IFF_IM1 : IFF_IM2);
            
            CPU.R      = ROM_Memory[20];

            CPU.AF.B.l = ROM_Memory[21];
            CPU.AF.B.h = ROM_Memory[22];
            
            CPU.SP.B.l = ROM_Memory[23];
            CPU.SP.B.h = ROM_Memory[24];
            
            // M_RET
            CPU.PC.B.l=RAM_Memory[CPU.SP.W++]; 
            CPU.PC.B.h=RAM_Memory[CPU.SP.W++];
        }
        else if (speccy_mode == 3) // Diagnostic ROM
        {
            CPU.PC.W     = 0x0000;
            CPU.SP.W     = 0xF000;
            CPU.AF.W     = 0x0000;
            CPU.BC.W     = 0x0000;
            CPU.DE.W     = 0x0000;
            CPU.HL.W     = 0x0000;
            CPU.AF1.W    = 0x0000;
            CPU.BC1.W    = 0x0000;
            CPU.DE1.W    = 0x0000;
            CPU.HL1.W    = 0x0000;
            CPU.IX.W     = 0x0000;
            CPU.IY.W     = 0x0000;
            CPU.I        = 0x00;
            CPU.R        = 0x00;
            CPU.R_HighBit= 0x00;
            CPU.IFF      = 0x00;
        }
        else
        {
            CPU.AF.B.h = ROM_Memory[0]; //A
            CPU.AF.B.l = ROM_Memory[1]; //F
            
            CPU.BC.B.l = ROM_Memory[2]; //C
            CPU.BC.B.h = ROM_Memory[3]; //B

            CPU.HL.B.l = ROM_Memory[4]; //L
            CPU.HL.B.h = ROM_Memory[5]; //H
             
            CPU.PC.B.l = ROM_Memory[6]; // PC low byte
            CPU.PC.B.h = ROM_Memory[7]; // PC high byte
            
            CPU.SP.B.l = ROM_Memory[8]; // SP low byte
            CPU.SP.B.h = ROM_Memory[9]; // SP high byte
            
            CPU.I   = ROM_Memory[10];
            CPU.R   = ROM_Memory[11];
            CPU.R_HighBit = (ROM_Memory[12] & 1 ? 0x80:0x00);
           
            CPU.DE.B.l  = ROM_Memory[13];
            CPU.DE.B.h  = ROM_Memory[14];

            CPU.BC1.B.l = ROM_Memory[15];
            CPU.BC1.B.h = ROM_Memory[16];

            CPU.DE1.B.l = ROM_Memory[17];
            CPU.DE1.B.h = ROM_Memory[18];

            CPU.HL1.B.l = ROM_Memory[19];
            CPU.HL1.B.h = ROM_Memory[20];

            CPU.AF1.B.h = ROM_Memory[21];
            CPU.AF1.B.l = ROM_Memory[22];
            
            CPU.IY.B.l  = ROM_Memory[23];
            CPU.IY.B.h  = ROM_Memory[24];
            
            CPU.IX.B.l  = ROM_Memory[25];
            CPU.IX.B.h  = ROM_Memory[26];
            
            CPU.IFF     = (ROM_Memory[27] ? IFF_1 : 0x00);
            CPU.IFF    |= (ROM_Memory[28] ? IFF_2 : 0x00);
            CPU.IFF    |= ((ROM_Memory[29] & 3) == 1 ? IFF_IM1 : IFF_IM2);
        }
        
        CPU.IBackup  = 224;
        CPU.IRequest = INT_NONE;
        CPU.User     = 0;
        CPU.Trace    = 0;
        CPU.Trap     = 0;
        CPU.TrapBadOps = 1;
        CPU.IAutoReset = 1;        
        
        myConfig.soundDriver = SND_DRV_BEEPER; // By default we're in beeper mode... plus mix in AY if available
    }
}

void speccy_run(void)
{
  if (++CurLine >= tms_num_lines) CurLine=0;
  
  // Execute 1 scanline worth of CPU instructions

  CPU.CycleDeficit = ExecZ80(224>>2);
  if (CurLine & 3) processDirectBeeper(zx_AY_enabled);
  
  CPU.CycleDeficit += ExecZ80(224>>2);
  processDirectBeeper(zx_AY_enabled);
  
  CPU.CycleDeficit += ExecZ80(224>>2);
  processDirectBeeper(zx_AY_enabled);
  
  ExecZ80((224>>2)+CPU.CycleDeficit);
  processDirectBeeper(zx_AY_enabled);

  if (CurLine == tms_end_line)
  {
      speccy_render_screen();
      CPU.IRequest = INT_RST38;
  }
  
  // Generate an interrupt if called for...
  if(CPU.IRequest!=INT_NONE)
  {
      IntZ80(&CPU, CPU.IRequest);
      CPU.User++;   // Track Interrupt Requests
  } 
}

// End of file
