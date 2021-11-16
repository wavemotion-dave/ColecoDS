#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../colecoDS.h"
#include "../../colecomngt.h"
#include "../../colecogeneric.h"

#include "sn76496_c.h"

#define NORAM 0xFF

u8 channel_a_enable = 0;
u8 channel_b_enable = 0;
u8 channel_c_enable = 0;
u8 noise_enable = 0;


extern u8 sgm_enable;
extern u8 sgm_idx;
extern u8 sgm_reg[256];
extern u16 sgm_low_addr;
extern sn76496 sncol;



static const unsigned char Envelopes[16][32] =
{
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
  { 15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0 },
  { 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }
};

u16 noise_period = 0;

static const u8 Volumes[32] = { 15,15,14,14,13,13,12,12,11,11,10,10,9,9,8,8,7,7,6,6,5,5,4,4,3,3,2,2,1,1,0,0 };
u8 a_idx=0;
u8 b_idx=0;
u8 c_idx=0;
ITCM_CODE void LoopAY(void)
{
    static u16 delay=0;
    
    if (sgm_reg[0x07] == 0xFF) return;  // Nothing enabled - nobody using the AY chip.
    
    u16 envelope_period = (((sgm_reg[0x0C] << 8) | sgm_reg[0x0B])>>4) & 0xFFFF;
    if (envelope_period == 0) return;
    if (++delay > (envelope_period+1))
    {
        delay = 0;
        u8 shape=0;
        shape = sgm_reg[0x0D] & 0x0F;
        if ((sgm_reg[0x08] & 0x20) && (!(sgm_reg[0x07] & 0x01)))
        {
            u8 vol = Envelopes[shape][31-a_idx]; 
            if (++a_idx > 31)
            {
                if ((shape & 0x09) == 0x08) a_idx = 0; else a_idx=31;
            }
            SN76496_w(&sncol, 0x90 | vol);
        }
        
        if ((sgm_reg[0x09] & 0x20) && (!(sgm_reg[0x07] & 0x02)))
        {
            u8 vol = Envelopes[shape][31-b_idx];
            if (++b_idx > 31)
            {
                if ((shape & 0x09) == 0x08) b_idx = 0; else b_idx=31;
            }
            SN76496_w(&sncol, 0xB0 | vol);
        }

        if ((sgm_reg[0x0A] & 0x20) && (!(sgm_reg[0x07] & 0x04)))
        {
            u8 vol = Envelopes[shape][31-c_idx];
            if (++c_idx > 31)
            {
                if ((shape & 0x09) == 0x08) c_idx = 0; else c_idx=31;
            }
            SN76496_w(&sncol, 0xD0 | vol);
        }
    }
}

