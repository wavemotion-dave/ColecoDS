//
//  AY38910.h
//  AY-3-8910 / YM2149 sound chip emulator for arm32.
//
//  Created by Fredrik Ahlström on 2006-03-07.
//  Copyright © 2006-2024 Fredrik Ahlström. All rights reserved.
//

#ifndef AY38910_HEADER
#define AY38910_HEADER

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	u16 ch0Freq;
	u16 ch0Addr;
	u16 ch1Freq;
	u16 ch1Addr;
	u16 ch2Freq;
	u16 ch2Addr;
	u16 ch3Freq;
	u16 ch3Addr;

	u32 ayRng;
	u32 ayEnvFreq;
	u8 ayChState;
	u8 ayChDisable;
	u8 ayEnvType;
	u8 ayEnvAddr;
	u16 *ayEnvVolumePtr;

	u8 ayAttChg;
	u8 ayRegIndex;
	u8 ayPadding1[2];
	s16 ayCalculatedVolumes[8];
	u8 ayPortAOut;
	u8 ayPortBOut;
	u8 ayPortAIn;
	u8 ayPortBIn;
	u8 ayRegs[16];
	void *ayPortAInFptr;
	void *ayPortBInFptr;
	void *ayPortAOutFptr;
	void *ayPortBOutFptr;

} AY38910;

/**
 * Reset/initialize AY38910 chip.
 * @param  *chip: The AY38910 chip.
 */
void ay38910Reset(AY38910 *chip);

/**
 * Saves the state of the AY38910 chip to the destination.
 * @param  *destination: Where to save the state.
 * @param  *chip: The AY38910 chip to save.
 * @return The size of the state.
 */
int ay38910SaveState(void *dest, const AY38910 *chip);

/**
 * Loads the state of the AY38910 chip from the source.
 * @param  *chip: The AY38910 chip to load a state into.
 * @param  *source: Where to load the state from.
 * @return The size of the state.
 */
int ay38910LoadState(AY38910 *chip, const void *source);

/**
 * Gets the state size of a AY38910.
 * @return The size of the state.
 */
int ay38910GetStateSize(void);

/**
 * Renders count amount of samples, clocks the chip the same amount.
 * Internal oversampling can be set by defining AY_UPSHIFT to a number.
 * @param  count: Number of samples to render.
 * @param  *dest: Pointer to buffer where sound is rendered.
 * @param  *chip: The AY38910 chip.
 */
void ay38910Mixer(int count, s16 *dest, AY38910 *chip);

/**
 * Write index/register value to the AY38910 chip
 * @param  index: index to write.
 * @param  *chip: The AY38910 chip.
 */
void ay38910IndexW(u8 index, AY38910 *chip);

/**
 * Write data value to the selected index/register or IO-port in the AY38910 chip
 * @param  value: value to write.
 * @param  *chip: The AY38910 chip.
 */
void ay38910DataW(u8 value, AY38910 *chip);

/**
 * Read data from the selected index/register in the AY38910 chip
 * @param  *chip: The AY38910 chip.
 * @return The value in the selected index/register or IO-port.
 */
u8 ay38910DataR(AY38910 *chip);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AY38910_HEADER
