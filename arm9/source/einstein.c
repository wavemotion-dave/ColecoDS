// =====================================================================================
// Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
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
#include "fdc.h"
#include "cpu/z80/Z80_interface.h"
#include "cpu/z80/ctc.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "printf.h"

u16 einstein_ram_start __attribute__((section(".dtcm"))) = 0x8000;
u16 keyboard_interrupt __attribute__((section(".dtcm"))) = 0;
u16 joystick_interrupt __attribute__((section(".dtcm"))) = 0;
u8 keyboard_w        = 0x00;
u8 key_int_mask      = 0xFF;
u8 joy_int_mask      = 0xFF;
u8 myKeyData         = 0xFF;
u8 adc_mux           = 0x00;
u8 ein_alpha_lock    = 0x01;

extern u8 EinsteinBios[];
extern u8 EinsteinBios2[];

#define KEYBOARD_VECTOR  0xF7
#define JOYSTICK_VECTOR  0xFD

static u8 last_drive_select = 0x00;

// ---------------------------------------------------------------------------------------------
// We support two disk drives - Drive 0 is the normal floppy that can be mounted with Einstein 
// formatted .dsk files and Drive 1 is a RAMDisk that can be used to house common utilities.
// ---------------------------------------------------------------------------------------------
void einstein_drive_sel(u8 sel)
{
    if      (sel & 0x01) fdc_setDrive(0);
    else if (sel & 0x02) fdc_setDrive(1);
    else                 fdc_setDrive(2); // Doesn't exist... IO will fail
    
    if (FDC.drive != last_drive_select)
    {
        last_drive_select = FDC.drive;
        if (FDC.drive < 2)
        {
            memcpy(FDC.track_buffer, &ROM_Memory[((FDC.drive ? 768:512)*1024) + (FDC.track * 5120)], 5120); // Get the track into our buffer
            FDC.track_buffer_idx = FDC.sector*512;
            FDC.track_buffer_end = FDC.track_buffer_idx+512;
            FDC.sector_byte_counter = 0;
        }
        FDC.status = 0x80;      // Motor off and not busy
    }
}

// --------------------------------------------------------------------------------
// Scan the Einstein Keyboard and report back any keys that have been pressed
// --------------------------------------------------------------------------------
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
                  if (myConfig.overlay == 1) // Full Keyboard Selected
                  {
                      DSPrint(1,12,0, " ");     // Clear CTRL  indicator
                      DSPrint(1,15,0, " ");     // Clear SHIFT indicator
                      DSPrint(2,23,0, " ");     // Clear GRAPH indicator
                  }
              }
          }

          if (last_special_key == KBD_KEY_SHIFT) 
          { 
            if (myConfig.overlay == 1) DSPrint(1,15,2, "\\");  // Display round dot under SHIFT
            key_shift = 1;
          }
          else if (last_special_key == KBD_KEY_CTRL)  
          {
            if (myConfig.overlay == 1) DSPrint(1,12,2, "@");  // Display round dot under CTRL
            key_ctrl = 1;
          }
          else if (last_special_key == KBD_KEY_GRAPH)  
          {
            if (myConfig.overlay == 1) DSPrint(2,23,0, "@");  // Display round dot under GRAPH
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
              if (kbd_key == KBD_KEY_BREAK) myKeyData |= 0x01;
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
    memcpy(BIOS_Memory+0x4000, EinsteinBios2, 0x2000);
    
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

// ---------------------------------------------------------------------------------------
// Read the Einstein Joystick. This is reported back on the ADC port as an analog signal
// however it's really just a digital joystick that will report min/max ADC values
// when the user presses UP/DOWN or LEFT/RIGHT.
// ---------------------------------------------------------------------------------------
u8 einstein_joystick_read(void)
{
  u8 adc_port = 0x7F;   // Center Position

  if ((adc_mux & 0x02)) // Player 2 Joystick (not connected)
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
      u32 joy = JoyState;
      if (myConfig.dpad == DPAD_DIAGONALS) // Useful for games like QOGO2
      {
          if      (JoyState & JST_UP)    joy = (JST_UP    | JST_RIGHT);
          else if (JoyState & JST_DOWN)  joy = (JST_DOWN  | JST_LEFT);
          else if (JoyState & JST_LEFT)  joy = (JST_LEFT  | JST_UP);
          else if (JoyState & JST_RIGHT) joy = (JST_RIGHT | JST_DOWN);
      }

      // -------------------------------------------------------------------
      // On the Einstein, the Left/Right and Up/Down joystick are read on
      // the ADC port given that Left/Right can't be pressed at the same
      // time nor can Up/Down... so they represent the extremes of the ADC.
      // -------------------------------------------------------------------
      if ((adc_mux & 5) == 4) 
      {
          adc_port = 0x7F;
          if (joy & JST_RIGHT) adc_port = 0xFF;
          if (joy & JST_LEFT)  adc_port = 0x00;
      }
      else if ((adc_mux & 5) == 5) 
      {
          adc_port = 0x7F;
          if (joy & JST_UP)      adc_port = 0xFF;
          if (joy & JST_DOWN)    adc_port = 0x00;
      }
  }

  return adc_port;    
}

