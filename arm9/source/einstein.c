// =====================================================================================
// Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fat.h>
#include <fcntl.h>

#include "colecoDS.h"
#include "CRC32.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/z80/ctc.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "printf.h"

#define NORAM 0xFF

u16 einstein_ram_start __attribute__((section(".dtcm"))) = 0x8000;
u16 keyboard_interrupt __attribute__((section(".dtcm"))) = 0;
u16 joystick_interrupt __attribute__((section(".dtcm"))) = 0;
u8 keyboard_w = 0x00;
u8 key_int_mask = 0xFF;
u8 joy_int_mask = 0xFF;
u8 myKeyData = 0xFF;
u8 adc_mux = 0x00;
u8 drive_select = 0xFF;

extern u8 EinsteinBios[];

#define KEYBOARD_VECTOR  0xF7
#define JOYSTICK_VECTOR  0xFD

// -----------------------------------------------------------------------------------------
// Allocate the Floppy Drive Controller structure where we track the registers and the
// track buffer to handle FDC requests. Right now we're only handling basic seeks and 
// sector reads, but that's good enough to get the vast majority of Einstein .dsk games
// playing properly.
// -----------------------------------------------------------------------------------------
struct FDC_t FDC;


// Status Register:
//   Bit |      Type I      |    Type II    |   Type III    | Comments
//   ----+------------------+---------------+---------------+-------------------
//    7  |    Motor on      | ---------- Motor on --------- | / If bit 3 is
//    6  |    Not used.     | -- Disk is write protected. - | | set then:
//    5  |  Motor spin-up   |    Data type : 0 = normal.    | | If bit 4 is
//       |    completed.    |                1 = deleted.   | | set, the error
//    4  | Record not found | ----- Record not found ------ | / is in the ID
//    3  |    CRC error.    | --------- CRC error --------- |<  field. If bit 4
//    2  |   Not track 0    | ------ Lost data / byte ----- | \ is clear, the
//    1  |   Data Request   | -------- Data request ------- | | error is in
//    0  |       Busy       | ------------ Busy ----------- | \ the data field.

void fdc_state_machine(void)
{
    static u8 fdc_busy_count = 0;
    
    if (FDC.seek_track_0)
    {
        if ((++fdc_busy_count % 3) == 0) FDC.status |= 0x02; else FDC.status &= ~0x02;
        if (fdc_busy_count > 12)
        {
            fdc_busy_count = 0;
        } else return;
    }
    else if (FDC.status & 0x01)
    {
        if (++fdc_busy_count > 6)   // Arbitrary amount of busy - change this for longer loads
        {
            fdc_busy_count = 0;     // Reset busy count
            FDC.status &= ~0x01;    // Clear Busy Status
        } else return;
    }
    
    switch(FDC.command & 0xF0)
    {
        case 0x00: // Restore
            FDC.track  = 0;         // Track buffer at zero
            FDC.actTrack = 0;       // We're actually at track zero
            FDC.seek_track_0  = 0;  // Not seeking track zero (we found it!)
            FDC.wait_for_read = 2;  // No data to transfer
            FDC.status = 0xA0;      // Motor spun up. Track zero. Not busy.
            break;
        case 0x10: // Seek Track
            FDC.track = FDC.data;                       // Settle on requested track
            FDC.actTrack = FDC.data;                    // Settle on requested track
            FDC.seek_track_0  = 0;                      // Not seeking track zero
            FDC.wait_for_read = 2;                      // No data to transfer
            FDC.status = (FDC.actTrack ? 0xA4 : 0xA0);  // Motor Spun Up... Not busy. Check if Track zero
            break;
        case 0x20: // Step
        case 0x30: // Step
            debug5++;
            FDC.status = 0x80;
            break;
        case 0x40: // Step in
        case 0x50: // Step in
            debug5++;
            FDC.status = 0x80;
            break;
        case 0x60: // Step out
        case 0x70: // Step out
            debug5++;
            FDC.status = 0x80;
            break;
        case 0x80: // Read Sector
        case 0x90: // Read Sector
            if (FDC.wait_for_read == 0)
            {
                if (FDC.track_buffer_idx >= FDC.track_buffer_end) // Is there any more data to put out?
                {
                    FDC.status &= ~0x03;            // Done. No longer busy.
                    FDC.wait_for_read=2;            // Don't fetch more FDC data
                    FDC.seek_byte_transfer = 0;     // And reset our counter
                }
                else
                {
                    FDC.status = 0x83;                                   // Data Ready and no errors... still busy
                    FDC.data = FDC.track_buffer[FDC.track_buffer_idx++]; // Read data from our track buffer
                    FDC.wait_for_read = 1;                               // Wait for the CPU to fetch the data
                    if (++FDC.seek_byte_transfer >= 512)                 // Did we cross a sector boundary?
                    {
                        FDC.sector++;                               // Bump the sector number
                        FDC.seek_byte_transfer = 0;                 // And reset our counter
                    }
                }
            }
            break;
        case 0xA0: // Write Sector
        case 0xB0: // Write Sector
            debug5++;
            FDC.status = 0x80;
            break;
        case 0xC0: // Read Address
            debug5++;
            FDC.status = 0x80;
            break;
        case 0xD0: // Force Interrupt
            debug5++;
            FDC.status = (FDC.actTrack ? 0xA4:0xA0);      // Motor Spun Up, Not Busy and Maybe Track Zero
            break;
        case 0xE0: // Read Track
            debug5++;
            FDC.status = 0x80;
            break;
        case 0xF0: // Write Track
            debug5++;
            FDC.status = 0x80;
            break;
    }
}

