/******************************************************************************
*  ColecoDS TMS9918A (video) file
*  Ver 1.0
*
*  Copyright (C) 2006 AlekMaul . All rights reserved.
*       http://www.portabledev.com
*
** File: tms9928a.c -- software implementation of the Texas Instruments
**                     TMS9918(A), TMS9928(A) and TMS9929(A), used by the Coleco, MSX and
**                     TI99/4(A).
**
** All undocumented features as described in the following file
** should be emulated.
**
** http://www.msxnet.org/tech/tms9918a.txt
**
******************************************************************************/
#include <nds.h>
#include <nds/arm9/console.h> //basic print functionality

#include <stdio.h>
#include <string.h>

#include "../../colecoDS.h"

#include "tms9918a.h"

#define MaxSprites  32


u16 *pVidFlipBuf= (u16*) (0x06000000);    // Video flipping buffer

u8 XBuf_A[256*256] ALIGN(32) = {0}; // Really it's only 256x192 - Ping Pong Buffer A
u8 XBuf_B[256*256] ALIGN(32) = {0}; // Really it's only 256x192 - Ping Pong Buffer B
u8 *XBuf __attribute__((section(".dtcm"))) = XBuf_A;

// Look up table for colors - pre-generated and in VRAM for maximum speed!
u32 (*lutTablehh)[16][16] = (void*)0x068A0000;

// Screen handlers and masks for VDP table address registers
tScrMode SCR[MAXSCREEN+1] __attribute__((section(".dtcm")))  = {
  { RefreshLine0,0x7F,0x00,0x3F,0x00,0x3F,0x00,0x00,0x00,0x00 },/* SCREEN 0:TEXT 40x24    */
  { RefreshLine1,0x7F,0xFF,0x3F,0xFF,0x3F,0x00,0x00,0x00,0x00 },/* SCREEN 1:TEXT 32x24    */
  { RefreshLine2,0x7F,0x80,0x3C,0xFF,0x3F,0x00,0x7F,0x03,0x00 },/* SCREEN 2:BLOCK 256x192 */
  { RefreshLine3,0x7F,0x00,0x3F,0xFF,0x3F,0x00,0x00,0x00,0x00 },/* SCREEN 3:GFX 64x48x16  */
};

/** Palette9918[] ********************************************/
/** 16 standard colors used by TMS9918/TMS9928 VDP chips.   **/
/*************************************************************/
u8 TMS9918A_palette[16*3] = {
  0x00,0x00,0x00,   0x00,0x00,0x00,   0x20,0xC0,0x20,   0x60,0xE0,0x60,
  0x20,0x20,0xE0,   0x40,0x60,0xE0,   0xA0,0x20,0x20,   0x40,0xC0,0xE0,
  0xE0,0x20,0x20,   0xE0,0x60,0x60,   0xC0,0xC0,0x20,   0xC0,0xC0,0x80,
  0x20,0x80,0x20,   0xC0,0x40,0xA0,   0xA0,0xA0,0xA0,   0xE0,0xE0,0xE0,
};

u8 pVDPVidMem[0x4000] ALIGN(32) ={0};               // VDP video memory
u16 CurLine __attribute__((section(".dtcm")));      // Current scanline
u8 VDP[16] __attribute__((section(".dtcm")));       // VDP Registers
u8 VDPStatus __attribute__((section(".dtcm")));     // VDP Status
u8 VDPDlatch __attribute__((section(".dtcm")));     // VDP register D Latch
u16 VAddr __attribute__((section(".dtcm")));        // Storage for VIDRAM addresses
u8 VKey;                                            // VDP address latch key
u8 *ChrGen,*ChrTab,*ColTab;                         // VDP tables (screens)
u8 *SprGen,*SprTab;                                 // VDP tables (sprites)
u8 ScrMode __attribute__((section(".dtcm")));       // Current screen mode
u8 FGColor __attribute__((section(".dtcm")));       // Foreground Color
u8 BGColor __attribute__((section(".dtcm")));       // Background Color

// Sprite and Character Masks for the VDP
u32 ChrTabM = ~0;
u32 ColTabM = ~0;
u32 ChrGenM = ~0;
u32 SprTabM = ~0;