// ---------------------------------------------------------------------------------------
// The fire button readout is also the same port used to read the special keyboard 
// keys such as GRAPH, CTRL and SHIFT.
// ---------------------------------------------------------------------------------------
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

  // These three special keys are fed back via the joystick interrupt 
  // probably so there would be no key ghosting with the actual keyboard.
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
// 22h          - /ALPHA (LED indicator)
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
                if (myAY.ayRegIndex == 14) // Port A read is not connected
                {
                  return 0xFF;
                }
                else if (myAY.ayRegIndex == 15) // Port B read is keyboard
                {
                  scan_keyboard();
                  return myKeyData;
                }            
                return ay38910DataR(&myAY);
            }
            else
            {
                myAY.ayRegIndex = 0;
                memset(myAY.ayRegs, 0x00, sizeof(myAY.ayRegs));    // Clear the AY registers... Port 0 or 1
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
            
        case 0x18:  // FDC Area
            return fdc_read(Port & 3);
            break;
            
        case 0x20:  // Key/Joy/ADC/ROM/RAM Select Area
            if      (Port == 0x20)  return einstein_fire_read();
            else if (Port == 0x22)  {ein_alpha_lock ^= 1; return 0xFF;}
            else if (Port == 0x23)  return (1 << FDC.drive);
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
                    ay38910DataW(Value, &myAY);
                    if (myAY.ayRegIndex == 14) 
                    {
                        keyboard_w = Value;
                        scan_keyboard();
                    }
                    else if (myAY.ayRegIndex == 8)
                    {
                          extern u16 beeperFreq;
                          if (!Value) beeperFreq++;
                    }                   
                }
                else 
                {
                    ay38910IndexW(Value&0xF, &myAY);
                }
            } 
            else
            {
                myAY.ayRegIndex = 0;
                memset(myAY.ayRegs, 0x00, sizeof(myAY.ayRegs));    // Clear the AY registers for port 0/1
                fdc_reset(FALSE);            // Reset is passed along to the FDC
            }
            break;
            
        case 0x08:  // VDP Area
            if ((Port & 1) == 0) WrData9918(Value);
            else WrCtrl9918(Value); // Einstein does not produce VDP interrupts
            break;
            
        case 0x10:  // PCI Area - Not implemented
            break;
            
        case 0x18:  // FDC Area
            fdc_write(Port & 3, Value);
            break;
            
        case 0x20:  // Key/Joy/ADC/ROM/RAM Select Area
            if      (Port == 0x20)  key_int_mask = Value;       // KEYBOARD INT MASK
            else if (Port == 0x21)  break;                      // ADC INT MASK (no game seems to use this so we don't implement)
            else if (Port == 0x22)  break;                      // ALPHA LOCK - nobody seems to write this
            else if (Port == 0x23)  einstein_drive_sel(Value);  // Drive Select
            else if (Port == 0x24)  einstein_swap_memory();     // ROM vs RAM bank port
            else if (Port == 0x25)  joy_int_mask = Value;       // JOYSTICK INT MASK
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
    memcpy(RAM_Memory+0x100, ROM_Memory, disk_last_size[0]);
    keyboard_interrupt=0;
    CPU.IRequest=INT_NONE;
    CPU.IFF&=~(IFF_1|IFF_EI);
    CPU.PC.W = 0x100;
    JumpZ80(CPU.PC.W);
}


