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
// ------------------------------------------------------------------------------------

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../colecoDS.h"
#include "../../colecomngt.h"
#include "../../colecogeneric.h"

#include "SN76496.h"

u8 channel_a_enable = 0;
u8 channel_b_enable = 0;
u8 channel_c_enable = 0;
u8 noise_enable = 0;

extern u8 sgm_enable;
extern u8 ay_reg_idx;
extern u8 ay_reg[256];
extern u16 sgm_low_addr;
extern SN76496 aycol;

static const u8 Volumes[32] = { 15,14,13,12,11,10,10,9,8,7,7,6,6,5,5,5,4,4,4,3,3,3,2,2,2,1,1,1,1,1,0,0 };
u16 envelope_period = 0;

static const unsigned char Envelopes[16][32] =
{
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    {15,14,13,12,11,10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15},
};

u16 noise_period = 0;

u8 a_idx=0;
u8 b_idx=0;
u8 c_idx=0;

// ---------------------------------------------------------------------------------------------
// We handle envelopes here... the timing is nowhere near exact but so few games utilize this 
// and so accuracy isn't all that critical. The sound will be a little off - but it will be ok.
// ---------------------------------------------------------------------------------------------
void FakeAY_Loop(void)
{
    static u16 delay=0;
    
    if (ay_reg[0x07] == 0xFF) return;  // Nothing enabled - nobody using the AY chip.
    
    if (envelope_period == 0) return;

    if (++delay > ((envelope_period)+1))
    {
        delay = 0;
        u8 shape = ay_reg[0x0D] & 0x0F;
        
        // ---------------------------------------------------------------
        // If Envelope is enabled for Channel A and Channel is enabled...
        // ---------------------------------------------------------------
        if ((ay_reg[0x08] & 0x20) && channel_a_enable)
        {
            u8 vol = Envelopes[shape][a_idx]; 
            if (++a_idx > 31)
            {
                if ((shape & 0x09) == 0x08) a_idx = 0; else a_idx=31;
            }
            sn76496W(0x90 | vol, &aycol);
        }
        
        // ---------------------------------------------------------------
        // If Envelope is enabled for Channel B and Channel is enabled...
        // ---------------------------------------------------------------
        if ((ay_reg[0x09] & 0x20) && channel_b_enable)
        {
            u8 vol = Envelopes[shape][b_idx]; 
            if (++b_idx > 31)
            {
                if ((shape & 0x09) == 0x08) b_idx = 0; else b_idx=31;
            }
            sn76496W(0xB0 | vol, &aycol);
        }

        // ---------------------------------------------------------------
        // If Envelope is enabled for Channel C and Channel is enabled...
        // ---------------------------------------------------------------
        if ((ay_reg[0x0A] & 0x20) && channel_c_enable)
        {
            u8 vol = Envelopes[shape][c_idx]; 
            if (++c_idx > 31)
            {
                if ((shape & 0x09) == 0x08) c_idx = 0; else c_idx=31;
            }
            sn76496W(0xD0 | vol, &aycol);
        }
    }
}

// -----------------------------------
// Write the AY register index...
// -----------------------------------
void FakeAY_WriteIndex(u8 Value)
{
    ay_reg_idx = Value;
}

// -----------------------------------
// Read an AY data value...
// -----------------------------------
u8 FakeAY_ReadData(void)
{
    return ay_reg[ay_reg_idx];
}


// ------------------------------------------------------------------
// Noise is a bit more complicated on the AY chip as we have to
// check each A,B,C channel to see if we should be mixing in noise. 
// ------------------------------------------------------------------
void UpdateNoiseAY(void)
{
      // Noise Channel - we turn it on if the noise channel is enabled along with the channel's volume not zero...
      if ( (!(ay_reg[0x07] & 0x08) && (ay_reg[0x08] != 0)) || 
           (!(ay_reg[0x07] & 0x10) && (ay_reg[0x09] != 0)) || 
           (!(ay_reg[0x07] & 0x20) && (ay_reg[0x0A] != 0)) )
      {
          if (!noise_enable)
          {
              noise_enable = 1;
              if      (noise_period > 16) sn76496W(0xE2 | 0x04, &aycol);   // E2 is the lowest frequency (highest period)
              else if (noise_period > 8)  sn76496W(0xE1 | 0x04, &aycol);   // E1 is the middle frequency (middle period)
              else                        sn76496W(0xE0 | 0x04, &aycol);   // E0 is the highest frequency (lowest period)
              
              // Now output the noise for the first channel it's enbled on...
              if (!(ay_reg[0x07] & 0x08) && (ay_reg[0x08] != 0) && channel_a_enable)      sn76496W(0xF0 | Volumes[ay_reg[0x08]], &aycol);
              else if (!(ay_reg[0x07] & 0x10) && (ay_reg[0x09] != 0) && channel_b_enable) sn76496W(0xF0 | Volumes[ay_reg[0x09]], &aycol);
              else if (!(ay_reg[0x07] & 0x20) && (ay_reg[0x0A] != 0) && channel_c_enable) sn76496W(0xF0 | Volumes[ay_reg[0x0A]], &aycol);
              else sn76496W(0xFF, &aycol);
          }
      }
      else
      {
          noise_enable = 0;
          sn76496W(0xFF, &aycol);
      }
}

