/******************************************************************************
*  ColecoDS TMS9918A (video) file
*  Ver 1.0
*
** File: tms9928a.c -- software implementation of the Texas Instruments
**                     TMS9918A used by the Colecovision.
**
** All undocumented features as described in the following file
** should be emulated.
**
** http://www.msxnet.org/tech/tms9918a.txt
**
** Note: much of this file is from the ColEM emulator core by Marat Fayzullin
**       but heavily modified for specific NDS use. If you want to use this
**       code, you are advised to seek out the latest ColEM core online.
**       We've added the VDP Control Latch reset on data read/write and
**       on control reads - this was missing from the ColEM handlers. 
*        We've also patched in a lutTablehh[] for very fast color handling.
**
******************************************************************************/
#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "../../colecoDS.h"
#include "../../colecogeneric.h"

#include "tms9918a.h"

u8 MaxSprites[2] __attribute__((section(".dtcm"))) = {32, 4};     // Normally the CV only shows 4 sprites on a line... for emulation we bump this up if configured

u16 *pVidFlipBuf __attribute__((section(".dtcm"))) = (u16*) (0x06000000);    // Video flipping buffer

u8 XBuf_A[256*256] ALIGN(32) = {0}; // Really it's only 256x192 - Ping Pong Buffer A
u8 XBuf_B[256*256] ALIGN(32) = {0}; // Really it's only 256x192 - Ping Pong Buffer B
u8 *XBuf __attribute__((section(".dtcm"))) = XBuf_A;

// Look up table for colors - pre-generated and in VRAM for maximum speed!
u32 (*lutTablehh)[16][16] __attribute__((section(".dtcm"))) = (void*)0x068A0000;

// For 16K only VDP modes
u8 vdp_16k_mode_only __attribute__((section(".dtcm"))) = 0;

u16 vdp_int_source       __attribute__((section(".dtcm"))) = 0;
u16 my_config_clear_int  __attribute__((section(".dtcm"))) = 0;


u8 OH __attribute__((section(".dtcm"))) = 0;
u8 IH __attribute__((section(".dtcm"))) = 0;

// ---------------------------------------------------------------------------------------
// Screen handlers and masks for VDP table address registers. 
// Screen modes are confusing as different documentation (MSX, Coleco, VDP manuals, etc)
// all seem to refer to 'Modes' vs 'Screens' vs more colorful names for the modes
// plus there are the undocumented modes. So I've done my best to comment using 
// all of the names you will find out there in the wild world of VDP documentation!
// ---------------------------------------------------------------------------------------
tScrMode SCR[MAXSCREEN+1] __attribute__((section(".dtcm")))  = {
                // R2,  R3,  R4,  R5,  R6,  M2,  M3,  M4,  M5
  { RefreshLine0,0x7F,0x00,0x3F,0x00,0x3F,0x00,0x00,0x00,0x00 }, /* VDP Mode 1 aka MSX SCREEN 0 aka "TEXT 1"     */
  { RefreshLine1,0x7F,0xFF,0x3F,0xFF,0x3F,0x00,0x00,0x00,0x00 }, /* VDP Mode 0 aka MSX SCREEN 1 aka "GRAPHIC 1"  */
  { RefreshLine2,0x7F,0x80,0x3C,0xFF,0x3F,0x00,0x7F,0x03,0x00 }, /* VDP Mode 3 aka MSX SCREEN 2 aka "GRAPHIC 2"  */
  { RefreshLine3,0x7F,0x00,0x3F,0xFF,0x3F,0x00,0x00,0x00,0x00 }, /* VDP Mode 2 aka MSX SCREEN 3 aka "MULTICOLOR" */
};

void (*RefreshLine)(u8 uY) __attribute__((section(".dtcm"))) = RefreshLine0;

/** Palette9918[] ********************************************/
/** 16 standard colors used by TMS9918/TMS9928 VDP chips.   **/
/*************************************************************/
u8 TMS9918A_palette[16*3] __attribute__((section(".dtcm")))  = {
  0x00,0x00,0x00,   0x00,0x00,0x00,   0x20,0xC0,0x20,   0x60,0xE0,0x60,
  0x20,0x20,0xE0,   0x40,0x60,0xE0,   0xA0,0x20,0x20,   0x40,0xC0,0xE0,
  0xE0,0x20,0x20,   0xE0,0x60,0x60,   0xC0,0xC0,0x20,   0xC0,0xC0,0x80,
  0x20,0x80,0x20,   0xC0,0x40,0xA0,   0xA0,0xA0,0xA0,   0xE0,0xE0,0xE0,
};