// --------------------------------------------------------------------------------------------------------------
// This is used for CPC EXTENDED .dsk images which are the kind found for Tatung Einstein use.
// ColecoDS only supports the standard 200K single sided, 40 track Einstein formatted disks
// (actual size 215,296 bytes). Most of the disk images found in the wild will be in this format.
//
// For Einstein disk images, we are breaking up the 1MB ROM_Memory into four (4) chunks as follows:
//      ROM_Memory+0K    =  Loaded .dsk image in natural Einstein format
//      ROM_Memory+256K  =  Loaded RAMDisk image in natural Einstein format
//      ROM_Memory+512K  =  Loaded .dsk image converted to raw sector format for use with the FDC Controller
//      ROM_Memory+768K  =  Loaded RAMDisk image converted to raw sector format for use with the FDC Controller
// --------------------------------------------------------------------------------------------------------------
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


// ---------------------------------------------------------------------
// If this is an Einstein .DSK file we need to re-assemble the sectors
// into some logical pattern so we can more easily fetch and return 
// the data when asked for by the CPU.  The Einstein disk is laid out
// in a somewhat unusual manner probably to help speed up access. But
// we want tracks and sectors laid out sequentially.
// ---------------------------------------------------------------------
void einstein_load_disk(u8 disk)
{
    getcwd(disk_last_path[disk], MAX_ROM_NAME);   // Save the path
    
    // -------------------------------------------------------------------------------------------
    // First read in the raw .DSK file into memory so we can manipulate the tracks/sectors below
    // -------------------------------------------------------------------------------------------
    FILE *fp = fopen(disk_last_file[disk], "rb");
    
    if ((fp == NULL) && (disk==1)) // If we didn't find one and we are disk 1, we can use a blank RAMDISK
    {
        einstein_init_ramdisk();
        fp = fopen(disk_last_file[disk], "rb");
    }
    
    // --------------------------------------------------------------------
    // We assemble disk 0 vs disk 1 in different parts of our ROM_Memory[]
    // --------------------------------------------------------------------
    int offset = disk ? 256:0;
    
    if (fp != NULL)
    {
        disk_last_size[disk] = fread(ROM_Memory + (offset*1024), 1, 256*1024, fp);    // Read file into memory
        fclose(fp);
    } else disk_last_size[disk] = 0;
    
    u8 *trackInfoPtr = ROM_Memory + 0x100;
    for (int i=0; i<40; i++)    // 40 tracks
    {
        trackInfoPtr = (ROM_Memory + (offset*1024)) + (0x100 + (i*(5120+0x100)));
        memcpy(&TrackInfo, trackInfoPtr, sizeof(TrackInfo));  // Get the Track Info into our buffer...
        trackInfoPtr += 0x100;                                // Skip over the TrackInfo

        for (int j=0; j<10; j++) // 10 sectors per track
        {
            u8 *dest = &ROM_Memory[((512+offset)*1024) + (TrackInfo.SectorInfo[j].track * 5120) + (TrackInfo.SectorInfo[j].sectorID*512)];
            u8 *src  = trackInfoPtr + (j*512);
            for (int k=0; k<512; k++) // 512 bytes per sector
            {
                *dest++ = *src++;
            }
        }
    }
}

