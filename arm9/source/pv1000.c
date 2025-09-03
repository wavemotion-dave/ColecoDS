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

#include "colecoDS.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "printf.h"

// ======================================================================================
// The best information about the PV-1000 memory map, registers, interrupt handling 
// and sound handling come from this site: https://obscure.nesdev.org/wiki/Casio_PV-1000
// ======================================================================================

uint8_t  bg[192][256];
uint8_t* pv1000_vram = RAM_Memory+0xB800;
uint8_t* pv1000_pcg = RAM_Memory+0xBC00;
uint8_t* pv1000_pattern = RAM_Memory;
uint8_t  pv1000_force_pattern = 0;
uint8_t  pv1000_column = 0x00;
uint8_t  pv1000_status = 0x00;
uint8_t  pv1000_enable = 0x00;
uint16_t pv1000_scanline = 0;
uint8_t  pv1000_bg_color = 0x00;
uint16_t pv1000_freqA = 0;
uint16_t pv1000_freqB = 0;
uint16_t pv1000_freqC = 0;

u8 pv1000_palette[8*3] = {
  0x00,0x00,0x00,   // Black
  0xFF,0x00,0x00,   // Red
  0x00,0xFF,0x00,   // Green
  0xFF,0xFF,0x00,   // Cyan

  0x00,0x00,0xFF,   // Blue
  0xFF,0x00,0xFF,   // Magenta
  0x00,0xFF,0xFF,   // Yellow
  0xFF,0xFF,0xFF,   // White
};

/*********************************************************************************
 * Set Spectrum color palette... 8 colors in 2 intensities
 ********************************************************************************/
void pv1000_SetPalette(void)
{
  u8 uBcl,r,g,b;

  for (uBcl=0;uBcl<8;uBcl++)
  {
    r = (u8) ((float) pv1000_palette[uBcl*3+0]*0.121568f);
    g = (u8) ((float) pv1000_palette[uBcl*3+1]*0.121568f);
    b = (u8) ((float) pv1000_palette[uBcl*3+2]*0.121568f);

    SPRITE_PALETTE[uBcl] = RGB15(r,g,b);
    BG_PALETTE[uBcl] = RGB15(r,g,b);
  }
}

// ------------------------------------------------------------------
// Casio PV-1000 reset
// ------------------------------------------------------------------
void pv1000_reset(void)
{
    memcpy(RAM_Memory, ROM_Memory, 0x8000);

    memset(bg, 0, sizeof(bg));

    pv1000_scanline = 0;

    pv1000_vram = RAM_Memory+0xB800;
    pv1000_pcg = RAM_Memory+0xBC00;
    pv1000_pattern = RAM_Memory;
    pv1000_force_pattern = 0;
    pv1000_column = 0x00;
    pv1000_status = 0x00;
    pv1000_bg_color = 0x00;
    pv1000_freqA = 0;
    pv1000_freqB = 0;
    pv1000_freqC = 0;    

    vdp_int_source = INT_RST38;

    pv1000_SetPalette();
}

