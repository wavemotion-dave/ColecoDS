#ifndef _CTC_H_
#define _CTC_H_

#include <nds.h>

#include "./cz80/Z80.h"
#include "./drz80/drz80.h"
#include "./cz80/Z80.h"

#define CTC_INT_ENABLE      0x80    // 1=Interupt Enabled (0=No Interrupt Generated)
#define CTC_COUNTER_MODE    0x40    // 1=Counter Mode (vs Timer mode)
#define CTC_PRESCALER_256   0x20    // 1=Prescale is x256 (vs x16)
#define CTC_EDGE            0x10    // 1=Rising vs Falling (not used - we don't care what edge it clocks on)
#define CTC_TRIGGER         0x08    // 1=Trigger (start) timer (not used - we just start the timer)
#define CTC_LATCH           0x04    // 1=Software wants to latch a timer value on the next write
#define CTC_RESET           0x02    // 1=Timer Reset (not running), 0=Timer Running Normally
#define CTC_CONTROL         0x01    // 1=Control Word, 0=Vector Set

#define CTC_CHAN0           0x00    // CTC Channel 0
#define CTC_CHAN1           0x01    // CTC Channel 1
#define CTC_CHAN2           0x02    // CTC Channel 2
#define CTC_CHAN3           0x03    // CTC Channel 3
#define CTC_CHAN_MAX        (CTC_CHAN3+1)


typedef struct
{
    u8  control;            // The last control word written for this CTC counter
    u8  constant;           // Time constant to reload after each countdown
    u8  prescale;           // 1..256 with 0=256
    u8  counter;            // The actual CTC counter register (can be read by Z80 software)
    u8  running;            // Is the timer running?
    u8  intStatus;          // Some flags to let us know if the interrupt is pending or processed
    u8  vector;             // The Interrupt Vector word for this counter
    u8  intPending;         // In case we need it - can use as spare if needed
    u32 cpuClocksPerCTC;    // This is set on a per machine basis. This represents the number of cpu clocks per CTC timer tick-down.
    u32 cpuClockRemainder;  // And we use this to handle the cpu-to-CTC ticker
} CTC_t;

extern CTC_t CTC[CTC_CHAN_MAX];

extern void CTC_Timer(u32 cpu_cycles);
extern void CTC_ResetCounter(u8 chan);
extern void CTC_Write(u8 chan, u8 data);
extern u8   CTC_Read(u8 chan);
extern void CTC_Init(u8 vdp_chan);

#endif // _CTC_H_
