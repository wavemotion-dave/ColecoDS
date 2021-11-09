#include <nds.h>
#include <nds/arm9/console.h> //basic print funcionality

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Z80_interface.h"

#include "../../../colecoDS.h"

#define Z80_IGNORE_INT	-1 /* Ignore interrupt */
#define Z80_NMI_INT 	-2 /* Execute NMI */
#define Z80_IRQ_INT 	-1000 /* Execute IRQ */

#define INT_IRQ 0x01
#define NMI_IRQ 0x02

#define PUSH_PC() { drz80.Z80SP=drz80.z80_rebaseSP(drz80.Z80SP-drz80.Z80SP_BASE-2); drz80.z80_write16(drz80.Z80PC - drz80.Z80PC_BASE,drz80.Z80SP - drz80.Z80SP_BASE); }

struct DrZ80 drz80 __attribute((aligned(4)));

int	Z80_pending_irq;

/*
int	Z80_request_irq, Z80_pending_irq;
*/
void drz80_write8(unsigned char data,unsigned short address) {
  if((address>0x5FFF)&&(address<0x8000)) {
    address&=0x03FF;
    pColecoMem[0x6000+address]=pColecoMem[0x6400+address]=pColecoMem[0x6800+address]=pColecoMem[0x6C00+address]=
    pColecoMem[0x7000+address]=pColecoMem[0x7400+address]=pColecoMem[0x7800+address]=pColecoMem[0x7C00+address]=data;
  }
}

unsigned char drz80_read8(unsigned short address) {
  return (pColecoMem[address]);
}

unsigned short drz80MemReadW(unsigned short addr)
{
  return (((drz80_read8(addr))&0xFF)|((drz80_read8(addr+1)&0xff)<<8));
}

void drz80MemWriteW(unsigned short data,unsigned short addr) {
  drz80_write8(data & 0xff , addr);
  drz80_write8(data>>8,addr+1);
}

unsigned int z80_rebaseSP(unsigned short address) {
  drz80.Z80SP_BASE = (unsigned int) pColecoMem;
  drz80.Z80SP      = drz80.Z80SP_BASE + address ;
  return (drz80.Z80SP);
}

unsigned int z80_rebasePC(unsigned short address) {
  drz80.Z80PC_BASE = (unsigned int) pColecoMem;
  drz80.Z80PC      = drz80.Z80PC_BASE + address ;
  return (drz80.Z80PC);
}

void z80_irq_callback(void) {
  //drz80.Z80_IRQ = 0x00;//0xFF;
}

/* Return program counter */
unsigned Z80_GetPC (void) {
	return (drz80.Z80PC - drz80.Z80PC_BASE);
}

/****************************************************************************
 * Execute IPeriod T-states. Return number of T-states really executed
 ****************************************************************************/
void DrZ80_InitFonct() {
#ifdef DEBUG
  iprintf("DrZ80_Init\n");
#endif
  drz80.z80_write8=drz80_write8;
  drz80.z80_write16=drz80MemWriteW;
  drz80.z80_in=cpu_readport16;
  drz80.z80_out=cpu_writeport16;
  drz80.z80_read8=drz80_read8;
  drz80.z80_read16=drz80MemReadW;
  drz80.z80_rebasePC=z80_rebasePC;
  drz80.z80_rebaseSP=z80_rebaseSP;
  drz80.z80_irq_callback=z80_irq_callback;
#ifdef DEBUG
  iprintf("RUN=%08x\n",DrZ80Run);
  iprintf("D=%08x A=%08x\n",drz80.z80_write8,cpu_writemem16);
#endif
}

