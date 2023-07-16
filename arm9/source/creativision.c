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

#include "colecogeneric.h"
#include "cpu/m6502/M6502.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/sn76496/SN76496.h"
#define NORAM 0xFF

M6502 m6502 __attribute__((section(".dtcm")));        // Our core 6502 CPU

extern u8 RAM_Memory[];
extern u8 ROM_Memory[];
extern u8 CreativisionBios[];
extern byte Loop9918(void);
extern SN76496 sncol;

#define KBD_KEY_RET 20


/* PIA handling courtesy of the creatiVision emulator:  https://sourceforge.net/projects/creativisionemulator/ 
   and used with permission. Do not use this code unless you contact the authors of the emulator at the URL above */
#define PA0 1
#define PA1 2
#define PA2 3
#define PA3 4

#define PIA_INPUT    0
#define PIA_OUTALL 0xff
#define PIA_KEY0   0xf7
#define PIA_KEY1   0xfb
#define PIA_KEY2   0xfd
#define PIA_KEY3   0xfe
#define PIA_PDR    4
#define PIA_DDR    0

typedef struct {
    int PDR;  /* Peripheral Data Register */
    int DDR;  /* Data Direction Register */
    int CR;   /* Control Register */
    unsigned long long int prev_cycles;
} M6821;

/* PIA 0 and 1 */
M6821 pia0 = {0};
M6821 pia1 = {0};

int total_cycles = 0;

int EmuQuit = 0;
int have_io = 0;
unsigned char KEYBD[8] = { 255, 255, 255, 255, 255, 255, 255, 255 };
unsigned char JOYSM[2] = { 255, 255 };
int io_timeout = 0;
 								   
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
                sn76496W(data, &sncol);
                //SN76496SPWrite(0,data);
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
unsigned char PIA_Read(word addr)
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
                pia1.PDR = KEYBD[4];
                break;
            case 0xfb:      /* Keyboard Mux 4 */
                pia1.PDR = KEYBD[3];
                break;
            case 0xfd:      /* Keyboard Mux 2 */
                pia1.PDR = KEYBD[2];
                break;
            case 0xfe:      /* Keyboard Mux 1 */
                pia1.PDR = KEYBD[1];
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


#define JST_UP              0x0100
#define JST_RIGHT           0x0200
#define JST_DOWN            0x0400
#define JST_LEFT            0x0800
#define JST_FIRER           0x0040
#define JST_FIREL           0x4000
#define JST_0               0x0005
#define JST_1               0x0002
#define JST_2               0x0008
#define JST_3               0x0003
#define JST_4               0x000D
#define JST_5               0x000C
#define JST_6               0x0001
#define JST_7               0x000A
#define JST_8               0x000E
#define JST_9               0x0004
#define JST_STAR            0x0006
#define JST_POUND           0x0009
#define JST_PURPLE          0x0007
#define JST_BLUE            0x000B
#define JST_RED             JST_FIRER
#define JST_YELLOW          JST_FIREL

extern u8 kbd_key;

