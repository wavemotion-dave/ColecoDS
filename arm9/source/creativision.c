// =====================================================================================
// Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty. Please see readme.md
//
// This CreatiVision driver comes from a lot of pioneers who have done a great job
// of experimenting, sleuthing and documenting this somewhat mysterious machine. 
// Much of this information was gleaned from looking at the discussion on the 
// CreatiVemu forums: http://www.madrigaldesign.it/forum/ 
// 
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

M6502 m6502 __attribute__((section(".dtcm")));        // Our core 6502 CPU in fast NDS memory cache

extern u8 RAM_Memory[];
extern u8 *ROM_Memory;
extern u8 CreativisionBios[];
extern byte Loop9918(void);
extern SN76496 mySN;
extern u8 BufferedKeys[32];
extern u8 BufferedKeysWriteIdx;
extern u8 BufferedKeysReadIdx;


/* PIA handling courtesy of the creatiVision emulator:  https://sourceforge.net/projects/creativisionemulator/
   and used with permission. Do not use this code unless you contact the author of the emulator at the URL above */
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

unsigned short int total_cycles = 0; //TODO: maybe hook this into the main CPU emulation cycle counter. In practice this won't matter as well-behaved games read input at proper timing.
unsigned char KEYBD[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };

// We use this structure to hold the save state and pass the buffer back to the caller for writing to the .sav file
struct 
{
    M6502 m6502;
    M6821 pia0;
    M6821 pia1;
    unsigned short int total_cycles;
    u32   spare;
} CV_SaveState;

/**
 * PIA_Write
 *
 * Called from cpu on access to memory in the range $1000 - $1FFF
 */
void PIA_Write(word addr, byte data)
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
                sn76496W(data, &mySN);

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

u8 *creativision_get_savestate(u16 *cv_cpu_size)
{
    memcpy(&CV_SaveState.m6502, &m6502, sizeof(m6502));
    memcpy(&CV_SaveState.pia0,  &pia0,  sizeof(pia0));
    memcpy(&CV_SaveState.pia1,  &pia1,  sizeof(pia1));
    CV_SaveState.total_cycles = total_cycles;
    CV_SaveState.spare = 0;
    
    *cv_cpu_size = (u16)sizeof(CV_SaveState);
    return (u8*)&CV_SaveState;
}