//  Address offset      Contains on read    on write
//  ------------------------------------------------------
//         0                 Status         Command
//         1                 ------- Track --------
//         2                 ------- Sector -------
//         3                 ------- Data ---------
//
u8 fdc_read(u8 addr)
{
    fdc_state_machine();    // Clock the floppy drive controller state machine
    
    switch (addr)
    {
        case 0: return FDC.status;
        case 1: return FDC.track;
        case 2: return FDC.sector;
        case 3: 
            FDC.status &= ~0x02;     // Clear Data Available flag
            FDC.wait_for_read = 0;   // Clock in next byte (or end sequence if we're read all there is)
            return FDC.data;         // Return data to caller
    }
    
    return 0x00;
}

// FDC Commands:
//   I    Restore            0   0   0   0   h   v   r1  r0
//   I    Seek               0   0   0   1   h   v   r1  r0
//   I    Step               0   0   1   u   h   v   r1  r0
//   I    Step in            0   1   0   u   h   v   r1  r0
//   I    Step out           0   1   1   u   h   v   r1  r0
//   II   Read sector        1   0   0   m  h/s  e  0/c  0
//   II   Write sector       1   0   1   m  h/s  e  p/c  a
//   III  Read address       1   1   0   0  h/0  e   0   0
//   III  Read track         1   1   1   0  h/0  e   0   0
//   III  Write track        1   1   1   1  h/0  e  p/0  0
//   IV   Force interrupt    1   1   0   1   i3  i2  i1  i0
void fdc_write(u8 addr, u8 data)
{
    if (drive_select != 0x01) return;
    
    if (FDC.status & 0x01) // If BUSY
    {
        if ((addr == 0) && ((data & 0xF0) == 0xD0))     // Only a Force Interrupt can override busy
        {
            debug4++;
            FDC.status = (FDC.actTrack ? 0xA4:0xA0);    // Motor Spun Up, Not Busy and Maybe Track Zero
        }
        else
        {
            debug6++;
            return;                                     // We were given a command while busy - ignore it.
        }
    }
    
    // -------------------------------------------------------
    // Handle the write - most of the time it's a command...
    // -------------------------------------------------------
    switch (addr)
    {
        case 0: FDC.command = data; break;
        case 1: FDC.track = data;   break;
        case 2: FDC.sector = data;  break;
        case 3: FDC.data = data;    break;
    }
    
    // If command....
    if (addr == 0x00)
    {
        if (data == 0x05)   // Restore
        {
            FDC.status = (FDC.actTrack ? 0xA5:0xA1);      // Motor Spun Up, Busy and Maybe Track Zero
            FDC.wait_for_read = 2;                        // Not feteching any data
            FDC.seek_track_0  = 1;                        // Seeking track 0
        }
        else
        {
            FDC.status = 0x81; // Default to motor ON and busy...
            
            if ((data&0xF0) == 0x90) // Read Sector... multiple
            {
                memcpy(FDC.track_buffer, &ROM_Memory[(512*1024) + (FDC.actTrack * 5120)], 5120); // Get the track into our buffer
                FDC.track_buffer_idx = FDC.sector*512;
                FDC.track_buffer_end = 5120;
                FDC.wait_for_read = 0; // Start fetching data
                if (io_show_status == 0) io_show_status = 3;
            }
            else if ((data&0xF0) == 0x80) // Read Sector... one sector
            {
                memcpy(FDC.track_buffer, &ROM_Memory[(512*1024) + (FDC.actTrack * 5120)], 5120); // Get the track into our buffer
                FDC.track_buffer_idx = FDC.sector*512;
                FDC.track_buffer_end = FDC.track_buffer_idx+512;
                FDC.wait_for_read = 0; // Start fetching data
                FDC.seek_byte_transfer = 0;
                if (io_show_status == 0) io_show_status = 3;
            }
            else if ((data&0xF0) == 0xE0) // Read Track
            {
                debug5++;   
            }
            else if ((data&0xF0) == 0x10) // Seek Track
            {
                FDC.status = (FDC.actTrack ? 0xA5:0xA1);      // Motor Spun Up, Busy and Maybe Track Zero
                FDC.wait_for_read = 2;  // Not feteching any data
            }
        }
    }
}

