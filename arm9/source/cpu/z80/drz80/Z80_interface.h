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

struct Z80_Regs {
  struct DrZ80 regs;
  u8 *z80Base;
  u32 port16bits;
};

extern u16 cpuirequest;

extern struct DrZ80 drz80;

#define DrZ80_nmi Z80_Cause_Interrupt(Z80_NMI_INT)
#define DrZ80_int(value) Z80_Cause_Interrupt(value)

extern void Z80_Cause_Interrupt(int type);
extern void Interrupt(void);
extern void Z80_Clear_Pending_Interrupts(void);
extern void DrZ80_Reset(void);
extern void DrZ80_GetContext(void *pData);
extern void DrZ80_SetContext(void *pData);
extern unsigned short DrZ80_GetPC (void);
extern int DrZ80_execute(u32 cycles);
extern u32 DrZ80_GetElapsedTicks(u32 dwClear);

extern void cpu_writeport16(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport16(register unsigned short Port);

#endif
