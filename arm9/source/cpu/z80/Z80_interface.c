// ---------------------------------------------------------------------------------
// This file is our bridge between the Z80 CPU core and the rest of the system.
// ColecoDS currently supports 2 CPU cores:
//    DRZ80 - Fast but not 100% compatible (some games won't run with it)
//    CZ80  - Slower but much higher compatibility (about 10% slower)
// ---------------------------------------------------------------------------------
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Z80_interface.h"
#include "../../colecoDS.h"
#include "../../colecomngt.h"

#define DR_INT_IRQ 0x01
#define DR_NMI_IRQ 0x02

// ----------------------------------------------------------------------------
// Put the whole Z80 register set into fast memory for better performance...
// ----------------------------------------------------------------------------
struct DrZ80 drz80 __attribute((aligned(4))) __attribute__((section(".dtcm")));

u16 cpuirequest     __attribute__((section(".dtcm"))) = 0;
u8 lastBank         __attribute__((section(".dtcm"))) = 199;
s32 cycle_deficit   __attribute__((section(".dtcm"))) = 0;
u8 lastBlock[4]     __attribute__((section(".dtcm"))) = {99,99,99,99};
u32 msx_offset      __attribute__((section(".dtcm"))) = 0;

extern u8 romBankMask;
extern u8 romBuffer[];
extern u8 Slot1ROM[];

// -----------------------------
// Normal 8-bit Read... fast!
// -----------------------------
u8 cpu_readmem16 (u16 address) {
  return (pColecoMem[address]);
}

// -------------------------------------------------
// Switch banks... do this as fast as possible..
// Up to 256K of the Colecovision Mega Cart ROM can 
// be stored in fast VRAM so we check that here.
// -------------------------------------------------
ITCM_CODE void BankSwitch(u8 bank)
{
    if (lastBank != bank)   // Only if the bank was changed...
    {
        u32 *src;
        u32 *dest = (u32*)(pColecoMem+0xC000);
        if (bank < 8)   // If bank area under 128k - grab from first VRAM chunk of memory - it's faster
        {
            src = (u32*)((u32)0x06880000 + ((u32)bank * (u32)0x4000));
            DC_FlushRange(dest, 0x4000);
            dmaCopyWords(3, src, dest, 0x4000);                            
        }
        else if (bank < 16)   // If bank area between 128k and 256k - grab from the other VRAM area - it's faster
        {
            src = (u32*)((u32)0x06820000 + ((u32)(bank-8) * (u32)0x4000));
            DC_FlushRange(dest, 0x4000);
            dmaCopyWords(3, src, dest, 0x4000);                            
        }
        else  // It's in main memory... best we can do is copy 4 bytes at a time
        {
            src = (u32*)((u32)romBuffer + ((u32)bank * (u32)0x4000));
            for (int i=0; i<0x1000; i++) *dest++ = *src++;       // Copy 4 bytes at a time for best speed...          
        }
        lastBank = bank;
    }
}

// -------------------------------------------------
// 8-bit read with bankswitch support... slower...
// -------------------------------------------------
ITCM_CODE u8 cpu_readmem16_banked (u16 address) 
{
  if (bMagicMegaCart) // Handle Megacart Hot Spots
  {
      if (address >= 0xFFC0)
      {
          BankSwitch(address & romBankMask);
      }
  }    
  return (pColecoMem[address]);
}

// ---------------------------------------------------------------------
// If the memory is in our VRAM cache, use dmaCopy() which is faster 
// but if our memory is out in normal RAM, just use loop copy.
// ---------------------------------------------------------------------
ITCM_CODE void MSXBlockCopy(u32* src, u32* dest, u16 size)
{
    if (msx_offset < 0x28000)  // In VRAM so DMA copy will be faster
    {
        DC_FlushRange(dest, size);
        dmaCopyWords(3, src, dest, size);                            
    }
    else  // In normal memory, just use loop copy
    {
        for (u16 i=0; i<(size/4); i++)  *dest++ = *src++;
    }
}