void fdc_reset(u8 full_reset)
{
    if (full_reset)
    {
        memset(&FDC, 0x00, sizeof(FDC));    // Clear all registers and the buffers
    }
    FDC.status = 0x80;                  // Motor on... nothing else (not busy)
}

void scan_keyboard(void)
{
    if (kbd_key == 0)
    {
        myKeyData = 0xFF;
        return;
    }
    else
    {
      // For the full keyboard overlay... this is a bit of a hack for SHIFT, CTRL and GRAPH
      if (last_special_key != 0)
      {
          if ((last_special_key_dampen > 0) && (last_special_key_dampen != 50))
          {
              if (--last_special_key_dampen == 0)
              {
                  last_special_key = 0;
                  DSPrint(4,0,6, "    ");
              }
          }

          if (last_special_key == KBD_KEY_SHIFT) 
          { 
            DSPrint(4,0,6, "SHFT");
            key_shift = 1;
          }
          else if (last_special_key == KBD_KEY_CTRL)  
          {
            DSPrint(4,0,6, "CTRL");
            key_ctrl = 1;
          }
          else if (last_special_key == KBD_KEY_GRAPH)  
          {
            DSPrint(4,0,6, "GRPH");
            key_graph = 1;
          }

          if ((kbd_key != 0) && (kbd_key != KBD_KEY_SHIFT) && (kbd_key != KBD_KEY_CTRL) && (kbd_key != KBD_KEY_CODE) && (kbd_key != KBD_KEY_GRAPH))
          {
              if (last_special_key_dampen == 50) last_special_key_dampen = 49;    // Start the SHIFT/CONTROL countdown... this should be enough time for it to register
          }
      }

      myKeyData = 0x00;
        
      // -------------------------------------------------
      // Check every key that might have been pressed...
      // -------------------------------------------------
      for (u8 i=0; i < kbd_keys_pressed; i++)
      {
          kbd_key = kbd_keys[i];
          
          if (!(keyboard_w & 0x01))
          {
              if (kbd_key == KBD_KEY_STOP)  myKeyData |= 0x01;  // Break
              if (kbd_key == KBD_KEY_F8)    myKeyData |= 0x04; 
              if (kbd_key == KBD_KEY_F7)    myKeyData |= 0x08;
              if (kbd_key == KBD_KEY_CAPS)  myKeyData |= 0x10;
              if (kbd_key == KBD_KEY_RET)   myKeyData |= 0x20;
              if (kbd_key == ' ')           myKeyData |= 0x40;
              if (kbd_key == KBD_KEY_ESC)   myKeyData |= 0x80;
          }
          
          if (!(keyboard_w & 0x02))
          {
              if (kbd_key == 'I')           myKeyData |= 0x01;
              if (kbd_key == 'O')           myKeyData |= 0x02;
              if (kbd_key == 'P')           myKeyData |= 0x04;
              if (kbd_key == KBD_KEY_LEFT)  myKeyData |= 0x08;
              if (kbd_key == '_')           myKeyData |= 0x10;
              if (kbd_key == KBD_KEY_LF)    myKeyData |= 0x20;
              if (kbd_key == '|')           myKeyData |= 0x40;
              if (kbd_key == '0')           myKeyData |= 0x80;
          }

          if (!(keyboard_w & 0x04))
          {
              if (kbd_key == 'K')           myKeyData |= 0x01;
              if (kbd_key == 'L')           myKeyData |= 0x02;
              if (kbd_key == ';')           myKeyData |= 0x04;
              if (kbd_key == ':')           myKeyData |= 0x08;
              if (kbd_key == KBD_KEY_RIGHT) myKeyData |= 0x10;
              if (kbd_key == KBD_KEY_BS)    myKeyData |= 0x20;
              if (kbd_key == '9')           myKeyData |= 0x40;
              if (kbd_key == KBD_KEY_F5)    myKeyData |= 0x80;
          }

          if (!(keyboard_w & 0x08))
          {
              if (kbd_key == ',')           myKeyData |= 0x01;
              if (kbd_key == '.')           myKeyData |= 0x02;
              if (kbd_key == '/')           myKeyData |= 0x04;
              if (kbd_key == '8')           myKeyData |= 0x08;
              if (kbd_key == KBD_KEY_INS)   myKeyData |= 0x10;
              if (kbd_key == '=')           myKeyData |= 0x20;
              if (kbd_key == KBD_KEY_UP)    myKeyData |= 0x40;
              if (kbd_key == KBD_KEY_F4)    myKeyData |= 0x80;
          }

          if (!(keyboard_w & 0x10))
          {
              if (kbd_key == '7')           myKeyData |= 0x01;
              if (kbd_key == '6')           myKeyData |= 0x02;
              if (kbd_key == '5')           myKeyData |= 0x04;
              if (kbd_key == '4')           myKeyData |= 0x08;
              if (kbd_key == '3')           myKeyData |= 0x10;
              if (kbd_key == '2')           myKeyData |= 0x20;
              if (kbd_key == '1')           myKeyData |= 0x40;
              if (kbd_key == KBD_KEY_F3)    myKeyData |= 0x80;
          }
          
          if (!(keyboard_w & 0x20))
          {
              if (kbd_key == 'U')           myKeyData |= 0x01;
              if (kbd_key == 'Y')           myKeyData |= 0x02;
              if (kbd_key == 'T')           myKeyData |= 0x04;
              if (kbd_key == 'R')           myKeyData |= 0x08;
              if (kbd_key == 'E')           myKeyData |= 0x10;
              if (kbd_key == 'W')           myKeyData |= 0x20;
              if (kbd_key == 'Q')           myKeyData |= 0x40;
              if (kbd_key == KBD_KEY_F2)    myKeyData |= 0x80;
          }
          
          if (!(keyboard_w & 0x40))
          {
              if (kbd_key == 'J')           myKeyData |= 0x01;
              if (kbd_key == 'H')           myKeyData |= 0x02;
              if (kbd_key == 'G')           myKeyData |= 0x04;
              if (kbd_key == 'F')           myKeyData |= 0x08;
              if (kbd_key == 'D')           myKeyData |= 0x10;
              if (kbd_key == 'S')           myKeyData |= 0x20;
              if (kbd_key == 'A')           myKeyData |= 0x40;
              if (kbd_key == KBD_KEY_F1)    myKeyData |= 0x80;
          }
          
          if (!(keyboard_w & 0x80))
          {
              if (kbd_key == 'M')           myKeyData |= 0x01;
              if (kbd_key == 'N')           myKeyData |= 0x02;
              if (kbd_key == 'B')           myKeyData |= 0x04;
              if (kbd_key == 'V')           myKeyData |= 0x08;
              if (kbd_key == 'C')           myKeyData |= 0x10;
              if (kbd_key == 'X')           myKeyData |= 0x20;
              if (kbd_key == 'Z')           myKeyData |= 0x40;
              if (kbd_key == KBD_KEY_F6)    myKeyData |= 0x80;
          }
      }
    }
    myKeyData = ~myKeyData;
}


