/* AY38910 sound chip emulator
*/

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
	s16 *ayEnvVolumePtr;

	u8 ayAttChg;
	u8 ayRegIndex;
	u8 ayPadding1[2];
	u8 ayPortAOut;
	u8 ayPortBOut;
	u8 ayPortAIn;
	u8 ayPortBIn;
	u8 ayRegs[0x10];
	u32 ayPortAInFptr;
	u32 ayPortBInFptr;
	u32 ayPortAOutFptr;
	u32 ayPortBOutFptr;
	s16 ayCalculatedVolumes[8];

} AY38910;

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

void ay38910Mixer(int len, void *dest, AY38910 *chip);
void ay38910DataW(u8 value, AY38910 *chip);
void ay38910IndexW(u8 value, AY38910 *chip);
void ay38910DataR(AY38910 *chip);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // AY38910_HEADER