// -----------------------------------------------------------------------
// Zemina 8K mapper:
//Page (8kB)	Switching address	Initial segment
//4000h~5FFFh (mirror: C000h~DFFFh)	4000h (mirrors: 4001h~5FFFh)	0
//6000h~7FFFh (mirror: E000h~FFFFh)	6000h (mirrors: 6001h~7FFFh)	1
//8000h~9FFFh (mirror: 0000h~1FFFh)	8000h (mirrors: 8001h~9FFFh)	2
//A000h~BFFFh (mirror: 2000h~3FFFh)	A000h (mirrors: A001h~BFFFh)	3
// -----------------------------------------------------------------------
void HandleZemina8K(u32* src, u8 block, u16 address)
{
    if (bROMInSlot[1] && (address >= 0x4000) && (address < 0x6000))
    {
        if (lastBlock[0] != block)
        {
            u32 *dest = (u32*)(pColecoMem+0x4000);
            Slot1ROMPtr[2] = (u8*)src;  // Main ROM
            Slot1ROMPtr[6] = (u8*)src;  // Mirror
            MSXBlockCopy(src, dest, 0x2000);
            lastBlock[0] = block;
        }
    }
    else if (bROMInSlot[1] && (address >= 0x6000) && (address < 0x8000))
    {
        if (lastBlock[1] != block)
        {
            u32 *dest = (u32*)(pColecoMem+0x6000);
            Slot1ROMPtr[3] = (u8*)src;  // Main ROM
            Slot1ROMPtr[7] = (u8*)src;  // Mirror
            MSXBlockCopy(src, dest, 0x2000);
            lastBlock[1] = block;
        }
    }
    else if (bROMInSlot[2] && (address >= 0x8000) && (address < 0xA000))
    {
        if (lastBlock[2] != block)
        {
            u32 *dest = (u32*)(pColecoMem+0x8000);
            Slot1ROMPtr[4] = (u8*)src;  // Main ROM
            Slot1ROMPtr[0] = (u8*)src;  // Mirror                            
            MSXBlockCopy(src, dest, 0x2000);
            lastBlock[2] = block;
        }
    }
    else if (bROMInSlot[2] && (address >= 0xA000) && (address < 0xC000))
    {
        if (lastBlock[3] != block)
        {
            u32 *dest = (u32*)(pColecoMem+0xA000);
            Slot1ROMPtr[5] = (u8*)src;  // Main ROM
            Slot1ROMPtr[1] = (u8*)src;  // Mirror                            
            MSXBlockCopy(src, dest, 0x2000);
            lastBlock[3] = block;
        }
    }
}    

// -------------------------------------------------------------------------
// The ZENMIA 16K Mapper:
// 4000h~7FFFh 	via writes to 4000h-7FFF
// 8000h~BFFFh 	via writes to 8000h-BFFF
// -------------------------------------------------------------------------
void HandleZemina16K(u32* src, u8 block, u16 address)
{
    if (bROMInSlot[1] && (address >= 0x4000) && (address < 0x8000))
    {
        if (lastBlock[0] != block)
        {
            u32 *dest = (u32*)(pColecoMem+0x4000);
            Slot1ROMPtr[2] = (u8*)src;
            Slot1ROMPtr[3] = (u8*)src+0x2000;
            // Mirrors
            Slot1ROMPtr[6] = (u8*)src;
            Slot1ROMPtr[7] = (u8*)src+0x2000;
            MSXBlockCopy(src, dest, 0x4000);
            if (bROMInSlot[3]) 
            {
                src = (u32*)Slot1ROMPtr[2];
                dest = (u32*)(pColecoMem+0xC000);
                MSXBlockCopy(src, dest, 0x4000);
            }
            lastBlock[0] = block;
        }
    }
    else if (bROMInSlot[1] && (address >= 0x8000) && (address < 0xC000))
    {
        if (lastBlock[1] != block)
        {
            u32 *dest = (u32*)(pColecoMem+0x8000);
            Slot1ROMPtr[4] = (u8*)src;
            Slot1ROMPtr[5] = (u8*)src+0x2000;
            // Mirrors
            Slot1ROMPtr[0] = (u8*)src;
            Slot1ROMPtr[1] = (u8*)src+0x2000;
            if (bROMInSlot[2]) MSXBlockCopy(src, dest, 0x4000);
            if (bROMInSlot[0]) 
            {
                src = (u32*)Slot1ROMPtr[4];
                dest = (u32*)(pColecoMem+0x0000);
                MSXBlockCopy(src, dest, 0x4000);
            }            
            lastBlock[1] = block;
        }
    }
}    
    
