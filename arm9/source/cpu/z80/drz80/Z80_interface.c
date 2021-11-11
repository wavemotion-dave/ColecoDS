#include <nds.h>
#include <nds/arm9/console.h> //basic print funcionality

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Z80_interface.h"
#include "../../../colecoDS.h"

#define INT_IRQ 0x01
#define NMI_IRQ 0x02

s16 xfer_buf[4] ALIGN(32) = {0};

//#define PUSH_PC() { drz80.Z80SP=drz80.z80_rebaseSP(drz80.Z80SP-drz80.Z80SP_BASE-2); drz80.z80_write16(drz80.Z80PC - drz80.Z80PC_BASE,drz80.Z80SP - drz80.Z80SP_BASE); }

struct DrZ80 drz80 __attribute((aligned(4))) __attribute__((section(".dtcm")));

u16 previouspc,cpuirequest;

extern u8 romBankMask;
extern u8 romBuffer[];
u8 lastBank = 199;


ITCM_CODE u32 z80_rebaseSP(u16 address) {
  drz80.Z80SP_BASE = (unsigned int) pColecoMem;
  drz80.Z80SP      = drz80.Z80SP_BASE + address;
  return (drz80.Z80SP);
}

ITCM_CODE u32 z80_rebasePC(u16 address) {
  drz80.Z80PC_BASE = (unsigned int) pColecoMem;
  drz80.Z80PC      = drz80.Z80PC_BASE + address;
  return (drz80.Z80PC);
}

ITCM_CODE void z80_irq_callback(void) {
	//drz80.pending_irq &= ~drz80.Z80_IRQ;
	drz80.Z80_IRQ = 0x00;
	previouspc=0;
}

ITCM_CODE u8 cpu_readmem16 (u16 address) {
  return (pColecoMem[address]);
}

ITCM_CODE u8 cpu_readmem16_banked (u16 address) 
{
  if (address >= 0xFFC0)
  {
      address &= romBankMask;
      if (lastBank != address)
      {
        memcpy(pColecoMem+0xC000, romBuffer + (address * 0x4000), 0x4000);
        lastBank = address;
      }
  }    
  return (pColecoMem[address]);
}

ITCM_CODE void cpu_writemem16 (u8 value,u16 address) 
{
    extern u8 sgm_enable;
    extern u16 sgm_low_addr;
    if (sgm_enable)
    {
      if ((address >= sgm_low_addr) && (address < 0x8000)) pColecoMem[address]=value;
      if (address == 0xFFFF)    // SGM can write to this address to set bank #
      {
          value &= romBankMask;
          if (lastBank != value)
          {
            memcpy(pColecoMem+0xC000, romBuffer + (value * 0x4000), 0x4000);
            lastBank = value;
          }
      }
    }
    else if((address>0x5FFF)&&(address<0x8000)) 
    {
        address&=0x03FF;
        pColecoMem[0x6000+address]=pColecoMem[0x6400+address]=pColecoMem[0x6800+address]=pColecoMem[0x6C00+address]=
        pColecoMem[0x7000+address]=pColecoMem[0x7400+address]=pColecoMem[0x7800+address]=pColecoMem[0x7C00+address]=value;
    }
}

ITCM_CODE u16 drz80MemReadW(u16 addr) 
{
    return (pColecoMem[addr]  |  (pColecoMem[addr+1] << 8));
}

ITCM_CODE u16 drz80MemReadW_banked(u16 addr) 
{
  if (addr >= 0xFFC0)
  {
      addr &= romBankMask;
      if (lastBank != addr)
      {
          memcpy(pColecoMem+0xC000, romBuffer + (addr * 0x4000), 0x4000);
          lastBank = addr;
      }
  }    
  return (pColecoMem[addr]  |  (pColecoMem[addr+1] << 8));
}


ITCM_CODE void drz80MemWriteW(u16 data,u16 addr) {
  cpu_writemem16(data & 0xff , addr);
  cpu_writemem16(data>>8,addr+1);
}

ITCM_CODE void Z80_Cause_Interrupt(int type) {
  if (type == Z80_NMI_INT) {
    drz80.pending_irq |= NMI_IRQ;
	} else if (type != Z80_IGNORE_INT) {
		drz80.z80irqvector = type & 0xff;
    drz80.pending_irq |= INT_IRQ;
  }
}

ITCM_CODE void Z80_Clear_Pending_Interrupts(void) {
  drz80.pending_irq = 0;
  drz80.Z80_IRQ = 0;
}

