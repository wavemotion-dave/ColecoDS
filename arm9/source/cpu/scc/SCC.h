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

/**
 * Reset/initialize SCC chip.
 * @param  *chip: The SCC chip.
 */
void SCCReset(SCC *chip);

/**
 * Saves the state of the SN76496 chip to the destination.
 * @param  *destination: Where to save the state.
 * @param  *chip: The SN76496 chip to save.
 * @return The size of the state.
 */
int SCCSaveState(void *destination, const SCC *chip);

/**
 * Loads the state of the SN76496 chip from the source.
 * @param  *chip: The SN76496 chip to load a state into.
 * @param  *source: Where to load the state from.
 * @return The size of the state.
 */
int SCCLoadState(SCC *chip, const void *source);

/**
 * Gets the state size of a SN76496.
 * @return The size of the state.
 */
int SCCGetStateSize(void);

/**
 * Runs the sound chip for count number of cycles shifted by "SCC_UPSHIFT",
 * @param  count: Number of samples to generate.
 * @param  *dest: Pointer to buffer where sound is rendered.
 * @param  *chip: The SCC chip.
 */
void SCCMixer(int count, s16 *dest, SCC *chip);

/**
 * Write value to SCC chip.
 * @param  value: value to write.
 * @param  address: The address to write to.
 * @param  *chip: The SCC chip.
 */
void SCCWrite(u8 value, u16 adress, SCC *chip);

/**
 * Write value to SCC chip.
 * @param  address: The address to read from.
 * @param  *chip: The SCC chip.
 * @return The value of the address.
 */
u8 SCCRead(u16 address, SCC *chip);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SCC_HEADER