// --------------------------------------------------------------------------
// Move the Einstein BIOS back into default memory and point to it...
// --------------------------------------------------------------------------
void einstein_restore_bios(void)
{
    
    memset(BIOS_Memory, 0xFF, 0x10000);
    memcpy(BIOS_Memory, EinsteinBios, 0x2000);
    if (tape_len == 1626) // A bit of a hack... the size of the Diagnostic ROM
    {
        memcpy(BIOS_Memory+0x4000, ROM_Memory, tape_len);   // only for Diagnostics ROM
    }
    MemoryMap[0] = BIOS_Memory + 0x0000;
    MemoryMap[1] = BIOS_Memory + 0x2000;
    MemoryMap[2] = BIOS_Memory + 0x4000;
    MemoryMap[3] = BIOS_Memory + 0x6000;
    
    MemoryMap[4] = RAM_Memory + 0x8000;
    MemoryMap[5] = RAM_Memory + 0xA000;
    MemoryMap[6] = RAM_Memory + 0xC000;
    MemoryMap[7] = RAM_Memory + 0xE000;
}

// --------------------------------------------------------------------------
// The Einstein is interesting in that it will always respond to writes to
// RAM even if the ROM is swapped in (since you can't write to ROM anyway).
// This is handled in the memory write interface in Z80_interface.c
// --------------------------------------------------------------------------
void einstein_swap_memory(void)
{
    // Reads or Writes to port 24h will toggle ROM vs RAM access
    if (einstein_ram_start == 0x8000)  
    {
        MemoryMap[0] = RAM_Memory+0x0000;
        MemoryMap[1] = RAM_Memory+0x2000;
        MemoryMap[2] = RAM_Memory+0x4000;
        MemoryMap[3] = RAM_Memory+0x6000;
        einstein_ram_start = 0x0000;
    }
    else
    {
        MemoryMap[0] = BIOS_Memory+0x0000;
        MemoryMap[1] = BIOS_Memory+0x2000;
        MemoryMap[2] = BIOS_Memory+0x4000;
        MemoryMap[3] = BIOS_Memory+0x6000;
        einstein_ram_start = 0x8000;
    }
}

