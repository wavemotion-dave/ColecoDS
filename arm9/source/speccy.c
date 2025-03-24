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

u8 portFE               __attribute__((section(".dtcm"))) = 0x00;
u8 portFD               __attribute__((section(".dtcm"))) = 0x00;
u8 zx_AY_enabled        __attribute__((section(".dtcm"))) = 0;
u8 zx_AY_index_written  __attribute__((section(".dtcm"))) = 0;
u32 flash_timer         __attribute__((section(".dtcm"))) = 0;
u8  bFlash              __attribute__((section(".dtcm"))) = 0;
u8 *zx_PagedRAM         __attribute__((section(".dtcm"))) = 0;
u8  zx_128k_mode        __attribute__((section(".dtcm"))) = 0;
u8  zx_ScreenRendering  __attribute__((section(".dtcm"))) = 0;

u8  zx_special_key = 0;
int last_z80_size  = 0;
u8  isCompressed   = 1;

// ---------------------------------------------
// ZX Spectrum 48K and 128K Emulation driver...
// ---------------------------------------------

unsigned char cpu_readport_speccy(register unsigned short Port)
{
    static u8 bNonSpecialKeyWasPressed = 0;

    if ((Port & 1) == 0) // Any Even Address will cause the ULA to respond
    {
        u8 key = 0x40; // Mic IN will be zero

        for (u8 i=0; i< (kbd_keys_pressed ? kbd_keys_pressed:1); i++) // Always one pass at least for joysticks...
        {
            kbd_key = kbd_keys[i];
            word inv = ~Port;
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

            // Handle the Symbol and Shift keys which are special "modifier keys"
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
        }

        return (u8)~key;
    }

    if ((Port & 0x3F) == 0x1F)  // Kempston Joystick interface... (only A5 driven low)
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

    if ((Port & 0xc002) == 0xc000) // AY input
    {
        return ay38910DataR(&myAY);
    }
    
    // ---------------------------------------------------------------------------------------------
    // Poor Man's floating bus. Very few games use this - so we basically handle it very roughly.
    // If we are not drawing the screen - the ULA will be idle and we will return 0xFF (below).
    // If we are rending the screen... we will return the Attribute byte mid-scanline which is 
    // good enough for games like Sidewine and Short Circuit and Cobra, etc.
    // ---------------------------------------------------------------------------------------------
    if (zx_ScreenRendering)
    {
        u8 *floatBusPtr;
        
        // For the ZX 128K, we might be using page 7 for video display... it's rare, but possible...
        if (zx_128k_mode) floatBusPtr = zx_PagedRAM + (((portFD & 0x08) ? 7:5) * 0x4000) + 0x1800;
        else floatBusPtr = RAM_Memory + 0x5800;

        u8 *attrPtr = &floatBusPtr[((CurLine - 64)/8)*32];
        
        static u8 floating_fetcher = 0;
        return attrPtr[floating_fetcher++ % 32]; 
    }

    return 0xFF;  // Unused port returns 0xFF when ULA is idle
}

// --------------------------------------------------------------------------------------
// For the ZX Spectrum 128K this is the banking routine that will swap the BIOS ROM and
// swap out the bank of memory that will be visible at 0xC000 in CPU address space.
// --------------------------------------------------------------------------------------
void zx_bank(u8 new_bank)
{
    if (portFD & 0x20) return; // Lock out - no more bank swaps allowed

    // Map in the correct bios segment... make sure this isn't a diagnostic ROM
    if (speccy_mode != 3)
    {
        MemoryMap[0] = SpectrumBios128 + ((new_bank & 0x10) ? 0x4000 : 0x0000);
        MemoryMap[1] = SpectrumBios128 + ((new_bank & 0x10) ? 0x6000 : 0x2000);
    }

    // Map in the correct page of banked memory
    MemoryMap[6] = zx_PagedRAM + ((new_bank & 0x07) * 0x4000) + 0x0000;
    MemoryMap[7] = zx_PagedRAM + ((new_bank & 0x07) * 0x4000) + 0x2000;

    portFD = new_bank;
}


