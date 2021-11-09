#ifndef __Z80_CONTROL__
#define __Z80_CONTROL__

#include "DrZ80.h"

#define Z80_IGNORE_INT	-1 /* Ignore interrupt */
#define Z80_NMI_INT 	-2 /* Execute NMI */
#define Z80_IRQ_INT 	-1000 /* Execute IRQ */

extern struct DrZ80 drz80;

extern void DrZ80_Reset();
//extern void DrZ80_Init();
extern void DrZ80_InitFonct(void);
extern void DrZ80_Set_Irq(unsigned int irq);
extern void DrZ80_irq_callback(void);
extern void z80_push(int reg);
extern unsigned int DrZ80_Rebase_SP(unsigned int newsp);
extern unsigned int DrZ80_Rebase_PC(unsigned int newpc);
extern unsigned int DrZ80_get_elapsed_cycles(void);
extern void DrZ80_execute(int cycles);

extern void Interrupt(void);
extern int	Z80_request_irq, Z80_service_irq;
extern void Z80_Cause_Interrupt(int type);
extern void Z80_Clear_Pending_Interrupts(void);

extern void cpu_writeport16(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport16(register unsigned short Port);

#endif