u8 einstein_joystick_read(void)
{
  u8 adc_port = 0x7F;   // Center Position

  if ((adc_mux & 0x02)) // Player 2 Joystick
  {
      if ((adc_mux & 5) == 4) 
      {
          adc_port = 0x7F;
      } 
      else if ((adc_mux & 5) == 5) 
      {
          adc_port = 0x7F;
      }
  }
  else              // Player 1 Joystick
  {
      if (myConfig.dpad == DPAD_DIAGONALS)
      {
          if      (JoyState & JST_UP)    JoyState = (JST_UP    | JST_RIGHT);
          else if (JoyState & JST_DOWN)  JoyState = (JST_DOWN  | JST_LEFT);
          else if (JoyState & JST_LEFT)  JoyState = (JST_LEFT  | JST_UP);
          else if (JoyState & JST_RIGHT) JoyState = (JST_RIGHT | JST_DOWN);
      }

      if ((adc_mux & 5) == 4) 
      {
          adc_port = 0x7F;
          if (JoyState & JST_RIGHT) adc_port = 0xFF;
          if (JoyState & JST_LEFT)  adc_port = 0x00;
      }
      else if ((adc_mux & 5) == 5) 
      {
          adc_port = 0x7F;
          if (JoyState & JST_UP)      adc_port = 0xFF;
          if (JoyState & JST_DOWN)    adc_port = 0x00;
          if ((JoyState&0xF) == JST_PURPLE) adc_port = 0xFF;
          if ((JoyState&0xF) == JST_BLUE)   adc_port = 0x00;
      }
  }

  return adc_port;    
}