void cpu_writeport_speccy(register unsigned short Port,register unsigned char Value)
{
    if ((Port & 1) == 0) // Any even port (usually 0xFE) is our ULA and beeper output
    {
        portFE = Value;
    }

    if (zx_128k_mode && ((Port & 0x8002) == 0x0000)) // 128K Bankswitch
    {
        zx_bank(Value);
    }

    if ((Port & 0xc002) == 0xc000) // AY Register Select
    {
        ay38910IndexW(Value&0xF, &myAY);
        zx_AY_index_written = 1;
    }
    else if ((Port & 0xc002) == 0x8000) // AY Data Write
    {
        ay38910DataW(Value, &myAY);
        if (zx_AY_index_written) zx_AY_enabled = 1;
    }
}

// A fast look-up table when we are rendering background pixels
u32 zx_colors_extend32[16] __attribute__((section(".dtcm"))) =
{
    0x00000000, 0x01010101, 0x02020202, 0x03030303,
    0x04040404, 0x05050505, 0x06060606, 0x07070707,
    0x08080808, 0x09090909, 0x0A0A0A0A, 0x0B0B0B0B,
    0x0C0C0C0C, 0x0D0D0D0D, 0x0E0E0E0E, 0x0F0F0F0F
};

// ----------------------------------------------------------------------------
// Render one screen line of pixels. This is called on every visible scanline.
// ----------------------------------------------------------------------------
ITCM_CODE void speccy_render_screen(u8 line)
{
    u8 *zx_ScreenPage = 0;
    u32 *vidBuf = (u32*) (0x06000000 + (line << 8));    // Video buffer... write 32-bits at a time for maximum speed

    if (line == 0) // At start of each new frame, handle the flashing 'timer'
    {
        if (++flash_timer & 0x10) {flash_timer=0; bFlash ^= 1;} // Same timing as real ULA - 16 frames on and 16 frames off
    }
    
    if (!isDSiMode() && (flash_timer & 1)) return; // For DS-Lite/Phat, we draw every other frame...

    // For the ZX 128K, we might be using page 7 for video display... it's rare, but possible...
    if (zx_128k_mode) zx_ScreenPage = zx_PagedRAM + ((portFD & 0x08) ? 7:5) * 0x4000;
    else zx_ScreenPage = RAM_Memory + 0x4000;

    // -----------------------------------------------
    // Now run through the entire screen from top to
    // bottom and render it into our NDS video memory
    // -----------------------------------------------
    int y = line;
    {
        // ----------------------------------------------------------------
        // The color attribute is stored independently from the pixel data
        // ----------------------------------------------------------------
        u8 *attrPtr = &zx_ScreenPage[0x1800 + ((y/8)*32)];
        word offset = ((y&0x07) << 8) | ((y&0x38) << 2) | ((y&0xC0) << 5);
        u8 *pixelPtr = zx_ScreenPage+offset;
        
        // ---------------------------------------------------------------------
        // With 8 pixels per byte, there are 32 bytes of horizontal screen data
        // ---------------------------------------------------------------------
        for (int x=0; x<32; x++)
        {
            u8 attr = *attrPtr++;           // The color attribute and possible flashing
            u8 paper = ((attr>>3) & 0x0F);  // Paper is the background
            u8 pixel = *pixelPtr++;         // And here is 8 pixels to draw

            if (attr & 0x80) // Flashing swaps pen/ink
            {
                if (bFlash) pixel = ~pixel; // Faster to just invert the pixel itself...
            }
            
            // ---------------------------------------------------------------
            // Normal drawing... We try to speed this up as much as possible.
            // ---------------------------------------------------------------
            if (pixel) // Is at least one pixel on?
            {
                u8 ink   = (attr & 0x07);       // Color
                if (attr & 0x40) ink |= 0x08;   // Brightness

                *vidBuf++ = (((pixel & 0x80) ? ink:paper)) | (((pixel & 0x40) ? ink:paper) << 8) | (((pixel & 0x20) ? ink:paper) << 16) | (((pixel & 0x10) ? ink:paper) << 24);
                *vidBuf++ = (((pixel & 0x08) ? ink:paper)) | (((pixel & 0x04) ? ink:paper) << 8) | (((pixel & 0x02) ? ink:paper) << 16) | (((pixel & 0x01) ? ink:paper) << 24);
            }
            else // Just drawing all background which is common...
            {
                // Draw background directly to the screen
                *vidBuf++ = zx_colors_extend32[paper];
                *vidBuf++ = zx_colors_extend32[paper];
            }
        }
    }
}

