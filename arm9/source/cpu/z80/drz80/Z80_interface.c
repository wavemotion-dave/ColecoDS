// ---------------------------------------------------------------------------------
// Most of this is related to the Dr Z80 code and interfaced to the ColecoDS code.
// It was ported by Alekmaul with some updates by wavemotion-dave.
// ---------------------------------------------------------------------------------
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Z80_interface.h"
#include "../../../colecoDS.h"
#include "../../../colecomngt.h"

#define INT_IRQ 0x01
#define NMI_IRQ 0x02

// ----------------------------------------------------------------------------
// Put the whole Z80 register set into fast memory for better performance...
// ----------------------------------------------------------------------------
struct DrZ80 drz80 __attribute((aligned(4))) __attribute__((section(".dtcm")));

u16 cpuirequest     __attribute__((section(".dtcm"))) = 0;
u8 lastBank         __attribute__((section(".dtcm"))) = 199;
s32 cycle_deficit   __attribute__((section(".dtcm"))) = 0;

extern u8 romBankMask;
extern u8 romBuffer[];


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

// -----------------------------
// Normal 8-bit Read... fast!
// -----------------------------
 u8 cpu_readmem16 (u16 address) {
  return (pColecoMem[address]);
}

// -----------------------------
// Normal 16-bit Read... fast!
// -----------------------------
 u16 drz80MemReadW(u16 address) 
{
    return (pColecoMem[address]  |  (pColecoMem[address+1] << 8));
}

// ------------------------------------------------
// Switch banks... do this as fast as possible..
// ------------------------------------------------
 ITCM_CODE void BankSwitch(u8 bank)
{
    if (lastBank != bank)   // Only if the bank was changed...
    {
        u32 *src;
        if (bank < 8)   // If bank area under 128 - grab from VRAM - it's faster
            src = (u32*)((u32)0x06880000 + ((u32)bank * (u32)0x4000));
        else
            src = (u32*)((u32)romBuffer + ((u32)bank * (u32)0x4000));
        u32 *dest = (u32*)(pColecoMem+0xC000);
        for (int i=0; i<0x1000; i++)
        {
            *dest++ = *src++;       // Copy 4 bytes at a time for best speed...
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



// ------------------------------------------------------------------
// Write memory handles both normal writes and bankswitched since
// write is much less common than reads... 
// ------------------------------------------------------------------
 void cpu_writemem16 (u8 value,u16 address) 
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
    // ------------------------------------------------------------------------------------------
    // For the MSX, we are only supporting a paultry 16K or 32K machine - good enough for carts
    // ------------------------------------------------------------------------------------------
    else if (msx_mode)
    {
        extern u16 msx_ram_low;
        extern u16 msx_ram_high;

        if ((address >= msx_ram_low) && (address <= msx_ram_high))
        {
            pColecoMem[address]=value;  // Allow pretty much anything above the base ROM area
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
// The 16-bit write simply makes 2 calls into the 8-bit writes...
// -----------------------------------------------------------------
 void drz80MemWriteW(u16 data,u16 addr) 
{
    cpu_writemem16(data & 0xff , addr);
    cpu_writemem16(data>>8,addr+1);
}

 void Z80_Cause_Interrupt(int type) 
{
#ifdef DEBUG_Z80        
        extern unsigned int num_irqs;
        if ((drz80.pending_irq & NMI_IRQ) == 0)  num_irqs++;
#endif        
    if (type == Z80_NMI_INT) 
    {
        drz80.pending_irq |= NMI_IRQ;
    }
    else if (type != Z80_IGNORE_INT) 
    {
        drz80.z80irqvector = type & 0xff;
        drz80.pending_irq |= INT_IRQ;
    }
}

 void Z80_Clear_Pending_Interrupts(void) 
{
    drz80.pending_irq = 0;
    drz80.Z80_IRQ = 0;
}

 void Interrupt(void) 
{
    if (drz80.pending_irq & NMI_IRQ)  /* NMI IRQ */
    {
        drz80.Z80_IRQ = NMI_IRQ;
        drz80.pending_irq &= ~NMI_IRQ;
    } 
    else if (drz80.Z80IF & 1)  /* INT IRQ and Interrupts enabled */
    {
        drz80.Z80_IRQ = INT_IRQ;
        drz80.pending_irq &= ~INT_IRQ;
    }
}

void DrZ80_InitHandlers() {
  extern u8 romBankMask;
  drz80.z80_write8=cpu_writemem16;
  drz80.z80_write16=drz80MemWriteW;
  drz80.z80_in=(sg1000_mode ? cpu_readport_sg:cpu_readport16);
  drz80.z80_out=(sg1000_mode ? cpu_writeport_sg:cpu_writeport16);    
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

  Z80_Clear_Pending_Interrupts();
  cpuirequest=0;
  lastBank = 199;
  cycle_deficit = 0;
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