u8 einstein_fire_read(void)
{
  u8 key_port = 0xFF;

  // If either the keyboard or joystick interrupt is fired, reset it here...
  if (keyboard_interrupt || joystick_interrupt)
  {
      CPU.IRequest=INT_NONE;
      keyboard_interrupt = 0;
      joystick_interrupt = 0;
  }

  // Fire buttons
  if (JoyState & JST_FIREL) key_port &= ~0x01;  // P1 Button 1
  if (JoyState & JST_FIRER) key_port &= ~0x02;  // P1 Button 2
   
  if (key_graph) key_port &= ~0x20;  // GRAPH KEY
  if (key_ctrl)  key_port &= ~0x40;  // CTRL KEY
  if (key_shift) key_port &= ~0x80;  // SHIFT KEY

  return key_port;
}

// ---------------------------------------------------------------------
// The Einstein IO map is broken into chunks of 8 ports that are 
// semi-related. The map looks like:
// 00h to 01h   - RESET
// 02h to 07h   - PSG (AY Sound Chip + PortA/PortB for keyboard)
// 08h to 0Fh   - VDP
// 10h to 17h   - PCI (not emulated)
// 18h to 1Fh   - FDC (not emulated)
// 20h          - /KBDINT_MSK
// 21h          - /ADCINT_MSK
// 22h          - /ALPHA
// 23h          - /DRSEL (Drive Select)
// 24h          - /ROM (select ROM vs RAM)
// 25h          - /FIREINT_MSK
// 26h to 27h   - Unused
// 28h to 2Fh   - CTC
// 30h to 37h   - PIO (not emulated)
// 38h to 3Fh   - ADC (partially emulated for Joystick Only)
// ---------------------------------------------------------------------
unsigned char cpu_readport_einstein(register unsigned short Port) 
{
    // Einstein ports are 8-bit
    Port &= 0xFF; 
    
    // ---------------------------------------------------------------
    // Ports are broken up into blocks of 8 bytes selected by A3-A7
    // ---------------------------------------------------------------
    switch (Port & 0xF8) 
    {
        case 0x00:  // PSG Area
            if (Port & 0x06)    // Is this port 2-6?
            {
                if (ay_reg_idx == 14) // Port A read is not connected
                {
                  return 0xFF;
                }
                else if (ay_reg_idx == 15) // Port B read is keyboard
                {
                  scan_keyboard();
                  return myKeyData;
                }            
                return FakeAY_ReadData();
            }
            else
            {
                memset(ay_reg, 0x00, 16);    // Clear the AY registers... Port 0 or 1
                fdc_reset(FALSE);            // Reset is passed along to the FDC
            }
            break;
            
        case 0x08:  // VDP Area
            if ((Port & 1)==0) return(RdData9918());
            return(RdCtrl9918());
            break;
            
        case 0x10:  // PCI Area - Not implemented
            return 0xFF;
            break;
            
        case 0x18:  // FDC Area - Not implemented
            return fdc_read(Port & 3);
            break;
            
        case 0x20:  // Key/Joy/ADC/ROM/RAM Select Area
            if      (Port == 0x20)  return einstein_fire_read();
            else if (Port == 0x23)  return drive_select;
            else if (Port == 0x24)  {einstein_swap_memory(); return 0xFF;}
            else if (Port == 0x25)  return joy_int_mask;
            break;
            
        case 0x28:  // Z80-CTC Area
            return CTC_Read(Port & 0x03);
            break;
            
        case 0x30:  // PIO Area - Not implemented
            return 0x00;
            break;
            
        case 0x38:  // ADC Area
            return einstein_joystick_read();
            break;
    }
    
    // No such port
    return(NORAM);
}