// -------------------------------------------------------------------------
// The ASCII 16K Mapper:
// 4000h~7FFFh 	via writes to 6000h
// 8000h~BFFFh 	via writes to 7000h or 77FFh
// -------------------------------------------------------------------------
void HandleAscii16K(u32* src, u8 block, u16 address)
{
    if (bROMInSlot[1] && (address >= 0x6000) && (address < 0x7000))
    {
        if (lastBlock[0] != block)
        {
            Slot1ROMPtr[2] = (u8*)src;
            Slot1ROMPtr[3] = (u8*)src+0x2000;
            // Mirrors
            Slot1ROMPtr[6] = (u8*)src;
            Slot1ROMPtr[7] = (u8*)src+0x2000;
            MSXBlockCopy(src, (u32*)(pColecoMem+0x4000), 0x4000);            
            if (bROMInSlot[3]) MSXBlockCopy(src, (u32*)(pColecoMem+0xC000), 0x4000);
            lastBlock[0] = block;
        }
    }
    else if (bROMInSlot[1] && (address >= 0x7000) && (address < 0x8000))
    {
        if ((file_crc == 0xfea70207) && (address != 0x7000)) return;  // Vaxol writes garbage to 7xxx so we ignore that
        
        if (lastBlock[1] != block)
        {
            Slot1ROMPtr[4] = (u8*)src;
            Slot1ROMPtr[5] = (u8*)src+0x2000;
            // Mirrors
            Slot1ROMPtr[0] = (u8*)src;
            Slot1ROMPtr[1] = (u8*)src+0x2000;
            if (bROMInSlot[2]) MSXBlockCopy(src, (u32*)(pColecoMem+0x8000), 0x4000);
            if (bROMInSlot[0]) MSXBlockCopy(src, (u32*)(pColecoMem+0x0000), 0x4000);
            lastBlock[1] = block;
        }
    }
}

