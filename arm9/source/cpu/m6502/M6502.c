/** M6502: portable 6502 emulator ****************************/
/**                                                         **/
/**                         M6502.c                         **/
/**                                                         **/
/** This file contains implementation for 6502 CPU. Don't   **/
/** forget to provide Rd6502(), Wr6502(), Loop6502(), and   **/
/** possibly Op6502() functions to accomodate the emulated  **/
/** machine's architecture.                                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996                      **/
/**               Alex Krasivsky  1996                      **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/   
/**     changes to this file.                               **/
/*************************************************************/

#include "M6502.h"
#include "Tables.h"
#include <stdio.h>

/** System-Dependent Stuff ***********************************/
/** This is system-dependent code put here to speed things  **/
/** up. It has to stay inlined to be fast.                  **/
/*************************************************************/

#define CREATIVISION_CYCLES_PER_SCANLINE    128

extern byte RAM_Memory[];
extern unsigned int debug[];
extern void Trap_Bad_Ops(char *prefix, byte I, word W);
extern byte Rd6502_full(register word Addr);

#define Op6502(A) RAM_Memory[A]
inline byte Rd6502(register word Addr) {if (Addr & 0xC000) return RAM_Memory[Addr]; return Rd6502_full(Addr);}

/** Addressing Methods ***************************************/
/** These macros calculate and return effective addresses.  **/
/*************************************************************/
#define MC_Ab(Rg)   M_LDWORD(Rg)
#define MC_Zp(Rg)   Rg.W=Op6502(R->PC.W++);
#define MC_Zx(Rg)   Rg.B.l=Op6502(R->PC.W++)+R->X;Rg.B.h=0
#define MC_Zy(Rg)   Rg.B.l=Op6502(R->PC.W++)+R->Y;Rg.B.h=0
#define MC_Ax(Rg)   M_LDWORD(Rg);Rg.W+=R->X
#define MC_Ay(Rg)   M_LDWORD(Rg);Rg.W+=R->Y
#define MC_Ix(Rg)   K.B.l=Op6502(R->PC.W++)+R->X;K.B.h=0; \
                    Rg.B.l=Op6502(K.W++);Rg.B.h=Op6502(K.W)
#define MC_Iy(Rg)   K.W=Op6502(R->PC.W++); \
                    Rg.B.l=Op6502(K.W++);Rg.B.h=Op6502(K.W); \
                    Rg.W+=R->Y

// ----------------------------------------------------------------------------
// These versions will compensate for a page boundary crossing and consume the 
// extra cycle which was not part of the original emulation here for the M6502
// ----------------------------------------------------------------------------
static byte hi __attribute__((section(".dtcm"))) = 0;
#define MC_AxP(Rg)   M_LDWORD(Rg);hi=Rg.B.h; Rg.W+=R->X; if (hi != Rg.B.h) R->ICount--;
#define MC_AyP(Rg)   M_LDWORD(Rg);hi=Rg.B.h; Rg.W+=R->Y; if (hi != Rg.B.h) R->ICount--;
#define MC_IyP(Rg)   K.W=Op6502(R->PC.W++); Rg.B.l=Op6502(K.W++); Rg.B.h=Op6502(K.W); \
                     hi=Rg.B.h; Rg.W+=R->Y; if (hi != Rg.B.h) R->ICount--;


/** Reading From Memory **************************************/
/** These macros calculate address and read from it.        **/
/*************************************************************/
#define MR_Ab(Rg)   MC_Ab(J);Rg=Rd6502(J.W)
#define MR_Im(Rg)   Rg=Op6502(R->PC.W++)
#define MR_Zp(Rg)   MC_Zp(J);Rg=Op6502(J.W)
#define MR_Zx(Rg)   MC_Zx(J);Rg=Op6502(J.W)
#define MR_Zy(Rg)   MC_Zy(J);Rg=Op6502(J.W)
#define MR_Ax(Rg)   MC_Ax(J);Rg=Rd6502(J.W)
#define MR_Ay(Rg)   MC_Ay(J);Rg=Rd6502(J.W)
#define MR_Ix(Rg)   MC_Ix(J);Rg=Rd6502(J.W)
#define MR_Iy(Rg)   MC_Iy(J);Rg=Rd6502(J.W)

// These versions check for the page boundary cross
#define MR_AxP(Rg)  MC_AxP(J);Rg=Rd6502(J.W)
#define MR_AyP(Rg)  MC_AyP(J);Rg=Rd6502(J.W)
#define MR_IyP(Rg)  MC_IyP(J);Rg=Rd6502(J.W)


