#ifndef _Z80_INTERFACE_H_
#define _Z80_INTERFACE_H_

#include <nds.h>

#include "./drz80/drz80.h"
#include "./cz80/Z80.h"

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
extern Z80 CPU;

extern void ClearMSXInterrupt(void);

extern void DrZ80_Cause_Interrupt(int type);
extern void DrZ80_Interrupt(void);
extern void DrZ80_Clear_Pending_Interrupts(void);
extern void DrZ80_Reset(void);
extern int  DrZ80_execute(int cycles);

extern u32 z80_rebaseSP(u16 address);
extern u32 z80_rebasePC(u16 address);

extern void cpu_writeport16(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport16(register unsigned short Port);

extern void cpu_writeport_sg(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_sg(register unsigned short Port);

extern void cpu_writeport_m5(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_m5(register unsigned short Port);

extern void cpu_writeport_pv2000(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_pv2000(register unsigned short Port);

extern void cpu_writeport_memotech(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_memotech(register unsigned short Port);

extern void cpu_writeport_msx(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_msx(register unsigned short Port);

extern void cpu_writeport_svi(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_svi(register unsigned short Port);

extern void cpu_writeport_einstein(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_einstein(register unsigned short Port);

extern u8 cpu_readmem_pv2000 (u16 address);
extern void cpu_writemem_pv2000 (u8 value,u16 address);


#endif
