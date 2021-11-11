/******************************************************************************
*  ColecoDS TMS9928A (video) file
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
** By Sean Young 1999 (sean@msxnet.org).
** Based on code by Mike Balfour. Features added:
** - read-ahead
** - single read/write address
** - AND mask for mode 2
** - multicolor mode
** - undocumented screen modes
** - illegal sprites (max 4 on one line)
** - vertical coordinate corrected -- was one to high (255 => 0, 0 => 1)
** - errors in interrupt emulation
** - back drop correctly emulated.
**
** 19 feb 2000, Sean:
** - now uses plot_pixel (..), so -ror works properly
** - fixed bug in tms.patternmask
**
** 3 nov 2000, Raphael Nabet:
** - fixed a nasty bug in _TMS9928A_sprites. A transparent sprite caused
**   sprites at lower levels not to be displayed, which is wrong.
**
** 3 jan 2001, Sean Young:
** - A few minor cleanups
** - Changed TMS9928A_vram_[rw] and  TMS9928A_register_[rw] to READ8_HANDLER
**   and WRITE8_HANDLER.
** - Got rid of the color table, unused. Also got rid of the old colors,
**   which were commented out anyway.
**
**
** Todo:
** - The screen image is rendered in `one go'. Modifications during
**   screen build up are not shown.
** - Correctly emulate 4,8,16 kb VRAM if needed.
** - uses plot_pixel (...) in TMS_sprites (...), which is rended in
**   in a back buffer created with malloc (). Hmm..
** - Colours are incorrect. [fixed by R Nabet ?]
******************************************************************************/
#include <nds.h>
#include <nds/arm9/console.h> //basic print functionality

#include <stdio.h>
#include <string.h>

#include "../../colecoDS.h"

#include "tms9928a.h"

extern u8 XBuf[256*256];

//u32 lutTablehh[16][16][16];
u32 (*lutTablehh)[16][16] = (void*)0x068A0000;

// Screen handlers and masks for VDP table address registers
tScrMode SCR[MAXSCREEN+1] __attribute__((section(".dtcm")))  = {
  { _TMS9928A_mode0,0x7F,0x00,0x3F,0x00,0x3F },   // SCREEN 0:TEXT 40x24
  { _TMS9928A_mode1,0x7F,0xFF,0x3F,0xFF,0x3F },   // SCREEN 1:TEXT 32x24
  { _TMS9928A_mode2,0x7F,0x80,0x3C,0xFF,0x3F },   // SCREEN 2:BLOCK 256x192
  { RefreshLine3,0x7F,0x00,0x3F,0xFF,0x3F }       // SCREEN 3:GFX 64x48x16
};

/** Palette9918[] ********************************************/
/** 16 standard colors used by TMS9918/TMS9928 VDP chips.   **/
/*************************************************************/
u8 TMS9928A_palette[16*3] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x20,0xC0,0x20,0x60,0xE0,0x60,
  0x20,0x20,0xE0,0x40,0x60,0xE0,0xA0,0x20,0x20,0x40,0xC0,0xE0,
  0xE0,0x20,0x20,0xE0,0x60,0x60,0xC0,0xC0,0x20,0xC0,0xC0,0x80,
  0x20,0x80,0x20,0xC0,0x40,0xA0,0xA0,0xA0,0xA0,0xE0,0xE0,0xE0,
};

u8 pVDPVidMem[0x4000]={0};                      // VDP video memory
u16 CurLine __attribute__((section(".dtcm")));                                 // Current scanline
u8 VDP[8] __attribute__((section(".dtcm")));
u8 VDPStatus __attribute__((section(".dtcm")));
u8 VDPClatch __attribute__((section(".dtcm")));              // VDP registers
u16 VAddr __attribute__((section(".dtcm")));                                 // Storage for VIDRAM addresses
u8 VKey, WKey;                              // VDP address latch key
u8 *ChrGen,*ChrTab,*ColTab;                 // VDP tables (screens)
u8 *SprGen,*SprTab;                         // VDP tables (sprites)
u8 ScrMode __attribute__((section(".dtcm"))); // Current screen mode
u8 FGColor __attribute__((section(".dtcm")));
u8 BGColor __attribute__((section(".dtcm")));                         // Colors

u32 ColTabM = ~0;
u32 ChrGenM = ~0;