// ------------------------------------------------------------------------------------
// Tatung Einstein IO Port Write - Need to handle AY sound, VDP and the Z80-CTC chip
// ------------------------------------------------------------------------------------
void cpu_writeport_einstein(register unsigned short Port,register unsigned char Value) 
{
    // Einstein ports are 8-bit
    Port &= 0xFF;
    
    // ---------------------------------------------------------------
    // Ports are broken up into blocks of 8 bytes selected by A3-A7
    // ---------------------------------------------------------------
    switch (Port & 0xF8) 
    {
        case 0x00:  // PSG Area
            if (Port & 0x06)    // Is this port 2-6?
            {
                if (Port & 1)
                {
                    FakeAY_WriteData(Value);
                    if (ay_reg_idx == 14) 
                    {
                        keyboard_w = Value;
                        scan_keyboard();
                    }
                    else if (ay_reg_idx == 8)
                    {
                          extern u16 beeperFreq;
                          if (!Value) beeperFreq++;
                    }                   
                }
                else FakeAY_WriteIndex(Value & 0x0F);
            } 
            else
            {
                memset(ay_reg, 0x00, 16);    // Clear the AY registers for port 0/1
                fdc_reset(FALSE);            // Reset is passed along to the FDC
            }
            break;
            
        case 0x08:  // VDP Area
            if ((Port & 1) == 0) WrData9918(Value);
            else if (WrCtrl9918(Value)) CPU.IRequest=INT_NONE;
            break;
            
        case 0x10:  // PCI Area - Not implemented
            break;
            
        case 0x18:  // FDC Area - Not implemented
            fdc_write(Port & 3, Value);
            break;
            
        case 0x20:  // Key/Joy/ADC/ROM/RAM Select Area
            if      (Port == 0x20)  key_int_mask = Value;   // KEYBOARD INT MASK
            else if (Port == 0x21)  break;                  // ADC INT MASK (no game seems to use this so we don't implement)
            else if (Port == 0x22)  break;                  // ALPHA LOCK
            else if (Port == 0x23)  drive_select = Value & 0xF; // Drive Select
            else if (Port == 0x24)  einstein_swap_memory(); // ROM vs RAM bank port
            else if (Port == 0x25)  joy_int_mask = Value;   // JOYSTICK INT MASK
            break;
            
        case 0x28:  // Z80-CTC Area
            CTC_Write(Port & 0x03, Value);
            break;
            
        case 0x30:  // PIO Area - Not implemented
            break;
            
        case 0x38:  // ADC Area
            adc_mux = Value;
            break;
    }
}


// --------------------------------------------------------------------
// For the einstein, we handle keyboard and joystick fire interrupts.
// --------------------------------------------------------------------
void einstein_handle_interrupts(void)
{
  static u8 ein_key_dampen=0;
    
  if (++ein_key_dampen < 100) return;
  ein_key_dampen=0;
    
  if (CPU.IRequest == INT_NONE)
  {
      if (keyboard_interrupt != KEYBOARD_VECTOR)
      {
          if ((key_int_mask&1) == 0) // Bit 0 clear means enable interrupt handling
          {
            scan_keyboard();
            if (myKeyData != 0xFF)
            {
                keyboard_interrupt = KEYBOARD_VECTOR;
            }
          }
      }
      
      // If we haven't fired a keyboard ISR, check the joystick...
      if (keyboard_interrupt != KEYBOARD_VECTOR)
      {
          if ((joy_int_mask&1) == 0) // Bit 0 clear means enable interrupt handling
          {
            if (JoyState & (JST_FIREL|JST_FIRER) || key_ctrl || key_shift || key_graph)
            {
                joystick_interrupt = JOYSTICK_VECTOR;
            }
          }
      }
  }
}

// ---------------------------------------------------------------------------------------
// This is equivilent to the 'QUICKLOAD' option in MAME where we load up the .com file 
// directly into memory starting at offset 0x100 and jump to it. Not every game works
// this way but a fair number do... best we can do for now.
// ---------------------------------------------------------------------------------------
void einstein_load_com_file(void)
{
    // --------------------------------------
    // Ensure we are running in all-RAM mode
    // --------------------------------------
    einstein_ram_start = 0x0000;
    
    MemoryMap[0] = RAM_Memory + 0x0000;
    MemoryMap[1] = RAM_Memory + 0x2000;
    MemoryMap[2] = RAM_Memory + 0x4000;
    MemoryMap[3] = RAM_Memory + 0x6000;
    MemoryMap[4] = RAM_Memory + 0x8000;
    MemoryMap[5] = RAM_Memory + 0xA000;
    MemoryMap[6] = RAM_Memory + 0xC000;
    MemoryMap[7] = RAM_Memory + 0xE000;
    
    // The Quickload will write the .COM file into memory at offset 0x100 and jump to it
    memcpy(RAM_Memory+0x100, ROM_Memory, tape_len);
    keyboard_interrupt=0;
    CPU.IRequest=INT_NONE;
    CPU.IFF&=~(IFF_1|IFF_EI);
    CPU.PC.W = 0x100;
    JumpZ80(CPU.PC.W);
}