// ------------------------------------------------------------------
// Write memory handles both normal writes and bankswitched since
// write is much less common than reads...   We handle the MSX
// Konami 8K, SCC and ASCII 8K mappers directly here for max speed.
// ------------------------------------------------------------------
ITCM_CODE void cpu_writemem16 (u8 value,u16 address) 
{
    extern u8 sRamAtE000_OK;
    extern u8 sgm_enable;
    extern u16 sgm_low_addr;
    
    // -------------------------------------------------------------
    // If SG-1000 mode, we provide the Dhajee RAM expansion...
    // We aren't doing a lot of error/bounds checking here - we 
    // are going to assume well-behaved .sg ROMs as this is 
    // primarily a Colecovision emu with partial SG-1000 support.
    // -------------------------------------------------------------
    if (sg1000_mode)
    {
        if (address >= 0x8000)
        {
            pColecoMem[address]=value;  // Allow pretty much anything above the base ROM area
        }
        else
        {        
            if (address >= 0x2000 && address < 0x4000)
            {
                pColecoMem[address]=value;  // Allow writes in this 8K range to emulate DahJee RAM expander
            }
        }        
    }
    // ----------------------------------------------------------------------------------
    // For the Sord M5, RAM is at 0x7000 and we emulate the 32K RAM Expander above that
    // ----------------------------------------------------------------------------------
    else if (sordm5_mode)
    {
        if (address >= 0x7000)
        {
            pColecoMem[address]=value;  // Allow pretty much anything above the base ROM area
        }
    }
    // ----------------------------------------------------------------------------------------------------------
    // For the MSX, we support a 64K main RAM machine plus some of the more common memory mappers...
    // ----------------------------------------------------------------------------------------------------------
    else if (msx_mode)
    {
        // -------------------------------------------------------
        // First see if this is a write to a RAM enabled slot...
        // -------------------------------------------------------
        if (bRAMInSlot[0] && (address < 0x4000))
        {
            pColecoMem[address]=value;  // Allow write - this is a RAM mapped slot
        }
        else if (bRAMInSlot[1] && (address >= 0x4000) && (address <= 0x7FFF))
        {
            pColecoMem[address]=value;  // Allow write - this is a RAM mapped slot
        }
        else if (bRAMInSlot[2] && (address >= 0x8000) && (address <= 0xBFFF))
        {
            pColecoMem[address]=value;  // Allow write - this is a RAM mapped slot
        }
        else if (bRAMInSlot[3] && (address >= 0xC000))
        {
            pColecoMem[address]=value;  // Allow write - this is a RAM mapped slot
        }
        else    // Check for MSX Mappers Mappers
        {
            if (mapperMask)
            {
                // ---------------------------------------------------------
                // Up to 128K we can fetch from fast VRAM shadow copy
                // otherwise we are forced to fetch from slow romBuffer[]
                // ---------------------------------------------------------
                u32 *src;
                u32 block = (value & mapperMask);
                msx_offset = block * ((mapperType == ASC16 || mapperType == ZEN16) ? 0x4000:0x2000);

                // -------------------------------------------------------------------------------------------------------
                // Up to 128K is in first VRAM area then next 32K is in second VRAM area and finally we're in normal RAM
                // -------------------------------------------------------------------------------------------------------
                if      (msx_offset < 0x20000) src = (u32*)(0x06880000 + msx_offset);
                else if (msx_offset < 0x28000) src = (u32*)(0x06820000 + (msx_offset - 0x20000));
                else    src = (u32*)((u8*)romBuffer + msx_offset);
            
                // ---------------------------------------------------------------------------------
                // The Konami 8K Mapper without SCC:
                // 4000h-5FFFh - fixed ROM area (not swappable)
                // 6000h~7FFFh (mirror: E000h~FFFFh)	6000h (mirrors: 6001h~7FFFh)	1
                // 8000h~9FFFh (mirror: 0000h~1FFFh)	8000h (mirrors: 8001h~9FFFh)	Random
                // A000h~BFFFh (mirror: 2000h~3FFFh)	A000h (mirrors: A001h~BFFFh)	Random
                // ---------------------------------------------------------------------------------
                if (mapperType == KON8)
                {
                    if (bROMInSlot[1] && (address == 0x4000))
                    {
                        if (lastBlock[0] != block)
                        {
                            Slot1ROMPtr[2] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[6] = (u8*)src;  // Mirror
                            MSXBlockCopy(src, (u32*)(pColecoMem+0x4000), 0x2000);
                            lastBlock[0] = block;
                        }
                    }
                    else if (bROMInSlot[1] && (address == 0x6000))
                    {
                        if (lastBlock[1] != block)
                        {
                            Slot1ROMPtr[3] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[7] = (u8*)src;  // Mirror
                            MSXBlockCopy(src, (u32*)(pColecoMem+0x6000), 0x2000);
                            lastBlock[1] = block;
                        }
                    }
                    else if (bROMInSlot[2] && (address == 0x8000))
                    {
                        if (lastBlock[2] != block)
                        {
                            Slot1ROMPtr[4] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[0] = (u8*)src;  // Mirror                            
                            MSXBlockCopy(src, (u32*)(pColecoMem+0x8000), 0x2000);
                            lastBlock[2] = block;
                        }
                    }
                    else if (bROMInSlot[2] && (address == 0xA000))
                    {
                        if (lastBlock[3] != block)
                        {
                            Slot1ROMPtr[5] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[1] = (u8*)src;  // Mirror       
                            MSXBlockCopy(src, (u32*)(pColecoMem+0xA000), 0x2000);
                            lastBlock[3] = block;
                        }
                    }
                }
                else if (mapperType == ASC8)
                {
                    // -------------------------------------------------------------------------
                    // The ASCII 8K Mapper:
                    // 4000h~5FFFh (mirror: C000h~DFFFh)	6000h (mirrors: 6001h~67FFh)	0
                    // 6000h~7FFFh (mirror: E000h~FFFFh)	6800h (mirrors: 6801h~68FFh)	0
                    // 8000h~9FFFh (mirror: 0000h~1FFFh)	7000h (mirrors: 7001h~77FFh)	0
                    // A000h~BFFFh (mirror: 2000h~3FFFh)	7800h (mirrors: 7801h~7FFFh)	0     
                    // -------------------------------------------------------------------------
                    if (bROMInSlot[1] && (address == 0x6000))
                    {
                        if (lastBlock[0] != block)
                        {
                            Slot1ROMPtr[2] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[6] = (u8*)src;  // Mirror
                            MSXBlockCopy(src, (u32*)(pColecoMem+0x4000), 0x2000);
                            if (bROMInSlot[3])
                            {
                                MSXBlockCopy(src, (u32*)(pColecoMem+0xC000), 0x2000);
                            }
                            lastBlock[0] = block;
                        }
                    }
                    else if (bROMInSlot[1] && (address == 0x6800))
                    {
                        if (lastBlock[1] != block)
                        {
                            Slot1ROMPtr[3] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[7] = (u8*)src;  // Mirror
                            MSXBlockCopy(src, (u32*)(pColecoMem+0x6000), 0x2000);
                            if (bROMInSlot[3])
                            {
                                MSXBlockCopy(src, (u32*)(pColecoMem+0xE000), 0x2000);
                            }
                            lastBlock[1] = block;
                        }
                    }
                    else if (bROMInSlot[1] && (address == 0x7000))
                    {
                        if (lastBlock[2] != block)
                        {
                            Slot1ROMPtr[4] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[0] = (u8*)src;  // Mirror    
                            if (bROMInSlot[2])
                            {
                                MSXBlockCopy(src, (u32*)(pColecoMem+0x8000), 0x2000);
                            }
                            if (bROMInSlot[0])
                            {
                                MSXBlockCopy(src, (u32*)(pColecoMem+0x0000), 0x2000);
                            }                            
                            lastBlock[2] = block;
                        }
                    }
                    else if (bROMInSlot[1] && (address == 0x7800))
                    {
                        if (lastBlock[3] != block)
                        {
                            Slot1ROMPtr[5] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[1] = (u8*)src;  // Mirror                            
                            if (bROMInSlot[2]) 
                            {
                                MSXBlockCopy(src, (u32*)(pColecoMem+0xA000), 0x2000);
                            }
                            if (bROMInSlot[0])
                            {
                                MSXBlockCopy(src, (u32*)(pColecoMem+0x2000), 0x2000);
                            }                            
                            lastBlock[3] = block;
                        }
                    }
                }
                else if (mapperType == SCC8)
                {
                    // --------------------------------------------------------
                    // Konami 8K mapper with SCC 
                    //	Bank 1: 4000h - 5FFFh - mapped via writes to 5000h
                    //	Bank 2: 6000h - 7FFFh - mapped via writes to 7000h
                    //	Bank 3: 8000h - 9FFFh - mapped via writes to 9000h
                    //	Bank 4: A000h - BFFFh - mapped via writes to B000h
                    // --------------------------------------------------------
                    if (bROMInSlot[1] && (address == 0x5000))
                    {
                        if (lastBlock[0] != block)
                        {
                            Slot1ROMPtr[2] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[6] = (u8*)src;  // Mirror
                            MSXBlockCopy(src, (u32*)(pColecoMem+0x4000), 0x2000);
                            lastBlock[0] = block;
                        }
                    }
                    else if (bROMInSlot[1] && (address == 0x7000))
                    {
                        if (lastBlock[1] != block)
                        {
                            Slot1ROMPtr[3] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[7] = (u8*)src;  // Mirror
                            MSXBlockCopy(src, (u32*)(pColecoMem+0x6000), 0x2000);
                            lastBlock[1] = block;
                        }
                    }
                    else if (bROMInSlot[2] && (address == 0x9000))
                    {
                        if ((value&0x3F) == 0x3F) return;       // SCC sound isn't supported anyway - just return and save the CPU cycles
                        if (lastBlock[2] != block)
                        {
                            Slot1ROMPtr[4] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[0] = (u8*)src;  // Mirror
                            MSXBlockCopy(src, (u32*)(pColecoMem+0x8000), 0x2000);
                            lastBlock[2] = block;
                        }
                    }
                    else if (bROMInSlot[2] && (address == 0xB000))
                    {
                        if (lastBlock[3] != block)
                        {
                            Slot1ROMPtr[5] = (u8*)src;  // Main ROM
                            Slot1ROMPtr[1] = (u8*)src;  // Mirror
                            MSXBlockCopy(src, (u32*)(pColecoMem+0xA000), 0x2000);
                            lastBlock[3] = block;
                        }
                    }                        
                }
                else if (mapperType == ASC16)
                {
                    HandleAscii16K(src, block, address);
                }
                else if (mapperType == ZEN8)
                {
                    HandleZemina8K(src, block, address);
                }
                else if (mapperType == ZEN16)
                {
                    HandleZemina16K(src, block, address);
                }                
            }
        }
    }
    // -----------------------------------------------------------
    // If the Super Game Module has been enabled, we have a much 
    // wider range of RAM that can be written (and no mirroring)
    // -----------------------------------------------------------
    else if (sgm_enable)
    {
        if ((address >= sgm_low_addr) && (address < 0x8000)) pColecoMem[address]=value;
    }    
    else if((address>0x5FFF)&&(address<0x8000)) // Normal memory RAM write... with mirrors...
    {
        address&=0x03FF;
        pColecoMem[0x6000+address]=pColecoMem[0x6400+address]=pColecoMem[0x6800+address]=pColecoMem[0x6C00+address]=
        pColecoMem[0x7000+address]=pColecoMem[0x7400+address]=pColecoMem[0x7800+address]=pColecoMem[0x7C00+address]=value;
    }
    else if ((address >= 0xE000) && (address < 0xE800)) // Allow SRAM if cart doesn't extend this high...
    {
        if (sRamAtE000_OK) pColecoMem[address+0x800]=value;
    }
    else if (address >= 0xFFC0)
    {
        if (bMagicMegaCart) BankSwitch(address & romBankMask);  // Handle Megacart Hot Spot writes (don't think anyone actually uses this but it's possible)
    }
    /* Activision PCB Cartridges, potentially containing EEPROM, use [1111 1111 10xx 0000] addresses for hotspot bankswitch */
    else if (bActivisionPCB)
    {
      if ((address == 0xFF90) || (address == 0xFFA0) || (address == 0xFFB0))
      {
          BankSwitch((address>>4) & romBankMask);
      }
    }     
}


