#ifndef _AY38910_H
#define _AY38910_H

extern void FakeAY_Loop(void);
extern void FakeAY_WriteData(u8 Value);
extern u8 ay_reg_idx;
extern u8 ay_reg[];

extern u16 envelope_period;
extern u16 envelope_counter;
extern u16 noise_period;
extern u8 a_idx;
extern u8 b_idx;
extern u8 c_idx;


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
