/*
*/

#ifndef SN76496_HEADER
#define SN76496_HEADER

typedef struct {
	u8 lastreg;
	u8 ggstereo;
	u8 dummy2;
	u8 dummy3;

	u32 ch0reg;
	u32 ch1reg;
	u32 ch2reg;
	u32 ch3reg;

	u16 ch0freq;
	u16 ch0addr;
	u16 ch1freq;
	u16 ch1addr;
	u16 ch2freq;
	u16 ch2addr;
	u16 ch3freq;
	u16 ch3addr;

	u32 rng;
	u32 noisefb;

	u32 ch0volume;
	u32 ch1volume;
	u32 ch2volume;
	u32 ch3volume;

	u8* pcmptr;
	u32 mixlength;
	u32 mixrate;
	u32 freqconv;
	u16* freqtableptr;
} sn76496;


void SN76496_init(sn76496*, u16* freqtableptr);
void SN76496_reset(sn76496*, int chiptype);
void SN76496_set_mixrate(sn76496*, int);
void SN76496_set_frequency(sn76496*, int);
void SN76496_mixer(sn76496*);
void SN76496_w(sn76496*, u8);


#endif