// -----------------------------------------------------
// Z80 Snapshot v1 is always a 48K game... 
// The header is 30 bytes long - most of which will be
// used when we reset the game to set the state of the
// CPU registers, Stack Pointer and Program Counter.
// -----------------------------------------------------
u8 decompress_v1(int romSize)
{
    int offset = 0; // Current offset into memory
    
    isCompressed = (ROM_Memory[12] & 0x20 ? 1:0); // V1 files are usually compressed

    for (int i = 30; i < romSize; i++)
    {
        if (offset > 0xC000)
        {
            break;
        }

        // V1 headers always end in 00 ED ED 00
        if (ROM_Memory[i] == 0x00 && ROM_Memory[i + 1] == 0xED && ROM_Memory[i + 2] == 0xED && ROM_Memory[i + 3] == 0x00)
        {
            break;
        }

        if (i < romSize - 3)
        {
            if (ROM_Memory[i] == 0xED && ROM_Memory[i + 1] == 0xED && isCompressed)
            {
                i += 2;
                word repeat = ROM_Memory[i++];
                byte value = ROM_Memory[i];
                for (int j = 0; j < repeat; j++)
                {
                    RAM_Memory[0x4000 + offset++] = value;
                }
            }
            else
            {
                RAM_Memory[0x4000 + offset++] = ROM_Memory[i];
            }
        }
        else
        {
            RAM_Memory[0x4000 + offset++] = ROM_Memory[i];
        }
    }

    return 0; // 48K Spectrum
}

// ---------------------------------------------------------------------------------------------
// Z80 Snapshot v2 or v2 could be 48K game but is usually a 128K game. The header will tell us.
// ---------------------------------------------------------------------------------------------
u8 decompress_v2_v3(int romSize)
{
    int offset;

    word extHeaderLen = 30 + ROM_Memory[30] + 2;
    
    // Uncompress all the data and store into the proper place in our buffers
    int idx = extHeaderLen;
    while (idx < romSize)
    {
        isCompressed = 1;
        word compressedLen = ROM_Memory[idx] | (ROM_Memory[idx+1] << 8);
        if (compressedLen == 0xFFFF) {isCompressed = 0; compressedLen = (16*1024);}
        if (compressedLen > 0x4000) compressedLen = 0x4000;
        u8 pageNum = ROM_Memory[idx+2];
        u8 *UncompressedData = zx_PagedRAM + ((pageNum-3) * 0x4000);
        idx += 3;
        offset = 0x0000;
        for (int i=0; i<compressedLen; i++)
        {
            if (i < compressedLen - 3)
            {
                if (ROM_Memory[idx+i] == 0xED && ROM_Memory[idx+i + 1] == 0xED && isCompressed)
                {
                    i += 2;
                    u16 repeat = ROM_Memory[idx + i++];
                    byte value = ROM_Memory[idx + i];
                    for (int j = 0; j < repeat; j++)
                    {
                        UncompressedData[offset++] = value;
                    }
                }
                else
                {
                    UncompressedData[offset++] = ROM_Memory[idx+i];
                }
            }
            else
            {
                UncompressedData[offset++] = ROM_Memory[idx+i];
            }
        }

        idx += compressedLen;

        if (ROM_Memory[34] >= 3) // 128K mode?
        {
            // Already placed in RAM by default above...
        }
        else // 48K mode
        {
                 if (pageNum == 8) memcpy(RAM_Memory+0x4000, UncompressedData, 0x4000);
            else if (pageNum == 4) memcpy(RAM_Memory+0x8000, UncompressedData, 0x4000);
            else if (pageNum == 5) memcpy(RAM_Memory+0xC000, UncompressedData, 0x4000);
        }
    }

    return ((ROM_Memory[34] >= 3) ? 1:0); // 128K Spectrum or 48K
}