/** CheckSprites() *******************************************/
/** This function is periodically called to check for the   **/
/** sprite collisions and 5th sprite, and set appropriate   **/
/** bits in the VDP status register.                        **/
/*************************************************************/
byte ITCM_CODE CheckSprites(void) {
  unsigned int I,J,LS,LD;
  byte *S,*D,*PS,*PD,*T;
  int DH,DV;

  /* Find valid, displayed sprites */
  DV = TMS9918_Sprites16? -16:-8;
  for(I=J=0,S=SprTab;(I<32)&&(S[0]!=208);++I,S+=4)
    if(((S[0]<191)||(S[0]>255+DV))&&((int)S[1]-(S[3]&0x80? 32:0)>DV))
      J|=1<<I;

  if(TMS9918_Sprites16)
  {
    for(S=SprTab;J;J>>=1,S+=4)
      if(J&1)
        for(I=J>>1,D=S+4;I;I>>=1,D+=4)
          if(I&1)
          {
            DV=(int)S[0]-(int)D[0];
            if((DV<16)&&(DV>-16))
            {
              DH=(int)S[1]-(int)D[1]-(S[3]&0x80? 32:0)+(D[3]&0x80? 32:0);
              if((DH<16)&&(DH>-16))
              {
                PS=SprGen+((int)(S[2]&0xFC)<<3);
                PD=SprGen+((int)(D[2]&0xFC)<<3);
                if(DV>0) PD+=DV; else { DV=-DV;PS+=DV; }
                if(DH<0) { DH=-DH;T=PS;PS=PD;PD=T; }
                while(DV<16)
                {
                  LS=((unsigned int)PS[0]<<8)+PS[16];
                  LD=((unsigned int)PD[0]<<8)+PD[16];
                  if(LD&(LS>>DH)) break;
                  else { ++DV;++PS;++PD; }
                }
                if(DV<16) return(1);
              }
            }
          }
  }
  else
  {
    for(S=SprTab;J;J>>=1,S+=4)
      if(J&1)
        for(I=J>>1,D=S+4;I;I>>=1,D+=4)
          if(I&1)
          {
            DV=(int)S[0]-(int)D[0];
            if((DV<8)&&(DV>-8))
            {
              DH=(int)S[1]-(int)D[1]-(S[3]&0x80? 32:0)+(D[3]&0x80? 32:0);
              if((DH<8)&&(DH>-8))
              {
                PS=SprGen+((int)S[2]<<3);
                PD=SprGen+((int)D[2]<<3);
                if(DV>0) PD+=DV; else { DV=-DV;PS+=DV; }
                if(DH<0) { DH=-DH;T=PS;PS=PD;PD=T; }
                while((DV<8)&&!(*PD&(*PS>>DH))) { ++DV;++PS;++PD; }
                if(DV<8) return(1);
              }
            }
          }
  }

  /* No collision */
  return(0);
}


/** ScanSprites() ********************************************/
/** Compute bitmask of sprites shown in a given scanline.   **/
/** Returns the first sprite to show or -1 if none shown.   **/
/** Also updates 5th sprite fields in the status register.  **/
/*************************************************************/
ITCM_CODE int ScanSprites(register byte Y,unsigned int *Mask)
{
  static const byte SprHeights[4] = { 8,16,16,32 };
  register byte OH,IH,*AT;
  register int L,K,C1,C2;
  register unsigned int M;

  /* No 5th sprite yet */
  VDPStatus &= ~(TMS9918_STAT_5THNUM|TMS9918_STAT_5THSPR);

  /* Must have MODE1+ and screen enabled */
  if(!ScrMode || !TMS9918_ScreenON)
  {
    if(Mask) *Mask=0;
    return(-1);
  }

  OH = SprHeights[VDP[1]&0x03];
  IH = SprHeights[VDP[1]&0x02];
  AT = SprTab;
  C1 = MaxSprites+1;
  C2 = 5;
  M  = 0;

  for(L=0;L<32;++L,AT+=4)
  {
    K=AT[0];             /* K = sprite Y coordinate */
    if(K==208) break;    /* Iteration terminates if Y=208 */
    if(K>256-IH) K-=256; /* Y coordinate may be negative */

    /* Mark all valid sprites with 1s, break at MaxSprites */
    if((Y>K)&&(Y<=K+OH))
    {
      /* If we exceed four sprites per line, set 5th sprite flag */
      if(!--C2) VDPStatus|=TMS9918_STAT_5THSPR|L;

      /* If we exceed maximum number of sprites per line, stop here */
      if(!--C1) break;

      /* Mark sprite as ready to draw */
      M|=(1<<L);
    }
  }

  /* Set last checked sprite number (5th sprite, or Y=208, or sprite #31) */
  if(C2>0) VDPStatus |= L<32? L:31;

  /* Return first shown sprite and bit mask of shown sprites */
  if(Mask) *Mask=M;
  return(L-1);
}


