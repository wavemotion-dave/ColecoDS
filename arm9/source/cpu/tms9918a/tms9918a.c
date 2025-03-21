/******************************************************************************
*  ColecoDS TMS9918A (video) file
*
* File: tms9928a.c -- software implementation of the Texas Instruments TMS9918A.
* 
* Note: Most of this file is from the ColEm emulator core by Marat Fayzullin
*       but heavily modified for specific NDS use. If you want to use this
*       code, you are advised to seek out the much more portable ColEm core
*       and contact Marat.       
* 
*       I've added proper init of the VDP[] registers per findings on real
*       hardware and the VDP Control Latch reset on data read/write and
*       on control reads - this was not quite accurate in the ColEm handlers.
*
*       The Scanning of sprites for the 5th sprite is also overhauled
*       for improved accuracy - the 5S flag should be set and latched
*       and neither the sprite number nor the flag should be cleared
*       until the status register is read (or the VDP is reset).
* 
*       There is one caveat here, however: the 5th sprite number acts
*       like a counter when the 5S flag is not set - it will represent
*       the last sprite scanned on a line (either 31 or the one where
*       the Y coordinate is 208).
* 
*       Some improvements have been made to the undocumented VDP modes
*       which are used by a few homebrews and are now properly rendered.
* 
*       Border refresh has been overhauled - it's only used in TEXT 
*       mode and handles the first and last 8 pixels on a line using
*       a memcpy() which is pretty fast.
* 
*       We've also patched in several color tables for very fast color 
*       handling and check that the FG/BG colors have changed before
*       going back through a semi-CPU-expensive fetch into VDP memory. 
*       Whenever possible, we also write 16-bits or 32-bits at a time
*       since the 16-bit writes are exactly as CPU intensive as the 
*       8-bit variety on the DS hardware (and 32-bit writes are slightly
*       faster than two 16-bit writes).
*
*       I've added the tms9918a.txt file to the github repository that
*       also houses this emulator - it's a wealth of info on the VDP 
*       including many things that the original TI manuals did not tell us.
*
******************************************************************************/
#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "../../colecoDS.h"
#include "../../colecogeneric.h"
#include "../z80/Z80_interface.h"
#include "../z80/ctc.h"

#include "tms9918a.h"

u8 MaxSprites[2] __attribute__((section(".dtcm"))) = {32, 4};     // Normally the CV only shows 4 sprites on a line... for emulation we bump this up if configured

u16 *pVidFlipBuf __attribute__((section(".dtcm"))) = (u16*) (0x06000000);    // Video flipping buffer

u8 XBuf_A[256*192] ALIGN(32) = {0}; // TMS9918 screen is 256x192 - Ping Pong Buffer A
u8 XBuf_B[256*192] ALIGN(32) = {0}; // TMS9918 screen is 256x192 - Ping Pong Buffer B
u8 *XBuf __attribute__((section(".dtcm"))) = XBuf_A;

// Look up table for colors - pre-generated and in VRAM for maximum speed!
u32 (*lutTablehh)[16][16] __attribute__((section(".dtcm"))) = (void*)0x068A0000;    // this is actually 16x16x16x4 = 16K

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

u8 ZX_Spectrum_palette[16*3] __attribute__((section(".dtcm")))  = {
  0x00,0x00,0x00,   // Black
  0x00,0x00,0xD8,   // Blue
  0xD8,0x00,0x00,   // Red
  0xD8,0x00,0xD8,   // Magenta
  0x00,0xD8,0x00,   // Green
  0x00,0xD8,0xD8,   // Cyan
  0xD8,0xD8,0x00,   // Yellow
  0xD8,0xD8,0xD8,   // White
  0x00,0x00,0x00,   // Bright Black
  0x00,0x00,0xFF,   // Bright Blue
  0xFF,0x00,0x00,   // Bright Red
  0xFF,0x00,0xFF,   // Bright Magenta
  0x00,0xFF,0x00,   // Bright Green
  0x00,0xFF,0xFF,   // Bright Cyan
  0xFF,0xFF,0x00,   // Bright Yellow
  0xFF,0xFF,0xFF,   // Bright White
};

u8 pVDPVidMem[0x4000] ALIGN(32) ={0};                   // VDP video memory... TMS9918A has 16K of VRAM

u16 CurLine     __attribute__((section(".dtcm")));      // Current scanline
u8 VDP[16]      __attribute__((section(".dtcm")));      // VDP Registers
u8 VDPStatus    __attribute__((section(".dtcm")));      // VDP Status
u8 VDPDlatch    __attribute__((section(".dtcm")));      // VDP register D Latch
u16 VAddr       __attribute__((section(".dtcm")));      // VDP Video Address
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


