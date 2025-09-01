#ifndef _Z80_INTERFACE_H_
#define _Z80_INTERFACE_H_

#include <nds.h>

#include "./cz80/Z80.h"

#define word u16
#define byte u8

extern Z80 CPU;

extern void ClearCPUInterrupt(void);

extern void cpu_writeport16(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport16(register unsigned short Port);

extern void cpu_writeport_sg(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_sg(register unsigned short Port);

extern void cpu_writeport_m5(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_m5(register unsigned short Port);

extern void cpu_writeport_pv1000(register unsigned short Port,register unsigned char Value);
extern unsigned char cpu_readport_pv1000(register unsigned short Port);

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

extern void Trap_Bad_Ops(char *prefix, byte I, word W);

#endif