void einstein_save_disk(u8 disk)
{
    chdir(disk_last_path[disk]);    // Change back to the right directory
    
    // --------------------------------------------------------------------
    // We assemble disk 0 vs disk 1 in different parts of our ROM_Memory[]
    // --------------------------------------------------------------------
    int offset = disk ? 256:0;
    
    u8 *trackInfoPtr = ROM_Memory + (offset * 1024) + 0x100;
    for (int i=0; i<40; i++)    // 40 tracks
    {
        trackInfoPtr = (ROM_Memory + (offset * 1024)) + (0x100 + (i*(5120+0x100)));
        memcpy(&TrackInfo, trackInfoPtr, sizeof(TrackInfo));  // Get the Track Info into our buffer...
        trackInfoPtr += 0x100;                                // Skip over the TrackInfo

        for (int j=0; j<10; j++) // 10 sectors per track
        {
            u8 *src = &ROM_Memory[((512+offset)*1024) + (TrackInfo.SectorInfo[j].track * 5120) + (TrackInfo.SectorInfo[j].sectorID*512)];
            u8 *dest  = trackInfoPtr + (j*512);
            for (int k=0; k<512; k++) // 512 bytes per sector
            {
                *dest++ = *src++;
            }
        }
    }
    
    FILE *fp = fopen(disk_last_file[disk], "wb");
    if (fp != NULL)
    {
        fwrite(ROM_Memory + (offset * 1024), 1, disk_last_size[disk], fp);
        fclose(fp);
    }
    
    disk_unsaved_data[disk] = 0;
}

// ---------------------------------------------------------------------------
// We allow swapping of DISK0 - as the RAMDisk is always present as drive 1:
// ---------------------------------------------------------------------------
void einstein_swap_disk(u8 disk, char *szFileName)
{
    strcpy(disk_last_file[disk], szFileName);
    
    einstein_load_disk(disk);           // Get disk into memory and decode the tracks/sectors
    disk_unsaved_data[disk] = 0;        // Fresh install of disk has no unsaved data
    last_drive_select = 0x00;           // Force us to re-read the first track
    einstein_drive_sel(FDC.drive);      // Read the track into memory
}


void einstein_install_ramdisk(void)
{
    strcpy(disk_last_file[1], "/data/einstein.ramd");
    einstein_load_disk(1);
}