u8 pVDPVidMem[0x10000] ALIGN(32) ={0};                  // VDP video memory... only 16K but we set to 64K so we don't have to handle out of bounds checks

u16 CurLine     __attribute__((section(".dtcm")));      // Current scanline
u8 VDP[16]      __attribute__((section(".dtcm")));      // VDP Registers
u8 VDPStatus    __attribute__((section(".dtcm")));      // VDP Status
u8 VDPDlatch    __attribute__((section(".dtcm")));      // VDP register D Latch
u16 VAddr       __attribute__((section(".dtcm")));      // Storage for VIDRAM addresses
u8 VDPCtrlLatch __attribute__((section(".dtcm")));      // VDP control latch key
u8 *ChrGen      __attribute__((section(".dtcm")));      // VDP tables (screens)
u8 *ChrTab      __attribute__((section(".dtcm")));      // VDP tables (screens)
u8 *ColTab      __attribute__((section(".dtcm")));      // VDP tables (screens)
u8 *SprGen      __attribute__((section(".dtcm")));      // VDP tables (sprites)
u8 *SprTab      __attribute__((section(".dtcm")));      // VDP tables (sprites)
u8 ScrMode      __attribute__((section(".dtcm")));      // Current screen mode
u8 FGColor      __attribute__((section(".dtcm")));      // Foreground Color
u8 BGColor      __attribute__((section(".dtcm")));      // Background Color