// ------------------------------------------------------------------------------------------------------------------
// Writing AY data is where the magic mapping happens between the AY chip and the standard SN colecovision chip.
// This is a bit of a hack... and it reduces the sound quality a bit on the AY chip but it allows us to use just
// one sound driver for the SN audio chip for everythign in the system. On a retro-handheld, this is good enough.
// ------------------------------------------------------------------------------------------------------------------
u8 AY_RegisterMasks[] = {0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF, 0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF};
u8 prevEnvelopeA_enabled = 0;
u8 prevEnvelopeB_enabled = 0;
u8 prevEnvelopeC_enabled = 0;
void FakeAY_WriteData(u8 Value)
{
      // ----------------------------------------------------------------------------------------
      // This is the AY sound chip support... we're cheating here and just mapping those sounds
      // onto the original Colecovision SN sound chip. Not perfect but good enough for now...
      // ----------------------------------------------------------------------------------------
      Value &= AY_RegisterMasks[ay_reg_idx & 0x0F];
      ay_reg[ay_reg_idx]=Value;
      u16 freq=0;
      switch (ay_reg_idx)
      {
          // Channel A frequency (period) - low and high
          case 0x00:
          case 0x01:
              if ((!(ay_reg[0x07] & 0x01)) || (!(ay_reg[0x07] & 0x08)))
              {
                  freq = (ay_reg[0x01] << 8) | ay_reg[0x00];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  sn76496W(0x80 | (freq & 0xF), &aycol);
                  sn76496W((freq >> 4) & 0x3F, &aycol);
              }
              break;
              
             
          // Channel B frequency (period) - low and high
          case 0x02:
          case 0x03:
              if ((!(ay_reg[0x07] & 0x02)) || (!(ay_reg[0x07] & 0x10)))
              {
                  freq = (ay_reg[0x03] << 8) | ay_reg[0x02];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  sn76496W(0xA0 | (freq & 0xF), &aycol);
                  sn76496W((freq >> 4) & 0x3F, &aycol);
              }
              break;
          
           // Channel C frequency (period) - low and high
          case 0x04:
          case 0x05:
              if ((!(ay_reg[0x07] & 0x04)) || (!(ay_reg[0x07] & 0x20)))
              {
                  freq = (ay_reg[0x05] << 8) | ay_reg[0x04];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  sn76496W(0xC0 | (freq & 0xF), &aycol);
                  sn76496W((freq >> 4) & 0x3F, &aycol);
              }
              break;
              
          // Noise Period     
          case 0x06:
               noise_period = Value & 0x1F;
               if (noise_enable)
               {
                   noise_enable = 0; // Force Update of Noise
                   UpdateNoiseAY();  // Update the Noise output
               }
              break;
              
          // Global Sound Enable/Disable Register
          case 0x07:
              // Channel A Enable/Disable
              if ((!(ay_reg[0x07] & 0x01)) || (!(ay_reg[0x07] & 0x08)))
              {
                  if (!channel_a_enable)
                  {
                      channel_a_enable=1;
                      a_idx=0;
                      freq = (ay_reg[0x01] << 8) | ay_reg[0x00];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      sn76496W(0x80 | (freq & 0xF), &aycol);
                      sn76496W((freq >> 4) & 0x3F, &aycol);
                      sn76496W(0x90 | Volumes[(ay_reg[0x08] & 0x1F)], &aycol);
                  }
              }
              else
              {
                  if (channel_a_enable)
                  {
                      channel_a_enable = 0;
                      sn76496W(0x90 | 0x0F, &aycol);
                  }
              }
              
              // Channel B Enable/Disable
              if ((!(ay_reg[0x07] & 0x02)) || (!(ay_reg[0x07] & 0x10)))
              {
                  if (!channel_b_enable)
                  {
                      channel_b_enable=1;
                      b_idx=0;
                      freq = (ay_reg[0x03] << 8) | ay_reg[0x02];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      sn76496W(0xA0 | (freq & 0xF), &aycol);
                      sn76496W((freq >> 4) & 0x3F, &aycol);
                      sn76496W(0xB0 | Volumes[(ay_reg[0x09] & 0x1F)], &aycol);
                  }
              }
              else
              {
                  if (channel_b_enable)
                  {
                      channel_b_enable = 0;
                      sn76496W(0xB0 | 0x0F, &aycol);    // Set Volume to OFF
                  }
              }
              
              
              // Channel C Enable/Disable
              if ((!(ay_reg[0x07] & 0x04)) || (!(ay_reg[0x07] & 0x20)))
              {
                  if (!channel_c_enable)
                  {
                      channel_c_enable=1;
                      c_idx=0;
                      freq = (ay_reg[0x05] << 8) | ay_reg[0x04];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      sn76496W(0xC0 | (freq & 0xF), &aycol);
                      sn76496W((freq >> 4) & 0x3F, &aycol);
                      sn76496W(0xD0 | Volumes[(ay_reg[0x0A] & 0x1F)], &aycol);
                  }
              }
              else
              {
                  if (channel_c_enable)
                  {
                      channel_c_enable = 0;
                      sn76496W(0xD0 | 0x0F, &aycol);
                  }
              }
              UpdateNoiseAY();
              break;
              
          // ----------------------------------
          // Volume Registers are below...
          // ----------------------------------
          case 0x08:
              if (Value & 0x20)                                   // If Envelope Mode... see if this is being enabled
              {
                  if (!prevEnvelopeA_enabled)
                  {
                      prevEnvelopeA_enabled = true;
                      Value = 0x0;
                      a_idx = 0;
                  }
                  prevEnvelopeA_enabled = true;
              } else prevEnvelopeA_enabled = false;
              if (ay_reg[0x07] & 0x01) Value = 0x0;               // If Channel A is disabled, volume OFF
              sn76496W(0x90 | Volumes[(Value & 0x1F)],&aycol);    // Write new Volume for Channel A
              UpdateNoiseAY();
              break;
          case 0x09:
              if (Value & 0x20)                                   // If Envelope Mode... see if this is being enabled
              {
                  if (!prevEnvelopeB_enabled)
                  {
                      prevEnvelopeB_enabled = true;
                      Value = 0x0;
                      b_idx = 0;
                  }
                  prevEnvelopeB_enabled = true;
              } else prevEnvelopeB_enabled = false;
              if (ay_reg[0x07] & 0x02) Value = 0x0;               // If Channel B is disabled, volume OFF
              sn76496W(0xB0 | Volumes[(Value & 0x1F)],&aycol);    // Write new Volume for Channel B
              UpdateNoiseAY();
              break;
          case 0x0A:
              if (Value & 0x20)                                   // If Envelope Mode... see if this is being enabled
              {
                  if (!prevEnvelopeC_enabled)
                  {
                      prevEnvelopeC_enabled = true;
                      Value = 0x0;
                      c_idx = 0;
                  }
                  prevEnvelopeC_enabled = true;
              } else prevEnvelopeC_enabled = false;
              if (ay_reg[0x07] & 0x04) Value = 0x0;               // If Channel C is disabled, volume OFF
              sn76496W(0xD0 | Volumes[(Value & 0x1F)],&aycol);    // Write new Volume for Channel C
              UpdateNoiseAY();
              break;
             
          // -----------------------------
          // Envelope Period Register
          // -----------------------------
          case 0x0B:
          case 0x0C:
              {
              u16 new_period = ((ay_reg[0x0C] << 8) | ay_reg[0x0B]) & 0x3FFF;
              if ((envelope_period > 0) && (new_period == 0))
              {
                  prevEnvelopeA_enabled = false;
                  prevEnvelopeB_enabled = false;
                  prevEnvelopeC_enabled = false;
                  sn76496W(0x9F, &aycol);
                  sn76496W(0xBF, &aycol);
                  sn76496W(0xDF, &aycol);
              }
              envelope_period = new_period;
              }
              break;
      }
}

// End of file