// -----------------------------------------------------------------
// All functions below are interfaces into the DrZ80 core
// -----------------------------------------------------------------

// -----------------------------------------------------------------
// The 16-bit write simply makes 2 calls into the 8-bit writes...
// -----------------------------------------------------------------
void drz80MemWriteW(u16 data,u16 addr) 
{
    cpu_writemem16(data & 0xff , addr);
    cpu_writemem16(data>>8,addr+1);
}

u32 z80_rebaseSP(u16 address) {
  drz80.Z80SP_BASE = (unsigned int) pColecoMem;
  drz80.Z80SP      = drz80.Z80SP_BASE + address;
  return (drz80.Z80SP);
}

u32 z80_rebasePC(u16 address) {
  drz80.Z80PC_BASE = (unsigned int) pColecoMem;
  drz80.Z80PC      = drz80.Z80PC_BASE + address;
  return (drz80.Z80PC);
}

void z80_irq_callback(void) {
	drz80.Z80_IRQ = 0x00;
}


void DrZ80_Cause_Interrupt(int type) 
{
    if (type == Z80_NMI_INT) 
    {
        drz80.pending_irq |= DR_NMI_IRQ;
    }
    else if (type != Z80_IGNORE_INT) 
    {
        drz80.z80irqvector = type & 0xff;
        drz80.pending_irq |= DR_INT_IRQ;
    }
}

 void DrZ80_Clear_Pending_Interrupts(void) 
{
    drz80.pending_irq = 0;
    drz80.Z80_IRQ = 0;
}

 void DrZ80_Interrupt(void) 
{
    if (drz80.pending_irq & DR_NMI_IRQ)  /* NMI IRQ */
    {
        drz80.Z80_IRQ = DR_NMI_IRQ;
        drz80.pending_irq &= ~DR_NMI_IRQ;
    } 
    else if (drz80.Z80IF & 1)  /* INT IRQ and Interrupts enabled */
    {
        drz80.Z80_IRQ = DR_INT_IRQ;
        drz80.pending_irq &= ~DR_INT_IRQ;
    }
}

