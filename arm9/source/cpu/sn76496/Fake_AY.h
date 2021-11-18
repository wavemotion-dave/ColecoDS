#ifndef _AY38910_H
#define _AY38910_H

extern u8 channel_a_enable;
extern u8 channel_b_enable;
extern u8 channel_c_enable;
extern u8 noise_enable;


extern void FakeAY_Loop(void);
extern void FakeAY_WriteIndex(u8 Value);
extern void FakeAY_WriteData(u8 Value);
extern u8   FakeAY_ReadData(void);

#endif