/** CheckSprites() ***********************************************/
/** This function is periodically called to check for sprite    **/
/** collisions. The caller of this will set the flag as needed. **/
/** Returning zero (0) means no collision. Otherwise collision. **/
/*****************************************************************/
ITCM_CODE byte CheckSprites(void) 
{
  unsigned int I,J,LS,LD;
  byte *S,*D,*PS,*PD,*T;
  int DH,DV;

  /* Find valid, displayed sprites */
  DV = TMS9918_Sprites16 ? -16:-8;
  for(I=J=0,S=SprTab;(I<32)&&(S[0]!=208);++I,S+=4)
  {
    if(((S[0]<191)||(S[0]>255+DV))&&((int)S[1]-(S[3]&0x80? 32:0)>DV))  J|=1<<I;
  }

  // ------------------------------------------------------------------
  // Run through all displayed sprites and see if there is any overlap. 
  // This is a bit CPU intensive - we check vertical overlap first as
  // it's a bit faster to see if these have any chance of collision.
  // ------------------------------------------------------------------
  if(TMS9918_Sprites16)
  {
    for(S=SprTab;J;J>>=1,S+=4)
      if(J&1)
        for(I=J>>1,D=S+4;I;I>>=1,D+=4)
          if(I&1)
          {
            DV=(int)S[0]-(int)D[0]; // Check if these sprites might coincide vertically
            if((DV<16)&&(DV>-16))
            {
              DH=(int)S[1]-(int)D[1]-(S[3]&0x80? 32:0)+(D[3]&0x80? 32:0);
              if((DH<16)&&(DH>-16)) // Check if these sprites might coincide horizontally
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
/** Returns the last sprite to be scanned or -1 if none.    **/
/** Also updates 5th sprite fields in the status register.  **/
/*************************************************************/
ITCM_CODE int ScanSprites(byte Y, unsigned int *Mask)
{
    byte *AT;
    u8 sprite,MS,S5;
    s16 K;
    
    // Assume no sprites shown - we OR in a '1' for each visible sprite
    *Mask = 0x00000000;
    
    // Must have MODE1+ and screen enabled - otherwise no sprites rendered 
    if(!ScrMode || !TMS9918_ScreenON)
    {
        return(-1);
    }    

    s16 fifth_sprite_num =-1;                   // Used to detect the 5th sprite on a line
    AT = SprTab;                                // Pointer to the sprite table in VDP memory
    MS = MaxSprites[myConfig.maxSprites]+1;     // We either render 4 sprites (normal - this is how an 9918 would work) or 32 sprites (enhanded mode for emulation only)
    S5 = 5;                                     // We always want to trap on the 5th sprite
    u8 last = 31;                               // The last sprite number is 31 but we may break early if Y==208
    
    // ------------------------------------------------------------------
    // Scan through all possible 32 sprites to see what's being shown...
    // ------------------------------------------------------------------
    for(sprite=0;sprite<32;++sprite,AT+=4)
    {
        K=AT[0];                            // K = sprite Y coordinate 
        if(K==208) {last=sprite; break;}    // Iteration terminates if Y=208 and we save the last scanned sprite 
        if(K>256-IH) K-=256;                // Y coordinate may be negative

        // -------------------------------------------------------------------------------------------
        // Mark all valid sprites with 1s, break at MaxSprites. Track last scanned sprite for 5S num
        // At first this looked wrong as if it was off by 1 for comparing the Y (scanline) number
        // with the sprite Y coordinate but the Y position is tricky. A coordinate of 0 means draw
        // at the first pixel line (one below the top-most pixel line of the screen). A 255 means
        // draw at the 0th top-most pixel line of the screen. Y positions below 255 but above 208 are
        // negative indexes which allow for the sprite to be positioned partially cropped at the top.
        // Finally, the reason 208 was chosen by TI as the sentinal value is that it's 16 pixels below
        // the lowest pixel row of 192 and the sprite would be completely off-screen. Tricky...
        // -------------------------------------------------------------------------------------------
        if((Y>K)&&(Y<=K+OH))
        {
            // If we exceed four sprites per line, set 5th sprite number
            if(!--S5) fifth_sprite_num = sprite;

            // If we exceed maximum number of sprites per line, stop here
            if(!--MS) break;

            // Mark sprite as ready to draw
            *Mask |= (1<<sprite);
        }
    }

    // ------------------------------------------------------------------------
    // The if a 5th  sprite was found on this line, we check to see if we've
    // already got a 5th sprite latched and if not, we will set this sprite as
    // the fifth sprite. The 5th sprite flag will be cleared on status read.
    // ------------------------------------------------------------------------
    if ((VDPStatus & TMS9918_STAT_5THSPR) == 0) // If the 5S flag is not already latched
    {
        if (fifth_sprite_num != -1) // If we have a 5th sprite number detected
        {
            VDPStatus &= ~TMS9918_STAT_5THNUM;                      // Clear out any previous sprite number
            VDPStatus |= (TMS9918_STAT_5THSPR | fifth_sprite_num);  // Set the 5th sprite flag and number
        }
        else // This is undocumented behavior but a real VDP will behave like this and Miner 2049er will rely on it
        {
            VDPStatus &= ~TMS9918_STAT_5THNUM;      // Clear out any previous sprite number
            VDPStatus |= last;                      // Set the 5th sprite number to the last scanned sprite on the line (the one with Y==208 or else sprite 31)
        }
    }

  // Return last scanned sprite - the caller's Mask is also filled in with a list of all shown sprites 
  return(sprite-1);
}


/** RefreshSprites() *****************************************/
/** This function is called from RefreshLine#() to refresh  **/
/** and draw sprites to a given pixel line.                 **/
/*************************************************************/
ITCM_CODE void RefreshSprites(register byte Y) 
{
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


/** RefreshLine0() *********************************************/
/** Refresh line Y (0..191) of SCREEN0, including sprites in  **/
/** this line.  This is the only mode that shows fewer than   **/
/** 256 horizontal pixels and so we must deal with the border **/
/** (backdrop) here which is always the background color.     **/
/***************************************************************/
ITCM_CODE void RefreshLine0(u8 Y)
{
  register byte *T,K,Offset;
  register byte *P,FC,BC;
  u16 word1=0, word2=0, word3=0;

  P=XBuf+(Y<<8);
  BC = BGColor;
  FC = FGColor;

  if(!ScreenON)
    memset(P,BGColor,256);
  else
  {
    T=ChrTab+(Y>>3)*40;
    Offset=Y&0x07;

    u8 lastT = ~(*T);

    memset(P, BGColor, 8);  // Fill the first 8 pixels with background color since the screen in TEXT mode is 240 pixels and needs the border filled
    P += 8;                 // For this TEXT mode, we shift in 8 pixels to center the screen. We memset the background color to the first and last 8 pixels of a line to blank them.

    for(int X=0;X<40;X++)
    {
      if (lastT != *T) // Is this set of pixels different than the last one?
      {
          lastT=*T;
          K=ChrGen[((int)*T<<3)+Offset];
          P[0]=K&0x80? FC:BC;
          P[1]=K&0x40? FC:BC;
          P[2]=K&0x20? FC:BC;
          P[3]=K&0x10? FC:BC;
          P[4]=K&0x08? FC:BC;
          P[5]=K&0x04? FC:BC;
          // Save the data as we are likely to just repeat it...
          word1 = *((u16*)(P+0));
          word2 = *((u16*)(P+2));
          word3 = *((u16*)(P+4));
      }
      else // No change so we can just blast repeat the 6 bytes (16-bits at at time). This occurs frequently and saves us time.
      {
          u16 *destPtr = (u16*)P;
          *destPtr++ = word1;
          *destPtr++ = word2;
          *destPtr   = word3;
      }
      P+=6;T++;
    }

    memset(P, BGColor, 8);  // Fill the last 8 pixels with background color since the screen in TEXT mode is 240 pixels and needs the border filled
  }
}

/** RefreshLine1() *******************************************/
/** Refresh line Y (0..191) of SCREEN1, including sprites   **/
/** in this line.                                           **/
/*************************************************************/
ITCM_CODE void RefreshLine1(u8 uY) 
{
  register byte K=0,Offset,FC,BC;
  register u8 *T;
  register u32 *P;
  u8 lastT;

  P=(u32*) (XBuf+(uY<<8));
  u32 ptLow = 0; u32 ptHigh = 0;

  if(!ScreenON) 
    memset(P,BGColor,256);
  else 
  {
    T=ChrTab+((int)(uY&0xF8)<<2);
    Offset=uY&0x07;

    lastT = ~(*T);
      
    for(int X=0;X<32;X++) 
    {
      if (lastT != *T)
      {
          lastT=*T;
          BC=ColTab[lastT>>3];
          K=ChrGen[((int)lastT<<3)+Offset];
          FC=BC>>4;
          BC=BC&0x0F;
          u32* ptLut = (u32*) (lutTablehh[FC][BC]);
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
ITCM_CODE void RefreshLine2(u8 uY) {
  u32 *P;
  register byte FC,BC;
  register byte K,*T;
  u16 J,I;

  P=(u32*)(XBuf+(uY<<8));

  if (!ScreenON) 
    memset(P,BGColor,256);
  else 
  {
    u32 ptLow = 0; u32 ptHigh = 0;
      
    J   = ((u16)((u16)uY&0xC0)<<5)+(uY&0x07);
    T   = ChrTab+((u16)((u16)uY&0xF8)<<2);
    u8 lastT = ~(*T);

    for(int X=0;X<32;X++)
    {
      if (lastT != *T)
      {
          lastT = *T;
          I    = (u16)lastT<<3;
          K    = ColTab[(J+I)&ColTabM];
          FC   = (K>>4);
          BC   = K & 0x0F;
          K    = ChrGen[(J+I)&ChrGenM];
          u32* ptLut = (u32*)(lutTablehh[FC][BC]);
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
ITCM_CODE void RefreshLine3(u8 uY) 
{
  byte X,K,Offset;
  byte *P,*T;
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
    u32 dword1=0, dword2=0;
    for(X=0;X<32;X++) 
    {
      if (lastT != *T)
      {
          lastT = *T;
          K=ChrGen[((int)lastT<<3)+Offset];
          ptLow = K>>4;
          ptHigh = K&0x0F;
          P[0]=P[1]=P[2]=P[3]=ptLow;
          P[4]=P[5]=P[6]=P[7]=ptHigh;
          dword1 = *((u32*)(P+0));
          dword2 = *((u32*)(P+4));
      }
      else
      {
          u32 *destPtr = (u32*)P;
          *destPtr++ = dword1;
          *destPtr   = dword2;          
      }
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

ITCM_CODE byte Write9918(u8 iReg, u8 value) 
{ 
  u16 newMode;
  u16 VRAMMask;
  byte bIRQ;
    
  /* There are 8 VDP registers - map down to these 8 and mask off irrelevant bits */
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
    //      0x00    0       0       0       Mode 0   - MSX SCREEN 1 aka "GRAPHIC 1"
    //      0x01    0       0       1       Mode 3   - MSX SCREEN 2 aka "GRAPHIC 2"
    //      0x02    0       1       0       Mode 2   - MSX SCREEN 3 aka "MULTICOLOR"
    //      0x04    1       0       0       Mode 1   - MSX SCREEN 0 aka "TEXT"
    //      0x06    1       1       0       Mode 1+2 - Undocumented. Like Mode 1 aka "HALF BITMAP"
    //      0x03    0       1       1       Mode 2+3 - Undocumented. Like Mode 3 aka "BITMAP TEXT"
      switch(TMS9918_Mode) 
      {
        case 0x00: newMode=1;break;         /* VDP Mode 0 aka MSX SCREEN 1 aka "GRAPHIC 1"        */
        case 0x01: newMode=2;break;         /* VDP Mode 3 aka MSX SCREEN 2 aka "GRAPHIC 2"        */
        case 0x02: newMode=3;break;         /* VDP Mode 2 aka MSX SCREEN 3 aka "MULTICOLOR"       */
        case 0x04: newMode=0;break;         /* VDP Mode 1 aka MSX SCREEN 0 aka "TEXT"             */
        case 0x06: newMode=0;break;         /* Undocumented Mode 1+2 is like Mode 1 (HALF BITMAP) */
        case 0x03: newMode=2;break;         /* Undocumented Mode 2+3 is like Mode 3 (BITMAP TEXT) */
        default:   newMode=ScrMode;break;   /* Illegal mode. Just keep screen mode as-is.         */
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
  byte data = VDPDlatch;
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
  byte data = VDPStatus;
  VDPStatus &= 0x1F; // Top bits are cleared on a read... 
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

ITCM_CODE byte Loop9918(void) 
{
  extern void colecoUpdateScreen(void);
  register byte bIRQ = 0;  // No IRQ yet
  
  if (myConfig.soundDriver)
  {
      processDirectAudio();
  }

  /* Increment scanline */
  if (++CurLine >= tms_num_lines) CurLine=0;
  else
  /* If refreshing display area, call scanline handler */
  if ((CurLine >= tms_start_line) && (CurLine < tms_end_line))
  {
#ifndef ZEXALL_TEST      
      unsigned int tmp;
      if ((frameSkipIdx & frameSkip[myConfig.frameSkip]) == 0)
          ScanSprites(CurLine - tms_start_line, &tmp);    // Skip rendering - but still scan sprites for the 5th sprite flag
      else
          RefreshLine(CurLine - tms_start_line);
          
      // ---------------------------------------------------------------------
      // Some programs require that we handle collisions more frequently
      // than just end of frame. So we check every 64 scanlines (or 255 if 
      // we are the older DS-Lite/Phat). This is somewhat CPU intensive so
      // we are careful how often we run it - especially on older hardware.
      // ---------------------------------------------------------------------
      if ((CurLine % (isDSiMode() ? 64:255)) == 0)
      {
          if(!(VDPStatus&TMS9918_STAT_OVRLAP)) // If not already in collision...
          {
            if(CheckSprites()) VDPStatus|=TMS9918_STAT_OVRLAP; // Set the collision bit
          }
      }
#endif      
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
      if ((myGlobalConfig.showFPS != 2) && (myConfig.vertSync))   // If not full speed we can try vertical sync if enabled
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
      bIRQ = TMS9918_VBlankON && !(VDPStatus&TMS9918_STAT_VBLANK);
      if (einstein_mode) bIRQ = 0;  // The Tatung Einstein does not generate interrupts on VSYNC
      else if (memotech_mode)
      {
          if ((CTC[CTC_CHAN0].control & CTC_INT_ENABLE) == 0x00) bIRQ = 0;  // For Memotech MTX: if IE is disabled for CTC channel 0, do not generate interrupt.
      }
      else if (sordm5_mode)
      {
          if ((CTC[CTC_CHAN3].control & CTC_INT_ENABLE) == 0x00) bIRQ = 0;  // For Sord M5: if IE is disabled for CTC channel 3, do not generate interrupt.
      }

      /* Set VBlank status flag */
      VDPStatus|=TMS9918_STAT_VBLANK;

      /* Set Sprite Collision status flag */
      if(!(VDPStatus&TMS9918_STAT_OVRLAP))
      {
          if(CheckSprites()) VDPStatus|=TMS9918_STAT_OVRLAP;
      }
  }

  /* Done */
  return(bIRQ);
}

/** Loop6502() **********************************************/
/** The 6502 version of the above... slimmer and trimmer.  **/
/************************************************************/
ITCM_CODE byte Loop6502(void)
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
      unsigned int tmp;
      if ((frameSkipIdx & frameSkip[myConfig.frameSkip]) == 0)
          ScanSprites(CurLine - tms_start_line, &tmp);    // Skip rendering - but still scan sprites for the 5th sprite flag
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
      {
        if(CheckSprites()) VDPStatus|=TMS9918_STAT_OVRLAP;
      }
      
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
    
    // --------------------------------------------------------------------------
    // These default VDP registers are courtesy of ADAMem which seems to be one
    // of the few ADAM emulators to not have glitches on title screens such
    // as ADAM BOMB 2 and other third party loaders... After reading the
    // TMS9918 data sheet, I can't see where these values are garunteed at
    // power-up, but they work and I'll assume someone much smarter than me
    // has figured it out on real hardare...
    // --------------------------------------------------------------------------
    VDP[0] = 0x00;                      // Control Bits I:  Graphics Mode 1 (M3... M1,M2 in VDP[1])
    VDP[1] = 0x80;                      // Control Bits II: Force 16K Video Memory (M2,M3 zero)
    VDP[2] = 0x06;                      // Default for pattern table base address
    VDP[3] = 0x80;                      // Default for color table base address
    VDP[4] = 0x00;                      // Default for pattern generator base address
    VDP[5] = 0x36;                      // Default for sprite attribute table base address
    VDP[6] = 0x07;                      // Default for sprite generator table base address
    VDP[7] = 0x00;                      // FG color and BG color both 0x00 to start
    
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
  
    ChrTabM = 0x3FFF;                   // Full mask
    ColTabM = 0x3FFF;                   // Full mask
    ChrGenM = 0x3FFF;                   // Full mask
    SprTabM = 0x3FFF;                   // Full mask
    
    BG_PALETTE[0] = RGB15(0x00,0x00,0x00);
    
    pVidFlipBuf = (u16*) (0x06000000);    // Video flipping buffer
    
    RefreshLine = RefreshLine0;

    OH = IH = 0;
    
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
    if (msx_mode || adam_mode || svi_mode || einstein_mode) vdp_16k_mode_only = 1;
    else vdp_16k_mode_only = 0;
    
    if (msx_mode || svi_mode || sg1000_mode)
    {
        vdp_int_source = INT_RST38; 
    }
    else if (sordm5_mode || memotech_mode)
    {
        vdp_int_source = INT_NONE;  // Both of these machines use the CTC for VDP interrupts and those will happen with CTC writes (ctc.c)
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
