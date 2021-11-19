// ------------------------------------------------------------------------------------
// The 'Fake' AY handler simply turns AY sound register access into corresponding
// SN sound chip calls. There is some loss of fidelity and we have to handle the
// volume envelopes in a very crude way... but it works and is good enough for now.
// ------------------------------------------------------------------------------------

#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../colecoDS.h"
#include "../../colecomngt.h"
#include "../../colecogeneric.h"

#include "SN76496.h"

#define NORAM 0xFF

u8 channel_a_enable = 0;
u8 channel_b_enable = 0;
u8 channel_c_enable = 0;
u8 noise_enable = 0;

extern u8 sgm_enable;
extern u8 sgm_idx;
extern u8 sgm_reg[256];
extern u16 sgm_low_addr;
extern SN76496 sncol;

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
ITCM_CODE void FakeAY_Loop(void)
{
    static u16 delay=0;
    
    if (sgm_reg[0x07] == 0xFF) return;  // Nothing enabled - nobody using the AY chip.
    
    if (envelope_period == 0) return;

    if (++delay > ((envelope_period)+1))
    {
        delay = 0;
        u8 shape=0;
        shape = sgm_reg[0x0D] & 0x0F;
        if ((sgm_reg[0x08] & 0x20) && (!(sgm_reg[0x07] & 0x01)))
        {
            u8 vol = Envelopes[shape][a_idx]; 
            if (++a_idx > 31)
            {
                if ((shape & 0x09) == 0x08) a_idx = 0; else a_idx=31;
            }
            sn76496W(0x90 | vol, &sncol);
        }
        
        if ((sgm_reg[0x09] & 0x20) && (!(sgm_reg[0x07] & 0x02)))
        {
            u8 vol = Envelopes[shape][b_idx]; 
            if (++b_idx > 31)
            {
                if ((shape & 0x09) == 0x08) b_idx = 0; else b_idx=31;
            }
            sn76496W(0xB0 | vol, &sncol);
        }

        if ((sgm_reg[0x0A] & 0x20) && (!(sgm_reg[0x07] & 0x04)))
        {
            u8 vol = Envelopes[shape][c_idx]; 
            if (++c_idx > 31)
            {
                if ((shape & 0x09) == 0x08) c_idx = 0; else c_idx=31;
            }
            sn76496W(0xD0 | vol, &sncol);
        }
    }
}

void FakeAY_WriteIndex(u8 Value)
{
    sgm_idx = Value;
}

u8 FakeAY_ReadData(void)
{
    return sgm_reg[sgm_idx];   
}


void UpdateNoiseAY(void)
{
      // Noise Channel - we turn it on if the noise channel is enable along with the channel's volume not zero...
      if ( (!(sgm_reg[0x07] & 0x08) && (sgm_reg[0x08] != 0) && channel_a_enable) || 
           (!(sgm_reg[0x07] & 0x10) && (sgm_reg[0x09] != 0) && channel_b_enable) || 
           (!(sgm_reg[0x07] & 0x20) && (sgm_reg[0x0A] != 0) && channel_c_enable) )
      {
          if (!noise_enable)
          {
              noise_enable=1;
              if (noise_period > 16) sn76496W(0xE2|0x04, &sncol);       // E2 is the lowest frequency (highest period)
              else if (noise_period > 8) sn76496W(0xE1|0x04, &sncol);   // E1 is the middle frequency (middle period)
              else sn76496W(0xE0|0x04, &sncol);                         // E0 is the highest frequency (lowest period)
              if (!(sgm_reg[0x07] & 0x08) && (sgm_reg[0x08] != 0) && channel_a_enable) sn76496W(0xF0 | Volumes[sgm_reg[0x08]], &sncol);
              if (!(sgm_reg[0x07] & 0x10) && (sgm_reg[0x09] != 0) && channel_b_enable) sn76496W(0xF0 | Volumes[sgm_reg[0x09]], &sncol);
              if (!(sgm_reg[0x07] & 0x20) && (sgm_reg[0x0A] != 0) && channel_c_enable) sn76496W(0xF0 | Volumes[sgm_reg[0x0A]], &sncol);
          }
      }
      else
      {
          sn76496W(0xFF, &sncol);
      }
}

