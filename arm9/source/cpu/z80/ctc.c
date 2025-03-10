// =====================================================================================
// Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original chan) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty. Please see readme.md
//
// Much of this code is gleaned from the wonderful MEMU emulator by Andy Key though 
// largely re-writen for ColecoDS specific use as we are only roughly estimating CTC.
// =====================================================================================
#include <nds.h>

#include "ctc.h"
#include "Z80_interface.h"
#include "../../colecoDS.h"
#include "../../colecomngt.h"
#include "../../colecogeneric.h"
#include "../../Adam.h"
#include "../../C24XX.h"
#include "../../printf.h"


// -----------------------------------------------------------
// The Memotech MTX hooks the VDP into CTC channel 0.
// The Sord M5 hooks the VDP into CTC channel 3.
// The Tatung Einstein does not hook the VDP into anything.
// -----------------------------------------------------------
u8 ctc_to_vdp = CTC_CHAN_MAX;   

// ----------------------------------------------------
// This is our master CTC array for all four channels.
// ----------------------------------------------------
CTC_t CTC[CTC_CHAN_MAX] __attribute__((section(".dtcm")));

// ----------------------------------------------------
// Tick down the channel... see if we have timed out.
// ----------------------------------------------------
static u8 CTC_ProcessChannel(u8 chan)
{
    if (CTC[chan].counter > 0)
    {
        if (--CTC[chan].counter == 0) // Has this timer reached 0?
        {
            CTC_ResetCounter(chan);
            if (CTC[chan].control & CTC_INT_ENABLE)  {CPU.IRequest = CTC[chan].vector;}
            return 1;
        }
    } else CTC_ResetCounter(chan);
    return 0;
}

// ----------------------------------------------------------------
// Fires every scanline if we are in CTC mode - this provides
// some rough timing for the Z80-CTC chip. It's not perfectly
// accurate but it's good enough for our purposes.  Many of the
// M5 games use the CTC timers to generate sound/music as do
// a few of the Einstein and Memotech games.
// ----------------------------------------------------------------
ITCM_CODE void CTC_Timer(u32 cpu_cycles)
{    
    if (einstein_mode)
    {
        // Einstein is a bit unique... channel 2 clocks into channel 3 as a cascade so we handle that below.
        for (u8 chan = CTC_CHAN0; chan <= CTC_CHAN2; chan++)
        {
            if (CTC[chan].running)
            {
                if ((CTC[chan].control & CTC_COUNTER_MODE) == 0) // We only process timers here... 
                {
                    u32 cpu_clocks_to_process = cpu_cycles + CTC[chan].cpuClockRemainder;
                    u32 process_count = (cpu_clocks_to_process / CTC[chan].cpuClocksPerCTC);
                    CTC[chan].cpuClockRemainder = (cpu_clocks_to_process % CTC[chan].cpuClocksPerCTC);
                    for (u32 i=0; i < process_count; i++)
                    {
                        if (CTC_ProcessChannel(chan))
                        {
                            if (chan == CTC_CHAN2) // Channel 2 clocks Channel 3 for RTC
                            {
                                CTC_ProcessChannel(CTC_CHAN3);
                            }
                        }
                    }
                }
            }
        }
    }
    else if (memotech_mode)
    {
        // ------------------------------------------------------------------
        // Channel 0 is the VDP interrupt for the Memotech but it can be used
        // as a timer. Channel 3 is for the cassette and is not handled.
        // ------------------------------------------------------------------
        for (u8 chan = CTC_CHAN0; chan <= CTC_CHAN2; chan++)
        {
            if (CTC[chan].running)
            {
                if ((CTC[chan].control & CTC_COUNTER_MODE) == 0) // We only process timers here... 
                {
                    u32 cpu_clocks_to_process = cpu_cycles + CTC[chan].cpuClockRemainder;
                    u32 process_count = (cpu_clocks_to_process / CTC[chan].cpuClocksPerCTC);
                    CTC[chan].cpuClockRemainder = (cpu_clocks_to_process % CTC[chan].cpuClocksPerCTC);
                    for (u32 i=0; i < process_count; i++)
                    {
                        CTC_ProcessChannel(chan);
                    }
                }
            }
        }
    }
    else    // Sord M5 mode
    {
        // ---------------------------------------------------------------------------------------------
        // CTC Channel 1 is always the sound generator - it's the main one we have to contend with.
        // Channel 3 is the VDP interrupt.
        // Channel 0 and 2 are used for Serial IO. It's possible they can be repurposed for general
        // use timers but I've not seen any game that uses them. Although channel 0 and 2 are
        // likely never re-purposed for general use timing, we check them to be safe. 
        // --------------------------------------------------------------------------------------------
        if (CTC[CTC_CHAN1].running)
        {
            if ((CTC[CTC_CHAN1].control & CTC_COUNTER_MODE) == 0) // We only process timers here... 
            {
                u32 cpu_clocks_to_process = cpu_cycles + CTC[CTC_CHAN1].cpuClockRemainder;
                u32 process_count = (cpu_clocks_to_process / CTC[CTC_CHAN1].cpuClocksPerCTC);
                CTC[CTC_CHAN1].cpuClockRemainder = (cpu_clocks_to_process % CTC[CTC_CHAN1].cpuClocksPerCTC);
                for (u32 i=0; i < process_count; i++)
                {
                    CTC_ProcessChannel(CTC_CHAN1);
                }
            }
        }
    }
}