void HandleAYsound(u8 Value)
{
      // ----------------------------------------------------------------------------------------
      // This is the AY sound chip support... we're cheating here and just mapping those sounds
      // onto the original Colecovision SN sound chip. Not perfect but good enough for now...
      // ----------------------------------------------------------------------------------------
      switch (sgm_idx)
      {
          case 1:
          case 3:
          case 5:
          case 13:
              Value &= 0x0F;
              break;
          case 6:
          case 8:
          case 9:
          case 10:
              Value &= 0x1F;
              break;
      }
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
                  SN76496_w(&sncol, 0x80 | (freq & 0xF));
                  SN76496_w(&sncol, (freq >> 4) & 0x3F);
              }
              break;
              
             
          // Channel B frequency (period)
          case 0x02:
          case 0x03:
              if (!(sgm_reg[0x07] & 0x02))
              {
                  freq = (sgm_reg[0x03] << 8) | sgm_reg[0x02];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  SN76496_w(&sncol, 0xA0 | (freq & 0xF));
                  SN76496_w(&sncol, (freq >> 4) & 0x3F);
              }
              break;
          
           // Channel C frequency (period)
          case 0x04:
          case 0x05:
              if (!(sgm_reg[0x07] & 0x04))
              {
                  freq = (sgm_reg[0x05] << 8) | sgm_reg[0x04];
                  freq = ((freq & 0x0C00) ? 0x3FF : freq&0x3FF);
                  SN76496_w(&sncol, 0xC0 | (freq & 0xF));
                  SN76496_w(&sncol, (freq >> 4) & 0x3F);
              }
              break;
              
              
          case 0x06:
               noise_period = Value & 0x1F;
               if (noise_enable)
               {
                  if (noise_period > 16) SN76496_w(&sncol, 0xE2);       // E2 is the lowest frequency (highest period)
                  else if (noise_period > 8) SN76496_w(&sncol, 0xE1);   // E1 is the middle frequency (middle period)
                  else SN76496_w(&sncol, 0xE0);                         // E0 is the highest frequency (lowest period)
                  SN76496_w(&sncol, 0xF9);
               }
              break;
              
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
                      SN76496_w(&sncol, 0x80 | (freq & 0xF));
                      SN76496_w(&sncol, (freq >> 4) & 0x3F);
                      SN76496_w(&sncol, 0x90 | Volumes[(sgm_reg[0x08] & 0x1F)]);
                  }
              }
              else
              {
                  if (channel_a_enable)
                  {
                      channel_a_enable = 0;
                      SN76496_w(&sncol, 0x90 | 0x0F);
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
                      SN76496_w(&sncol, 0xA0 | (freq & 0xF));
                      SN76496_w(&sncol, (freq >> 4) & 0x3F);
                      SN76496_w(&sncol, 0xB0 | Volumes[(sgm_reg[0x09] & 0x1F)]);
                  }
              }
              else
              {
                  if (channel_b_enable)
                  {
                      channel_b_enable = 0;
                      SN76496_w(&sncol, 0xB0 | 0x0F);
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
                      SN76496_w(&sncol, 0xC0 | (freq & 0xF));
                      SN76496_w(&sncol, (freq >> 4) & 0x3F);
                      SN76496_w(&sncol, 0xD0 | Volumes[(sgm_reg[0x0A] & 0x1F)]);
                  }
              }
              else
              {
                  if (channel_c_enable)
                  {
                      channel_c_enable = 0;
                      SN76496_w(&sncol, 0xD0 | 0x0F);
                  }
              }
              
              // Noise Channel - we turn it on if the noise channel is enable along with the normal tone channel...
              if ( (!(sgm_reg[0x07] & 0x08) && channel_a_enable) || (!(sgm_reg[0x07] & 0x10) && channel_b_enable) || (!(sgm_reg[0x07] & 0x20) && channel_c_enable) )
              {
                  if (!noise_enable)
                  {
                      noise_enable=1;
                      if (noise_period > 16) SN76496_w(&sncol, 0xE2);       // E2 is the lowest frequency (highest period)
                      else if (noise_period > 8) SN76496_w(&sncol, 0xE1);   // E1 is the middle frequency (middle period)
                      else SN76496_w(&sncol, 0xE0);                         // E0 is the highest frequency (lowest period)
                      SN76496_w(&sncol, 0xF9);
                  }
              }
              else
              {
                  SN76496_w(&sncol, 0xFF);
              }
              
              break;
              
              
          case 0x08:
              if (Value & 0x20) Value = 0x0;                    // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x01) Value = 0x0;            // If Channel A is disabled, volume OFF
              SN76496_w(&sncol, 0x90 | Volumes[(Value & 0x1F)]);     // Write new Volume for Channel A
              a_idx=0;
              break;
          case 0x09:
              if (Value & 0x20) Value = 0x0;                    // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x02) Value = 0x0;            // If Channel B is disabled, volume OFF
              SN76496_w(&sncol, 0xB0 | Volumes[(Value & 0x1F)]);     // Write new Volume for Channel B
              b_idx=0;
              break;
          case 0x0A:
              if (Value & 0x20) Value = 0x0;                    // If Envelope Mode... start with volume OFF
              if (sgm_reg[0x07] & 0x04) Value = 0x0;            // If Channel C is disabled, volume OFF
              SN76496_w(&sncol, 0xD0 | Volumes[(Value & 0x1F)]);     // Write new Volume for Channel C
              c_idx=0;
              break;

      }
}

// End of file