void creativision_input(void)
{
    extern u32 JoyState;
    
    KEYBD[4] = 0xFF;
    KEYBD[3] = 0xFF;
    KEYBD[2] = 0xFF;
    KEYBD[1] = 0xFF;
    
    if (JoyState & JST_FIREL)   KEYBD[1] &= 0x7f;  // P1 Right Button
    if (JoyState & JST_FIRER)   KEYBD[2] &= 0x7f;  // P1 Left Button

    if (JoyState & JST_UP)      KEYBD[1] &= 0xf7;  // P1 up
    if (JoyState & JST_DOWN)    KEYBD[1] &= 0xfd;  // P1 down
    if (JoyState & JST_LEFT)    KEYBD[1] &= 0xdf;  // P1 left
    if (JoyState & JST_RIGHT)   KEYBD[1] &= 0xfb;  // P1 right

    if (JoyState == JST_1)      KEYBD[1] &= 0xf3;  // 1      
    if (JoyState == JST_2)      KEYBD[2] &= 0xcf;  // 2
    if (JoyState == JST_3)      KEYBD[2] &= 0x9f;  // 3
    if (JoyState == JST_4)      KEYBD[2] &= 0xd7;  // 4
    if (JoyState == JST_5)      KEYBD[2] &= 0xb7;  // 5
    if (JoyState == JST_6)      KEYBD[2] &= 0xaf;  // 6

    if (JoyState == JST_7)      KEYBD[3] &= 0xf3;   // SPACE
    if (JoyState == JST_8)      KEYBD[4] &= 0xfa;   // Y
    if (JoyState == JST_9)      KEYBD[4] &= 0xaf;   // N
    if (JoyState == JST_0)      KEYBD[4] &= 0xf6;   // RETURN
    
    if (JoyState == JST_STAR)   Int6502(&m6502, INT_NMI);  // Game Reset (note, this is needed to start games)
    if (JoyState == JST_POUND)  KEYBD[4] &= 0xed;          // 0 but graphic shows ST=START
    
    // And now the keyboard maps...
    if (kbd_key)
    {
        if (kbd_key == '0')         KEYBD[4] &= 0xed;   // 0
        if (kbd_key == '1')         KEYBD[1] &= 0xf3;   // 1      
        if (kbd_key == '2')         KEYBD[2] &= 0xcf;   // 2
        if (kbd_key == '3')         KEYBD[2] &= 0x9f;   // 3
        if (kbd_key == '4')         KEYBD[2] &= 0xd7;   // 4
        if (kbd_key == '5')         KEYBD[2] &= 0xb7;   // 5
        if (kbd_key == '6')         KEYBD[2] &= 0xaf;   // 6
        if (kbd_key == '7')         KEYBD[4] &= 0xf9;   // 7
        if (kbd_key == '8')         KEYBD[4] &= 0xbd;   // 8
        if (kbd_key == '9')         KEYBD[4] &= 0xdd;   // 9
        
        if (kbd_key == 'A')         KEYBD[2] &= 0xee;   // A
        if (kbd_key == 'B')         KEYBD[2] &= 0xf9;   // B
        if (kbd_key == 'C')         KEYBD[2] &= 0xdd;   // C        
        if (kbd_key == 'D')         KEYBD[2] &= 0xbe;   // D
        if (kbd_key == 'E')         KEYBD[2] &= 0xeb;   // E
        if (kbd_key == 'F')         KEYBD[2] &= 0xfc;   // F
        if (kbd_key == 'G')         KEYBD[2] &= 0xfa;   // G
        if (kbd_key == 'H')         KEYBD[4] &= 0xbb;   // H
        if (kbd_key == 'I')         KEYBD[4] &= 0xbe;   // I        
        if (kbd_key == 'J')         KEYBD[4] &= 0xdb;   // J
        if (kbd_key == 'K')         KEYBD[4] &= 0xeb;   // K
        if (kbd_key == 'L')         KEYBD[4] &= 0xf3;   // L
        if (kbd_key == 'M')         KEYBD[4] &= 0xb7;   // M
        if (kbd_key == 'N')         KEYBD[4] &= 0xaf;   // N
        if (kbd_key == 'O')         KEYBD[4] &= 0xde;   // O
        if (kbd_key == 'P')         KEYBD[4] &= 0xee;   // P
        if (kbd_key == 'Q')         KEYBD[2] &= 0xe7;   // Q
        if (kbd_key == 'R')         KEYBD[2] &= 0xdb;   // R
        if (kbd_key == 'S')         KEYBD[2] &= 0xde;   // S
        if (kbd_key == 'T')         KEYBD[4] &= 0xdd;   // T
        if (kbd_key == 'U')         KEYBD[4] &= 0xfc;   // U
        if (kbd_key == 'V')         KEYBD[2] &= 0xbd;   // V
        if (kbd_key == 'W')         KEYBD[2] &= 0xf3;   // W
        if (kbd_key == 'X')         KEYBD[2] &= 0xed;   // X        
        if (kbd_key == 'Y')         KEYBD[4] &= 0xfa;   // Y
        if (kbd_key == 'Z')         KEYBD[2] &= 0xf5;   // Z
        if (kbd_key == ' ')         KEYBD[3] &= 0xf3;   // SPACE
        if (kbd_key == KBD_KEY_RET) KEYBD[4] &= 0xf6;   // RETURN
    }
}