// -----------------------------------------------------------------------------------------------------------
// Our RAMDisk is a standard 200K floppy with no data - just a disk full of 0xE5 bytes ready to be loaded.
// This is useful to keep a copy or two of various XBAS versions and maybe the COPY XTal DOS utility.
// -----------------------------------------------------------------------------------------------------------
unsigned char RAMDisk_Header[] = {
  0x45, 0x58, 0x54, 0x45, 0x4e, 0x44, 0x45, 0x44,
  0x20, 0x43, 0x50, 0x43, 0x20, 0x44, 0x53, 0x4b,
  0x20, 0x46, 0x69, 0x6c, 0x65, 0x0d, 0x0a, 0x44,
  0x69, 0x73, 0x6b, 0x2d, 0x49, 0x6e, 0x66, 0x6f,
  0x0d, 0x0a, 0x43, 0x6f, 0x6c, 0x65, 0x63, 0x6f,
  0x44, 0x53, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00,
  0x28, 0x01, 0x00, 0x00, 0x15, 0x15, 0x15, 0x15,
  0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
  0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
  0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
  0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15, 0x15,
  0x15, 0x15, 0x15, 0x15, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char RAMDisk_TrackInfo[] = {
  0x54, 0x72, 0x61, 0x63, 0x6b, 0x2d, 0x49, 0x6e,
  0x66, 0x6f, 0x0d, 0x0a, 0x00, 0x00, 0x00, 0x00,
  0x27, 0x00, 0x00, 0x00, 0x02, 0x0a, 0x4e, 0xe5,
  0x27, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x02, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x03, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x04, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x06, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x07, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x08, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x27, 0x00, 0x09, 0x02, 0x00, 0x00, 0x00, 0x02,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

void einstein_init_ramdisk(void)
{
    strcpy(disk_last_file[1], "/data/einstein.ramd");
    FILE *fp = fopen(disk_last_file[1], "wb+");
    fwrite(RAMDisk_Header, 1, sizeof(RAMDisk_Header), fp);
    for (u8 track=0; track<40; track++)
    {
        RAMDisk_TrackInfo[0x10] = track;
        RAMDisk_TrackInfo[0x18] = track;
        RAMDisk_TrackInfo[0x20] = track;
        RAMDisk_TrackInfo[0x28] = track;
        RAMDisk_TrackInfo[0x30] = track;
        RAMDisk_TrackInfo[0x38] = track;
        RAMDisk_TrackInfo[0x40] = track;
        RAMDisk_TrackInfo[0x48] = track;
        RAMDisk_TrackInfo[0x50] = track;
        RAMDisk_TrackInfo[0x58] = track;
        RAMDisk_TrackInfo[0x60] = track;

        RAMDisk_TrackInfo[0x18+2] = 0x00;
        RAMDisk_TrackInfo[0x20+2] = 0x01;
        RAMDisk_TrackInfo[0x28+2] = 0x02;
        RAMDisk_TrackInfo[0x30+2] = 0x03;
        RAMDisk_TrackInfo[0x38+2] = 0x04;
        RAMDisk_TrackInfo[0x40+2] = 0x05;
        RAMDisk_TrackInfo[0x48+2] = 0x06;
        RAMDisk_TrackInfo[0x50+2] = 0x07;
        RAMDisk_TrackInfo[0x58+2] = 0x08;
        RAMDisk_TrackInfo[0x60+2] = 0x09;

        fwrite(RAMDisk_TrackInfo, 1, sizeof(RAMDisk_TrackInfo), fp);
        memset(FDC.track_buffer, 0xE5, 5120);
        fwrite(FDC.track_buffer, 1, 5120, fp);
    }       
    fclose(fp);
}

// -----------------------------------------------------------
// The Einstein has CTC, FDC plus some memory handling stuff
// -----------------------------------------------------------
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
        
        myAY.ayRegIndex = 0;
        memset(myAY.ayRegs, 0x00, sizeof(myAY.ayRegs));    // Clear the AY registers...
        
        fdc_reset(TRUE);            // Reset the Floppy Controller
        
        einstein_restore_bios();    // And restore the Einstein BIOS
        
        IssueCtrlBreak = 0;         // CTRL-Break is how the Einstein boots to floppy
        
        io_show_status = 0;         // No disk activity to start
        
        ein_alpha_lock = 0x01;      // CAPS lock is enabled by default
        
        if (einstein_mode == 2) // Are we loading a .dsk file (fairly common)
        {
            // The two disk drive paths so we can write-back changes
            strcpy(disk_last_file[0], gpFic[ucGameChoice].szName);
            
            // --------------------------------------------------------
            // Setup two (2) disk drives for the Einstein. By default, 
            // our persistant RAMDISK will be the second drive.
            // --------------------------------------------------------
            fdc_init(WD1770, 2, 1, 40, 10, 512, 0, ROM_Memory+(512*1024), ROM_Memory+(768*1024));
            
            last_drive_select = 0x00;   // Ensure we buffer the first track
            einstein_drive_sel(0x01);   // Default is the first drive
            fdc_setSide(0);             // And side 0 of that disk
            einstein_load_disk(0);      // Disk 0 : assemble the sectors of this disk for easy manipulation
            einstein_install_ramdisk(); // Disk 1 : assemble the sectors of this disk for easy manipulation
        }
        disk_unsaved_data[0] = 0;
        disk_unsaved_data[1] = 0;
    }
}


// ------------------------------------------------------------------------
// A few ZX Speccy ports utilize the beeper to "simulate" the sound... 
// For the Einstein, this appears to be a rapid hit (writes) on PSG 
// register 8 and we handle that in the IO write routine above. Not great
// but it will render enough sounds to make the few speccy ports playable.
// ------------------------------------------------------------------------
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