// ------------------------------------------------------------------
// Casio PV-1000 IO Port Read
// ------------------------------------------------------------------
unsigned char cpu_readport_pv1000(register unsigned short Port)
{
  uint8_t val = 0xff;

  // PV-1000 ports are 8-bit
  Port &= 0x00FF;

  switch (Port)
  {
    case 0xFC:
        val = pv1000_status | 0x80; // High bit always set by pull-up near Z80
        
        // Port FC also returns the P2 input bits same as 0xFD
        if(pv1000_column & 1)
        {
            if(JoyState == JST_STAR<<16) val |= 4;    // #2 select
            if(JoyState == JST_0<<16)    val |= 8;    // #2 start
        }
        if(pv1000_column & 2)
        {
            if(JoyState & JST_DOWN<<16)  val |= 4;    // #2 down
            if(JoyState & JST_RIGHT<<16) val |= 8;    // #2 right
        }
        if(pv1000_column & 4)
        {
            if(JoyState & JST_LEFT<<16)  val |= 4;    // #2 left
            if(JoyState & JST_UP<<16)    val |= 8;    // #2 up
        }
        if(pv1000_column & 8)
        {
            if(JoyState & JST_FIREL<<16) val |= 4;  // #2 trig1
            if(JoyState & JST_FIRER<<16) val |= 8;  // #2 trig2
        }
        break;

    case 0xFD:
        val = 0x80;     // High bit always set by pull-up near Z80
        
        if(pv1000_column & 1)
        {
            if(JoyState == JST_STAR)     val |= 1;    // #1 select
            if(JoyState == JST_0)        val |= 2;    // #1 start
            if(JoyState == JST_STAR<<16) val |= 4;    // #2 select
            if(JoyState == JST_0<<16)    val |= 8;    // #2 start
        }
        if(pv1000_column & 2)
        {
            if(JoyState & JST_DOWN)      val |= 1;    // #1 down
            if(JoyState & JST_RIGHT)     val |= 2;    // #1 right
            if(JoyState & JST_DOWN<<16)  val |= 4;    // #2 down
            if(JoyState & JST_RIGHT<<16) val |= 8;    // #2 right
        }
        if(pv1000_column & 4)
        {
            if(JoyState & JST_LEFT)      val |= 1;    // #1 left
            if(JoyState & JST_UP)        val |= 2;    // #1 up
            if(JoyState & JST_LEFT<<16)  val |= 4;    // #2 left
            if(JoyState & JST_UP<<16)    val |= 8;    // #2 up
        }
        if(pv1000_column & 8)
        {
            if(JoyState & JST_FIREL)     val |= 1;  // #1 trig1
            if(JoyState & JST_FIRER)     val |= 2;  // #1 trig2
            if(JoyState & JST_FIREL<<16) val |= 4;  // #2 trig1
            if(JoyState & JST_FIRER<<16) val |= 8;  // #2 trig2
        }

        if (pv1000_status & 2)
        {
            pv1000_status &= ~2;     // Clear Matrix Scan IRQ
            CPU.IRequest = INT_NONE; // Acknowledge interrupt.
        }
        break;
    }

  return(val);
}

// ---------------------------------------------------------------------------------------------
// This table maps the PV-1000 sound channels to a reasonable SN-76496 equivalent. Not perfect.
// ---------------------------------------------------------------------------------------------
static const uint16_t freq_table[] =
{
    0,      13,     26,     38,     51,     64,     77,     90,     102,    115,    128,    141,    154,    166,    179,    192,
    205,    218,    230,    243,    256,    269,    282,    294,    307,    320,    333,    346,    358,    371,    384,    397,
    410,    422,    435,    448,    461,    474,    486,    499,    512,    525,    538,    550,    563,    576,    589,    602,
    614,    627,    640,    653,    666,    678,    691,    704,    717,    730,    743,    755,    768,    781,    794,    807
};

