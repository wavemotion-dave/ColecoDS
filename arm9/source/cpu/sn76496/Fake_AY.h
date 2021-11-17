#ifndef _AY38910_H
#define _AY38910_H

extern u8 channel_a_enable;
extern u8 channel_b_enable;
extern u8 channel_c_enable;
extern u8 noise_enable;


extern void LoopAY(void);
extern void HandleAYsound(u8 Value);


#endif