// -----------------------------
// Normal 16-bit Read... fast!
// -----------------------------
 u16 drz80MemReadW(u16 address) 
{
    return (pColecoMem[address]  |  (pColecoMem[address+1] << 8));
}

// -------------------------------------------------
// 16-bit read with bankswitch support... slower...
// -------------------------------------------------
 u16 drz80MemReadW_banked(u16 addr) 
{
  if (bMagicMegaCart) // Handle Megacart Hot Spots
  {
      return (cpu_readmem16_banked(addr) | (cpu_readmem16_banked(addr+1)<<8));   // These handle both hotspots - slower but easier than reproducing the hotspot stuff
  }    
  return (pColecoMem[addr]  |  (pColecoMem[addr+1] << 8));
}

// ------------------------------------------------------
// DrZ80 uses pointers to various read/write memory/IO
// ------------------------------------------------------
void DrZ80_InitHandlers() {
  extern u8 romBankMask;
  drz80.z80_write8=cpu_writemem16;
  drz80.z80_write16=drz80MemWriteW;
    
  if (msx_mode)
  {
      drz80.z80_in=cpu_readport_msx;
      drz80.z80_out=cpu_writeport_msx;    
  }
  else
  {
      drz80.z80_in=(sg1000_mode ? cpu_readport_sg:cpu_readport16);
      drz80.z80_out=(sg1000_mode ? cpu_writeport_sg:cpu_writeport16);    
  }    
    
  drz80.z80_read8= (romBankMask ? cpu_readmem16_banked : cpu_readmem16 );
  drz80.z80_read16= (romBankMask ? drz80MemReadW_banked : drz80MemReadW);
  drz80.z80_rebasePC=(unsigned int (*)(short unsigned int))z80_rebasePC;
  drz80.z80_rebaseSP=(unsigned int (*)(short unsigned int))z80_rebaseSP;
  drz80.z80_irq_callback=z80_irq_callback;
}


