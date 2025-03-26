#ifndef _TMS9918A_H_
#define _TMS9918A_H_

#include <nds.h>

#define MAXSCREEN           3   // Highest screen mode supported

#define TMS9918_BASE        10738635    // Standard 3.58 Mhz
#define TMS9918_BASE_MTX    11998475    // Not really but this achieves the "faster" 4MHz clock rate.


// ---------------------------------------------------
// The default NTSC machine time bases 
// ---------------------------------------------------
#define TMS9918_FRAMES      60
#define TMS9918_LINE        ((TMS9918_BASE/(3*60*262)))
#define TMS9918_LINE_MTX    ((TMS9918_BASE_MTX/(3*60*262)))

#define TMS9918_LINES       262
#define TMS9918_START_LINE  (3+13+27)
#define TMS9918_END_LINE    (TMS9918_START_LINE+192)

// ---------------------------------------------------
// For the PAL machine, the frames, number of lines 
// and start/end lines are different.
// ---------------------------------------------------
#define TMS9929_FRAMES      50
#define TMS9929_LINE        ((TMS9918_BASE/(3*50*313)))
#define TMS9929_LINE_MTX    ((TMS9918_BASE_MTX/(3*50*313)))

#define TMS9929_LINES       312
#define TMS9929_START_LINE  (3+13+51)
#define TMS9929_END_LINE    (TMS9929_START_LINE+192)

#define TMS9918_REG1_RAM16K 0x80 /* 1: 16kB VRAM (0=4kB)     */
#define TMS9918_REG1_SCREEN 0x40 /* 1: Enable display        */
#define TMS9918_REG1_IRQ    0x20 /* 1: IRQs on VBlanks       */
#define TMS9918_REG1_SPR16  0x02 /* 1: 16x16 sprites (0=8x8) */
#define TMS9918_REG1_BIGSPR 0x01 /* 1: Magnify sprites x2    */

#define TMS9918_STAT_VBLANK 0x80 /* 1: VBlank has occured    */
#define TMS9918_STAT_5THSPR 0x40 /* 1: 5th Sprite Detected   */
#define TMS9918_STAT_OVRLAP 0x20 /* 1: Sprites overlap       */
#define TMS9918_STAT_5THNUM 0x1F /* Number of the 5th sprite */

#define TMS9918_Mode      (((VDP[0]&0x02)>>1)|(((VDP[1]&0x18)>>2)))
#define TMS9918_VRAMMask  (VDP[1]&TMS9918_REG1_RAM16K ? 0x3FFF:0x0FFF)
#define TMS9918_VBlankON  (VDP[1]&TMS9918_REG1_IRQ)
#define TMS9918_Sprites16 (VDP[1]&TMS9918_REG1_SPR16)
#define TMS9918_ScreenON  (VDP[1]&TMS9918_REG1_SCREEN)


#define ScreenON      (VDP[1]&0x40)   // Show screen         
#define BigSprites    (VDP[1]&0x01)   // Zoomed sprites      
#define Sprites16x16  (VDP[1]&0x02)   // 16x16/8x8 sprites   

typedef struct {
  void (*Refresh)(u8 uY);
  byte R2,R3,R4,R5,R6,M2,M3,M4,M5;
} tScrMode;

extern u8 *XBuf;
extern u8 XBuf_A[];
extern u8 XBuf_B[];
extern u8 OH;
extern u8 IH;

extern u8 bResetVLatch;

extern u8 TMS9918A_palette[16*3];
extern tScrMode SCR[MAXSCREEN+1];

extern void RefreshLine0(u8 uY);
extern void RefreshLine1(u8 uY);
extern void RefreshLine2(u8 uY);
extern void RefreshLine3(u8 uY);

extern byte WrCtrl9918(byte value);
extern u8 pVDPVidMem[];

extern byte RdData9918(void);
extern byte RdCtrl9918(void);
extern void Reset9918(void);

extern u16 CurLine;                            // Current Scanline
extern u8 VDP[16],VDPStatus,VDPDlatch;         // VDP registers
extern u16 VAddr;                              // Storage for VIDRAM addresses
extern u8 VDPCtrlLatch;                        // VDP control latch
extern u8 *ChrGen,*ChrTab,*ColTab;             // VDP tables (screens)
extern u8 *SprGen,*SprTab;                     // VDP tables (sprites)
extern u8 ScrMode;                             // Current screen mode
extern u8 FGColor,BGColor;                     // Colors
extern u16 ColTabM, ChrGenM;                   // Color and Character Masks

/** WrData9918() *********************************************/
/** Write a value V to the VDP Data Port.                   **/
/*************************************************************/
inline __attribute__((always_inline)) void WrData9918(byte V)  // This one is used frequently so we always inline it
{
    VDPDlatch = pVDPVidMem[VAddr] = V;
    VAddr     = (VAddr+1)&0x3FFF;
    VDPCtrlLatch = 0;
}

extern u16 tms_num_lines;
extern u16 tms_start_line;
extern u16 tms_end_line;
extern u16 tms_cpu_line;

#endif