/** RefreshSprites() *****************************************/
/** This function is called from RefreshLine#() to refresh  **/
/** sprites.                                                **/
/*************************************************************/
void ITCM_CODE RefreshSprites(register byte Y) {
  static const byte SprHeights[4] = { 8,16,16,32 };
  register byte OH,IH,*PT,*AT;
  register byte *P,*T,C;
  register int L,K,N;
  unsigned int M;

  /* Find sprites to show, update 5th sprite status */
  N = ScanSprites(Y,&M);
  if((N<0) || !M) return;

  T  = XBuf+256*Y;
  OH = SprHeights[VDP[1]&0x03];
  IH = SprHeights[VDP[1]&0x02];
  AT = SprTab+(N<<2);

  /* For each possibly shown sprite... */
  for( ; N>=0 ; --N, AT-=4)
  {
    /* If showing this sprite... */
    if(M&(1<<N))
    {
      C=AT[3];                  /* C = sprite attributes */
      L=C&0x80? AT[1]-32:AT[1]; /* Sprite may be shifted left by 32 */
      C&=0x0F;                  /* C = sprite color */

      if((L<256) && (L>-OH) && C)
      {
        K=AT[0];                /* K = sprite Y coordinate */
        if(K>256-IH) K-=256;    /* Y coordinate may be negative */

        //C  = VDP->XPal[C];
        P  = T+L;
        K  = Y-K-1;
        PT = SprGen
           + ((int)(IH>8? (AT[2]&0xFC):AT[2])<<3)
           + (OH>IH? (K>>1):K);

        /* Mask 1: clip left sprite boundary */
        K=L>=0? 0xFFFF:(0x10000>>(OH>IH? (-L>>1):-L))-1;

        /* Mask 2: clip right sprite boundary */
        L+=(int)OH-257;
        if(L>=0)
        {
          L=(IH>8? 0x0002:0x0200)<<(OH>IH? (L>>1):L);
          K&=~(L-1);
        }

        /* Get and clip the sprite data */
        K&=((int)PT[0]<<8)|(IH>8? PT[16]:0x00);

        if(OH>IH)
        {
          /* Big (zoomed) sprite */

          /* Draw left 16 pixels of the sprite */
          if(K&0xFF00)
          {
            if(K&0x8000) P[1]=P[0]=C;
            if(K&0x4000) P[3]=P[2]=C;
            if(K&0x2000) P[5]=P[4]=C;
            if(K&0x1000) P[7]=P[6]=C;
            if(K&0x0800) P[9]=P[8]=C;
            if(K&0x0400) P[11]=P[10]=C;
            if(K&0x0200) P[13]=P[12]=C;
            if(K&0x0100) P[15]=P[14]=C;
          }

          /* Draw right 16 pixels of the sprite */
          if(K&0x00FF)
          {
            if(K&0x0080) P[17]=P[16]=C;
            if(K&0x0040) P[19]=P[18]=C;
            if(K&0x0020) P[21]=P[20]=C;
            if(K&0x0010) P[23]=P[22]=C;
            if(K&0x0008) P[25]=P[24]=C;
            if(K&0x0004) P[27]=P[26]=C;
            if(K&0x0002) P[29]=P[28]=C;
            if(K&0x0001) P[31]=P[30]=C;
          }
        }
        else
        {
          /* Normal (unzoomed) sprite */

          /* Draw left 8 pixels of the sprite */
          if(K&0xFF00)
          {
            if(K&0x8000) P[0]=C;
            if(K&0x4000) P[1]=C;
            if(K&0x2000) P[2]=C;
            if(K&0x1000) P[3]=C;
            if(K&0x0800) P[4]=C;
            if(K&0x0400) P[5]=C;
            if(K&0x0200) P[6]=C;
            if(K&0x0100) P[7]=C;
          }

          /* Draw right 8 pixels of the sprite */
          if(K&0x00FF)
          {
            if(K&0x0080) P[8]=C;
            if(K&0x0040) P[9]=C;
            if(K&0x0020) P[10]=C;
            if(K&0x0010) P[11]=C;
            if(K&0x0008) P[12]=C;
            if(K&0x0004) P[13]=C;
            if(K&0x0002) P[14]=C;
            if(K&0x0001) P[15]=C;
          }
        }
      }
    }
  }
}