// ----------------------------------------------------------------------
// Casio PV-1000 IO Port Write
// ----------------------------------------------------------------------
void cpu_writeport_pv1000(register unsigned short Port,register unsigned char data)
{
    // PV-1000 ports are 8-bit
    Port &= 0x00FF;

    switch (Port)
    {
        case 0xF8:
            pv1000_freqA = freq_table[0x3f - (data & 0x3f)];
            sn76496W(0x80 | (pv1000_freqA & 0x0F),       &mySN);    // Write new Frequency for Channel A
            sn76496W(0x00 | ((pv1000_freqA >> 4) & 0x3F),&mySN);    // Write new Frequency for Channel A
            sn76496W(0x90 | (pv1000_freqA ? 0x0A:0x0F),  &mySN);    // Write new Volume for Channel A
            break;

        case 0xF9:
            pv1000_freqB = freq_table[0x3f - (data & 0x3f)];
            sn76496W(0xA0 | (pv1000_freqB & 0x0F),       &mySN);    // Write new Frequency for Channel B
            sn76496W(0x00 | ((pv1000_freqB >> 4) & 0x3F),&mySN);    // Write new Frequency for Channel B
            sn76496W(0xB0 | (pv1000_freqB ? 0x07:0x0F),  &mySN);    // Write new Volume for Channel B
            break;

        case 0xFA:
            pv1000_freqC = freq_table[0x3f - (data & 0x3f)];
            sn76496W(0xC0 | (pv1000_freqC & 0x0F),       &mySN);    // Write new Frequency for Channel C
            sn76496W(0x00 | ((pv1000_freqC >> 4) & 0x3F),&mySN);    // Write new Frequency for Channel C
            sn76496W(0xD0 | (pv1000_freqC ? 0x05:0x0F),  &mySN);    // Write new Volume for Channel C
            break;

        case 0xFB:
            if (data & 2) // Sound Enable
            {
                sn76496W(0x90 | (pv1000_freqA ? 0x0A:0x0F),  &mySN); // Write new Volume for Channel A
                sn76496W(0xB0 | (pv1000_freqB ? 0x07:0x0F),  &mySN); // Write new Volume for Channel B
                sn76496W(0xD0 | (pv1000_freqC ? 0x05:0x0F),  &mySN); // Write new Volume for Channel C
            }
            else // Sound Disable (Mute)
            {
                sn76496W(0x90 | 0x0F,  &mySN);    // Write new Volume for Channel A (sound off)
                sn76496W(0xB0 | 0x0F,  &mySN);    // Write new Volume for Channel B (sound off)
                sn76496W(0xD0 | 0x0F,  &mySN);    // Write new Volume for Channel C (sound off)
            }
            
            // We don't support XOR/Ring (bit 0) - though no game known uses this 'feature'
            break;

        case 0xFC:
            pv1000_enable = data;
            break;

        case 0xFD:
            pv1000_column = data;
            if (pv1000_status & 1)
            {
                pv1000_status &= ~1;     // Clear Prerender IRQ
                CPU.IRequest = INT_NONE; // Acknowledge interrupt.
            }
            break;

        case 0xFE:
            data = (data & 0xFC);
            pv1000_vram = RAM_Memory + (data << 8);
            pv1000_pcg = RAM_Memory + ((data << 8) | 0xC00); // Bits A10-A11 both set
            break;

        case 0xFF:
            pv1000_pattern = RAM_Memory + ((data & 0x20) << 8);
            pv1000_force_pattern = ((data & 0x10) != 0);
            pv1000_bg_color = data & 7; // We're not using this right now as we don't have any real spare LCD drawing to render a border
            break;
    }
}

ITCM_CODE void draw_pattern(int x8, int y8, uint16_t top)
{
    uint8_t  plane[4] = {0, 1, 2, 4};

    // draw pv1000_pattern on rom
    for(int p = 1; p < 4; p++)
    {
        uint8_t col = plane[p];
        uint16_t p8 = top + (p << 3);

        for(int l = 0; l < 8; l++)
        {
            if (p==1)
            {
                uint32_t* dest32 = (uint32_t*) (&bg[y8+l][x8]);
               *dest32++ = 0; *dest32=0;
            }
            uint8_t pat = pv1000_pattern[p8 + l];

            if (pat)
            {
                uint8_t* dest = &bg[y8 + l][x8];

                if(pat & 0x80) dest[0] |= col;
                if(pat & 0x40) dest[1] |= col;
                if(pat & 0x20) dest[2] |= col;
                if(pat & 0x10) dest[3] |= col;
                if(pat & 0x08) dest[4] |= col;
                if(pat & 0x04) dest[5] |= col;
                if(pat & 0x02) dest[6] |= col;
                if(pat & 0x01) dest[7] |= col;
            }
        }
    }
}