const u16 einstein_ctc_chan3_offset[] = {0, 1, 2, 3, 5, 10, 20, -1, -2, -3, -5, -10, -20};

// --------------------------------------------------------
// Reset the CRC counter for a given channel
// --------------------------------------------------------
void CTC_ResetCounter(u8 chan)
{
    // --------------------------------------------------------------
    // This is where we handle the timing of various machines.
    // From the timing defines in tms9918a.h we compute how many
    // CPU clocks per CTC timer tick and then we can do reasonable
    // estimates when we call CTC_Timer() every scanline.
    // --------------------------------------------------------------
    CTC[chan].counter = CTC[chan].constant;
    if (chan == CTC_CHAN3) CTC[chan].counter+=einstein_ctc_chan3_offset[myConfig.ein_ctc3]; // We allow some small fudge-factor tweaking of CTC3 for timing issues (JSW2 mostly)
    
    if (memotech_mode)
    {
        CTC[chan].cpuClocksPerCTC = (CTC[chan].control & CTC_PRESCALER_256) ? 255:16;    // Prescale of x256 means longer timers.. so it requires more CPU clocks per tick
    }
    else if (einstein_mode)
    {
        // ----------------------------------------------------------------------------------------------
        // I find the Einstein seems to run a little slow with the values that Memotech was using above.
        // In theory, it should be the same - but I've "sped" things up a bit here and it seems to run
        // much closer to more capable emulators. A bit of a fudge-factor but such is life!
        // ----------------------------------------------------------------------------------------------
        CTC[chan].cpuClocksPerCTC = (CTC[chan].control & CTC_PRESCALER_256) ? 225:14;    // Prescale of x256 means longer timers.. so it requires more CPU clocks per tick
    }
    else // Sord M5
    {
        CTC[chan].cpuClocksPerCTC = (CTC[chan].control & CTC_PRESCALER_256) ? 245:15;    // Prescale of x256 means longer timers.. so it requires more CPU clocks per tick
    }
}


// --------------------------------------------------------
// Return the current CTC counter for the given channel...
// --------------------------------------------------------
u8 CTC_Read(u8 chan)
{
    return CTC[chan].counter;
}

// --------------------------------------------------------
// Write CTC channel data
// --------------------------------------------------------
void CTC_Write(u8 chan, u8 data)
{
    if (CTC[chan].control & CTC_LATCH)          // If previously latched, we now have the countdown timer data
    {
        if (data == 0) data = 255;              // Good enough... and it saves us a headache (our timing isn't accurate enough to matter between 255 and 256)
        CTC[chan].counter = data;               // Counter data... 1-256 with 0 representing 256 - this is the live countdown value reported back by CTC_Read()
        CTC[chan].constant = data;              // This is our reload value
        CTC[chan].control &= (~CTC_LATCH);      // Reset the latched bit (back to control word mode)
        CTC_ResetCounter(chan);                 // Reset the counter
        CTC[chan].running = 1;                  // Start the counter running
    }
    else    // This is a non-latched data write.
    {
        if (data & CTC_CONTROL)   // Control Word
        {
            CTC[chan].control = data;       // Keep track of the most recent control word for this channel
            CTC[chan].running = ((data & CTC_RESET) ? 0:1);
            if (data & CTC_RESET) CPU.IRequest = INT_NONE;
        }
        else            // Vector Word
        {
            if (chan == 0x00) // Channel 0, bit0 clear is special - this is where the 4 CTC vector addresses are setup
            {
                CTC[CTC_CHAN0].vector = (data & 0xf8) | 0;                                  // VDP Interrupt for Memotech. General timer use on Einstein.
                CTC[CTC_CHAN1].vector = (data & 0xf8) | 2;                                  // General use timer... Sord M5 makes use of this for sound.
                CTC[CTC_CHAN2].vector = (data & 0xf8) | 4;                                  // General use timer... Memotech and Einstein use this sometimes.
                CTC[CTC_CHAN3].vector = (data & 0xf8) | 6;                                  // VDP Interrupt for Sord M5. Einstein can use as general purpose.
                if (ctc_to_vdp != CTC_CHAN_MAX) vdp_int_source = CTC[ctc_to_vdp].vector;    // When the VDP interrupts the CPU, it's this channel on the CTC
                CPU.IRequest = INT_NONE;                                                    // Games like Formula F1 rely on the interrupt being clear on vector set
            }
        }
    }
}

// --------------------------------------------------------
// Initialize the CTC and reset all channels
// --------------------------------------------------------
void CTC_Init(u8 vdp_chan)
{
    ctc_to_vdp = vdp_chan;
    for (u8 chan = CTC_CHAN0; chan < CTC_CHAN_MAX; chan++)
    {
        CTC[chan].control           = (CTC_CONTROL | CTC_RESET);
        CTC[chan].constant          = 0;
        CTC[chan].prescale          = 0;
        CTC[chan].counter           = 0;
        CTC[chan].running           = 0;
        CTC[chan].intStatus         = 0;
        CTC[chan].intPending        = 0;
        CTC[chan].vector            = 0x00;
        CTC[chan].cpuClocksPerCTC   = 227;
        CTC[chan].cpuClockRemainder = 0;
    }
}

// End of file