/** Writing To Memory ****************************************/
/** These macros calculate address and write to it.         **/
/*************************************************************/
#define MW_Ab(Rg)   MC_Ab(J);Wr6502(J.W,Rg)
#define MW_Zp(Rg)   MC_Zp(J);Wr6502(J.W,Rg)
#define MW_Zx(Rg)   MC_Zx(J);Wr6502(J.W,Rg)
#define MW_Zy(Rg)   MC_Zy(J);Wr6502(J.W,Rg)
#define MW_Ax(Rg)   MC_Ax(J);Wr6502(J.W,Rg)
#define MW_Ay(Rg)   MC_Ay(J);Wr6502(J.W,Rg)
#define MW_Ix(Rg)   MC_Ix(J);Wr6502(J.W,Rg)
#define MW_Iy(Rg)   MC_Iy(J);Wr6502(J.W,Rg)

// These versions check for the page boundary cross
#define MW_AxP(Rg)  MC_AxP(J);Wr6502(J.W,Rg)
#define MW_AyP(Rg)  MC_AyP(J);Wr6502(J.W,Rg)
#define MW_IyP(Rg)  MC_IyP(J);Wr6502(J.W,Rg)


/** Modifying Memory *****************************************/
/** These macros calculate address and modify it.           **/
/*************************************************************/
#define MM_Ab(Cmd)  MC_Ab(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)
#define MM_Zp(Cmd)  MC_Zp(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)
#define MM_Zx(Cmd)  MC_Zx(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)
#define MM_Ax(Cmd)  MC_Ax(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)
#define MM_Ay(Cmd)  MC_Ay(J);I=Rd6502(J.W);Cmd(I);Wr6502(J.W,I)

/** Other Macros *********************************************/
/** Calculating flags, stack, jumps, arithmetics, etc.      **/
/*************************************************************/
#define M_FL(Rg)        R->P=(R->P&~(Z_FLAG|N_FLAG))|ZNTable[Rg]
#define M_LDWORD(Rg)    Rg.B.l=Op6502(R->PC.W++);Rg.B.h=Op6502(R->PC.W++)

#define M_PUSH(Rg)      RAM_Memory[0x0100|R->S]=Rg;R->S--
#define M_POP(Rg)       R->S++;Rg=RAM_Memory[0x0100|R->S]
#define M_JR            R->PC.W+=(offset)Op6502(R->PC.W)+1;R->ICount--

#define M_ADC(Rg) \
  if(R->P&D_FLAG) \
  { \
    K.B.l=(R->A&0x0F)+(Rg&0x0F)+(R->P&C_FLAG); \
    if(K.B.l>9) K.B.l+=6; \
    K.B.h=(R->A>>4)+(Rg>>4)+(K.B.l>15? 1:0); \
    if(K.B.h>9) K.B.h+=6; \
    R->A=(K.B.l&0x0F)|(K.B.h<<4); \
    R->P=(R->P&~C_FLAG)|(K.B.h>15? C_FLAG:0); \
  } \
  else \
  { \
    K.W=R->A+Rg+(R->P&C_FLAG); \
    R->P&=~(N_FLAG|V_FLAG|Z_FLAG|C_FLAG); \
    R->P|=(~(R->A^Rg)&(R->A^K.B.l)&0x80? V_FLAG:0)| \
          (K.B.h? C_FLAG:0)|ZNTable[K.B.l]; \
    R->A=K.B.l; \
  }

/* Warning! C_FLAG is inverted before SBC and after it */
#define M_SBC(Rg) \
  if(R->P&D_FLAG) \
  { \
    K.B.l=(R->A&0x0F)-(Rg&0x0F)-(~R->P&C_FLAG); \
    if(K.B.l&0x10) K.B.l-=6; \
    K.B.h=(R->A>>4)-(Rg>>4)-((K.B.l&0x10)>>4); \
    if(K.B.h&0x10) K.B.h-=6; \
    R->A=(K.B.l&0x0F)|(K.B.h<<4); \
    R->P=(R->P&~C_FLAG)|(K.B.h>15? 0:C_FLAG); \
  } \
  else \
  { \
    K.W=R->A-Rg-(~R->P&C_FLAG); \
    R->P&=~(N_FLAG|V_FLAG|Z_FLAG|C_FLAG); \
    R->P|=((R->A^Rg)&(R->A^K.B.l)&0x80? V_FLAG:0)| \
          (K.B.h? 0:C_FLAG)|ZNTable[K.B.l]; \
    R->A=K.B.l; \
  }

#define M_CMP(Rg1,Rg2) \
  K.W=Rg1-Rg2; \
  R->P&=~(N_FLAG|Z_FLAG|C_FLAG); \
  R->P|=ZNTable[K.B.l]|(K.B.h? 0:C_FLAG)

#define M_BIT(Rg) \
  R->P&=~(N_FLAG|V_FLAG|Z_FLAG); \
  R->P|=(Rg&(N_FLAG|V_FLAG))|(Rg&R->A? 0:Z_FLAG)