ITCM_CODE void draw_pcg(int x8, int y8, uint16_t top)
{
    uint8_t  plane[4] = {0, 1, 2, 4};

    // draw pv1000_pattern on ram
    for(int p = 1; p < 4; p++)
    {
        uint8_t col = plane[p];
        uint16_t p8 = top + (p << 3);

        for(int l = 0; l < 8; l++)
        {
            if (p==1)
            {
                uint32_t* dest32 = (uint32_t*) (&bg[y8+l][x8]);
               *dest32++ = 0; *dest32=0;
            }
            uint8_t pat = pv1000_pcg[p8 + l];

            if (pat)
            {
                uint8_t* dest = &bg[y8 + l][x8];

                if(pat & 0x80) dest[0] |= col;
                if(pat & 0x40) dest[1] |= col;
                if(pat & 0x20) dest[2] |= col;
                if(pat & 0x10) dest[3] |= col;
                if(pat & 0x08) dest[4] |= col;
                if(pat & 0x04) dest[5] |= col;
                if(pat & 0x02) dest[6] |= col;
                if(pat & 0x01) dest[7] |= col;
            }
        }
    }
}


ITCM_CODE void pv1000_drawscreen(void)
{
    static int ds_lite_fameskip =0;

    if (ds_lite_fameskip++ & 1)
    {
        if (!isDSiMode()) return;
    }

    for(int y = 0; y < 24; y++)
    {
        int y8 = y << 3, y32 = y << 5;

        for(int x = 2; x < 30; x++)
        {
            int x8 = x << 3;
            uint8_t code = pv1000_vram[y32 + x];

            if(code < 0xe0 || pv1000_force_pattern)
            {
                draw_pattern(x8, y8, code << 5);
            }
            else
            {
                draw_pcg(x8, y8, (code & 0x1f) << 5);
            }
        }
    }

    // Now copy the pv1000_pattern to the DS LCD screen memory...
    uint8_t *dest = (uint8_t*) (0x06000000 );
    memcpy(dest, bg, 192*256);
}

// -------------------------------------------------------------------------------------------
// This routine runs 1 scanline worth of CPU instructions, processes 1 scanline of 3-channel
// sound and will fire the interrupts / render the screen during the emulated 'VSYNC'.
// -------------------------------------------------------------------------------------------
ITCM_CODE u32 pv1000_run(void)
{
    // ---------------------------------------------------------------------------------------------------
    // For the Casio PV-1000, the CPU only gets to do work on the Horizontal Blank and the Vertical Blank
    // A full scanline is 228 clocks - so we only get those when we are in the Vertical Blank.
    // ---------------------------------------------------------------------------------------------------
    u32 cycles_to_process = (pv1000_scanline >= 192 ? 228:32) + CPU.CycleDeficit;
    CPU.CycleDeficit = ExecZ80(cycles_to_process);

    if (myConfig.soundDriver)
    {
        processDirectAudioSN();
    }

    // There are 16 VSYNC interrupts...
    // 195, 199, 203, 207, 211, 215, 219, 223, 227, 231, 235, 239, 243, 247, 251, 255
    // See https://obscure.nesdev.org/wiki/Casio_PV-1000/ASIC_registers
    if (++pv1000_scanline >= 195)
    {
        switch (pv1000_scanline)
        {
            case 195: case 199: case 203: case 207:
            case 211: case 215: case 219: case 223:
            case 227: case 231: case 235: case 239:
            case 243: case 247: case 251:
                if (pv1000_enable & 2)
                {
                    CPU.IRequest = vdp_int_source;
                    IntZ80(&CPU, CPU.IRequest);
                    pv1000_status |= 2;
                }
                break;

            case 255:
                if (pv1000_enable & 3)
                {
                    CPU.IRequest = vdp_int_source;
                    IntZ80(&CPU, CPU.IRequest);
                    pv1000_status = pv1000_enable;
                }
                break;
        }

        if (pv1000_scanline == 262)
        {
            pv1000_scanline = 0;
            pv1000_drawscreen();
            return 0;
        }
    }

    return 1;
}

// End of file
