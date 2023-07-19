// =====================================================================================
// Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)
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
#include "colecogeneric.h"
#include "cpu/m6502/M6502.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/sn76496/SN76496.h"
#include "printf.h"

M6502 m6502 __attribute__((section(".dtcm")));        // Our core 6502 CPU

extern u8 RAM_Memory[];
extern u8 ROM_Memory[];
extern u8 CreativisionBios[];
extern byte Loop9918(void);
extern SN76496 sncol;


/* PIA handling courtesy of the creatiVision emulator:  https://sourceforge.net/projects/creativisionemulator/
   and used with permission. Do not use this code unless you contact the authors of the emulator at the URL above */
#define PA0         1
#define PA1         2
#define PA2         3
#define PA3         4

#define PIA_OUTALL  0xff
#define PIA_PDR     4
#define PIA_DDR     0

typedef struct {
    short int PDR;  /* Peripheral Data Register */
    short int DDR;  /* Data Direction Register */
    short int CR;   /* Control Register */
    unsigned short int prev_cycles;
} M6821;

/* PIA 0 and 1 */
M6821 pia0 = {0};
M6821 pia1 = {0};

short  total_cycles = 0;
unsigned char KEYBD[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

/**
 * PIA_Write
 *
 * Called from cpu on access to memory in the range $1000 - $1FFF
 */
ITCM_CODE void PIA_Write(word addr, byte data)
{

    switch (addr & 3) {
    case 0: /* PIA 0 DATA PORT */
        if ( pia0.CR & PIA_PDR) {
            if ( pia0.DDR == PIA_OUTALL ) {
                pia0.PDR = data;
                pia0.CR = (pia0.CR & 0x3f) | ((pia0.CR & 3) << 6);
            } else {
                /* Input */
                pia0.PDR = data;
            }
        } else
            pia0.DDR = data;
        break;

    case 1: /* PIA 0 CONTROL */
        pia0.CR = (data & 0x3f);
        break;

    case 2: /* PIA 1 DATA PORT */
        if (pia1.CR & PIA_PDR) {
            if (pia1.DDR == PIA_OUTALL) {
                /* Store for retrieval */
                pia1.PDR = data;

                /* Output to SN76489 */
                sn76496W(data, &sncol);

                pia1.prev_cycles = total_cycles;

                /* Set IRQ as complete */
                pia1.CR = (pia1.CR & 0x3f) | ( ( pia1.CR & 3) << 6);
            }
            else
                pia1.PDR = data;
        } else {
            pia1.DDR = data;
        }
        break;

    case 3: /* PIA 1 CONTROL */
        pia1.CR = data & 0x3f;
        break;
    }
}

/**
 * PIA_Read
 *
 * Updated to reflect the disassembly of the PIA diagnostic cart
 */
ITCM_CODE unsigned char PIA_Read(word addr)
{
    switch( addr & 3 )
    {
    case 0: /* PIA-0 Data Port */

        /* Is the PDR selected */
        if (pia0.CR & PIA_PDR)
        {
            if (!( pia0.DDR & ~pia0.PDR ))/* Is it not an input? */
                return 0xff;

            return pia0.PDR;

        }
        /* Request for DDR */
        return pia0.DDR;


    case 1: /* PIA-0 Control Port */
        pia0.CR &= 0x3f;
        return pia0.CR;

    case 2: /* PIA-1 Data Port */

        /* Clear interrupts */
        if (pia1.CR & PIA_PDR)
        {
            /* Fortunately, the mask set in pia0.PDR tells us which device */
            switch(pia0.PDR)
            {
            case 0xf7:      /* Keyboard Mux 8 */
                pia1.PDR = KEYBD[PA3];
                break;
            case 0xfb:      /* Keyboard Mux 4 */
                pia1.PDR = KEYBD[PA2];
                break;
            case 0xfd:      /* Keyboard Mux 2 */
                pia1.PDR = KEYBD[PA1];
                break;
            case 0xfe:      /* Keyboard Mux 1 */
                pia1.PDR = KEYBD[PA0];
                break;
            default:
                if (pia1.DDR == PIA_OUTALL) {
                    pia1.CR &= 0x3f;
                    return pia1.PDR;
                }

                if (!(pia1.PDR & ~pia1.DDR)) return 0xff;

                return pia1.PDR;
            }

            return pia1.PDR;
        }

        return pia1.DDR;

    case 3: /* PIA-1 Control Port */
        if ( (total_cycles - pia1.prev_cycles) < 36)
            return pia1.CR;

        pia1.CR &= 0x3f;
        return pia1.CR;
    }

    return 0;   /* Will never get here */
}


// Below this point is code not borrowed from the creatiVision emulator...

u8 *creativision_get_cpu(u16 *cv_cpu_size)
{
    *cv_cpu_size = (u16)sizeof(m6502);
    return (u8*)&m6502;
}

void creativision_put_cpu(u8 *mem)
{
    memcpy(&m6502, mem, sizeof(m6502));
}

void creativision_reset(void)
{
    Reset6502(&m6502);
}

u32 creativision_run(void)
{
    Exec6502(&m6502);   // Run 1 scanline worth of CPU instructions
    return 0;
}

#define JOY_SE 0
#define JOY_NE 1
#define JOY_NW 2
#define JOY_SW 3
unsigned char jdmap[4] = { 0x07, 0x4c, 0x38, 0x62 };

// -----------------------------------------------------------------------------------
// Here we map all of the DS keys (buttons or touch-screen) to their CreatiVision
// equivilents. This is just a matter of setting (or, in this case, clearing) the
// right bits in the keyboard map which will be reported back to the program via PIA.
// -----------------------------------------------------------------------------------
ITCM_CODE void creativision_input(void)
{
    extern u32 JoyState;
    extern u8 key_shift, key_ctrl;

    KEYBD[PA3] = 0xFF;
    KEYBD[PA2] = 0xFF;
    KEYBD[PA1] = 0xFF;
    KEYBD[PA0] = 0xFF;

    // First the DS button maps...
    if (JoyState)
    {
        if (JoyState & JST_FIREL)   KEYBD[PA0] &= 0x7f;  // P1 Right Button
        if (JoyState & JST_FIRER)   KEYBD[PA1] &= 0x7f;  // P1 Left Button

        if      ((JoyState & JST_UP) && (JoyState & JST_LEFT))     KEYBD[PA0] &= ~jdmap[JOY_NW];
        else if ((JoyState & JST_UP) && (JoyState & JST_RIGHT))    KEYBD[PA0] &= ~jdmap[JOY_NE];
        else if ((JoyState & JST_DOWN) && (JoyState & JST_LEFT))   KEYBD[PA0] &= ~jdmap[JOY_SW];
        else if ((JoyState & JST_DOWN) && (JoyState & JST_RIGHT))  KEYBD[PA0] &= ~jdmap[JOY_SE];
        else
        {
            if (JoyState & JST_UP)      KEYBD[PA0] &= 0xf7;  // P1 up
            if (JoyState & JST_DOWN)    KEYBD[PA0] &= 0xfd;  // P1 down
            if (JoyState & JST_LEFT)    KEYBD[PA0] &= 0xdf;  // P1 left
            if (JoyState & JST_RIGHT)   KEYBD[PA0] &= 0xfb;  // P1 right
        }

        if (JoyState == JST_1)      KEYBD[PA0] &= 0xf3;  // 1
        if (JoyState == JST_2)      KEYBD[PA1] &= 0xcf;  // 2
        if (JoyState == JST_3)      KEYBD[PA1] &= 0x9f;  // 3
        if (JoyState == JST_4)      KEYBD[PA1] &= 0xd7;  // 4
        if (JoyState == JST_5)      KEYBD[PA1] &= 0xb7;  // 5
        if (JoyState == JST_6)      KEYBD[PA1] &= 0xaf;  // 6
        if (JoyState == JST_7)      KEYBD[PA3] &= 0xf9;  // 7

        if (JoyState == JST_8)      KEYBD[PA3] &= 0xfa;  // Y
        if (JoyState == JST_9)      KEYBD[PA3] &= 0xaf;  // N
        if (JoyState == JST_0)      KEYBD[PA3] &= 0xf6;  // RETURN

        if (JoyState == JST_STAR)   Int6502(&m6502, INT_NMI);  // Game Reset (note, this is needed to start games)
        if (JoyState == JST_POUND)  KEYBD[PA1] &= 0xaf;          // 6 but graphic overlay shows ST=START (which is how it works on a real overlay for the CV)
    }

    // And now the keyboard maps...
    if (kbd_key)
    {
        if (kbd_key == '0')         KEYBD[PA3] &= 0xed;   // 0
        if (kbd_key == '1')         KEYBD[PA0] &= 0xf3;   // 1
        if (kbd_key == '2')         KEYBD[PA1] &= 0xcf;   // 2
        if (kbd_key == '3')         KEYBD[PA1] &= 0x9f;   // 3
        if (kbd_key == '4')         KEYBD[PA1] &= 0xd7;   // 4
        if (kbd_key == '5')         KEYBD[PA1] &= 0xb7;   // 5
        if (kbd_key == '6')         KEYBD[PA1] &= 0xaf;   // 6
        if (kbd_key == '7')         KEYBD[PA3] &= 0xf9;   // 7
        if (kbd_key == '8')         KEYBD[PA3] &= 0xbd;   // 8
        if (kbd_key == '9')         KEYBD[PA3] &= 0xdd;   // 9

        if (kbd_key == 'A')         KEYBD[PA1] &= 0xee;   // A
        if (kbd_key == 'B')         KEYBD[PA1] &= 0xf9;   // B
        if (kbd_key == 'C')         KEYBD[PA1] &= 0xdd;   // C
        if (kbd_key == 'D')         KEYBD[PA1] &= 0xbe;   // D
        if (kbd_key == 'E')         KEYBD[PA1] &= 0xeb;   // E
        if (kbd_key == 'F')         KEYBD[PA1] &= 0xfc;   // F
        if (kbd_key == 'G')         KEYBD[PA1] &= 0xfa;   // G
        if (kbd_key == 'H')         KEYBD[PA3] &= 0xbb;   // H
        if (kbd_key == 'I')         KEYBD[PA3] &= 0xbe;   // I
        if (kbd_key == 'J')         KEYBD[PA3] &= 0xdb;   // J
        if (kbd_key == 'K')         KEYBD[PA3] &= 0xeb;   // K
        if (kbd_key == 'L')         KEYBD[PA3] &= 0xf3;   // L
        if (kbd_key == 'M')         KEYBD[PA3] &= 0xb7;   // M
        if (kbd_key == 'N')         KEYBD[PA3] &= 0xaf;   // N
        if (kbd_key == 'O')         KEYBD[PA3] &= 0xde;   // O
        if (kbd_key == 'P')         KEYBD[PA3] &= 0xee;   // P
        if (kbd_key == 'Q')         KEYBD[PA1] &= 0xe7;   // Q
        if (kbd_key == 'R')         KEYBD[PA1] &= 0xdb;   // R
        if (kbd_key == 'S')         KEYBD[PA1] &= 0xde;   // S
        if (kbd_key == 'T')         KEYBD[PA1] &= 0xbb;   // T
        if (kbd_key == 'U')         KEYBD[PA3] &= 0xfc;   // U
        if (kbd_key == 'V')         KEYBD[PA1] &= 0xbd;   // V
        if (kbd_key == 'W')         KEYBD[PA1] &= 0xf3;   // W
        if (kbd_key == 'X')         KEYBD[PA1] &= 0xed;   // X
        if (kbd_key == 'Y')         KEYBD[PA3] &= 0xfa;   // Y
        if (kbd_key == 'Z')         KEYBD[PA1] &= 0xf5;   // Z
        if (kbd_key == '?')         KEYBD[PA3] &= 0x7f;   // EQUALS (only on small keyboard so we repurpose)
        if (kbd_key == '=')         KEYBD[PA3] &= 0x7f;   // EQUALS
        if (kbd_key == '.')         KEYBD[PA3] &= 0x9f;   // PERIOD

        if (kbd_key == ',')         KEYBD[PA3] &= 0xd7;   // COMMA
        if (kbd_key == ':')         KEYBD[PA3] &= 0xf5;   // COLON
        if (kbd_key == '/')         KEYBD[PA3] &= 0xcf;   // SLASH
        if (kbd_key == '#')         KEYBD[PA3] &= 0xcf;   // SEMI COLON

        if (kbd_key == ' ')         KEYBD[PA2] &= 0xf3;   // SPACE
        if (kbd_key == KBD_KEY_DEL) KEYBD[PA1] &= 0xf6;   // LEFT/BS
        if (kbd_key == KBD_KEY_RET) KEYBD[PA3] &= 0xf6;   // RETURN

        if (key_shift)              KEYBD[PA0] &= 0x7f;   // SHIFT
        if (key_ctrl)               KEYBD[PA1] &= 0x7f;   // CTRL
        if (kbd_key == KBD_KEY_F1)  Int6502(&m6502, INT_NMI);  // Game Reset (note, this is needed to start games)
    }
}


// ========================================================================================
// CreatiVision Memory Map:
//
// $0000 - $03FF: 1K RAM (mirrored thrice at $0400 - $07FF, $0800 - $0BFF, $0C00 - $0FFF)
// $1000 - $1FFF: PIA (joysticks, sound)
// $2000 - $2FFF: VDP read
// $3000 - $3FFF: VDP write
// $4000 - $7FFF: 16K ROM2 (we map RAM here if not used by ROM)
// $8000 - $BFFF: 16K ROM1 (we map RAM here if not used by ROM)
// $C000 - $FFFF: 16K ROM0 (CV BIOS is the 2K from $F800 to $FFFF and the CSL BIOS uses all 16K)
// ========================================================================================
ITCM_CODE void Wr6502(register word Addr,register byte Value)
{
    switch (Addr & 0xF000)
    {
        case 0x0000:    // Zero-Page RAM writes
            if (myConfig.colecoRAM == COLECO_RAM_NORMAL_MIRROR) // Mirror RAM? Nothing really relies on this but a real CreatiVision machine will 'see' the replication.
            {
                RAM_Memory[(Addr & 0x3FF) + 0x000] = Value;
                RAM_Memory[(Addr & 0x3FF) + 0x400] = Value;
                RAM_Memory[(Addr & 0x3FF) + 0x800] = Value;
                RAM_Memory[(Addr & 0x3FF) + 0xC00] = Value;
            }
            else
            {
                RAM_Memory[Addr] = Value;
            }
            break;

        case 0x1000:    // PIA Writes
          PIA_Write(Addr, Value);
          break;

        case 0x3000:    // VDP Writes
            if (Addr & 1) {if (WrCtrl9918(Value)) Int6502(&m6502, INT_IRQ);}
            else WrData9918(Value);

        // Expanded RAM... very little uses this... but for future homebrews or for the CSL bios load
        // In theory we should guard against writes to areas where ROM is mapped, but we are going to 
        // assume well-behaved programs and save the time/effort. So far this has worked fine.
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:
        case 0x8000:
        case 0x9000:
        case 0xA000:
        case 0xB000:
            RAM_Memory[Addr] = Value;
            break;
    }
}

// ------------------------------------------------------------------------------------
// Reads mostly return from our 64K memory but we trap out reads from the PIA and VDP.
// Unused areas should return 0xFF but this is already well handled by pre-filling
// the unused areas of our 64K memory map with 0xFF so we don't have to do that here.
// ------------------------------------------------------------------------------------
ITCM_CODE byte Rd6502(register word Addr)
{
    switch (Addr & 0xF000)
    {
        case 0x1000:    // PIA Read
          return PIA_Read(Addr);
          break;

        case 0x2000:  // VDP Read
          if (Addr & 1) return(RdCtrl9918());
          else return(RdData9918());
          break;
    }

    return RAM_Memory[Addr];
}

// ----------------------------------------------------------------
// With each new game load, we must restore the BIOS to be safe...
// ----------------------------------------------------------------
void creativision_restore_bios(void)
{
    if (myConfig.cvisionLoad != 3) // 3=CSL or similar BIOS
    {
        memcpy(RAM_Memory+0xF800,CreativisionBios,0x800);
    }
}


// -----------------------------------------------------------------------------------------------------------------------------------------
// Most of the legacy commercial games were distributed on two PROMs and due to the way in which the carts were wired up, present the ROM
// in somewhat odd ways... The driver here will handle legacy A/B based on size of the ROM but the user can override this and select a
// linear load which places the ROM binary up against 0xC000 (the important vector information is right before this) and there is also
// a special handler for the 32K bankswapped ROMs which were available for things like the MegaCart. It's all a bit confusing in spots...
// but this generally works fine.
//
//
// From @username@ in the CreatiVemu forums:
// 4K ROMs
// Load directly to $B000
//
// 6K ROMs
// Tank Attack - 4K to $B000, 2K to $A800
// Deep Sea Rescue, Planet Defender, Tennis, TennisD1, TennisD2 - 4K to $B000, 2K to $A000
// Note Deep Sea Rescue needs the byte at $B4C8 fixed from $AF to $A7
//
// 8K ROMs
// Load directly to $A000
//
// 10K ROMs
// Locomotive - 8K to $A000, 2K to $7000
//
// 12K ROMs
// Load 8K to $A000, 4K to $7000
// Note BASIC82A and BASIC82B change byte at $7223 from $40 to $70
//
// 16K ROMs
// Load 8K to $A000, 8K to $8000
//
// 18K ROMs
// Load 8K to $A000, 8K to $8000 and 2K to $7800
// -----------------------------------------------------------------------------------------------------------------------------------------
void creativision_loadrom(int romSize)
{
    memset(RAM_Memory+0x1000, 0xFF, 0xE800);    // Blank everything between RAM and the BIOS at 0xF800
    memset(RAM_Memory+0x4000, 0x00, 0x8000);    // Blank everything between RAM and the BIOS at 0xF800

    if (myConfig.cvisionLoad == 3) // Special load of CSL or similar BIOS into C000-FFFF
    {
        memcpy(RAM_Memory+0xC000, ROM_Memory, romSize);
    }
    else if (myConfig.cvisionLoad == 2)  // 32K BANKSWAP
    {
        memcpy(RAM_Memory+0x4000, ROM_Memory+0x4000, romSize/2);
        memcpy(RAM_Memory+0x8000, ROM_Memory, romSize/2);
    }
    else if (myConfig.cvisionLoad == 1)  // Linear Load
    {
        memcpy(RAM_Memory+(0xC000-romSize), ROM_Memory+0x0000, romSize);    // load linear at 4000-BFFF
    }
    else if (romSize == 4096) // 4K
    {
        memcpy(RAM_Memory+0x9000, ROM_Memory, romSize);
        memcpy(RAM_Memory+0xB000, ROM_Memory, romSize);
    }
    else if (romSize == 1024 * 6) // 6K
    {
        memcpy(RAM_Memory+0xB000, ROM_Memory+0x0000, 0x1000);   // main 4k at 0xB000
        memcpy(RAM_Memory+0xA800, ROM_Memory+0x1000, 0x0800);   // main 2k at 0xA800

        memcpy(RAM_Memory+0x9000, RAM_Memory+0xB000, 0x1000);   // Mirror 4k
        memcpy(RAM_Memory+0xA000, RAM_Memory+0xA800, 0x0800);   // Mirror 2k
        memcpy(RAM_Memory+0x8800, RAM_Memory+0xA800, 0x0800);   // Mirror 2k
        memcpy(RAM_Memory+0x8000, RAM_Memory+0xA800, 0x0800);   // Mirror 2k
    }
    else if (romSize == 8192) // 8K
    {
        memcpy(RAM_Memory+0x8000, ROM_Memory, romSize);
        memcpy(RAM_Memory+0xA000, ROM_Memory, romSize);
    }
    else if (romSize == 1024 * 10) // 10K
    {
        memcpy(RAM_Memory+0xA000, ROM_Memory+0x0000, 0x2000);   // main 8Kb at 0xA000
        memcpy(RAM_Memory+0x7800, ROM_Memory+0x2000, 0x0800);   // second 2Kb at 0x7800

        memcpy(RAM_Memory+0x8000, RAM_Memory+0xA000, 0x2000);   // Mirror 8k at 0x8000

        memcpy(RAM_Memory+0x5800, RAM_Memory+0x7800, 0x0800);   // Mirror 2k at 0x5800
        memcpy(RAM_Memory+0x7000, RAM_Memory+0x7800, 0x0800);   // Mirror 2k
        memcpy(RAM_Memory+0x6800, RAM_Memory+0x7800, 0x0800);   // Mirror 2k
        memcpy(RAM_Memory+0x6000, RAM_Memory+0x7800, 0x0800);   // Mirror 2k
        memcpy(RAM_Memory+0x5000, RAM_Memory+0x7800, 0x0800);   // Mirror 2k
        memcpy(RAM_Memory+0x4800, RAM_Memory+0x7800, 0x0800);   // Mirror 2k
        memcpy(RAM_Memory+0x4000, RAM_Memory+0x7800, 0x0800);   // Mirror 2k
    }
    else if (romSize == 1024 * 12) // 12K
    {
        memcpy(RAM_Memory+0xA000, ROM_Memory+0x0000, 0x2000);   // main 8Kb at 0xA000
        memcpy(RAM_Memory+0x7000, ROM_Memory+0x2000, 0x1000);   // second 4Kb at 0x7000
        memcpy(RAM_Memory+0x8000, RAM_Memory+0xA000, 0x2000);   // Mirror 8k at 0x8000
        memcpy(RAM_Memory+0x5000, RAM_Memory+0x7000, 0x1000);   // Mirror 4k at 0x5000
        memcpy(RAM_Memory+0x6000, RAM_Memory+0x7000, 0x1000);   // Mirror 4k at 0x6000
        memcpy(RAM_Memory+0x4000, RAM_Memory+0x7000, 0x1000);   // Mirror 4k at 0x4000
    }
    else if (romSize == 1024 * 16) // 16K
    {
        memcpy(RAM_Memory+0xA000, ROM_Memory+0x0000, 0x2000);    // main 8Kb    at 0xA000
        memcpy(RAM_Memory+0x8000, ROM_Memory+0x2000, 0x2000);    // second 8Kb at 0x8000
    }
    else if (romSize == 1024 * 18) // 18K
    {
        memcpy(RAM_Memory+0xA000, ROM_Memory+0x0000, 0x2000);    // main 8Kb at 0xA000
        memcpy(RAM_Memory+0x8000, ROM_Memory+0x2000, 0x2000);    // second 8Kb at 0x8000
        memcpy(RAM_Memory+0x7800, ROM_Memory+0x4000, 0x0800);    // final 2Kb at 0x7800

        memcpy(RAM_Memory+0x6800, RAM_Memory+0x7800, 0x0800);    // And then the odd mirrors...
        memcpy(RAM_Memory+0x5800, RAM_Memory+0x7800, 0x0800);
        memcpy(RAM_Memory+0x4800, RAM_Memory+0x7800, 0x0800);
        memcpy(RAM_Memory+0x7000, RAM_Memory+0x7800, 0x0800);
        memcpy(RAM_Memory+0x6000, RAM_Memory+0x7800, 0x0800);
        memcpy(RAM_Memory+0x5000, RAM_Memory+0x7800, 0x0800);
        memcpy(RAM_Memory+0x4000, RAM_Memory+0x7800, 0x0800);
    }
    else if (romSize <= 1024 * 32) // 32K or less (load Linear at 0xC000 - romSize)
    {
        memcpy(RAM_Memory+(0xC000-romSize), ROM_Memory+0x0000, romSize);    // load linear at 4000-BFFF up against the $C000 (where the vectors are)
    }
}

// End of file
