// =====================================================================================
// Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, it's source code and associated 
// readme files, with or without modification, are permitted in any medium without 
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty.
// =====================================================================================

// ------------------------------------------------------------------------------------
// The 'Fake' AY handler simply turns AY sound register access into corresponding
// SN sound chip calls. There is some loss of fidelity and we have to handle the
// volume envelopes in a very crude way... but it works and is good enough for now.
//
// The AY register map is as follows:
// Reg      Description
// 0-5      Tone generator control for channels A,B,C
// 6        Noise generator control
// 7        Mixer control-I/O enable
// 8-10     Amplitude control for channels A,B,C
// 11-12    Envelope generator period
// 13       Envelope generator shape
// 14-15    I/O ports A & B (MSX Joystick mapped in here - otherwise unused)
//
//
// We make calls to ay76496W() to do the register writes... there is no such chip
// as an ay76496 - but this alternate function to sn76496W allows us to extend 1 extra
// bit for noise (to go from 3 frequency levels of noise to 7) and 1 extra bit of
// tone frequency (higher periods so we can produce 1 octave lower tones). This gets
// us part-way to what a real AY sound chip can produce... it's good enough.
// ------------------------------------------------------------------------------------

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../colecoDS.h"
#include "../../colecomngt.h"
#include "../../colecogeneric.h"

#include "SN76496.h"
#include "Fake_AY.h"

extern u8 sgm_enable;
extern u8 ay_reg_idx;
extern u16 sgm_low_addr;
extern SN76496 aycol;
extern SN76496 sccABC;
extern SN76496 sccDE;

u8 Volumes[16]       __attribute__((section(".dtcm"))) = { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 };
u16 envelope_period  __attribute__((section(".dtcm"))) = 0;
u16 envelope_counter __attribute__((section(".dtcm"))) = 0;
u16 scc_counter      __attribute__((section(".dtcm"))) = 0;

u8 scc_registers[256] = {0};

unsigned char Envelopes[16][32] __attribute__((section(".dtcm"))) =
{
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};