void creativision_put_savestate(u8 *mem)
{
    memcpy(&CV_SaveState, mem, sizeof(CV_SaveState));
    
    memcpy(&m6502,  &CV_SaveState.m6502,  sizeof(m6502));
    memcpy(&pia0,   &CV_SaveState.pia0,   sizeof(pia0));
    memcpy(&pia1,   &CV_SaveState.pia1,   sizeof(pia1));
    total_cycles = CV_SaveState.total_cycles;
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

// -----------------------------------------------------------------------------------
// Here we map all of the DS keys (buttons or touch-screen) to their CreatiVision
// equivilents. This is just a matter of setting (or, in this case, clearing) the
// right bits in the keyboard map which will be reported back to the program via PIA.
// -----------------------------------------------------------------------------------
void creativision_input(void)
{
    extern u32 JoyState;
    extern u8 key_shift, key_ctrl;

    // Assume nothing pressed... will clear bits as needed below
    KEYBD[PA0] = 0xFF;
    KEYBD[PA1] = 0xFF;
    KEYBD[PA2] = 0xFF;
    KEYBD[PA3] = 0xFF;
    
    // When Cassette icon is pressed, we insert the 'RUN' command automatically
    static int cv_dampen=5;
    if (BufferedKeysReadIdx != BufferedKeysWriteIdx)
    {
        kbd_key = BufferedKeys[BufferedKeysReadIdx];
        if (--cv_dampen == 0)
        {
            BufferedKeysReadIdx = (BufferedKeysReadIdx+1) % 32;
            cv_dampen=5;
            return;
        }
    } else cv_dampen=5;
    

    // First the DS button maps...
    if (JoyState)
    {
        if (JoyState & JST_FIREL)       KEYBD[PA0] &= 0x7f;  // P1 Right Button
        if (JoyState & JST_FIRER)       KEYBD[PA1] &= 0x7f;  // P1 Left Button
        if (JoyState & JST_FIREL<<16)   KEYBD[PA2] &= 0x7f;  // P2 Right Button
        if (JoyState & JST_FIRER<<16)   KEYBD[PA3] &= 0x7f;  // P2 Left Button

        // ----------------------------------------------------------------------------------------------
        // Handle diagonals first... these are not just the same bits in the PIA as: NE = UP+RIGHT,
        // SW = DN+LEFT, etc. so we have to check them seperately... most CreatiVision games don't seem
        // to use diagonals but we may as well support it. In theory there are 16 directions on the
        // joystick but we are only mapping 8 of them which is fine for gameplay.
        // ----------------------------------------------------------------------------------------------
        
        // Player 1
        if      ((JoyState & JST_UP) && (JoyState & JST_LEFT))     KEYBD[PA0] &= 0xc7; // P1 NW
        else if ((JoyState & JST_UP) && (JoyState & JST_RIGHT))    KEYBD[PA0] &= 0xb3; // P1 NE
        else if ((JoyState & JST_DOWN) && (JoyState & JST_LEFT))   KEYBD[PA0] &= 0x9d; // P1 SW
        else if ((JoyState & JST_DOWN) && (JoyState & JST_RIGHT))  KEYBD[PA0] &= 0xf8; // P1 SE
        else
        {
            if (JoyState & JST_UP)      KEYBD[PA0] &= 0xf7;  // P1 up    (N)
            if (JoyState & JST_DOWN)    KEYBD[PA0] &= 0xfd;  // P1 down  (S)
            if (JoyState & JST_LEFT)    KEYBD[PA0] &= 0xdf;  // P1 left  (W)
            if (JoyState & JST_RIGHT)   KEYBD[PA0] &= 0xfb;  // P1 right (E)
        }

        // Player 2
        if      ((JoyState & JST_UP<<16) && (JoyState & JST_LEFT)<<16)     KEYBD[PA2] &= 0xc7; // P2 NW
        else if ((JoyState & JST_UP<<16) && (JoyState & JST_RIGHT<<16))    KEYBD[PA2] &= 0xb3; // P2 NE
        else if ((JoyState & JST_DOWN<<16) && (JoyState & JST_LEFT<<16))   KEYBD[PA2] &= 0x9d; // P2 SW
        else if ((JoyState & JST_DOWN<<16) && (JoyState & JST_RIGHT<<16))  KEYBD[PA2] &= 0xf8; // P2 SE
        else
        {
            if (JoyState & JST_UP<<16)      KEYBD[PA2] &= 0xf7;  // P2 up    (N)
            if (JoyState & JST_DOWN<<16)    KEYBD[PA2] &= 0xfd;  // P2 down  (S)
            if (JoyState & JST_LEFT<<16)    KEYBD[PA2] &= 0xdf;  // P2 left  (W)
            if (JoyState & JST_RIGHT<<16)   KEYBD[PA2] &= 0xfb;  // P2 right (E)
        }
        
        // See if the onscreen keypad was pressed... All JST_x keypad codes map to the lower nibble...
        if (JoyState & 0x000F)
        {
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
            if (JoyState == JST_POUND)  KEYBD[PA1] &= 0xaf;        // 6 but graphic overlay shows ST=START (which is how it works on a real overlay for the CV)
        }
    }

    // And now the full keyboard maps...
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
        if (kbd_key == '?')         KEYBD[PA3] &= 0x7f;   // DASH/Equals (only on small keyboard so we repurpose)
        if (kbd_key == '.')         KEYBD[PA3] &= 0x9f;   // PERIOD

        if (kbd_key == ',')         KEYBD[PA3] &= 0xd7;   // COMMA
        if (kbd_key == ':')         KEYBD[PA3] &= 0xf5;   // COLON
        if (kbd_key == '/')         KEYBD[PA3] &= 0xcf;   // SLASH
        if (kbd_key == ';')         KEYBD[PA3] &= 0xe7;   // SEMI COLON
        if (kbd_key == '#')         KEYBD[PA3] &= 0xe7;   // repurpose to SEMI COLON 
        if (kbd_key == '-')         KEYBD[PA3] &= 0x7f;   // DASH

        if (kbd_key == ' ')         KEYBD[PA2] &= 0xf3;   // SPACE
        if (kbd_key == KBD_KEY_DEL) KEYBD[PA1] &= 0xf6;   // LEFT/BS
        if (kbd_key == KBD_KEY_RET) KEYBD[PA3] &= 0xf6;   // RETURN
        if (kbd_key == KBD_KEY_LEFT)  KEYBD[PA1] &= 0xf6; // LEFT ARROW
        if (kbd_key == KBD_KEY_RIGHT) KEYBD[PA2] &= 0x7f; // RIGHT ARROW

        if (key_shift)              KEYBD[PA1] &= 0x7f;   // SHIFT
        if (key_ctrl)               KEYBD[PA0] &= 0x7f;   // CTRL
        if (kbd_key == KBD_KEY_F1)  Int6502(&m6502, INT_NMI);  // Game Reset (note, this is needed to start games)
    }
    
    // For the full keyboard overlay... this is a bit of a hack for SHIFT and CTRL
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
        KEYBD[PA1] &= 0x7f;   // SHIFT
    }
    if (last_special_key == KBD_KEY_CTRL)  
    {
        DSPrint(4,0,6, "CTRL");
        KEYBD[PA0] &= 0x7f;   // CTRL
    }
    if ((kbd_key != 0) && (kbd_key != KBD_KEY_SHIFT) && (kbd_key != KBD_KEY_CTRL))
    {
        if (last_special_key_dampen == 20) last_special_key_dampen = 19;
    }
}