void DrZ80_Reset(void) {
#ifdef DEBUG
  iprintf("DrZ80_Reset\n");
#endif
  memset (&drz80, 0, sizeof(drz80));
  DrZ80_InitFonct();
/*
  drz80.Z80A = 0xFF000000;;
  drz80.Z80F = 1<<1;  // set ZFlag
  drz80.Z80BC = 0xFFFF0000;
  drz80.Z80DE = 0xFFFF0000;
  drz80.Z80HL = 0xFFFF0000;
  drz80.Z80A2 = 0;
  drz80.Z80F2 = 1<<2;  // set ZFlag
  drz80.Z80BC2 = 0;
  drz80.Z80DE2 = 0;
  drz80.Z80HL2 = 0;
  drz80.Z80IX = 0xFFFF0000;
  drz80.Z80IY = 0xFFFF0000;
  drz80.Z80I = 0;
  drz80.Z80IM = 1;
  drz80.Z80_IRQ = 0;
  drz80.Z80IF = 0;
*/

/* ok 
  drz80.Z80A = 0xFF <<24;
  drz80.Z80F = (1<<1); // set ZFlag 
  drz80.Z80BC = 0xFFFF	<<16;
  drz80.Z80DE = 0xFFFF	<<16;
  drz80.Z80HL = 0xFFFF	<<16;
  drz80.Z80A2 = 0x00 <<24;
  drz80.Z80F2 = 1<<2;  // set ZFlag 
  drz80.Z80BC2 = 0x0000 <<16;
  drz80.Z80DE2 = 0x0000 <<16;
  drz80.Z80HL2 = 0x0000 <<16;
  drz80.Z80IX = 0xFFFF	<<16;
  drz80.Z80IY = 0xFFFF	<<16;
  drz80.Z80I = 0x00;
  drz80.Z80IM = 0x01;
  drz80.Z80_IRQ = 0x00;
  drz80.Z80IF = 0x00;

  drz80.Z80PC=z80_rebasePC(0);
  drz80.Z80SP=z80_rebaseSP(0xdfff);
*/

/*
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
  drz80.Z80SP=z80_rebaseSP(0xf000); 
*/

/*

  drz80.Z80A = 0xFF <<24;
  drz80.Z80F = (1<<1); // set ZFlag 
  drz80.Z80BC = 0xFFFF	<<16;
  drz80.Z80DE = 0xFFFF	<<16;
  drz80.Z80HL = 0xFFFF	<<16;
  drz80.Z80A2 = 0x00 <<24;
  drz80.Z80F2 = 1<<2;  // set ZFlag 
  drz80.Z80BC2 = 0x0000 <<16;
  drz80.Z80DE2 = 0x0000 <<16;
  drz80.Z80HL2 = 0x0000 <<16;
  drz80.Z80IX = 0xFFFF	<<16;
  drz80.Z80IY = 0xFFFF	<<16;
  drz80.Z80I = 0x00;
  drz80.Z80IM = 0x01;
  drz80.Z80_IRQ = 0x00;
  drz80.Z80IF = 0x00;
  
  drz80.Z80PC=z80_rebasePC(0);
  drz80.Z80SP=z80_rebaseSP(0);
*/  

/*
	Z80_request_irq  = -1;
	Z80_Clear_Pending_Interrupts();
*/

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
  drz80.Z80SP=z80_rebaseSP(0); 

	Z80_Clear_Pending_Interrupts();
}

int z80_cycle_count = 0;

void DrZ80_execute(int cycles) {
  drz80.cycles = cycles;
	if (Z80_pending_irq)
		Interrupt();
  DrZ80Run(&drz80, cycles);

  z80_cycle_count += cycles;
}

unsigned int DrZ80_get_elapsed_cycles(void) {
  return z80_cycle_count;
}

void Z80_Cause_Interrupt(int type) {
	// type value :                                                            
	//  Z80_NMI_INT                      -> NMI request                        
	//  Z80_IGNORE_INT                   -> no request                         
	//  vector(0x00-0xff)                -> SINGLE interrupt request           
	//  Z80_VECTOR(device,status)        -> DaisyChain change interrupt status 
	//      device : device number of daisy-chain link                         
	//      status : Z80_INT_REQ  -> interrupt request                         
	//               Z80_INT_IEO  -> interrupt disable output                  

  if (type == Z80_NMI_INT) {
    Z80_pending_irq |= NMI_IRQ;
	} 
  else if (type != Z80_IGNORE_INT) {
    // single int mode 
    drz80.z80irqvector = type & 0xff;
    Z80_pending_irq |= INT_IRQ;
  }
}

void Z80_Clear_Pending_Interrupts(void) {
  Z80_pending_irq = 0;
}

void Interrupt(void) {
  // This extra check is because DrZ80 calls this function directly but does
  //    not have access to the Z80.pending_irq variable.  So we check here instead. 
  if(!Z80_pending_irq) {	return; } // If no pending ints exit 
	
  // Check if ints enabled 
  if ( (Z80_pending_irq & NMI_IRQ) || (drz80.Z80IF&1) ) {
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
		
		if (Z80_pending_irq & NMI_IRQ)  {
			drz80.Z80IF = (drz80.Z80IF&1)<<1;  // Save interrupt flip-flop 1 to 2 and Clear interrupt flip-flop 1 
			PUSH_PC();
			drz80.Z80PC=drz80.z80_rebasePC(0x0066);
			// reset NMI interrupt request 
			Z80_pending_irq &= ~NMI_IRQ;
		}
		else  {
			// Clear interrupt flip-flop 1 
      drz80.Z80IF &= ~1;
			// reset INT interrupt request 
			Z80_pending_irq &= ~INT_IRQ;
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
}
