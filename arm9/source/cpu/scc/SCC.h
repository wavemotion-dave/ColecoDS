//
//  SCC.h
//  Konami SCC/K051649 sound chip emulator for arm32.
//
//  Created by Fredrik Ahlström on 2006-04-01.
//  Copyright © 2006-2024 Fredrik Ahlström. All rights reserved.
//

#ifndef SCC_HEADER
#define SCC_HEADER

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	s8 ch0Wave[32];
	s8 ch1Wave[32];
	s8 ch2Wave[32];
	s8 ch3Wave[32];
	u16 ch0Frq;
	u16 ch1Frq;
	u16 ch2Frq;
	u16 ch3Frq;
	u16 ch4Frq;
	u8 ch0Volume;
	u8 ch1Volume;
	u8 ch2Volume;
	u8 ch3Volume;
	u8 ch4Volume;
	u8 chControl;
	u8 testReg;
	u8 padding[3];

	u16 ch0Freq;
	u16 ch0Addr;
	u16 ch1Freq;
	u16 ch1Addr;
	u16 ch2Freq;
	u16 ch2Addr;
	u16 ch3Freq;
	u16 ch3Addr;
	u16 ch4Freq;
	u16 ch4Addr;
} SCC;

void SCCReset(SCC *chip);
void SCCMixer(int len, void *dest, SCC *chip);
void SCCWrite(u8 value, u16 adress, SCC *chip);
u8 SCCRead(u16 adress, SCC *chip);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SCC_HEADER