#define M_SBX(Rg) \
  K.W=(R->A & R->X)-Rg; \
  R->P&=~(N_FLAG|Z_FLAG|C_FLAG); \
  R->P|=ZNTable[K.B.l]|(K.B.h? 0:C_FLAG); \
  R->X = K.B.l


#define M_AND(Rg)   R->A&=Rg;M_FL(R->A)
#define M_ORA(Rg)   R->A|=Rg;M_FL(R->A)
#define M_EOR(Rg)   R->A^=Rg;M_FL(R->A)
#define M_INC(Rg)   Rg++;M_FL(Rg)
#define M_DEC(Rg)   Rg--;M_FL(Rg)
#define M_DCP(Rg)   Rg--;M_CMP(R->A, Rg)

#define M_ASL(Rg)   R->P&=~C_FLAG;R->P|=Rg>>7;Rg<<=1;M_FL(Rg)
#define M_LSR(Rg)   R->P&=~C_FLAG;R->P|=Rg&C_FLAG;Rg>>=1;M_FL(Rg)
#define M_ROL(Rg)   K.B.l=(Rg<<1)|(R->P&C_FLAG); \
            R->P&=~C_FLAG;R->P|=Rg>>7;Rg=K.B.l; \
            M_FL(Rg)
#define M_ROR(Rg)   K.B.l=(Rg>>1)|(R->P<<7); \
            R->P&=~C_FLAG;R->P|=Rg&C_FLAG;Rg=K.B.l; \
            M_FL(Rg)

/** Reset6502() **********************************************/
/** This function can be used to reset the registers before **/
/** starting execution with Run6502(). It sets registers to **/
/** their initial values.                                   **/
/*************************************************************/
void Reset6502(M6502 *R)
{
  R->A=R->X=R->Y=0x00;
  R->P=Z_FLAG|R_FLAG;
  R->S=0xFF;
  R->PC.B.l=Rd6502(0xFFFC);
  R->PC.B.h=Rd6502(0xFFFD);   
  R->IPeriod = CREATIVISION_CYCLES_PER_SCANLINE;
  R->ICount = R->IPeriod;
  R->IRequest=INT_NONE;
  R->AfterCLI=0;
  R->TrapBadOps=1;
}


/** Int6502() ************************************************/
/** This function will generate interrupt of a given type.  **/
/** INT_NMI will cause a non-maskable interrupt. INT_IRQ    **/
/** will cause a normal interrupt, unless I_FLAG set in R.  **/
/*************************************************************/
void Int6502(M6502 *R,byte Type)
{
  register pair J;

  if((Type==INT_NMI)||((Type==INT_IRQ)&&!(R->P&I_FLAG)))
  {
    R->ICount-=7;
    M_PUSH(R->PC.B.h);
    M_PUSH(R->PC.B.l);
    M_PUSH(R->P&~B_FLAG);
    R->P&=~D_FLAG;
    if(Type==INT_NMI) J.W=0xFFFA; else { R->P|=I_FLAG;J.W=0xFFFE; }
    R->PC.B.l=Rd6502(J.W++);
    R->PC.B.h=Rd6502(J.W);
  }
}


/** Exec6502() ***********************************************/
/** This function will execute a single scanline worth of   **/
/** opcodes and then return next PC, and register values    **/
/** in R.                                                   **/
/*************************************************************/
word Exec6502(M6502 *R)
{
  register pair J,K;
  register byte I = INT_NONE;

  R->ICount+= R->IPeriod;
  while (R->ICount > 0)
  {
      I=Op6502(R->PC.W++);
      R->ICount-=Cycles[I];
      switch(I)
      {
#include "Codes.h"
default: if (R->TrapBadOps) Trap_Bad_Ops("6502", I, R->PC.W-1);
      }
  }
    
  /* If we have come after CLI, get INT_? from IRequest */
  /* Otherwise, get it from the loop handler            */
  if(R->AfterCLI)
  {
    I=R->IRequest;            /* Get pending interrupt     */
    R->ICount+=R->IBackup-1;  /* Restore the ICount        */
    R->AfterCLI=0;            /* Done with AfterCLI state  */
  }
  else
  {
    I=Loop6502();             /* Call the periodic handler */
    if(!I) I=R->IRequest;     /* Realize pending interrupt */
  }

  if(I==INT_QUIT) return(R->PC.W); /* Exit if INT_QUIT     */
  if(I) Int6502(R,I);              /* Interrupt if needed  */ 
    
  /* We are done */
  return(R->PC.W);
}



/** Run6502() ************************************************/
/** This function will run 6502 code until Loop6502() call  **/
/** returns INT_QUIT. It will return the PC at which        **/
/** emulation stopped, and current register values in R.    **/
/*************************************************************/
word Run6502(M6502 *R)
{
    return 0; // Not used...
}