ITCM_CODE void Interrupt(void) {
	previouspc = -1;
	if (drz80.pending_irq & NMI_IRQ) { /* NMI IRQ */
		drz80.Z80_IRQ = NMI_IRQ;
		drz80.pending_irq &= ~NMI_IRQ;
	} else if (drz80.Z80IF & 1) { /* INT IRQ and Interrupts enabled */
		drz80.Z80_IRQ = INT_IRQ;
		drz80.pending_irq &= ~INT_IRQ;
	}

/*
  // This extra check is because DrZ80 calls this function directly but does
  //    not have access to the Z80.pending_irq variable.  So we check here instead. 
  if(!cpu.pending_irq ) {	return; } // If no pending ints exit 
	
  // Check if ints enabled 
  if ( (cpu.pending_irq  & NMI_IRQ) || (drz80.Z80IF&1) ) {
    int irq_vector = Z80_IGNORE_INT;

		// DrZ80 Z80IF 
		// bit1 = _IFF1 
		// bit2 = _IFF2 
		// bit3 = _HALT 
		
    // Check if processor was halted 
		if (drz80.Z80IF&4)  {
			 drz80.Z80PC=drz80.z80_rebasePC(drz80.Z80PC - drz80.Z80PC_BASE + 1);  	// Inc PC 
			 drz80.Z80IF&= ~4; 	// and clear halt 
		}  
		
		if (cpu.pending_irq & NMI_IRQ)  {
			drz80.Z80IF = (drz80.Z80IF&1)<<1;  // Save interrupt flip-flop 1 to 2 and Clear interrupt flip-flop 1 
			PUSH_PC();
			drz80.Z80PC=drz80.z80_rebasePC(0x0066);
			// reset NMI interrupt request 
			cpu.pending_irq &= ~NMI_IRQ;
		}
		else  {
			// Clear interrupt flip-flop 1 
      drz80.Z80IF &= ~1;
			// reset INT interrupt request 
			cpu.pending_irq &= ~INT_IRQ;
      irq_vector = drz80.z80irqvector;

      // Interrupt mode 2. Call [Z80.I:databyte] 
			if( drz80.Z80IM == 2 ) {
				irq_vector = (irq_vector & 0xff) | (drz80.Z80I << 8);
        PUSH_PC();
				drz80.Z80PC=drz80.z80_rebasePC(drz80.z80_read16(irq_vector));
			}
			else {
				// Interrupt mode 1. RST 38h 
				if( drz80.Z80IM == 1 ) {
        //iprintf("int ! IM1\n");
					PUSH_PC();
					drz80.Z80PC=drz80.z80_rebasePC(0x0038);
				} 
				else  {
					// Interrupt mode 0. We check for CALL and JP instructions, 
					// if neither of these were found we assume a 1 byte opcode 
					// was placed on the databus 
					switch (irq_vector & 0xff0000)  {
						case 0xcd0000:	// call 
							PUSH_PC();
						case 0xc30000:	// jump 
							drz80.Z80PC=drz80.z80_rebasePC(irq_vector & 0xffff);
							break;
						default:
							irq_vector &= 0xff;
							PUSH_PC();
							drz80.Z80PC=drz80.z80_rebasePC(0x0038);
							break;
					}
				}
			}
		}
  }
*/
}

void DrZ80_InitFonct() {
  extern u8 romBankMask;
  drz80.z80_write8=cpu_writemem16;
  drz80.z80_write16=drz80MemWriteW;
  drz80.z80_in=cpu_readport16;
  drz80.z80_out=cpu_writeport16;
  drz80.z80_read8= (romBankMask ? cpu_readmem16_banked : cpu_readmem16 );
  drz80.z80_read16= (romBankMask ? drz80MemReadW_banked : drz80MemReadW);
  drz80.z80_rebasePC=(unsigned int (*)(short unsigned int))z80_rebasePC;
  drz80.z80_rebaseSP=(unsigned int (*)(short unsigned int))z80_rebaseSP;
  drz80.z80_irq_callback=z80_irq_callback;
}

void DrZ80_Reset(void) {
#ifdef DEBUG
  iprintf("DrZ80_Reset\n");
#endif
  memset (&drz80, 0, sizeof(struct DrZ80));
  DrZ80_InitFonct();

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
	previouspc=0;
  cpuirequest=0;
}

u16 DrZ80_GetPC (void) {
	return (drz80.Z80PC - drz80.Z80PC_BASE);
}

u32 dwElapsedTicks = 0;

ITCM_CODE int DrZ80_execute(u32 cycles) {
  drz80.cycles = cycles;
//	if (drz80.pending_irq)
//		Interrupt();
  DrZ80Run(&drz80, cycles);

  dwElapsedTicks += cycles;
  return (cycles-drz80.cycles);
}

ITCM_CODE u32 DrZ80_GetElapsedTicks(u32 dwClear) {
  u32 dwTemp = dwElapsedTicks;
  
  if (dwClear) {
    dwElapsedTicks = 0;
  }
  
  return(dwTemp);
}