struct SectorInfo_t
{
    u8 track;
    u8 side;
    u8 sectorID;
    u8 sectorSize;
    u8 FDC_Status1;
    u8 FDC_STatus2;
    u8 unused1;
    u8 unused2;
};

struct TrackInfo_t
{
    u8 trackName[12];
    u8 unused1[4];
    u8 trackNum;
    u8 sideNum;
    u8 unused2[2];
    u8 sectorSize;
    u8 numSectors;
    u8 gapLen;
    u8 filler;
    struct SectorInfo_t SectorInfo[10];
} TrackInfo;

// ---------------------------------------------------------
// The Einstein has CTC plus some memory handling stuff
// ---------------------------------------------------------
void einstein_reset(void)
{
    if (einstein_mode)
    {
        // Reset the Z80-CTC stuff...
        CTC_Init(CTC_CHAN_MAX);      // Einstein does not use CTC for VDP
        
        einstein_ram_start = 0x8000;
        keyboard_w = 0x00;
        myKeyData = 0xFF;
        keyboard_interrupt=0;
        key_int_mask = 0xFF;
        
        memset(ay_reg, 0x00, 16);    // Clear the AY registers...
        fdc_reset(TRUE);
        einstein_restore_bios();
        
        IssueCtrlBreak = 0;
        
        io_show_status = 0;
        
        // ---------------------------------------------------------------------
        // If this is an Einstein .DSK file we need to re-assemble the sectors
        // into some logical pattern so we can more easily fetch and return 
        // the data when asked for by the CPU.  The Einstein disk is laid out
        // in a somewhat unusual manner probably to help speed up access. But
        // we want tracks and sectors laid out sequentially.
        // ---------------------------------------------------------------------
        if (einstein_mode == 2) 
        {
            u8 *trackInfoPtr = ROM_Memory + 0x100;
            for (int i=0; i<40; i++)    // 40 tracks
            {
                trackInfoPtr = ROM_Memory + (0x100 + (i*(5120+0x100)));
                memcpy(&TrackInfo, trackInfoPtr, sizeof(TrackInfo));  // Get the Track Info into our buffer...
                trackInfoPtr += 0x100;                                // Skip over the TrackInfo
                
                for (int j=0; j<10; j++) // 10 sectors per track
                {
                    u8 *dest = &ROM_Memory[(512*1024) + (TrackInfo.SectorInfo[j].track * 5120) + (TrackInfo.SectorInfo[j].sectorID*512)];
                    u8 *src  = trackInfoPtr + (j*512);
                    for (int k=0; k<512; k++) // 512 bytes per sector
                    {
                        *dest++ = *src++;
                    }
                }
            }
        }
    }
}



/*********************************************************************************
 * A few ZX Speccy ports utilize the beeper to "simulate" the sound... For 
   the Einstein, this appears to be a rapid hit (writes) on PSG register 8
   and we handle that in the IO write routine above. Not great but it will 
   render enough sounds to make the few speccy ports playable.
 ********************************************************************************/
extern u16 beeperFreq;
extern u8 msx_beeper_process;
extern u8 beeperWasOn;
void einstein_HandleBeeper(void)
{
    if (++msx_beeper_process & 1)
    {
      if (beeperFreq > 0)
      {
          BeeperON(30 * beeperFreq); // Frequency in Hz
          beeperFreq = 0;            // Gather new Beeper freq
          beeperWasOn=1;
      } else {if (beeperWasOn) {BeeperOFF(); beeperWasOn=0;}}
    }
}

// End of file