/** RefreshBorder() ******************************************/
/** This function is called from RefreshLine#() to refresh  **/
/** the screen border.                                      **/
/*************************************************************/
#define Width 256
#define Height 192
void RefreshBorder(register byte Y)
{
  register byte *P,BC;
  register int J,N;

  /* Border color */
  BC=BGColor;

  /* Screen buffer */
  P=(byte *)XBuf;
  J=Width*(Y+(Height-192)/2);

  /* For the first line, refresh top border */
  if(Y) P+=J;
  else for(;J;J--) *P++=BC;

  /* Calculate number of pixels */
  N=(Width-(ScrMode ? 256:240))/2;  

  /* Refresh left border */
  for(J=N;J;J--) *P++=BC;

  /* Refresh right border */
  P+=Width-(N<<1);
  for(J=N;J;J--) *P++=BC;

  /* For the last line, refresh bottom border */
  if(Y==191)
    for(J=Width*(Height-192)/2;J;J--) *P++=BC;
}


/** RefreshLine0() *******************************************/
/** Refresh line Y (0..191) of SCREEN0, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE RefreshLine0(u8 Y) 
{
  register byte *T,X,K,Offset;
  register byte *P,FC,BC;

  P=XBuf+(Y<<8);
  BC = BGColor;
  FC = FGColor;

  if(!ScreenON)
    memset(P,BGColor,256);
  else
  {
    T=ChrTab+(Y>>3)*40;
    Offset=Y&0x07;

    for(X=0;X<40;X++)
    {
      K=ChrGen[((int)*T<<3)+Offset];
      P[0]=K&0x80? FC:BC;
      P[1]=K&0x40? FC:BC;
      P[2]=K&0x20? FC:BC;
      P[3]=K&0x10? FC:BC;
      P[4]=K&0x08? FC:BC;
      P[5]=K&0x04? FC:BC;
      P+=6;T++;
    }
  }
  RefreshBorder(Y);
}

/** RefreshLine1() *******************************************/
/** Refresh line Y (0..191) of SCREEN1, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE RefreshLine1(u8 uY) 
{
  register byte X,K,Offset,FC,BC;
  register u8 *T;
  register u32 *P;
  u32 *ptLut;

  P=(u32*) (XBuf+(uY<<8));

  if(!ScreenON) 
    memset(P,BGColor,256);
  else {
    T=ChrTab+((int)(uY&0xF8)<<2);
    Offset=uY&0x07;

    for(X=0;X<32;X++) {
      K=*T;
      BC=ColTab[K>>3];
      K=ChrGen[((int)K<<3)+Offset];
      FC=BC>>4;
      BC=BC&0x0F;
      ptLut = (u32*) (lutTablehh[FC][BC]);
      *P++ = *(ptLut + ((K>>4)));
      *P++ = *(ptLut + ((K & 0xF)));
      T++;
    }
    RefreshSprites(uY);
  }
  RefreshBorder(uY);
}

/** RefreshLine2() *******************************************/
/** Refresh line Y (0..191) of SCREEN2, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE RefreshLine2(u8 uY) {
  u32 *P;
  register byte FC,BC;
  register byte X,K,*T,*PGT,*CLT;
  register int J,I,PGTMask,CLTMask;
  u32 *ptLut;

  P=(u32*)(XBuf+(uY<<8));

  if (!ScreenON) 
    memset(P,BGColor,256);
  else 
  {
    J       = ((int)(uY&0xC0)<<5)+(uY&0x07);
    PGT     = ChrGen;
    CLT     = ColTab;
    PGTMask = ChrGenM;
    CLTMask = ColTabM;
    T       = ChrTab+((int)(uY&0xF8)<<2);

    for(X=0;X<32;X++)
    {
      I    = (int)*T<<3;
      K    = CLT[(J+I)&CLTMask];
      FC   = K>>4;
      BC   = K&0x0F;
      K    = PGT[(J+I)&PGTMask];
        
      ptLut = (u32*)(lutTablehh[FC][BC]);
      *P++ = *(ptLut + ((K>>4)));
      *P++ = *(ptLut + ((K & 0xF)));
      T++;
    }
      
    RefreshSprites(uY);
  }    
  RefreshBorder(uY);
}

/** RefreshLine3() *******************************************/
/** Refresh line Y (0..191) of SCREEN3, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE RefreshLine3(u8 uY) {
  register byte X,K,Offset;
  register byte *P,*T;

  P=XBuf+(uY<<8);

  if(!TMS9918_ScreenON) {
    memset(P,BGColor,256);
  }
  else {
    T=ChrTab+((int)(uY&0xF8)<<2);
    Offset=(uY&0x1C)>>2;
    for(X=0;X<32;X++) {
      K=ChrGen[((int)*T<<3)+Offset];
      P[0]=P[1]=P[2]=P[3]=K>>4;
      P[4]=P[5]=P[6]=P[7]=K&0x0F;
      P+=8;T++;
    }
    RefreshSprites(uY);
  }
  RefreshBorder(uY);
}


/*********************************************************************************
 * Emulator calls this function to write byte V into a VDP register R
 ********************************************************************************/