u8 AY_RegisterMasks[] = {0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF, 0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF};
void FakeAY_WriteData(u8 Value)
{
      // ----------------------------------------------------------------------------------------
      // This is the AY sound chip support... we're cheating here and just mapping those sounds
      // onto the original Colecovision SN sound chip. Not perfect but good enough for now...
      // ----------------------------------------------------------------------------------------
      Value &= AY_RegisterMasks[sgm_idx & 0x0F];
      sgm_reg[sgm_idx]=Value;
      u16 freq=0;
      switch (sgm_idx)
      {
          // Channel A frequency (period) - low and high
          case 0x00:
          case 0x01:
              if (!(sgm_reg[0x07] & 0x01))
              {
                  freq = (sgm_reg[0x01] << 8) | sgm_reg[0x00];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  sn76496W(0x80 | (freq & 0xF), &sncol);
                  sn76496W((freq >> 4) & 0x3F, &sncol);
              }
              break;
              
             
          // Channel B frequency (period) - low and high
          case 0x02:
          case 0x03:
              if (!(sgm_reg[0x07] & 0x02))
              {
                  freq = (sgm_reg[0x03] << 8) | sgm_reg[0x02];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  sn76496W(0xA0 | (freq & 0xF), &sncol);
                  sn76496W((freq >> 4) & 0x3F, &sncol);
              }
              break;
          
           // Channel C frequency (period) - low and high
          case 0x04:
          case 0x05:
              if (!(sgm_reg[0x07] & 0x04))
              {
                  freq = (sgm_reg[0x05] << 8) | sgm_reg[0x04];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  sn76496W(0xC0 | (freq & 0xF), &sncol);
                  sn76496W((freq >> 4) & 0x3F, &sncol);
              }
              break;
              
          // Noise Period     
          case 0x06:
               noise_period = Value & 0x1F;
               if (noise_enable)
               {
                  if (noise_period > 16) sn76496W(0xE2|0x04, &sncol);       // E2 is the lowest frequency (highest period)
                  else if (noise_period > 8) sn76496W(0xE1|0x04, &sncol);   // E1 is the middle frequency (middle period)
                  else sn76496W(0xE0|0x04, &sncol);                         // E0 is the highest frequency (lowest period)
                  if (!(sgm_reg[0x07] & 0x08) && (sgm_reg[0x08] != 0) && channel_a_enable) sn76496W(0xF0 | Volumes[sgm_reg[0x08]], &sncol);
                  if (!(sgm_reg[0x07] & 0x10) && (sgm_reg[0x09] != 0) && channel_b_enable) sn76496W(0xF0 | Volumes[sgm_reg[0x09]], &sncol);
                  if (!(sgm_reg[0x07] & 0x20) && (sgm_reg[0x0A] != 0) && channel_c_enable) sn76496W(0xF0 | Volumes[sgm_reg[0x0A]], &sncol);
               }
              break;
              
          // Global Sound Enable/Disable Register
          case 0x07:
              // Channel A Enable/Disable
              if (!(sgm_reg[0x07] & 0x01))
              {
                  if (!channel_a_enable)
                  {
                      channel_a_enable=1;
                      a_idx=0;
                      freq = (sgm_reg[0x01] << 8) | sgm_reg[0x00];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      sn76496W(0x80 | (freq & 0xF), &sncol);
                      sn76496W((freq >> 4) & 0x3F, &sncol);
                      sn76496W(0x90 | Volumes[(sgm_reg[0x08] & 0x1F)], &sncol);
                  }
              }
              else
              {
                  if (channel_a_enable)
                  {
                      channel_a_enable = 0;
                      sn76496W(0x90 | 0x0F, &sncol);
                  }
              }
              
              // Channel B Enable/Disable
              if (!(sgm_reg[0x07] & 0x02))
              {
                  if (!channel_b_enable)
                  {
                      channel_b_enable=1;
                      b_idx=0;
                      freq = (sgm_reg[0x03] << 8) | sgm_reg[0x02];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      sn76496W(0xA0 | (freq & 0xF), &sncol);
                      sn76496W((freq >> 4) & 0x3F, &sncol);
                      sn76496W(0xB0 | Volumes[(sgm_reg[0x09] & 0x1F)], &sncol);
                  }
              }
              else
              {
                  if (channel_b_enable)
                  {
                      channel_b_enable = 0;
                      sn76496W(0xB0 | 0x0F, &sncol);
                  }
              }
              
              
              // Channel C Enable/Disable
              if (!(sgm_reg[0x07] & 0x04))
              {
                  if (!channel_c_enable)
                  {
                      channel_c_enable=1;
                      c_idx=0;
                      freq = (sgm_reg[0x05] << 8) | sgm_reg[0x04];
                      freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                      sn76496W(0xC0 | (freq & 0xF), &sncol);
                      sn76496W((freq >> 4) & 0x3F, &sncol);
                      sn76496W(0xD0 | Volumes[(sgm_reg[0x0A] & 0x1F)], &sncol);
                  }
              }
              else
              {
                  if (channel_c_enable)
                  {
                      channel_c_enable = 0;
                      sn76496W(0xD0 | 0x0F, &sncol);
                  }
              }
              UpdateNoiseAY();
              break;
              
          // Volume Registers for all channels...
          case 0x08:
              if (Value & 0x20) Value = 0x0;                      // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x01) Value = 0x0;              // If Channel A is disabled, volume OFF
              sn76496W(0x90 | Volumes[(Value & 0x1F)],&sncol);    // Write new Volume for Channel A
              UpdateNoiseAY();
              a_idx=0;
              break;
          case 0x09:
              if (Value & 0x20) Value = 0x0;                      // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x02) Value = 0x0;              // If Channel B is disabled, volume OFF
              sn76496W(0xB0 | Volumes[(Value & 0x1F)],&sncol);    // Write new Volume for Channel B
              UpdateNoiseAY();
              b_idx=0;
              break;
          case 0x0A:
              if (Value & 0x20) Value = 0x0;                      // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x04) Value = 0x0;              // If Channel C is disabled, volume OFF
              sn76496W(0xD0 | Volumes[(Value & 0x1F)],&sncol);    // Write new Volume for Channel C
              UpdateNoiseAY();
              c_idx=0;
              break;
             
          // Envelope Period
          case 0x0B:
          case 0x0C:
              envelope_period = ((sgm_reg[0x0C] << 8) | sgm_reg[0x0B]) & 0x3FFF;
              break;
      }
}

// End of file
