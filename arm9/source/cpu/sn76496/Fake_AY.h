#ifndef _AY38910_H
#define _AY38910_H

extern u8 channel_a_enable;
extern u8 channel_b_enable;
extern u8 channel_c_enable;
extern u8 noise_enable;


extern void FakeAY_Loop(void);
extern void FakeAY_WriteData(u8 Value);
extern u8 ay_reg_idx;
extern u8 ay_reg[256];


// -----------------------------------
// Write the AY register index...
// -----------------------------------
inline void FakeAY_WriteIndex(u8 Value)
{
    ay_reg_idx = Value;
}

// -----------------------------------
// Read an AY data value...
// -----------------------------------
inline u8 FakeAY_ReadData(void)
{
    return ay_reg[ay_reg_idx];
}


#endif