ITCM_CODE byte Write9918(int iReg, u8 value) 
{ 
  int J;
  int VRAMMask;
  byte bIRQ;

  /* Enabling IRQs may cause an IRQ here */
  bIRQ  = (iReg==1) && ((VDP[1]^value)&value&TMS9918_REG1_IRQ) && (VDPStatus&TMS9918_STAT_VBLANK);

  /* VRAM can either be 4kB or 16kB */
  VRAMMask = (iReg==1) && ( (VDP[1]^value) & TMS9918_REG1_RAM16K ) ? 0 : TMS9918_VRAMMask;

  /* Store value into the register */
  VDP[iReg]=value;

  /* Depending on the register, do... */  
  switch (iReg) {
    case 0: /* Mode register 0 */
    case 1: /* Mode register 1 */
      /* Figure out new screen mode number */
      switch(TMS9918_Mode) {
        case 0x00: J=1;break;
        case 0x01: J=2;break;
        case 0x02: J=3;break;
        case 0x04: J=0;break;
        default:   J=ScrMode;break;
      }
        
      /* If mode was changed, recompute table addresses */
      if ((J!=ScrMode) || !VRAMMask) 
      {
        VRAMMask    = TMS9918_VRAMMask;
        ChrTab=pVDPVidMem+(((int)(VDP[2]&SCR[J].R2)<<10)&VRAMMask);
        ColTab=pVDPVidMem+(((int)(VDP[3]&SCR[J].R3)<<6)&VRAMMask);
        ChrGen=pVDPVidMem+(((int)(VDP[4]&SCR[J].R4)<<11)&VRAMMask);
        SprTab=pVDPVidMem+(((int)(VDP[5]&SCR[J].R5)<<7)&VRAMMask);
        SprGen=pVDPVidMem+(((int)(VDP[6]&SCR[J].R6)<<11)&VRAMMask);
          
        ChrTabM = ((int)(VDP[2]|~SCR[value].M2)<<10)|0x03FF;
        ColTabM = ((int)(VDP[3]|~SCR[value].M3)<<6)|0x1C03F;
        ChrGenM = ((int)(VDP[4]|~SCR[value].M4)<<11)|0x007FF;
        SprTabM = ((int)(VDP[5]|~SCR[value].M5)<<7)|0x1807F;
        ScrMode=J;
      }
      break;
    case  2: 
      ChrTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R2)<<10)&VRAMMask);
      ChrTabM = ((int)(value|~SCR[ScrMode].M2)<<10)|0x03FF;
      break;
    case  3: 
      ColTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R3)<<6)&VRAMMask);
      ColTabM = ((int)(value|~SCR[ScrMode].M3)<<6)|0x1C03F;
      break;
    case  4: 
      ChrGen=pVDPVidMem+(((int)(value&SCR[ScrMode].R4)<<11)&VRAMMask);
      ChrGenM = ((int)(value|~SCR[ScrMode].M4)<<11)|0x007FF;
      break;
    case  5: 
      SprTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R5)<<7)&VRAMMask);
      SprTabM = ((int)(value|~SCR[ScrMode].M5)<<7)|0x1807F;
      break;
    case  6: 
      SprGen=pVDPVidMem+(((int)(value&SCR[ScrMode].R6)<<11)&VRAMMask);
      break;
    case  7: 
      FGColor=value>>4;
      BGColor=value&0x0F;
      if (BGColor)
      {
         u8 r = (u8) ((float) TMS9918A_palette[BGColor*3+0]*0.121568f);
         u8 g = (u8) ((float) TMS9918A_palette[BGColor*3+1]*0.121568f);
         u8 b = (u8) ((float) TMS9918A_palette[BGColor*3+2]*0.121568f);
         BG_PALETTE[0] = RGB15(r,g,b);
      }
      else
      {
          BG_PALETTE[0] = RGB15(0x00,0x00,0x00);
      }          
      break;
  }

  /* Return IRQ, if generated */
  return(bIRQ);
}