// ----------------------------------------------------------------------
// Assumes .z80 file is in ROM_Memory[] - this will determine if we are
// a version 1, 2 or 3 snapshot and handle the header appropriately to 
// decompress the data out into emulation memory.
// ----------------------------------------------------------------------
void speccy_decompress_z80(int romSize)
{
    last_z80_size = romSize;

    zx_128k_mode = 0;   // Assume 48K until told otherwise

    zx_PagedRAM = ROM_Memory + 0x40000; // 128K RAM is placed in higher part of ROM Memory...

    if (romSize == (16*1024)) // Assume this is a diagnostic ROM of some kind
    {
        memcpy(RAM_Memory, ROM_Memory, romSize);   // Load diagnostics ROM into place
        speccy_mode = 3;                           // Force PC to 0x0000 to run diagnostic
        zx_128k_mode = 1;                          // Force ZX Spectrum 128K
        return;
    }

    if (speccy_mode == 2) // SNA snapshot - only 48K compatible
    {
        memcpy(RAM_Memory + 0x4000, ROM_Memory+27, 0xC000);
        zx_128k_mode = 0;
        return;
    }

    // V2 or V3 header... possibly 128K Spectrum snapshot
    if ((ROM_Memory[6] == 0x00) && (ROM_Memory[7] == 0x00))
    {
        zx_128k_mode = decompress_v2_v3(romSize);
    }
    else
    {
        // This is going to be 48K only
        zx_128k_mode = decompress_v1(romSize);
    }

    // Load the correct BIOS into place...
    if (zx_128k_mode)   memcpy(RAM_Memory, SpectrumBios128+0x4000, 0x4000);   // Load ZX 128K BIOS into place
    else                memcpy(RAM_Memory, SpectrumBios, 0x4000);             // Load ZX 48K BIOS into place
}


