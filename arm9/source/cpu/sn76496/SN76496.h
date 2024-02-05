//
//  SN76496.h
//  SN76496/SN76489 sound chip emulator for arm32.
//
//  Created by Fredrik Ahlström on 2009-08-25.
//  Copyright © 2009-2024 Fredrik Ahlström. All rights reserved.
//

#ifndef SN76496_HEADER
#define SN76496_HEADER

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	u16 ch0Frq;
	u16 ch0Cnt;
	u16 ch1Frq;
	u16 ch1Cnt;
	u16 ch2Frq;
	u16 ch2Cnt;
	u16 ch3Frq;
	u16 ch3Cnt;

	u32 currentBits;

	u32 rng;
	u32 noiseFB;

	u8 snAttChg;
	u8 snLastReg;
	u8 snPadding[2];

	s16 calculatedVolumes[16];

	u16 ch0Reg;
	u16 ch0Att;
	u16 ch1Reg;
	u16 ch1Att;
	u16 ch2Reg;
	u16 ch2Att;
	u16 ch3Reg;
	u16 ch3Att;
	u32 noiseType;
} SN76496;


/**
 * Reset/initialize SN76496 chip.
 * @param  chipType: selects version of chip, 0=SMS/GG VDP version, 1=SN76496, 2=NCR 8496.
 * @param  *chip: The SN76496 chip.
 */
void sn76496Reset(int chiptype, SN76496 *chip);

/**
 * Saves the state of the SN76496 chip to the destination.
 * @param  *destination: Where to save the state.
 * @param  *chip: The SN76496 chip to save.
 * @return The size of the state.
 */
int sn76496SaveState(void *destination, const SN76496 *chip);

/**
 * Loads the state of the SN76496 chip from the source.
 * @param  *chip: The SN76496 chip to load a state into.
 * @param  *source: Where to load the state from.
 * @return The size of the state.
 */
int sn76496LoadState(SN76496 *chip, const void *source);

/**
 * Gets the state size of a SN76496.
 * @return The size of the state.
 */
int sn76496GetStateSize(void);

/**
 * Renders count amount of samples, clocks the chip the same amount.
 * Internal oversampling can be set by defining SN_UPSHIFT to a number.
 * @param  count: Number of samples to render.
 * @param  *dest: Pointer to buffer where sound is rendered.
 * @param  *chip: The SN76496 chip.
 */
void sn76496Mixer(int count, s16 *dest, SN76496 *chip);

/**
 * Write value to SN76496 chip
 * @param  value: value to write.
 * @param  *chip: The SN76496 chip.
 */
void sn76496W(u8 val, SN76496 *chip);


#ifdef __cplusplus
}
#endif

#endif // SN76496_HEADER