u8 AY_RegisterMasks[] __attribute__((section(".dtcm"))) = {0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF, 
                                                           0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

u16 noise_period __attribute__((section(".dtcm"))) = 0;

u8 a_idx __attribute__((section(".dtcm"))) = 0;
u8 b_idx __attribute__((section(".dtcm"))) = 0;
u8 c_idx __attribute__((section(".dtcm"))) = 0;


void UpdateNoiseAY(void);
void UpdateTonesAY(void);

// ---------------------------------------------------------------------------------------------
// We handle envelopes here... the timing is nowhere near exact but so few games utilize this 
// and so accuracy isn't all that critical. The sound will be a little off - but it will be ok.
// ---------------------------------------------------------------------------------------------
void FakeAY_Loop(void)
{
    //if (++envelope_counter > envelope_period)  - for speed, the counter is handled by the caller
    {
        u8 bUpdateVols = false;
        envelope_counter = 0;
        u8 shape = ay_reg[0x0D] & 0x0F;
        
        // ---------------------------------------
        // If Envelope is enabled for Channel A 
        // ---------------------------------------
        if ((ay_reg[0x08] & 0x10))
        {
            u8 vol = Envelopes[shape][a_idx]; 
            if (vol != (ay_reg[0x08] & 0x0F))
            {
                ay_reg[0x08] = (ay_reg[0x08] & 0xF0) | vol;
                bUpdateVols = true;
            }
            if (++a_idx > 31)
            {
                if ((shape & 0x09) == 0x08) a_idx = 0; else a_idx=31;   // Decide if we continue the shape or hold 
            }
        }
        
        // ---------------------------------------
        // If Envelope is enabled for Channel B
        // ---------------------------------------
        if ((ay_reg[0x09] & 0x10))
        {
            u8 vol = Envelopes[shape][b_idx]; 
            if (vol != (ay_reg[0x09] & 0x0F))
            {
                ay_reg[0x09] = (ay_reg[0x09] & 0xF0) | vol;
                bUpdateVols = true;
            }
            if (++b_idx > 31)
            {
                if ((shape & 0x09) == 0x08) b_idx = 0; else b_idx=31;   // Decide if we continue the shape or hold 
            }
        }

        // ---------------------------------------
        // If Envelope is enabled for Channel C
        // ---------------------------------------
        if ((ay_reg[0x0A] & 0x10))
        {
            u8 vol = Envelopes[shape][c_idx]; 
            if (vol != (ay_reg[0x0A] & 0x0F))
            {
                ay_reg[0x0A] = (ay_reg[0x0A] & 0xF0) | vol;
                bUpdateVols = true;
            }
            if (++c_idx > 31)
            {
                if ((shape & 0x09) == 0x08) c_idx = 0; else c_idx=31;   // Decide if we continue the shape or hold 
            }
        }
        
        if (bUpdateVols)
        {
            UpdateTonesAY();
            UpdateNoiseAY();
        }
    }
}

// ------------------------------------------------------------------------------------
// If the MSX Beeper is being used (rare but a few of the ZX Spectrum ports use it), 
// then we need to service it here. We basically track the frequency at which the
// game has hit the beeper and approximate that by using AY Channel A to produce the 
// tone.  This is crude and doesn't sound quite right... but good enough.
// ------------------------------------------------------------------------------------
void BeeperON(u16 beeper_freq)
{
    if (beeper_freq > 0)
    {
        if (beeper_freq > 0x3FF) beeper_freq = 0x3FF;
        beeper_freq = 0x3FF - beeper_freq;
        ay76496W(0x80 | (beeper_freq & 0xF), &aycol);
        ay76496W((beeper_freq >> 4) & 0x3F, &aycol);
        ay76496W(0x97, &aycol); // Turn on sound at fixed volume 
    }
    else 
    {
        ay76496W(0x9F, &aycol); // Turn off tone sound on Channel A
    }
}

void BeeperOFF(void)
{
    ay76496W(0x9F, &aycol); // Turn off tone sound on Channel A
}

// ------------------------------------------------------------------
// Noise is a bit more complicated on the AY chip as we have to
// check each A,B,C channel to see if we should be mixing in noise. 
// ------------------------------------------------------------------
void UpdateNoiseAY(void)
{
      // Output the noise for the first channel it's enbled on...
      if      (!(ay_reg[0x07] & 0x08) && ((ay_reg[0x08]&0xF) != 0)) ay76496W(0xF0 | Volumes[ay_reg[0x08]&0xF], &aycol);
      else if (!(ay_reg[0x07] & 0x10) && ((ay_reg[0x09]&0xF) != 0)) ay76496W(0xF0 | Volumes[ay_reg[0x09]&0xF], &aycol);
      else if (!(ay_reg[0x07] & 0x20) && ((ay_reg[0x0A]&0xF) != 0)) ay76496W(0xF0 | Volumes[ay_reg[0x0A]&0xF], &aycol);
      else ay76496W(0xFF, &aycol);  // Otherwise Noise is OFF
}

void UpdateToneA(void)
{
    u16 freq=0;
    
    // ----------------------------------------------------------------------
    // If Channel A tone is enabled - set frequency and update SN sound core
    // ----------------------------------------------------------------------
    if (!(ay_reg[0x07] & 0x01))
    {
        freq = (ay_reg[0x01] << 8) | ay_reg[0x00];
        freq = ((freq & 0x0800) ? 0x7FF : freq&0x7FF);
        ay76496W(0x80 | (freq & 0xF), &aycol);
        ay76496W((freq >> 4) & 0x7F, &aycol);
        if (freq > 0)
            ay76496W(0x90 | Volumes[(ay_reg[0x08] & 0x0F)], &aycol);
        else 
            ay76496W(0x9F, &aycol); // Turn off tone sound on Channel A
    }
    else
    {
        ay76496W(0x9F, &aycol); // Turn off tone sound on Channel A
    }    
}

void UpdateToneB(void)
{
    u16 freq=0;
    
    // ----------------------------------------------------------------------
    // If Channel B tone is enabled - set frequency and update SN sound core
    // ----------------------------------------------------------------------
    if (!(ay_reg[0x07] & 0x02))
    {
        freq = (ay_reg[0x03] << 8) | ay_reg[0x02];
        freq = ((freq & 0x0800) ? 0x7FF : freq&0x7FF);
        ay76496W(0xA0 | (freq & 0xF), &aycol);
        ay76496W((freq >> 4) & 0x7F, &aycol);
        if (freq > 0)
            ay76496W(0xB0 | Volumes[(ay_reg[0x09] & 0x0F)], &aycol);
        else 
            ay76496W(0xBF, &aycol); // Turn off tone sound on Channel B
    }
    else
    {
        ay76496W(0xBF, &aycol); // Turn off tone sound on Channel B
    }    
}


void UpdateToneC(void)
{
    u16 freq=0;
    
    // ----------------------------------------------------------------------
    // If Channel C tone is enabled - set frequency and update SN sound core
    // ----------------------------------------------------------------------
    if (!(ay_reg[0x07] & 0x04))
    {
        freq = (ay_reg[0x05] << 8) | ay_reg[0x04];
        freq = ((freq & 0x0800) ? 0x7FF : freq&0x7FF);
        ay76496W(0xC0 | (freq & 0xF), &aycol);
        ay76496W((freq >> 4) & 0x7F, &aycol);
        if (freq > 0)
            ay76496W(0xD0 | Volumes[(ay_reg[0x0A] & 0x0F)], &aycol);
        else 
            ay76496W(0xDF, &aycol); // Turn off tone sound on Channel C
    }
    else
    {
        ay76496W(0xDF, &aycol); // Turn off tone sound on Channel C
    }    
}

// -----------------------------------------------------------------------
// Check if any of the Tone Channels A, B or C needs to be updated/output
// -----------------------------------------------------------------------
void UpdateTonesAY(void)
{
    UpdateToneA();
    UpdateToneB();
    UpdateToneC();
}

// ------------------------------------------------------------------------------------------------------------------
// Writing AY data is where the magic mapping happens between the AY chip and the standard SN colecovision chip.
// This is a bit of a hack... and it reduces the sound quality a bit on the AY chip but it allows us to use just
// one sound driver for the SN audio chip for everything in the system. On a retro-handheld, this is good enough.
// ------------------------------------------------------------------------------------------------------------------
void FakeAY_WriteData(u8 Value)
{
      // ----------------------------------------------------------------------------------------
      // This is the AY sound chip support... we're cheating here and just mapping those sounds
      // onto the original Colecovision SN sound chip. Not perfect but good enough.
      // ----------------------------------------------------------------------------------------
      //Value &= AY_RegisterMasks[ay_reg_idx & 0x0F];  // Not strictly necessary so we save the CPU time
    
      u8 prevVal = ay_reg[ay_reg_idx];
      ay_reg[ay_reg_idx]=Value;
      
      switch (ay_reg_idx)
      {
          // Channel A tone frequency (period) - low and high
          case 0x00:
          case 0x01:
              UpdateToneA();
              break;
              
          // Channel B tone frequency (period) - low and high
          case 0x02:
          case 0x03:
              UpdateToneB();
              break;

          // Channel C tone frequency (period) - low and high
          case 0x04:
          case 0x05:
              UpdateToneC();
              break;
              
          // Noise Period     
          case 0x06:
              noise_period = Value & 0x1F;
              if      (noise_period > 28) ay76496W(0xE6 , &aycol);   // E6 is the lowest frequency (highest period)
              else if (noise_period > 20) ay76496W(0xE5 , &aycol);   // E5 is the middle frequency (middle period)
              else if (noise_period > 12) ay76496W(0xE4 , &aycol);   // E4 is the middle frequency (middle period)
              else if (noise_period > 8)  ay76496W(0xE3 , &aycol);   // E3 is the middle frequency (middle period)
              else if (noise_period > 4)  ay76496W(0xE2 , &aycol);   // E2 is the middle frequency (middle period)
              else                        ay76496W(0xE1 , &aycol);   // E1 is the highest frequency (lowest period)              
              UpdateNoiseAY();  // Update the Noise output
              break;
              
          // Global Sound Enable/Disable Register
          case 0x07:
              UpdateTonesAY();  // Tones may have turned on/off
              UpdateNoiseAY();  // Update the Noise output
              break;
              
          // -------------------------------------------------------
          // Volume and Envelope Enable Registers are below...
          // -------------------------------------------------------
          case 0x08:
              if (Value & 0x10) // Is Envelope Mode for Channel A active?
              {
                  if (myConfig.ayEnvelope == 0) a_idx = 0;
                  ay_reg[0x08] &= 0xF0 | (prevVal & 0x0F);
                  envelope_counter = 0xF000;    // Force first state change immediately
                  AY_EnvelopeOn = true;
              }
              else
              {
                  AY_EnvelopeOn = (((ay_reg[0x08] & 0x10) || (ay_reg[0x09] & 0x10) || (ay_reg[0x0A] & 0x10))  ? true : false);
                  UpdateToneA();
                  UpdateNoiseAY();
              }
              break;
              
          case 0x09:
              if (Value & 0x10)  // Is Envelope Mode for Channel B active?
              {
                  if (myConfig.ayEnvelope == 0) b_idx = 0;
                  ay_reg[0x09] &= 0xF0 | (prevVal & 0x0F);
                  envelope_counter = 0xF000;    // Force first state change immediately
                  AY_EnvelopeOn = true;
              }
              else
              {
                  AY_EnvelopeOn = (((ay_reg[0x08] & 0x10) || (ay_reg[0x09] & 0x10) || (ay_reg[0x0A] & 0x10))  ? true : false);
                  UpdateToneB();
                  UpdateNoiseAY();
              }
              break;
              
          case 0x0A:
              if (Value & 0x10)   // Is Envelope Mode for Channel C active?
              {
                  if (myConfig.ayEnvelope == 0) c_idx = 0;
                  ay_reg[0x0A] &= 0xF0 | (prevVal & 0x0F);
                  envelope_counter = 0xF000;    // Force first state change immediately
                  AY_EnvelopeOn = true;
              }
              else
              {
                  AY_EnvelopeOn = (((ay_reg[0x08] & 0x10) || (ay_reg[0x09] & 0x10) || (ay_reg[0x0A] & 0x10))  ? true : false);
                  UpdateToneC();
                  UpdateNoiseAY();
              }
              break;
             
          // -----------------------------
          // Envelope Period Register
          // -----------------------------
          case 0x0B:
          case 0x0C:
              envelope_period = ((ay_reg[0x0C] << 8) | ay_reg[0x0B]);
              envelope_period = envelope_period / 10;  // This gets us "close"
              break;
              
          case 0x0D:
              a_idx=0; b_idx=0; c_idx=0;
              break;
      }
}

#define WAVE_A 0
#define WAVE_B 1
#define WAVE_C 2
#define WAVE_D 3    // D and E share same waveform on SCC chip
#define WAVE_E 3    // D and E share same waveform on SCC chip

u8 scc_waveform(u8 wave, u8 volume)
{
    return volume;  // For now we are ignoring the wavetable completely... suprisingly enough, the music sounds reasonably good anyway.
}

void UpdateSccToneA(void)
{
    u16 freq=0;
    
    // ----------------------------------------------------------------------
    // If Channel A tone is enabled - set frequency and update SN sound core
    // ----------------------------------------------------------------------
    if (scc_registers[0x8F] & 0x01)
    {
        freq = (scc_registers[0x81] << 8) | scc_registers[0x80];
        freq = ((freq & 0x0800) ? 0x7FF : freq&0x7FF);
        ay76496W(0x80 | (freq & 0xF), &sccABC);
        ay76496W((freq >> 4) & 0x7F, &sccABC);
        if (freq > 0)
            ay76496W(0x90 | Volumes[(scc_waveform(WAVE_A, scc_registers[0x8A]) & 0x0F)], &sccABC);
        else 
            ay76496W(0x9F, &sccABC); // Turn off tone sound on Channel A
    }
    else
    {
        ay76496W(0x9F, &sccABC); // Turn off tone sound on Channel A
    }    
}

void UpdateSccToneB(void)
{
    u16 freq=0;
    
    // ----------------------------------------------------------------------
    // If Channel B tone is enabled - set frequency and update SN sound core
    // ----------------------------------------------------------------------
    if (scc_registers[0x8F] & 0x02)
    {
        freq = (scc_registers[0x83] << 8) | scc_registers[0x82];
        freq = ((freq & 0x0800) ? 0x7FF : freq&0x7FF);
        ay76496W(0xA0 | (freq & 0xF), &sccABC);
        ay76496W((freq >> 4) & 0x7F, &sccABC);
        if (freq > 0)
            ay76496W(0xB0 | Volumes[(scc_waveform(WAVE_B, scc_registers[0x8B]) & 0x0F)], &sccABC);
        else 
            ay76496W(0xBF, &sccABC); // Turn off tone sound on Channel B
    }
    else
    {
        ay76496W(0xBF, &sccABC); // Turn off tone sound on Channel B
    }    
}

void UpdateSccToneC(void)
{
    u16 freq=0;
    
    // ----------------------------------------------------------------------
    // If Channel C tone is enabled - set frequency and update SN sound core
    // ----------------------------------------------------------------------
    if (scc_registers[0x8F] & 0x04)
    {
        freq = (scc_registers[0x85] << 8) | scc_registers[0x84];
        freq = ((freq & 0x0800) ? 0x7FF : freq&0x7FF);
        ay76496W(0xC0 | (freq & 0xF), &sccABC);
        ay76496W((freq >> 4) & 0x7F, &sccABC);
        if (freq > 0)
            ay76496W(0xD0 | Volumes[(scc_waveform(WAVE_C, scc_registers[0x8C]) & 0x0F)], &sccABC);
        else 
            ay76496W(0xDF, &sccABC); // Turn off tone sound on Channel C
    }
    else
    {
        ay76496W(0xDF, &sccABC); // Turn off tone sound on Channel C
    }    
}

void UpdateSccToneD(void)
{
    u16 freq=0;
    
    // ----------------------------------------------------------------------
    // If Channel A tone is enabled - set frequency and update SN sound core
    // ----------------------------------------------------------------------
    if (scc_registers[0x8F] & 0x08)
    {
        freq = (scc_registers[0x87] << 8) | scc_registers[0x86];
        freq = ((freq & 0x0800) ? 0x7FF : freq&0x7FF);
        ay76496W(0x80 | (freq & 0xF), &sccDE);
        ay76496W((freq >> 4) & 0x7F, &sccDE);
        if (freq > 0)
            ay76496W(0x90 | Volumes[(scc_waveform(WAVE_D, scc_registers[0x8D]) & 0x0F)], &sccDE);
        else 
            ay76496W(0x9F, &sccDE); // Turn off tone sound on Channel D
    }
    else
    {
        ay76496W(0x9F, &sccDE); // Turn off tone sound on Channel D
    }    
}

void UpdateSccToneE(void)
{
    u16 freq=0;
    
    // ----------------------------------------------------------------------
    // If Channel E tone is enabled - set frequency and update SN sound core
    // ----------------------------------------------------------------------
    if (scc_registers[0x8F] & 0x10)
    {
        freq = (scc_registers[0x89] << 8) | scc_registers[0x88];
        freq = ((freq & 0x0800) ? 0x7FF : freq&0x7FF);
        ay76496W(0xA0 | (freq & 0xF), &sccDE);
        ay76496W((freq >> 4) & 0x7F, &sccDE);
        if (freq > 0)
            ay76496W(0xB0 | Volumes[(scc_waveform(WAVE_E, scc_registers[0x8E]) & 0x0F)], &sccDE);
        else 
            ay76496W(0xBF, &sccDE); // Turn off tone sound on Channel E
    }
    else
    {
        ay76496W(0xBF, &sccDE); // Turn off tone sound on Channel E
    }    
}

// ================================================
// Address         Function
// 9800h - 981Fh   waveform channel 1
// 9820h - 983Fh   waveform channel 2
// 9840h - 985Fh   waveform channel 3
// 9860h - 987Fh   waveform channel 4 and 5
// 9880h - 9881h   frequency channel 1
// 9882h - 9883h   frequency channel 2
// 9884h - 9885h   frequency channel 3
// 9886h - 9887h   frequency channel 4
// 9888h - 9889h   frequency channel 5
// 988Ah           volume channel 1
// 988Bh           volume channel 2
// 988Ch           volume channel 3
// 988Dh           volume channel 4
// 988Eh           volume channel 5
// 988Fh           on/off switch channel 1 to 5
// ================================================

// -----------------------------------------------------------------------
// Check if any of the Tone Channels ABCDE needs to be updated/output
// -----------------------------------------------------------------------
void UpdateSccTonesAY(void)
{
    UpdateSccToneA();
    UpdateSccToneB();
    UpdateSccToneC();
    UpdateSccToneD();
    UpdateSccToneE();
}

void FakeSCC_Loop(void)
{
    // ---------------------------------------------------------------------
    // Amazingly enough, we are completely ignoring the waveform table... 
    // and, surprisingly enough, the music is still fairly reasonable
    // for the six (6) Konami SCC games that use it!
    // ---------------------------------------------------------------------
}

void FakeSCC_WriteData(u16 address, u8 value)
{
    u8 reg = address & 0xFF;
    scc_registers[reg] = value;
    if (reg & 0x80) // These registers come after the waveform table...
    {
        // ------------------------------------------------------------------
        // Check to see if the register affects one of the 5 SCC channels...
        // ------------------------------------------------------------------
        switch (reg)
        {
            case 0x80:
            case 0x81:
            case 0x8A:
                UpdateSccToneA();
                break;
            case 0x82:
            case 0x83:
            case 0x8B:
                UpdateSccToneB();
                break;
            case 0x84:
            case 0x85:
            case 0x8C:
                UpdateSccToneC();
                break;
            case 0x86:
            case 0x87:
            case 0x8D:
                UpdateSccToneD();
                break;
            case 0x88:
            case 0x89:
            case 0x8E:
                UpdateSccToneE();
                break;
            case 0x8F:
                UpdateSccTonesAY();
                break;
        }
    }
}


// End of file
