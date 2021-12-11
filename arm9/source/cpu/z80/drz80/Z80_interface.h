#ifndef _Z80_INTERFACE_H_
#define _Z80_INTERFACE_H_

#include <nds.h>

#include "drz80.h"

#define word u16
#define byte u8

#define Z80_IGNORE_INT	-1 /* Ignore interrupt */
#define Z80_NMI_INT 	0x01 /* Execute NMI */
#define Z80_IRQ_INT 	0xFF /* Execute IRQ */

#define Z80_INT_REQ     0x01    /* interrupt request mask       */
#define Z80_INT_IEO     0x02    /* interrupt disable mask(IEO)  */

#define Z80_VECTOR(device,state) (((device)<<8)|(state))

struct __attribute__((__packed__)) Z80_Regs {
  struct DrZ80 regs;
  u8 *z80Base;
  u32 port16bits;
};

extern u16 cpuirequest;

extern struct DrZ80 drz80;

extern void Z80_Cause_Interrupt(int type);
extern void Interrupt(void);
extern void Z80_Clear_Pending_Interrupts(void);
extern void DrZ80_Reset(void);
extern int DrZ80_execute(u32 cycles);

extern void cpu_writeport16(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport16(register unsigned short Port);

#endif