// ----------------------------------------------------------------------
// Reset the emulation. Freshly decompress the contents of RAM memory
// and setup the CPU registers exactly as the snapshot indicates. Then
// we can start the emulation at exactly the point it left off... This
// works fine so long as the game does not need to go back out to the
// tape to load another segment of the game.
// ----------------------------------------------------------------------
void speccy_reset(void)
{
    if (speccy_mode)
    {
        
        CPU.PC.W = 0;
        portFE = 0x00;
        portFD = 0x00;
        zx_AY_enabled = 0;
        zx_AY_index_written = 0;
        zx_special_key = 0;
        
        // A bit wasteful to decompress again... but 
        // we want to ensure that the memory is exactly
        // as it should be when we reset the system.
        speccy_decompress_z80(last_z80_size);

        if (speccy_mode == 2) // SNA snapshot
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
        else if (speccy_mode == 3) // Diagnostic ROM - launch in ZX 128K mode
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

            // Now set the memory map to point to the right banks...
            MemoryMap[2] = zx_PagedRAM + (5 * 0x4000) + 0x0000; // Bank 5
            MemoryMap[3] = zx_PagedRAM + (5 * 0x4000) + 0x2000; // Bank 5

            MemoryMap[4] = zx_PagedRAM + (2 * 0x4000) + 0x0000; // Bank 2
            MemoryMap[5] = zx_PagedRAM + (2 * 0x4000) + 0x2000; // Bank 2

            MemoryMap[6] = zx_PagedRAM + (0 * 0x4000) + 0x0000; // Bank 0
            MemoryMap[7] = zx_PagedRAM + (0 * 0x4000) + 0x2000; // Bank 0
        }
        else // Z80 snapshot
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

            CPU.I      = ROM_Memory[10]; // Interrupt register
            CPU.R      = ROM_Memory[11]; // Low 7-bits of Refresh
            CPU.R_HighBit = (ROM_Memory[12] & 1 ? 0x80:0x00); // High bit of refresh

            CPU.DE.B.l  = ROM_Memory[13]; // E
            CPU.DE.B.h  = ROM_Memory[14]; // D

            CPU.BC1.B.l = ROM_Memory[15]; // BC'
            CPU.BC1.B.h = ROM_Memory[16];

            CPU.DE1.B.l = ROM_Memory[17]; // DE'
            CPU.DE1.B.h = ROM_Memory[18];

            CPU.HL1.B.l = ROM_Memory[19]; // HL'
            CPU.HL1.B.h = ROM_Memory[20];

            CPU.AF1.B.h = ROM_Memory[21]; // AF'
            CPU.AF1.B.l = ROM_Memory[22];

            CPU.IY.B.l  = ROM_Memory[23]; // IY
            CPU.IY.B.h  = ROM_Memory[24];

            CPU.IX.B.l  = ROM_Memory[25]; // IX
            CPU.IX.B.h  = ROM_Memory[26];

            CPU.IFF     = (ROM_Memory[27] ? IFF_1 : 0x00);
            CPU.IFF    |= (ROM_Memory[28] ? IFF_2 : 0x00);
            CPU.IFF    |= ((ROM_Memory[29] & 3) == 1 ? IFF_IM1 : IFF_IM2);
            
            MemoryMap[0] = RAM_Memory + 0x0000;
            MemoryMap[1] = RAM_Memory + 0x2000;
            MemoryMap[2] = RAM_Memory + 0x4000;
            MemoryMap[3] = RAM_Memory + 0x6000;
            MemoryMap[4] = RAM_Memory + 0x8000;
            MemoryMap[5] = RAM_Memory + 0xA000;
            MemoryMap[6] = RAM_Memory + 0xC000;
            MemoryMap[7] = RAM_Memory + 0xE000;

            // ------------------------------------------------------------------------------------
            // If the Z80 snapshot indicated we are v2 or v3 - we use the extended header
            // ------------------------------------------------------------------------------------
            if (CPU.PC.W == 0x0000)
            {
                CPU.PC.B.l = ROM_Memory[32]; // PC low byte
                CPU.PC.B.h = ROM_Memory[33]; // PC high byte

                // ------------------------------------------------------------------------------
                // And if we are in ZX 128K mode, we need to setup the paging and restore the AY
                // ------------------------------------------------------------------------------
                if (zx_128k_mode)
                {
                    // Now set the memory map to point to the right banks...
                    MemoryMap[2] = zx_PagedRAM + (5 * 0x4000) + 0x0000; // Bank 5
                    MemoryMap[3] = zx_PagedRAM + (5 * 0x4000) + 0x2000; // Bank 5

                    MemoryMap[4] = zx_PagedRAM + (2 * 0x4000) + 0x0000; // Bank 2
                    MemoryMap[5] = zx_PagedRAM + (2 * 0x4000) + 0x2000; // Bank 2

                    zx_bank(ROM_Memory[35]);     // Last write to 0x7ffd (banking)

                    // ---------------------------------------------------------------------------------------
                    // Restore the sound chip exactly as it was... I've seen some cases (Lode Runner) where
                    // the AY in Use flag in byte 37 is not set correctly so we also check to see if the
                    // last AY index has been set or if any of the A,B,C volumes is non-zero to enable here.
                    // ---------------------------------------------------------------------------------------
                    if ((ROM_Memory[37] & 0x04) || (ROM_Memory[38] > 0) || (ROM_Memory[39+8] > 0) || 
                       (ROM_Memory[39+9] > 0) || (ROM_Memory[39+10] > 0)) // Was the AY enabled? 
                    {
                        zx_AY_enabled = 1;
                        for (u8 k=0; k<16; k++)
                        {
                            ay38910IndexW(k, &myAY);
                            ay38910DataW(ROM_Memory[39+k], &myAY);
                        }
                        ay38910IndexW(ROM_Memory[38], &myAY); // Last write to the AY index register
                    }
                }
            }
        }

        // And a few last CPU details before we start the emulation!
        CPU.IBackup    = 1;
        CPU.IRequest   = INT_NONE;
        CPU.User       = 0;
        CPU.Trace      = 0;
        CPU.TrapBadOps = 1;
        CPU.IAutoReset = 1;

        myConfig.soundDriver = SND_DRV_BEEPER; // We force in beeper mode... plus mix in AY if available
    }
}