/** WrData9918() *********************************************/
/** Write a value V to the VDP Data Port.                   **/
/*************************************************************/
ITCM_CODE void WrData9918(byte V) 
{
    VDPDlatch = pVDPVidMem[VAddr] = V;
    VAddr     = (VAddr+1)&0x3FFF;
}


/** RdData9918() *********************************************/
/** Read a value from the VDP Data Port.                    **/
/*************************************************************/
ITCM_CODE byte RdData9918(void) 
{
  register byte J;

  J         = VDPDlatch;
  VDPDlatch = pVDPVidMem[VAddr];
  VAddr     = (VAddr+1)&0x3FFF;
    
  return(J);
}


/** WrCtrl9918() *********************************************/
/** Write a value V to the VDP Control Port. Enabling IRQs  **/
/** in this function may cause an IRQ to be generated. In   **/
/** this case, WrCtrl9918() returns 1. Returns 0 otherwise. **/
/*************************************************************/
ITCM_CODE byte WrCtrl9918(byte value) 
{
  if(VKey) { VKey=0; VAddr=(VAddr&0xFF00)|value; }
  else 
  {
    VKey=1;
    VAddr = ((VAddr&0x00FF)|((int)value<<8))&0x3FFF;
    switch(value&0xC0) 
    {
      case 0x00:
        VDPDlatch = pVDPVidMem[VAddr];
        VAddr     = (VAddr+1)&0x3FFF;
        break;
      case 0x80:
        /* Enabling IRQs may cause an IRQ here */ 
        return(Write9918(value&0x0F,VAddr&0x00FF));
    }
  }

  /* No interrupts */
  return(0);
}


/** RdCtrl9918() *********************************************/
/** Read a value from the VDP Control Port.                 **/
/*************************************************************/
ITCM_CODE byte RdCtrl9918(void) 
{
  register byte J;

  J = VDPStatus;
  VDPStatus &= (TMS9918_STAT_5THNUM | TMS9918_STAT_5THSPR);
  return(J);
}


/** Loop9918() ***********************************************/
/** Call this routine on every scanline to update the       **/
/** screen buffer. Loop9918() returns 1 if an interrupt is  **/
/** to be generated, 0 otherwise.                           **/
/*************************************************************/
ITCM_CODE byte Loop9918(void) 
{
  extern void colecoUpdateScreen(void);
  register byte bIRQ;

  /* No IRQ yet */
  bIRQ=0;

  /* Increment scanline */
  if(++CurLine>=TMS9918_LINES) CurLine=0;

  /* If refreshing display area, call scanline handler */
  if((CurLine>=TMS9918_START_LINE)&&(CurLine<TMS9918_END_LINE))
      (SCR[ScrMode].Refresh)(CurLine-TMS9918_START_LINE);
  /* If time for VBlank... */
  else if(CurLine==TMS9918_END_LINE) 
  {
      /* Refresh screen */
      colecoUpdateScreen();

      /* Generate IRQ when enabled and when VBlank flag goes up */
      bIRQ=TMS9918_VBlankON && !(VDPStatus&TMS9918_STAT_VBLANK);

      /* Set VBlank status flag */
      VDPStatus|=TMS9918_STAT_VBLANK;

      /* Set Sprite Collision status flag */
      if(!(VDPStatus&TMS9918_STAT_OVRLAP))
        if(CheckSprites()) VDPStatus|=TMS9918_STAT_OVRLAP;
  }

  /* Done */
  return(bIRQ);
}