// ========================================================================================
// CreatiVision Memory Map (simplified):
//
// $0000 - $03FF: 1K RAM (mirrored at $0400 - $07FF, $0800 - $0BFF, $0C00 - $0FFF)
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
            if (myConfig.mirrorRAM == COLECO_RAM_NORMAL_MIRROR) // Mirror RAM? Nothing really relies on this but a real CreatiVision machine will 'see' the replication.
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
            break;

        // Expanded RAM... very little uses this... but for future homebrews or for CSL bios use.
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
ITCM_CODE byte Rd6502_full(register word Addr)
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
    last_special_key = 0;
    last_special_key_dampen = 0;
    
    memset(RAM_Memory+0x1000, 0xFF, 0xE800);    // Set all bytes to unused/unconnected (0xFF) between RAM and the BIOS at 0xF800
    memset(RAM_Memory+0x4000, 0x00, 0x8000);    // Then blank (zero) everything between RAM and the BIOS at 0xF800 - we use this for expanded RAM

    if (myConfig.cvisionLoad == 3) // Special load of CSL or similar BIOS into C000-FFFF (overwrites normal BIOS)
    {
        memcpy(RAM_Memory+0xC000, ROM_Memory, romSize);
    }
    else if (myConfig.cvisionLoad == 2)  // 32K BANKSWAP (some ROMs for MegaCart use are in this format)
    {
        memcpy(RAM_Memory+0x4000, ROM_Memory+0x4000, romSize/2);
        memcpy(RAM_Memory+0x8000, ROM_Memory, romSize/2);
    }
    else if (myConfig.cvisionLoad == 1)  // Linear Load (this is the sensible format... loading right up against C000h and leaving the rest of the middle 32K free for RAM)
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


/** LoadListing() ********************************************/
/** Loads "load.txt" data into VRAM from $1C00 to $3FFF     **/
/** (used to load BASIC listings)                           **/
/**                                                         **/
/** Taken from FunnyMU 0.49 and adapted for ColecoDS use.   **/
/*************************************************************/
char line[64];
char temp[64];
int count=0;
void creativision_loadBAS(void)  // Taken from FunnyMU and adapted to reduce memory and stack usage... less error checking but we can't afford the large buffer on the small DS
{
  #define START_CHECK   0x1800
  #define START_VID     0x1c00
  #define END_VID       0x4000
  #define SECOND_CHECK  0x1400
  #define MEM_SIZE (END_VID - START_VID)
    
  // We always load a .BAS file that has the same base filename as the ROM we loaded
  strcpy(line, disk_last_file[0]);
  int j = strlen(line)-1;
  while (line[j] != '.') j--;
  line[++j]='B';
  line[++j]='A';
  line[++j]='S';
  line[++j]= 0;    

  FILE *fp;
  fp=fopen(line, "r");
  if (fp) 
  {
    int prevlen = 0xdc00;
    short int offset_mem = 0;
    short int offset_check = 0;
      
    memset(&pVDPVidMem[START_CHECK], 0xff, START_VID-START_CHECK);
    memset(&pVDPVidMem[START_VID], 0x00, END_VID-START_VID);
      
    while (!feof(fp) && fgets(line, 64, fp)) 
    {
      int start = 0;
      short int len = (short int)strlen(line);
      short int num_line = -1;

      while (start < len && line[start] != ' ')             // parsing string to find line number
      {
         temp[start] = line[start]; start++;
      }
      temp[start] = 0;

      if (start > 0)                                        // ok, line number found
      {
        num_line = (short int)atoi(temp);                   // this is the line number
        while (start < len && line[start] == ' ') start++;  // skip white spaces after line number
        memcpy(temp, &line[start], len - start + 1);        // inserting only allowed line numbers
          
        sprintf(line, "%-4u %s", (unsigned)num_line, temp); // formatting line
          
        short int cc = 0;

        while (cc < (short int)strlen(line) && line[cc] != 0x0d && line[cc] != 0x0a)   // copy formatted line into VDP memory
        {
            pVDPVidMem[START_VID + offset_mem++] = line[cc++];
        }
        pVDPVidMem[START_VID + offset_mem++] = 0x0d;        // adding end of line

        unsigned char high = (prevlen / 256);
        unsigned char low = (unsigned char)(prevlen - ((int)high * 256));

        pVDPVidMem[SECOND_CHECK + offset_check * 2] = high;
        pVDPVidMem[SECOND_CHECK + offset_check * 2+1] = low;

        prevlen += (cc + 1);

        high = (num_line / 256);
        low = (unsigned char)(num_line - (high * 256));

        pVDPVidMem[START_CHECK + offset_check * 2] = high;
        pVDPVidMem[START_CHECK + offset_check * 2+1] = low;

        offset_check++;
      }
    }
    fclose(fp);
  }
}

int creativision_debug_CPU(int idx)
{
    char tmp[34];
    sprintf(tmp, "6502PC %04X", m6502.PC);
    DSPrint(0,idx++,7, tmp);
    sprintf(tmp, "6502-S %04X", m6502.S);
    DSPrint(0,idx++,7, tmp);
    sprintf(tmp, "6502-A %04X", m6502.A);
    DSPrint(0,idx++,7, tmp);
    sprintf(tmp, "6502-X %04X", m6502.X);
    DSPrint(0,idx++,7, tmp);
    sprintf(tmp, "6502-Y %04X", m6502.Y);
    DSPrint(0,idx++,7, tmp);
    idx++;
    idx++;
  
    return idx;
}
// End of file