// -----------------------------------------------------------------------------
// Run the emulation for exactly 1 scanline and handle the VDP interrupt if 
// the emulation has executed the last line of the frame.  This also handles
// direct beeper and possibly AY sound emulation as well. Crude but effective.
// -----------------------------------------------------------------------------
u32 speccy_run(void)
{
    u32 retVal = 1;
    static int zzz=1;
    
    if (zzz)
    {
        debug_printf("CPU.PC  = %04X\n", CPU.PC.W);
        debug_printf("CPU.SP  = %04X\n", CPU.SP.W);
        debug_printf("CPU.AF  = %04X\n", CPU.AF.W);
        debug_printf("CPU.BC  = %04X\n", CPU.BC.W);
        debug_printf("CPU.DE  = %04X\n", CPU.DE.W);
        debug_printf("CPU.HL  = %04X\n", CPU.HL.W);
        debug_printf("CPU.I   = %02X\n", CPU.I);
        debug_printf("CPU.AF' = %04X\n", CPU.AF1.W);
        debug_printf("CPU.BC' = %04X\n", CPU.BC1.W);
        debug_printf("CPU.HL' = %04X\n", CPU.HL1.W);
        debug_printf("CPU.DE' = %04X\n", CPU.DE1.W);
        debug_printf("CPU.IY  = %04X\n", CPU.IY.W);
        debug_printf("CPU.IX  = %04X\n", CPU.IX.W);
        debug_printf("CPU.IFF = %04X\n", CPU.IFF);
        debug_printf("CPU.R   = %02X\n", (CPU.R & 0x7F) | CPU.R_HighBit);
        
        for (int i=0xFD00; i<0x10000; i += 16)
        {
            debug_printf("%04X : %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n", i,
            RAM_Memory[i+0], RAM_Memory[i+1], RAM_Memory[i+2], RAM_Memory[i+3], 
            RAM_Memory[i+4], RAM_Memory[i+5], RAM_Memory[i+6], RAM_Memory[i+7], 
            RAM_Memory[i+8], RAM_Memory[i+9], RAM_Memory[i+10], RAM_Memory[i+11], 
            RAM_Memory[i+12], RAM_Memory[i+13], RAM_Memory[i+14], RAM_Memory[i+15] );
        }
        zzz=0;
    }
    if (++CurLine >= tms_num_lines) CurLine=0;
    
    // ----------------------------------------------
    // Execute 1 scanline worth of CPU instructions.
    // The ZX 128K is slightly faster than 48K
    // ----------------------------------------------
    int cycles_to_run = (zx_128k_mode ? 228:224);

    // -----------------------------------------------
    // We break this up into four pieces in order
    // to get more chances to render the audio beeper
    // which requires a fairly high sample rate...
    // -----------------------------------------------
    processDirectBeeperAY4((CurLine & 3) ? 4:3); // Grab 3 or 4 samples from AY to mix in...

    CPU.CycleDeficit = ExecZ80_Speccy(cycles_to_run>>2);
    if (CurLine & 3) processDirectBeeper();

    CPU.CycleDeficit += ExecZ80_Speccy(cycles_to_run>>2);
    processDirectBeeper();

    CPU.CycleDeficit += ExecZ80_Speccy(cycles_to_run>>2);
    processDirectBeeper();

    zx_ScreenRendering = 0; // On this final chunk we are drawing border and doing a horizontal refresh... no contention

    ExecZ80_Speccy((cycles_to_run>>2)+CPU.CycleDeficit); // Catch up any partial cycles here...
    processDirectBeeper();

    // ----------------------------------------
    // Generate an interrupt if called for...
    // ----------------------------------------
    if(CPU.IRequest!=INT_NONE) IntZ80(&CPU, CPU.IRequest);

    if (CurLine == tms_end_line)
    {
        CPU.IRequest = INT_RST38;
        IntZ80(&CPU, CPU.IRequest);
        tms_num_lines = (zx_128k_mode ? 311:312);
        CPU.TStates = 0;
        retVal = 0;
    }
    
    // -----------------------------------------------------------
    // Render one line if we're in the visible area of the screen
    // -----------------------------------------------------------
    if (CurLine & 0xC0)
    {
        if ((CurLine & 0x100) == 0)
        {
            // Render one scanline... 
            speccy_render_screen(CurLine - 64);
            zx_ScreenRendering = 1;
        }
    }
    
    return retVal;
}

// End of file