/** Reset9918() **********************************************/
/** Reset the VDP. The user can provide a new screen buffer **/
/** by pointing Buffer to it and setting Width and Height.  **/
/** Set Buffer to 0 to use the existing screen buffer.      **/
/*************************************************************/
void Reset9918(void) 
{
    memset(VDP,0x00,sizeof(VDP));       // Initialize VDP registers
    memset(pVDPVidMem, 0x00, 0x4000);   // Reset Video memory 
    VKey=1;                             // VDP address latch key
    VDPStatus=0x00;                     // VDP status register
    VAddr = 0x0000;                     // VDP address register
    FGColor=BGColor=0;                  // Fore/Background color
    ScrMode=0;                          // Current screenmode
    CurLine=0;                          // Current scanline
    ChrTab=ColTab=ChrGen=pVDPVidMem;    // VDP tables (screen)
    SprTab=SprGen=pVDPVidMem;           // VDP tables (sprites)
    VDPDlatch = 0;                      // VDP Data latch
   
    ChrGenM = ~0;                       // Full mask
    ColTabM = ~0;                       // Full mask
    ChrGenM = ~0;                       // Full mask
    SprTabM = ~0;                       // Full mask
    
    BG_PALETTE[0] = RGB15(0x00,0x00,0x00);

    // -------------------------------------------------------------
    // Our background/foreground table makes computations FAST!
    // -------------------------------------------------------------
  int colfg,colbg;
  for (colfg=0;colfg<16;colfg++) {
    for (colbg=0;colbg<16;colbg++) {
      lutTablehh[colfg][colbg][ 0] = (colbg<<0) | (colbg<<8) | (colbg<<16) | (colbg<<24); // 0 0 0 0
      lutTablehh[colfg][colbg][ 1] = (colbg<<0) | (colbg<<8) | (colbg<<16) | (colfg<<24); // 0 0 0 1
      lutTablehh[colfg][colbg][ 2] = (colbg<<0) | (colbg<<8) | (colfg<<16) | (colbg<<24); // 0 0 1 0
      lutTablehh[colfg][colbg][ 3] = (colbg<<0) | (colbg<<8) | (colfg<<16) | (colfg<<24); // 0 0 1 1
      lutTablehh[colfg][colbg][ 4] = (colbg<<0) | (colfg<<8) | (colbg<<16) | (colbg<<24); // 0 1 0 0
      lutTablehh[colfg][colbg][ 5] = (colbg<<0) | (colfg<<8) | (colbg<<16) | (colfg<<24); // 0 1 0 1
      lutTablehh[colfg][colbg][ 6] = (colbg<<0) | (colfg<<8) | (colfg<<16) | (colbg<<24); // 0 1 1 0
      lutTablehh[colfg][colbg][ 7] = (colbg<<0) | (colfg<<8) | (colfg<<16) | (colfg<<24); // 0 1 1 1

      lutTablehh[colfg][colbg][ 8] = (colfg<<0) | (colbg<<8) | (colbg<<16) | (colbg<<24); // 1 0 0 0
      lutTablehh[colfg][colbg][ 9] = (colfg<<0) | (colbg<<8) | (colbg<<16) | (colfg<<24); // 1 0 0 1
      lutTablehh[colfg][colbg][10] = (colfg<<0) | (colbg<<8) | (colfg<<16) | (colbg<<24); // 1 0 1 0
      lutTablehh[colfg][colbg][11] = (colfg<<0) | (colbg<<8) | (colfg<<16) | (colfg<<24); // 1 0 1 1
      lutTablehh[colfg][colbg][12] = (colfg<<0) | (colfg<<8) | (colbg<<16) | (colbg<<24); // 1 1 0 0
      lutTablehh[colfg][colbg][13] = (colfg<<0) | (colfg<<8) | (colbg<<16) | (colfg<<24); // 1 1 0 1
      lutTablehh[colfg][colbg][14] = (colfg<<0) | (colfg<<8) | (colfg<<16) | (colbg<<24); // 1 1 1 0
      lutTablehh[colfg][colbg][15] = (colfg<<0) | (colfg<<8) | (colfg<<16) | (colfg<<24); // 1 1 1 1
    }
  }
}