void DrZ80_Reset(void) {
  memset (&drz80, 0, sizeof(struct DrZ80));
  DrZ80_InitHandlers();

  drz80.Z80A = 0x00 <<24;
  drz80.Z80F = (1<<2); // set ZFlag 
  drz80.Z80BC = 0x0000	<<16;
  drz80.Z80DE = 0x0000	<<16;
  drz80.Z80HL = 0x0000	<<16;
  drz80.Z80A2 = 0x00 <<24;
  drz80.Z80F2 = 1<<2;  // set ZFlag 
  drz80.Z80BC2 = 0x0000 <<16;
  drz80.Z80DE2 = 0x0000 <<16;
  drz80.Z80HL2 = 0x0000 <<16;
  drz80.Z80IX = 0xFFFF	<<16;
  drz80.Z80IY = 0xFFFF	<<16;
  drz80.Z80I = 0x00;
  drz80.Z80IM = 0x00;
  drz80.Z80_IRQ = 0x00;
  drz80.Z80IF = 0x00;
  drz80.Z80PC=z80_rebasePC(0);
  drz80.Z80SP=z80_rebaseSP(0xF000); 
  drz80.z80intadr = 0x38;

  DrZ80_Clear_Pending_Interrupts();
  cpuirequest=0;
  lastBank = 199;
  cycle_deficit = 0;
    
  memset(lastBlock, 99, 4);
}


// --------------------------------------------------
// Execute the asked-for cycles and keep track
// of any deficit so we can apply that to the 
// next call (since we are very unlikely to 
// produce exactly an evenly-divisible number of
// cycles for a given scanline...
// --------------------------------------------------
 int DrZ80_execute(int cycles) 
{
  drz80.cycles = cycles + cycle_deficit;
    
  DrZ80Run(&drz80, cycles);

  cycle_deficit = drz80.cycles;
  return (cycle_deficit);
}


// End of file