/** CheckSprites() *******************************************/
/** This function is periodically called to check for the   **/
/** sprite collisions and 5th sprite, and set appropriate   **/
/** bits in the VDP status register.                        **/
/*************************************************************/
byte ITCM_CODE CheckSprites(void) {
  register int LS,LD;
  //register byte DH,DV,*PS,*PD,*T;
  unsigned int DH,DV;
  unsigned char *PS,*PD,*T;
  //byte I,J,N,*S,*D;
  unsigned int I,J,N;
  unsigned char *S,*D;

  /* Find first sprite to display */
  for(N=0,S=SprTab;(N<32)&&(S[0]!=208);N++,S+=4);

  if(TMS9918_Sprites16) {
    for(J=0,S=SprTab;J<N;J++,S+=4)
      if(S[3]&0x0F)
        for(I=J+1,D=S+4;I<N;I++,D+=4)
          if(D[3]&0x0F) {
            DV=S[0]-D[0];
            if((DV<16)||(DV>240)) {
              DH=S[1]-D[1];
              if((DH<16)||(DH>240)) {
                PS=SprGen+((int)(S[2]&0xFC)<<3);
                PD=SprGen+((int)(D[2]&0xFC)<<3);
                if(DV<16) PD+=DV; else { DV=256-DV;PS+=DV; }
                if(DH>240) { DH=256-DH;T=PS;PS=PD;PD=T; }
                while(DV<16) {
                  LS=((int)PS[0]<<8)+PS[16];
                  LD=((int)PD[0]<<8)+PD[16];
                  if(LD&(LS>>DH)) break;
                  else { DV++;PS++;PD++; }
                }
                if(DV<16) return(1);
              }
            }
          }
  }
  else {
    for(J=0,S=SprTab;J<N;J++,S+=4)
      if(S[3]&0x0F)
        for(I=J+1,D=S+4;I<N;I++,D+=4)
          if(D[3]&0x0F) {
            DV=S[0]-D[0];
            if((DV<8)||(DV>248)) {
              DH=S[1]-D[1];
              if((DH<8)||(DH>248)) {
                PS=SprGen+((int)S[2]<<3);
                PD=SprGen+((int)D[2]<<3);
                if(DV<8) PD+=DV; else { DV=256-DV;PS+=DV; }
                if(DH>248) { DH=256-DH;T=PS;PS=PD;PD=T; }
                while((DV<8)&&!(*PD&(*PS>>DH))) { DV++;PS++;PD++; }
                if(DV<8) return(1);
              }
            }
          }
  }

  /* No collision */
  return(0);
}
/*
void CheckSprites(void) {
  register u16 LS,LD;
  register byte DH,DV,*PS,*PD,*T;
  byte I,J,N,*S,*D;

  VDPStatus=(VDPStatus&0x9F)|0x1F;
  for(N=0,S=SprTab;(N<32)&&(S[0]!=208);N++,S+=4);

  if(Sprites16x16) {
    for(J=0,S=SprTab;J<N;J++,S+=4)
      if(S[3]&0x0F)
        for(I=J+1,D=S+4;I<N;I++,D+=4)
          if(D[3]&0x0F)
          {
            DV=S[0]-D[0];
            if((DV<16)||(DV>240))
            {
              DH=S[1]-D[1];
              if((DH<16)||(DH>240))
              {
                PS=SprGen+((long)(S[2]&0xFC)<<3);
                PD=SprGen+((long)(D[2]&0xFC)<<3);
                if(DV<16) PD+=DV; else { DV=256-DV;PS+=DV; }
                if(DH>240) { DH=256-DH;T=PS;PS=PD;PD=T; }
                while(DV<16)
                {
                  LS=((u16)*PS<<8)+*(PS+16);
                  LD=((u16)*PD<<8)+*(PD+16);
                  if(LD&(LS>>DH)) break;
                  else { DV++;PS++;PD++; }
                }
                if(DV<16) { VDPStatus|=0x20;return; }
              }
            }
          }
  }
  else   {
    for(J=0,S=SprTab;J<N;J++,S+=4)
      if(S[3]&0x0F)
        for(I=J+1,D=S+4;I<N;I++,D+=4)
          if(D[3]&0x0F)
          {
            DV=S[0]-D[0];
            if((DV<8)||(DV>248))
            {
              DH=S[1]-D[1];
              if((DH<8)||(DH>248))
              {
                PS=SprGen+((long)S[2]<<3);
                PD=SprGen+((long)D[2]<<3);
                if(DV<8) PD+=DV; else { DV=256-DV;PS+=DV; }
                if(DH>248) { DH=256-DH;T=PS;PS=PD;PD=T; }
                while((DV<8)&&!(*PD&(*PS>>DH))) { DV++;PS++;PD++; }
                if(DV<8) { VDPStatus|=0x20;return; }
              }
            }
          }
  }
}
*/

