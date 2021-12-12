#ifndef _TMS9918A_H_
#define _TMS9918A_H_

#include <nds.h>

#define MAXSCREEN           3   // Highest screen mode supported

#define TMS9918_BASE        10738635

#define TMS9918_FRAMES      60
#define TMS9918_CLOCK       (TMS9918_BASE/3)
#define TMS9918_FRAME       (TMS9918_BASE/(3*60))
#define TMS9918_LINE        ((TMS9918_BASE/(3*60*263)))   // Fathom and Centipede require this slightly shorter line time

#define TMS9918_REG1_RAM16K 0x80 /* 1: 16kB VRAM (0=4kB)     */
#define TMS9918_REG1_SCREEN 0x40 /* 1: Enable display        */
#define TMS9918_REG1_IRQ    0x20 /* 1: IRQs on VBlanks       */
#define TMS9918_REG1_SPR16  0x02 /* 1: 16x16 sprites (0=8x8) */
#define TMS9918_REG1_BIGSPR 0x01 /* 1: Magnify sprites x2    */

#define TMS9918_STAT_VBLANK 0x80 /* 1: VBlank has occured    */
#define TMS9918_STAT_5THSPR 0x40 /* 1: 5th Sprite Detected   */
#define TMS9918_STAT_OVRLAP 0x20 /* 1: Sprites overlap       */
#define TMS9918_STAT_5THNUM 0x1F /* Number of the 5th sprite */

#define TMS9918_LINES       263  // Really 262.5 but timing fares better at 263
#define TMS9918_START_LINE  (3+13+27)
#define TMS9918_END_LINE    (TMS9918_START_LINE+192)

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

extern u8 bResetVLatch;

extern u8 TMS9918A_palette[16*3];
extern tScrMode SCR[MAXSCREEN+1];

extern void ITCM_CODE RefreshLine0(u8 uY);
extern void ITCM_CODE RefreshLine1(u8 uY);
extern void ITCM_CODE RefreshLine2(u8 uY);
extern void ITCM_CODE RefreshLine3(u8 uY);

extern byte WrCtrl9918(byte value);
extern void WrData9918(byte value);
extern byte RdData9918(void);
extern byte RdCtrl9918(void);
extern void Reset9918(void);

extern u16 CurLine;                            // Current Scanline
extern u8 pVDPVidMem[0x10000];                 // VDP video memory
extern u8 VDP[16],VDPStatus,VDPDlatch;         // VDP registers
extern u16 VAddr;                              // Storage for VIDRAM addresses
extern u8 VDPCtrlLatch;                        // VDP control latch
extern u8 *ChrGen,*ChrTab,*ColTab;             // VDP tables (screens)
extern u8 *SprGen,*SprTab;                     // VDP tables (sprites)
extern u8 ScrMode;                             // Current screen mode
extern u8 FGColor,BGColor;                     // Colors
extern u16 ColTabM, ChrGenM;                   // Color and Character Masks

#endif
