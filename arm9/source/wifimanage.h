#ifndef _WIFIMANAGE_H_
#define _WIFIMANAGE_H_

#include <nds.h>

extern void wifiInit();
extern u8 wifiGetGameInfo(u32 uNoGame);
extern u8 wifiPlayGame(u32 uNoGame);
extern void showBitmapPng(u8 *bmpPng);
extern u8 wifiSendReport(u16 uGameAct,u8 gPlay, u8 gSpeed, u8 gGfx, u8 gSnd);

#endif