// Sprite and Character Masks for the VDP
u16 ChrTabM     __attribute__((section(".dtcm"))) = 0x3FFF;
u16 ColTabM     __attribute__((section(".dtcm"))) = 0x3FFF;
u16 ChrGenM     __attribute__((section(".dtcm"))) = 0x3FFF;
u16 SprTabM     __attribute__((section(".dtcm"))) = 0x3FFF;


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
  DV = TMS9918_Sprites16 ? -16:-8;
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
int ITCM_CODE ScanSprites(register byte Y,unsigned int *Mask)
{
  register byte *AT;
  register u8 L,K,C1,C2;
  register unsigned int M;

  /* No 5th sprite yet */
  VDPStatus &= ~(TMS9918_STAT_5THNUM|TMS9918_STAT_5THSPR);

  /* Must have MODE1+ and screen enabled */
  if(!ScrMode || !TMS9918_ScreenON)
  {
    if(Mask) *Mask=0;
    return(-1);
  }

  AT = SprTab;
  C1 = MaxSprites[myConfig.maxSprites]+1;
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
  register byte *PT,*AT;
  register byte *P,*T,C;
  register int L,K,N;
  unsigned int M;

  /* Find sprites to show, update 5th sprite status */
  N = ScanSprites(Y,&M);
  if((N<0) || !M) return;

  T  = XBuf+256*Y;
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


/** RefreshLine0() *******************************************/
/** Refresh line Y (0..191) of SCREEN0, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void RefreshLine0(u8 Y) 
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
}

/** RefreshLine1() *******************************************/
/** Refresh line Y (0..191) of SCREEN1, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE RefreshLine1(u8 uY) 
{
  register byte X,K=0,Offset,FC,BC;
  register u8 *T;
  register u32 *P;
  u8 lastT;
  u32 *ptLut=0;

  P=(u32*) (XBuf+(uY<<8));
  u32 ptLow = 0; u32 ptHigh = 0;

  if(!ScreenON) 
    memset(P,BGColor,256);
  else 
  {
    T=ChrTab+((int)(uY&0xF8)<<2);
    Offset=uY&0x07;

    lastT = ~(*T);
      
    for(X=0;X<32;X++) 
    {
      if (lastT != *T)
      {
          lastT=*T;
          BC=ColTab[lastT>>3];
          K=ChrGen[((int)lastT<<3)+Offset];
          FC=BC>>4;
          BC=BC&0x0F;
          ptLut = (u32*) (lutTablehh[FC][BC]);
          ptLow = *(ptLut + ((K>>4)));
          ptHigh= *(ptLut + ((K & 0xF)));
      }
      *P++ = ptLow;
      *P++ = ptHigh;
      T++;
    }
    RefreshSprites(uY);
  }
}

/** RefreshLine2() *******************************************/
/** Refresh line Y (0..191) of SCREEN2, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
void ITCM_CODE RefreshLine2(u8 uY) {
  u32 *P;
  register byte FC,BC;
  register byte X,K,*T;
  u8 lastT;
  u16 J,I;
  u32 *ptLut;

  P=(u32*)(XBuf+(uY<<8));

  if (!ScreenON) 
    memset(P,BGColor,256);
  else 
  {
    u32 ptLow = 0; u32 ptHigh = 0;
      
    J   = ((u16)((u16)uY&0xC0)<<5)+(uY&0x07);
    T   = ChrTab+((u16)((u16)uY&0xF8)<<2);
    lastT = ~(*T);

    for(X=0;X<32;X++)
    {
      if (lastT != *T)
      {
          lastT = *T;
          I    = (u16)lastT<<3;
          K    = ColTab[(J+I)&ColTabM];
          FC   = (K>>4);
          BC   = K & 0x0F;
          K    = ChrGen[(J+I)&ChrGenM];
          ptLut = (u32*)(lutTablehh[FC][BC]);
          ptLow = *(ptLut + ((K>>4)));
          ptHigh = *(ptLut + ((K & 0xF)));
      } 
      *P++ = ptLow;
      *P++ = ptHigh;
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
  u8 lastT;
  P=XBuf+(uY<<8);

  if(!TMS9918_ScreenON) {
    memset(P,BGColor,256);
  }
  else {
    u8 ptLow = 0; u8 ptHigh = 0;
    T=ChrTab+((int)(uY&0xF8)<<2);
    lastT = ~(*T);
    Offset=(uY&0x1C)>>2;
    for(X=0;X<32;X++) 
    {
      if (lastT != *T)
      {
          lastT = *T;
          K=ChrGen[((int)lastT<<3)+Offset];
          ptLow = K>>4;
          ptHigh = K&0x0F;
      }
      P[0]=P[1]=P[2]=P[3]=ptLow;
      P[4]=P[5]=P[6]=P[7]=ptHigh;
      P+=8;T++;
    }
    RefreshSprites(uY);
  }
}


/*********************************************************************************
 * Emulator calls this function to write byte 'value' into a VDP register 'iReg'
 ********************************************************************************/
u8 VDP_RegisterMasks[] __attribute__((section(".dtcm"))) = { 0x03, 0xfb, 0x0f, 0xff, 0x07, 0x7f, 0x07, 0xff };
byte SprHeights[4] __attribute__((section(".dtcm"))) = { 8,16,16,32 };

byte Write9918(u8 iReg, u8 value) 
{ 
  int newMode;
  int VRAMMask;
  byte bIRQ;
    
  iReg &= 0x07;
  value &= VDP_RegisterMasks[iReg];
    
  /* Enabling IRQs may cause an IRQ here */
  bIRQ  = (iReg==1) && ((VDP[1]^value)&value&TMS9918_REG1_IRQ) && (VDPStatus&TMS9918_STAT_VBLANK);

  /* VRAM can either be 4kB or 16kB - this checks if the bit has changed on this call which will force the logic in case 1 below */
  if (vdp_16k_mode_only) VRAMMask = 0x3FFF;    // For these machines, we only support 16K
  else VRAMMask = (iReg==1) && ( (VDP[1]^value) & TMS9918_REG1_RAM16K ) ? 0 : TMS9918_VRAMMask;  

  /* Store value into the register */
  VDP[iReg]=value;

  /* Depending on the register, do... */  
  switch (iReg) {
    case 0: /* Mode register 0 */
    case 1: /* Mode register 1 */
    // Figure out new screen mode number:
    //              M1      M2      M3      VDP Mode
    //      0x00    0       0       0       Mode 0   - MSX SCREEN 1
    //      0x01    0       0       1       Mode 3   - MSX SCREEN 2
    //      0x02    0       1       0       Mode 2   - MSX SCREEN 3
    //      0x04    1       0       0       Mode 1   - MSX SCREEN 0
    //      0x06    1       1       0       Mode 1+2 - Undocumented. Like Mode 1.
    //      0x03    0       1       1       Mode 2+3 - Undocumented. Like Mode 3.
      switch(TMS9918_Mode) 
      {
        case 0x00: newMode=1;break;         /* VDP Mode 0 aka MSX SCREEN 1 aka "GRAPHIC 1"     */
        case 0x01: newMode=2;break;         /* VDP Mode 3 aka MSX SCREEN 2 aka "GRAPHIC 2"     */
        case 0x02: newMode=3;break;         /* VDP Mode 2 aka MSX SCREEN 3 aka "MULTICOLOR"    */
        case 0x04: newMode=0;break;         /* VDP Mode 1 aka MSX SCREEN 0 aka "TEXT 1"        */
        case 0x06: newMode=0;break;         /* Undocumented Mode 1+2 is like Mode 1 (SCREEN 0) */
        case 0x03: newMode=2;break;         /* Undocumented Mode 2+3 is like Mode 3 (SCREEN 2) */
        default:   newMode=ScrMode;break;   /* Best we can do - just keep screen mode as-is    */
      }
          
      /* If mode was changed or VRAM size changed: recompute table addresses */
      if ((newMode!=ScrMode) || !VRAMMask) 
      {
        VRAMMask    = TMS9918_VRAMMask;
        ScrMode=newMode;
        RefreshLine = SCR[ScrMode].Refresh;
        ChrTab=pVDPVidMem+(((int)(VDP[2]&SCR[ScrMode].R2)<<10)&VRAMMask);
        ColTab=pVDPVidMem+(((int)(VDP[3]&SCR[ScrMode].R3)<<6)&VRAMMask);
        ChrGen=pVDPVidMem+(((int)(VDP[4]&SCR[ScrMode].R4)<<11)&VRAMMask);
        SprTab=pVDPVidMem+(((int)(VDP[5]&SCR[ScrMode].R5)<<7)&VRAMMask);
        SprGen=pVDPVidMem+(((int)(VDP[6]&SCR[ScrMode].R6)<<11)&VRAMMask);
          
        ChrTabM = ((int)(VDP[2]|(u8)~SCR[ScrMode].M2)<<10)|0x03FF;
        ColTabM = ((int)(VDP[3]|(u8)~SCR[ScrMode].M3)<<6) |0x003F;
        ChrGenM = ((int)(VDP[4]|(u8)~SCR[ScrMode].M4)<<11)|0x07FF;
        SprTabM = ((int)(VDP[5]|(u8)~SCR[ScrMode].M5)<<7) |0x007F;
      }
          
      OH = SprHeights[VDP[1]&0x03];
      IH = SprHeights[VDP[1]&0x02];
          
      break;
    case  2: 
      ChrTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R2)<<10)&VRAMMask);
      ChrTabM = ((int)(value|(u8)~SCR[ScrMode].M2)<<10)|0x03FF;
      break;
    case  3: 
      ColTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R3)<<6)&VRAMMask);
      ColTabM = ((int)(value|(u8)~SCR[ScrMode].M3)<<6)|0x003F;
      break;
    case  4: 
      ChrGen=pVDPVidMem+(((int)(value&SCR[ScrMode].R4)<<11)&VRAMMask);
      ChrGenM = ((int)(value|(u8)~SCR[ScrMode].M4)<<11)|0x07FF;
      break;
    case  5: 
      SprTab=pVDPVidMem+(((int)(value&SCR[ScrMode].R5)<<7)&VRAMMask);
      SprTabM = ((int)(value|(u8)~SCR[ScrMode].M5)<<7)|0x007F;
      break;
    case  6: 
      SprGen=pVDPVidMem+(((int)(value&SCR[ScrMode].R6)<<11)&VRAMMask);
      break;
    case  7: 
      FGColor=value>>4;
      BGColor=value&0x0F;
      if (BGColor)
      {
         // Handle "transparency"
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


/** RdData9918() *********************************************/
/** Read a value from the VDP Data Port.                    **/
/*************************************************************/
ITCM_CODE byte RdData9918(void) 
{
  byte data;

  data      = VDPDlatch;
  VDPDlatch = pVDPVidMem[VAddr];
  VAddr     = (VAddr+1)&0x3FFF;
  VDPCtrlLatch = 0;

  return(data);
}


/** WrCtrl9918() *********************************************/
/** Write a value V to the VDP Control Port. Enabling IRQs  **/
/** in this function may cause an IRQ to be generated. In   **/
/** this case, WrCtrl9918() returns 1. Returns 0 otherwise. **/
/*************************************************************/
ITCM_CODE byte WrCtrl9918(byte value) 
{
  if(VDPCtrlLatch)  // Write the high byte of the video address
  { 
    VDPCtrlLatch=0; // Set the VDP flip-flop so we do the low byte next
      
    VAddr = ((VAddr&0x00FF)|((u16)value<<8))&0x3FFF;                                // Set the high byte of the video address always
    if (value & 0x80) return(Write9918(value&0x07,VAddr&0x00FF));                   // Might generate an IRQ if we end up enabling interrupts and VBlank set
    if (!(value & 0x40)) {VDPDlatch = pVDPVidMem[VAddr]; VAddr = (VAddr+1)&0x3FFF;} // As long as we're not read inhibited (either uppper 2 bits set), read ahead
  }
  else  // Write the low byte of the video address / control register
  {
      VDPCtrlLatch=1;   // Set the VDP flip-flow so we do the high byte next
      VAddr=(VAddr&0xFF00)|value; 
  }

  /* No interrupts */
  return(0);
}


/** RdCtrl9918() *********************************************/
/** Read a value from the VDP Control Port.                 **/
/*************************************************************/
ITCM_CODE byte RdCtrl9918(void) 
{
  byte data;

  data = VDPStatus;
  VDPStatus &= (TMS9918_STAT_5THNUM | TMS9918_STAT_5THSPR);
  VDPCtrlLatch = 0;
    
  if (myConfig.clearInt == CPU_CLEAR_INT_ON_VDP_READ)
  {
    if ((CPU.IRequest == vdp_int_source)) CPU.IRequest=INT_NONE;
  }

  return(data);
}


/** Loop9918() ***********************************************/
/** Call this routine on every scanline to update the       **/
/** screen buffer. Loop9918() returns 1 if an interrupt is  **/
/** to be generated, 0 otherwise.                           **/
/*************************************************************/
u8 frameSkipIdx __attribute__((section(".dtcm"))) = 0;
u8 frameSkip[3] __attribute__((section(".dtcm"))) = {0xFF, 0x03, 0x01};   // Frameskip OFF, Light, Agressive

u16 tms_num_lines  __attribute__((section(".dtcm"))) = TMS9918_LINES;
u16 tms_start_line __attribute__((section(".dtcm"))) = TMS9918_START_LINE;
u16 tms_end_line   __attribute__((section(".dtcm"))) = TMS9918_END_LINE;
u16 tms_cpu_line   __attribute__((section(".dtcm"))) = TMS9918_LINE;

byte Loop9918(void) 
{
  extern void colecoUpdateScreen(void);
  register byte bIRQ;

  /* No IRQ yet */
  bIRQ=0;

  /* Increment scanline */
  if (++CurLine >= tms_num_lines) CurLine=0;
  else
  /* If refreshing display area, call scanline handler */
  if ((CurLine >= tms_start_line) && (CurLine < tms_end_line))
  {
      if ((frameSkipIdx & frameSkip[myConfig.frameSkip]) == 0)
          ScanSprites(CurLine - tms_start_line, 0);    // Skip rendering - but still scan sprites for collisions
      else
          RefreshLine(CurLine - tms_start_line);
  }
  /* If time for emulated VBlank... */
  else if (CurLine == tms_end_line)
  {
      // --------------------------------------------------------------------
      // !!!Into the Vertical Blank!!!
      // If we are not trying to run full-speed, wait for vBlank to ensure
      // we get minimal tearing... This is also how we throttle to 60 FPS
      // by using the DS vertical blank as our way of frame-to-frame timing.
      // --------------------------------------------------------------------
      if ((myConfig.showFPS != 2) && (myConfig.vertSync))   // If not full speed we can try vertical sync if enabled
      {
          swiWaitForVBlank();
      }
      
      /* Refresh screen */
      if ((frameSkipIdx & frameSkip[myConfig.frameSkip]) != 0)
      {
          colecoUpdateScreen();
      }

      frameSkipIdx++;
      
      /* Generate IRQ when enabled and when VBlank flag goes up */
      bIRQ=TMS9918_VBlankON && !(VDPStatus&TMS9918_STAT_VBLANK);

      /* Set VBlank status flag */
      VDPStatus|=TMS9918_STAT_VBLANK;

      /* Set Sprite Collision status flag */
      if(!(VDPStatus&TMS9918_STAT_OVRLAP))
        if(CheckSprites()) VDPStatus|=TMS9918_STAT_OVRLAP;
  }
    
  if (einstein_mode) bIRQ = 0;  // The Tatung Einstein does not generate interrupts on VSYNC

  /* Done */
  return(bIRQ);
}

/** Loop6502() **********************************************/
/** The 6502 version of the above... slimmer and trimmer.  **/
/************************************************************/
byte Loop6502(void)
{
  extern void creativision_input(void);
  extern void colecoUpdateScreen(void);
  register byte bIRQ;

  /* No IRQ yet */
  bIRQ=0;

  /* Increment scanline */
  if (++CurLine >= tms_num_lines) CurLine=0;

  /* If refreshing display area, call scanline handler */
  if ((CurLine >= tms_start_line) && (CurLine < tms_end_line))
  {
      if ((frameSkipIdx & frameSkip[myConfig.frameSkip]) == 0)
          ScanSprites(CurLine - tms_start_line, 0);    // Skip rendering - but still scan sprites for collisions
      else
         (SCR[ScrMode].Refresh)(CurLine - tms_start_line);
  }
  /* If time for emulated VBlank... */
  else if (CurLine == tms_end_line)
  {
      /* Refresh screen */
      if ((frameSkipIdx & frameSkip[myConfig.frameSkip]) != 0)
      {
          colecoUpdateScreen();
      }

      frameSkipIdx++;
      
      /* Generate IRQ when enabled (only) */
      bIRQ = (TMS9918_VBlankON ? 1:0);

      /* Set VBlank status flag */
      VDPStatus|=TMS9918_STAT_VBLANK;

      /* Set Sprite Collision status flag */
      if(!(VDPStatus&TMS9918_STAT_OVRLAP))
        if(CheckSprites()) VDPStatus|=TMS9918_STAT_OVRLAP;
      
      creativision_input();
  }
    
  return bIRQ;
}



/** Reset9918() **********************************************/
/** Reset the VDP. The user can provide a new screen buffer **/
/** by pointing Buffer to it and setting Width and Height.  **/
/** Set Buffer to 0 to use the existing screen buffer.      **/
/*************************************************************/
void Reset9918(void) 
{
    memset(VDP,0x00,sizeof(VDP));       // Initialize VDP registers
    VDP[1] = 0x80;                      // Force 16K Video Memory
    memset(pVDPVidMem, 0x00, 0x4000);   // Reset Video memory 
    VDPCtrlLatch=0;                     // VDP control latch (flip-flop)
    VDPStatus=0x00;                     // VDP status register
    VAddr = 0x0000;                     // VDP address register
    FGColor=BGColor=0;                  // Fore/Background color
    ScrMode=0;                          // Current screenmode
    CurLine=0;                          // Current scanline
    ChrTab=ColTab=ChrGen=pVDPVidMem;    // VDP tables (screen)
    SprTab=SprGen=pVDPVidMem;           // VDP tables (sprites)
    VDPDlatch = 0;                      // VDP Data latch
   
    ChrGenM = 0x3FFF;                   // Full mask
    ColTabM = 0x3FFF;                   // Full mask
    ChrGenM = 0x3FFF;                   // Full mask
    SprTabM = 0x3FFF;                   // Full mask
    
    BG_PALETTE[0] = RGB15(0x00,0x00,0x00);
    
    // ------------------------------------------------------------
    // Determine if we are PAL vs NTSC and adjust line timing...
    // ------------------------------------------------------------
    tms_start_line = (myConfig.isPAL ? TMS9929_START_LINE   :   TMS9918_START_LINE);
    tms_end_line   = (myConfig.isPAL ? TMS9929_END_LINE     :   TMS9918_END_LINE);
    tms_num_lines  = (myConfig.isPAL ? TMS9929_LINES        :   TMS9918_LINES);
    
    if (memotech_mode || einstein_mode) // These machines runs at 4MHz
        tms_cpu_line = (myConfig.isPAL ? TMS9929_LINE_MTX :   TMS9918_LINE_MTX);
    else
        tms_cpu_line = (myConfig.isPAL ? TMS9929_LINE     :   TMS9918_LINE);
    
    // Some machines only support a 16K VDP memory mode...
    if (msx_mode || adam_mode || svi_mode) vdp_16k_mode_only = 1;
    else vdp_16k_mode_only = 0;
    
    if (msx_mode || svi_mode || sordm5_mode || memotech_mode || sg1000_mode)
    {
        vdp_int_source = INT_RST38;
    }
    else if (einstein_mode)
    {
        vdp_int_source = INT_NONE;  // Einstein does NOT interrupt via VDP!
    }
    else    // The colecovision and ADAM plus Pencil II, PV-2000 use NMI 
    {
        vdp_int_source = INT_NMI;
    }
    my_config_clear_int = myConfig.clearInt;
    
    // ---------------------------------------------------------------
    // Our background/foreground color table makes computations FAST!
    // ---------------------------------------------------------------
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

// End of file