/** RefreshSprites() *****************************************/
/** This function is called from RefreshLine#() to refresh  **/
/** sprites.                                                **/
/*************************************************************/
void ITCM_CODE RefreshSprites(register byte Y) {
  static const byte SprHeights[4] = { 8,16,16,32 };
  byte OH,IH,*PT,*AT;
  byte *P,*T,C;
  signed int L,K;
  unsigned int M;
  unsigned short *POPT, COPT;
  
  T  = XBuf+256*Y;
  OH = SprHeights[VDP[1]&0x03];
  IH = SprHeights[VDP[1]&0x02];
  AT = SprTab-4;
  C  = 8;   // Was 4... allow 8 sprites on a line
  M  = 0;
  L  = 0;

  do {
    M<<=1;AT+=4;L++;     /* Iterate through SprTab */
    K=AT[0];             /* K = sprite Y coordinate */
    if(K==208) break;    /* Iteration terminates if Y=208 */
    if(K>256-IH) K-=256; /* Y coordinate may be negative */

    /* Mark all valid sprites with 1s, break at MaxSprites */
    if((Y>K)&&(Y<=K+OH)) { M|=1;if(!--C) break; }
  }
  while(L<32);

  for(;M;M>>=1,AT-=4)
    if(M&1) {
      C=AT[3];                  /* C = sprite attributes */
      L=C&0x80? AT[1]-32:AT[1]; /* Sprite may be shifted left by 32 */
      C&=0x0F;                  /* C = sprite color */

      if((L<256)&&(L>-OH)&&C)
      {
        K=AT[0];                /* K = sprite Y coordinate */
        if(K>256-IH) K-=256;    /* Y coordinate may be negative */

        P  = T+L;
        K  = Y-K-1;
        PT = SprGen
           + ((int)(IH>8? (AT[2]&0xFC):AT[2])<<3)
           + (OH>IH? (K>>1):K);

        /* Mask 1: clip left sprite boundary */
        K=L>=0? 0xFFFF:(0x10000>>(OH>IH? (-L>>1):-L))-1;

        /* Mask 2: clip right sprite boundary */
        L+=OH-257;
        if(L>=0) {
          L=(IH>8? 0x0002:0x0200)<<(OH>IH? (L>>1):L);
          K&=~(L-1);
        }

        /* Get and clip the sprite data */
        K&=((int)PT[0]<<8)|(IH>8? PT[16]:0x00);

        if(OH>IH) {
          /* Big (zoomed) sprite */
          /* Draw left 16 pixels of the sprite */
          POPT = (unsigned short *)P;
          COPT = (C<<8) | C;
          if(K&0xFF00) {
            if(K&0x8000) POPT[0] = COPT;
            if(K&0x4000) POPT[1] = COPT;
            if(K&0x2000) POPT[2] = COPT;
            if(K&0x1000) POPT[3] = COPT;
            if(K&0x0800) POPT[4] = COPT;
            if(K&0x0400) POPT[5] = COPT;
            if(K&0x0200) POPT[6] = COPT;
            if(K&0x0100) POPT[7] = COPT;
            /*
            if(K&0x8000) P[1]=P[0]=C;
            if(K&0x4000) P[3]=P[2]=C;
            if(K&0x2000) P[5]=P[4]=C;
            if(K&0x1000) P[7]=P[6]=C;
            if(K&0x0800) P[9]=P[8]=C;
            if(K&0x0400) P[11]=P[10]=C;
            if(K&0x0200) P[13]=P[12]=C;
            if(K&0x0100) P[15]=P[14]=C;
            */
          }

          /* Draw right 16 pixels of the sprite */
          if(K&0x00FF) {
            /*
            if(K&0x0080) P[17]=P[16]=C;
            if(K&0x0040) P[19]=P[18]=C;
            if(K&0x0020) P[21]=P[20]=C;
            if(K&0x0010) P[23]=P[22]=C;
            if(K&0x0008) P[25]=P[24]=C;
            if(K&0x0004) P[27]=P[26]=C;
            if(K&0x0002) P[29]=P[28]=C;
            if(K&0x0001) P[31]=P[30]=C;
            */
          }
        }
        else {
          /* Normal (unzoomed) sprite */

          /* Draw left 8 pixels of the sprite */
          if(K&0xFF00) {
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
/*

void RefreshSprites(u8 uY) {
  register byte C,H;
  register byte *P,*PT,*AT;
  register int L,K;
  register unsigned int M;

  H=Sprites16x16? 16:8;
  C=0;M=0;L=0;AT=SprTab-4;
  do
  {
    M<<=1;AT+=4;L++;    // Iterating through SprTab 
    K=AT[0];            // K = sprite Y coordinate 
    if(K==208) break;   // Iteration terminates if Y=208 
    if(K>256-H) K-=256; // Y coordinate may be negative 

    // Mark all valid sprites with 1s, break at 4 sprites 
    if((uY>K)&&(uY<=K+H)) { M|=1;if(++C==4) break; }
  }
  while(L<32);

  for(;M;M>>=1,AT-=4)
    if(M&1) {
      C=AT[3];                  // C = sprite attributes
      L=C&0x80? AT[1]-32:AT[1]; // Sprite may be shifted left by 32 
      C&=0x0F;                  // C = sprite color 

      if((L<256)&&(L>-H)&&C)
      {
        K=AT[0];                // K = sprite Y coordinate 
        if(K>256-H) K-=256;     // Y coordinate may be negative 

        P=XBuf+(uY<<8)+L;
        PT=SprGen+((int)(H>8? AT[2]&0xFC:AT[2])<<3)+uY-K-1;

        // Mask 1: clip left sprite boundary 
        K=L>=0? 0x0FFFF:(0x10000>>-L)-1;
        // Mask 2: clip right sprite boundary 
        if(L>256-H) K^=((0x00200>>(H-8))<<(L-257+H))-1;
        // Get and clip the sprite data 
        K&=((int)PT[0]<<8)|(H>8? PT[16]:0x00);

        // Draw left 8 pixels of the sprite 
        if(K&0xFF00) {
          if(K&0x8000) P[0]=C;if(K&0x4000) P[1]=C;
          if(K&0x2000) P[2]=C;if(K&0x1000) P[3]=C;
          if(K&0x0800) P[4]=C;if(K&0x0400) P[5]=C;
          if(K&0x0200) P[6]=C;if(K&0x0100) P[7]=C;
        }

        // Draw right 8 pixels of the sprite 
        if(K&0x00FF) {
          if(K&0x0080) P[8]=C; if(K&0x0040) P[9]=C;
          if(K&0x0020) P[10]=C;if(K&0x0010) P[11]=C;
          if(K&0x0008) P[12]=C;if(K&0x0004) P[13]=C;
          if(K&0x0002) P[14]=C;if(K&0x0001) P[15]=C;
        }
      }
    }
}
*/
/** RefreshLine0() *******************************************/
/** Refresh line Y (0..191) of SCREEN0, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE _TMS9928A_mode0(u8 uY) {
  register byte X,K,Offset,FC,BC;
  register u8 *P;
  u8 *T;

  P=XBuf+(uY<<8);

  if(!ScreenON) 
    memset(P,BGColor,256);
  else {
    BC=BGColor;
    FC=FGColor;
    T=ChrTab+(uY>>3)*40;
    Offset=uY&0x07;
  
    for(X=0;X<40;X++) {
      K=ChrGen[((int)*T<<3)+Offset];
      P[0]=K&0x80? FC:BC;P[1]=K&0x40? FC:BC;
      P[2]=K&0x20? FC:BC;P[3]=K&0x10? FC:BC;
      P[4]=K&0x08? FC:BC;P[5]=K&0x04? FC:BC;
      P+=6;T++;
    }
  }


/*
  register byte *T,X,K,Offset;
  register pixel *P,FC,BC;

  P  = (pixel *)(VDP->XBuf)
     + VDP->Width*(Y+(VDP->Height-192)/2)
     + VDP->Width/2-128+8;
  BC = VDP->BGColor;
  FC = VDP->FGColor;

  if(!TMS9918_ScreenON(VDP))
    for(X=0;X<240;X++) *P++=BC;
  else
  {
    T=VDP->ChrTab+(Y>>3)*40;
    Offset=Y&0x07;

    for(X=0;X<40;X++)
    {
      K=VDP->ChrGen[((int)*T<<3)+Offset];
      P[0]=K&0x80? FC:BC;
      P[1]=K&0x40? FC:BC;
      P[2]=K&0x20? FC:BC;
      P[3]=K&0x10? FC:BC;
      P[4]=K&0x08? FC:BC;
      P[5]=K&0x04? FC:BC;
      P+=6;T++;
    }
*/
}

/** RefreshLine1() *******************************************/
/** Refresh line Y (0..191) of SCREEN1, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE _TMS9928A_mode1(u8 uY) {
  register byte X,K,Offset,FC,BC;
  register u8 *T;
  register u32 *P;
  u32 *ptLut;

  P=(u32*) (XBuf+(uY<<8));

  if(!ScreenON) 
    memset(P,BGColor,256);
  else {
    T=ChrTab+(uY>>3)*32;
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
}

/** RefreshLine2() *******************************************/
/** Refresh line Y (0..191) of SCREEN2, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE _TMS9928A_mode2(u8 uY) {
  unsigned int X,K,BC,Offset;
  register u8 *T;
  register u32 *P;
  register byte *PGT,*CLT;
  unsigned int I;
  u32 *ptLut;

  P=(u32*)(XBuf+(uY<<8));

  if (!ScreenON) 
    memset(P,BGColor,256);
  else {
    Offset=uY&0x07;
    PGT = ChrGen+Offset;
    CLT = ColTab+Offset;
    if (uY >= 0x80) {
      if (VDP[4] & 0x02) {
        PGT += (0x200 << 3);
        CLT += (0x200 << 3);
      }
    } else if (uY >= 0x40) {
      if (VDP[4] & 0x01) {
        PGT += (0x100 << 3);
        CLT += (0x100 << 3);
      }
    }
    //PGT = ChrGen+Offset+((uY & 0xC0) << 5);
    //CLT = ColTab+Offset+((uY & 0xC0) << 5);
    T = ChrTab + ((uY & 0xF8) << 2);
    for(X=0;X<32;X++) {
      I=((int)*T<<3);
      K=PGT[I & ChrGenM];
      BC=CLT[I & ColTabM];
      ptLut = (u32*)(lutTablehh[BC>>4][BC&0x0F]);
      *P++ = *(ptLut + ((K>>4)));
      *P++ = *(ptLut + ((K & 0xF)));
      T++;
    }
    RefreshSprites(uY);
  }    
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
    //T=ChrTab+(uY>>3)*32;
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
}

u32 M3=0x7F;
u32 M4=0x03;

/*********************************************************************************
 * Emulator calls this function to write byte V into a VDP register R
 ********************************************************************************/
ITCM_CODE byte Write9918(int iReg,u8 value) { 
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
        case 0x02: J=0;break;
        case 0x04: J=3;break;
        default:   J=ScrMode;break;
      }

      /* If mode was changed, recompute table addresses */
      if((J!=ScrMode)||!VRAMMask) {
        VRAMMask    = TMS9918_VRAMMask;
        ChrTab=pVDPVidMem+(((int)(VDP[2]&SCR[J].R2)<<10)&VRAMMask);
        ColTab=pVDPVidMem+(((int)(VDP[3]&SCR[J].R3)<<6)&VRAMMask);
        ChrGen=pVDPVidMem+(((int)(VDP[4]&SCR[J].R4)<<11)&VRAMMask);
        SprTab=pVDPVidMem+(((int)(VDP[5]&SCR[J].R5)<<7)&VRAMMask);
        SprGen=pVDPVidMem+(((int)(VDP[6]&SCR[J].R6)<<11)&VRAMMask);
          
        ColTabM = ((int)(VDP[3]|~M3)<<6)|0x1C03F;
        ChrGenM = ((int)(VDP[4]|~M4)<<11)|0x007FF;          
        ScrMode=J;
      }
      break;
    case  2: 
      ChrTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R2)<<10)&VRAMMask);
      break;
    case  3: 
      ColTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R3)<<6)&VRAMMask);
      ColTabM = ((int)(value|~M3)<<6)|0x1C03F;
      break;
    case  4: 
      ChrGen=pVDPVidMem+(((int)(value&SCR[ScrMode].R4)<<11)&VRAMMask);
      ChrGenM = ((int)(value|~M4)<<11)|0x007FF;          
      break;
    case  5: 
      SprTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R5)<<7)&VRAMMask);
      break;
    case  6: 
      //value&=0x3F;
      //SprGen=pVDPVidMem+((long)value<<11);
      SprGen=pVDPVidMem+(((int)(value&SCR[ScrMode].R6)<<11)&VRAMMask);
      break;
    case  7: 
      FGColor=value>>4;
      BGColor=value&0x0F;
      if (BGColor)
      {
         u8 r = (u8) ((float) TMS9928A_palette[BGColor*3+0]*0.121568f);
         u8 g = (u8) ((float) TMS9928A_palette[BGColor*3+1]*0.121568f);
         u8 b = (u8) ((float) TMS9928A_palette[BGColor*3+2]*0.121568f);
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
ITCM_CODE void WrData9918(byte V) {
#ifdef DEBUG
  //iprintf("WrData9918 %d wkey%d\n",V,WKey);
#endif

  if(WKey) {
//  if(!VKey) {
    /* VDP in the proper WRITE mode */
    //VDP->DLatch = VDP->VDPVRAM[VDP->VAddr]=V;
    pVDPVidMem[VAddr]=V;
    VAddr  = (VAddr+1)&0x3FFF;
  }
  else
  {
    /* VDP in the READ mode */
    //VDP->DLatch = VDP->VDPVRAM[VDP->VAddr];
    VAddr  = (VAddr+1)&0x3FFF;
    pVDPVidMem[VAddr]=V;
  }
}

/** WrCtrl9918() *********************************************/
/** Write a value V to the VDP Control Port. Enabling IRQs  **/
/** in this function may cause an IRQ to be generated. In   **/
/** this case, WrCtrl9918() returns 1. Returns 0 otherwise. **/
/*************************************************************/
ITCM_CODE byte WrCtrl9918(byte value) {
  if(VKey) { VKey=0;VDPClatch=value; }
  else {
    VKey=1;
    switch(value&0xC0) {
      case 0x00:
      case 0x40:
        VAddr=(VDPClatch|((int)value<<8))&0x3FFF;
        WKey=value&0x40;
        break;
      case 0x80:
        /* Enabling IRQs may cause an IRQ here */ 
        return(Write9918(value&0x07,VDPClatch));
    }
  }

  /* No interrupts */
  return(0);
}


/** RdData9918() *********************************************/
/** Read a value from the VDP Data Port.                    **/
/*************************************************************/
ITCM_CODE byte RdData9918(void) {
  register byte J;

/*
  J=VDP->DLatch;
  VDP->DLatch=VDP->VRAM[VDP->VAddr];
*/
  J          = pVDPVidMem[VAddr];
  VAddr = (VAddr+1)&0x3FFF;
  return(J);
}


/** RdCtrl9918() *********************************************/
/** Read a value from the VDP Control Port.                 **/
/*************************************************************/
ITCM_CODE byte RdCtrl9918(void) {
  register byte J;

  J = VDPStatus;
  VDPStatus&= TMS9918_STAT_5THNUM;
  //VKey   = 1;
  return(J);
}

u8  UCount=0;

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
void Reset9918(void) {
    extern u8 VDPInit[8];
    memcpy(VDP,VDPInit,sizeof(VDP));   // Initialize VDP registers
    memset(pVDPVidMem, 0x00, 0x4000);
    VKey=1;                              // VDP address latch key
    WKey=1;
    VDPStatus=0x00;                      // VDP status register
    VAddr = 0x0000;                      // VDP address register
    FGColor=BGColor=0;                   // Fore/Background color
    ScrMode=0;                           // Current screenmode
    CurLine=0;                           // Current scanline
    ChrTab=ColTab=ChrGen=pVDPVidMem;     // VDP tables (screen)
    SprTab=SprGen=pVDPVidMem;            // VDP tables (sprites)
    UCount = 0;

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