// ========================================================================================
// Memory Map:
// 
// $0000 - $03FF: 1K RAM (mirrored thrice at $0400 - $07FF, $0800 - $0BFF, $0C00 - $0FFF)
// $1000 - $1FFF: PIA (joysticks, sound)
// $2000 - $2FFF: VDP read
// $3000 - $3FFF: VDP write
// $4000 - $7FFF: 16K ROM2 (we map RAM here if not used by ROM)
// $8000 - $BFFF: 16K ROM1 (we map RAM here if not used by ROM)
// $C000 - $E7FF: 10K Unused - we map RAM here
// $E800 - $EFFF: 2K I/O interface - we map RAM here
// $F000 - $F7FF: 2K Unused - we map RAM here
// $F800 - $FFFF: 2K ROM0 (BIOS)
// ========================================================================================
void Wr6502(register word Addr,register byte Value)
{
    switch (Addr & 0xF000)
    {
        case 0x0000:
            RAM_Memory[(Addr & 0x3FF) + 0x000] = Value;
            RAM_Memory[(Addr & 0x3FF) + 0x400] = Value;
            RAM_Memory[(Addr & 0x3FF) + 0x800] = Value;
            RAM_Memory[(Addr & 0x3FF) + 0xC00] = Value;
            break;

        case 0x1000:            // PIA
          PIA_Write(Addr, Value);
          break;
            
        case 0x3000:    // VDP Writes
            if ((Addr & 1)==0) WrData9918(Value);
            else if (WrCtrl9918(Value)) Int6502(&m6502, INT_IRQ);
            break;
            
        case 0x4000:
        case 0x5000:
        case 0x6000:
        case 0x7000:
        case 0x8000:
        case 0x9000:
        case 0xA000:
        case 0xB000:
        case 0xC000:
        case 0xD000:
        case 0xE000:
            RAM_Memory[(Addr & 0x3FF) + 0x000] = Value;
            break;
    }
}

byte Rd6502(register word Addr)
{
    switch (Addr & 0xF000)
    {
        case 0x1000:                // PIA
          return PIA_Read(Addr);      
          break;
            
        case 0x2000:  // VDP read 0x2000 to 0x2FFF
          if ((Addr & 1)==0) return(RdData9918());
          return(RdCtrl9918());
          break;
    }

    return RAM_Memory[Addr];
}

void creativision_restore_bios(void)
{
    if (myConfig.cvisionLoad != 99) // Special Laser 2001 BIOS 16K load
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
// -----------------------------------------------------------------------------------------------------------------------------------------

void creativision_loadrom(int romSize)
{
    memset(RAM_Memory+0x1000, 0xFF, 0xE800);    // Blank everything between RAM and the BIOS at 0xF800
    
    if (myConfig.cvisionLoad == 99) // Special Laser 2001 BIOS 16K load - experimental...
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
        memcpy(RAM_Memory+0xA000, ROM_Memory+0x0000, 0x2000);    // main 8Kb	at 0xA000
        memcpy(RAM_Memory+0x7800, ROM_Memory+0x2000, 0x0800);    // second 2Kb at 0x7800

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
        memcpy(RAM_Memory+0xA000, ROM_Memory+0x0000, 0x2000);   // main 8Kb	at 0xA000
        memcpy(RAM_Memory+0x7000, ROM_Memory+0x2000, 0x1000);   // second 4Kb at 0x7000
        memcpy(RAM_Memory+0x8000, RAM_Memory+0xA000, 0x2000);   // Mirror 8k at 0x8000
        memcpy(RAM_Memory+0x5000, RAM_Memory+0x7000, 0x1000);   // Mirror 4k at 0x5000
        memcpy(RAM_Memory+0x6000, RAM_Memory+0x7000, 0x1000);   // Mirror 4k at 0x6000
        memcpy(RAM_Memory+0x4000, RAM_Memory+0x7000, 0x1000);   // Mirror 4k at 0x4000
    }
    else if (romSize == 1024 * 16) // 16K
    {
        memcpy(RAM_Memory+0xA000, ROM_Memory+0x0000, 0x2000);    // main 8Kb	at 0xA000
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
        memcpy(RAM_Memory+(0xC000-romSize), ROM_Memory+0x0000, romSize);    // load linear at 4000-BFFF
    }    
}

// End of file
