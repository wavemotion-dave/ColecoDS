// =====================================================================================
// Copyright (c) 2021-2025 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty. Please see readme.md
//
// This file is our bridge between the Z80 CPU core and the rest of the system.
// ColecoDS currently supports the CZ80 CPU core.
// =====================================================================================
#include <nds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Z80_interface.h"
#include "../../colecoDS.h"
#include "../../colecomngt.h"
#include "../../colecogeneric.h"
#include "../../Adam.h"
#include "../../C24XX.h"
#include "../../printf.h"
#include "../scc/SCC.h"

u8  last_mega_bank     __attribute__((section(".dtcm"))) = 199;
u8  msx_sram_at_8000   __attribute__((section(".dtcm"))) = 0;
u8  msx_scc_enable     __attribute__((section(".dtcm"))) = 0;
u8  msx_last_block[4]  __attribute__((section(".dtcm"))) = {99,99,99,99};

// ---------------------------------------------------------------
// Switch banks... do this as fast as possible by switching only
// the memory map and not trying to copy blocks of memory around.
// ---------------------------------------------------------------
ITCM_CODE void MegaCartBankSwitch(u8 bank)
{
    if (last_mega_bank != bank)   // Only if the bank was changed...
    {
        MemoryMap[6] = ROM_Memory + ((u32)bank * (u32)0x4000);
        MemoryMap[7] = MemoryMap[6] + 0x2000;
        last_mega_bank = bank;
    }
}


// ------------------------------------------------------------------------
// For the DS-Lite/Phat, we use a super simplified coleco driver that only
// handles the RAM_Memory[] as a flat 64K address space. To make this work
// for banked games, we must actually swap in the memory by memcpy(). Not
// the fastest, but fortunately many Megacarts do very little swapping...
// ------------------------------------------------------------------------
ITCM_CODE void MegaCartBankSwap(u8 bank)
{
    bank &= romBankMask;    
    if (last_mega_bank != bank)   // Only if the bank was changed...
    {
        if (bMagicMegaCart)
        {
            MemoryMap[6] = ROM_Memory + ((u32)bank * (u32)0x4000);
            MemoryMap[7] = MemoryMap[6] + 0x2000;
            if (bank < 16) // First 256K of the ROM is in shadow VRAM for speed
            {
                //memcpy(RAM_Memory + 0xC000, ((u8*)0x06860000) + ((u32)bank * (u32)0x4000), 0x4000);
                u32 *src = (u32 *) (((u8*)0x06860000) + ((u32)bank * (u32)0x4000));
                u32 *dest = (u32*)(RAM_Memory + 0xC000);
                for (int i=0; i < 4096/8; i++)
                {
                    *dest++ = *src++; *dest++ = *src++; *dest++ = *src++; *dest++ = *src++;
                    *dest++ = *src++; *dest++ = *src++; *dest++ = *src++; *dest++ = *src++;
                }
                
            }
            else
                memcpy(RAM_Memory + 0xC000, ROM_Memory + ((u32)bank * (u32)0x4000), 0x4000);
            last_mega_bank = bank;
        }
    }
}

// ----------------------------------------------------------------------
// Here we might have a very large ROM and we don't expect much swapping
// so we will just re-read the file and pull in the correct 32K bank...
// ----------------------------------------------------------------------
void Mega31in1BankSwitch(u8 bank)
{
    FILE* handle = fopen(disk_last_file[0], "rb");
    if (handle != NULL) 
    {
        fseek(handle, (0x8000 * (u32)bank), SEEK_SET);          // Seek to the 32K chunk we want to read in
        fread((void*) RAM_Memory+0x8000, 0x8000, 1, handle);    // Read 32K from that paged block
        fclose(handle);
    }
}

// ----------------------------------------------------------------
// 8-bit read with bankswitch support... slower, so we only use it 
// for 'complicated' memory fetches. Otherwise a more direct read 
// of memory is implemented in the Z80.c file.
// ----------------------------------------------------------------
ITCM_CODE u8 cpu_readmem16_banked(u16 address) 
{
    if (pv2000_mode) return cpu_readmem_pv2000(address);
    
    // Everything in this block is accessing high-memory...
    if (address & 0x8000)
    {
      if (bMagicMegaCart) // Handle Megacart Hot Spots
      {
          if (address >= 0xFFC0)
          {
              MegaCartBankSwitch(address & romBankMask);
          }
      }
      else if (adam_mode)   // See if this is a read from the PCB area
      {
          if (PCBTable[address]) ReadPCB(address);
      }
      else if (bActivisionPCB) // Is this an Activision style PCB with EEPROM?
      {
          if (address==0xFF80)
          {
              return(Read24XX(&EEPROM));  // Return EEPROM output bit
          }
      }
#if 0  // For now, reading a SGC is just a normal memory fetch as handled further below...      
      else if (bSuperGameCart)
      {
          // Handle Super Game Cart
          return SuperGameCartRead(address);                
      }
#endif      
      else if (b31_in_1) // Handle 31-in-1 Hot Spots
      {
          if (address >= 0xFFC0)
          {
              Mega31in1BankSwitch(address & romBankMask);
          }
      }
      else if (msx_sram_at_8000) // Don't need to check msx_mode as this can only be true in that mode
      {
          if (address <= 0xBFFF) // Between 0x8000 and 0xBFFF
          {
              return SRAM_Memory[address&0x3FFF];
          }
      }
    }
    
    // Otherwise normal read - just index into the 8K memory block and fetch the byte...
    return *(MemoryMap[address>>13] + (address&0x1FFF));
}


// -----------------------------------------------------------------------
// Zemina 8K mapper:
//Page (8kB)    Switching address   Initial segment
//4000h~5FFFh (mirror: C000h~DFFFh) 4000h (mirrors: 4001h~5FFFh)    0
//6000h~7FFFh (mirror: E000h~FFFFh) 6000h (mirrors: 6001h~7FFFh)    1
//8000h~9FFFh (mirror: 0000h~1FFFh) 8000h (mirrors: 8001h~9FFFh)    2
//A000h~BFFFh (mirror: 2000h~3FFFh) A000h (mirrors: A001h~BFFFh)    3
// -----------------------------------------------------------------------
void HandleZemina8K(u32* src, u8 block, u16 address)
{
    if (bROMInSegment[1] && (address >= 0x4000) && (address < 0x6000))
    {
        if (msx_last_block[0] != block)
        {
            MSXCartPtr[2] = (u8*)src;  // Main ROM
            MSXCartPtr[6] = (u8*)src;  // Mirror
            MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
            msx_last_block[0] = block;
        }
    }
    else if (bROMInSegment[1] && (address >= 0x6000) && (address < 0x8000))
    {
        if (msx_last_block[1] != block)
        {
            MSXCartPtr[3] = (u8*)src;  // Main ROM
            MSXCartPtr[7] = (u8*)src;  // Mirror
            MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
            msx_last_block[1] = block;
        }
    }
    else if (bROMInSegment[2] && (address >= 0x8000) && (address < 0xA000))
    {
        if (msx_last_block[2] != block)
        {
            MSXCartPtr[4] = (u8*)src;  // Main ROM
            MSXCartPtr[0] = (u8*)src;  // Mirror                            
            MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
            msx_last_block[2] = block;
        }
    }
    else if (bROMInSegment[2] && (address >= 0xA000) && (address < 0xC000))
    {
        if (msx_last_block[3] != block)
        {
            MSXCartPtr[5] = (u8*)src;  // Main ROM
            MSXCartPtr[1] = (u8*)src;  // Mirror                            
            MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
            msx_last_block[3] = block;
        }
    }
}    

// -------------------------------------------------------------------------
// The ZENMIA 16K Mapper:
// 4000h~7FFFh  via writes to 4000h-7FFF
// 8000h~BFFFh  via writes to 8000h-BFFF
// -------------------------------------------------------------------------
void HandleZemina16K(u32* src, u8 block, u16 address)
{
    if (bROMInSegment[1] && (address >= 0x4000) && (address < 0x8000))
    {
        if (msx_last_block[0] != block)
        {
            MSXCartPtr[2] = (u8*)src;
            MSXCartPtr[3] = (u8*)src+0x2000;
            MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
            MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
            // Mirrors
            MSXCartPtr[6] = (u8*)src;
            MSXCartPtr[7] = (u8*)src+0x2000;
            if (bROMInSegment[3]) 
            {
                MemoryMap[6] = (u8 *)(MSXCartPtr[6]);
                MemoryMap[7] = (u8 *)(MSXCartPtr[7]);
            }
            msx_last_block[0] = block;
        }
    }
    else if (bROMInSegment[1] && (address >= 0x8000) && (address < 0xC000))
    {
        if (msx_last_block[1] != block)
        {
            MSXCartPtr[4] = (u8*)src;
            MSXCartPtr[5] = (u8*)src+0x2000;
            // Mirrors
            MSXCartPtr[0] = (u8*)src;
            MSXCartPtr[1] = (u8*)src+0x2000;
            if (bROMInSegment[2])
            {
                MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
                MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
            }
            if (bROMInSegment[0]) 
            {
                MemoryMap[0] = (u8 *)(MSXCartPtr[0]);
                MemoryMap[1] = (u8 *)(MSXCartPtr[1]);
            }            
            msx_last_block[1] = block;
        }
    }
}    
    
ITCM_CODE void HandleKonamiSCC8(u32* src, u8 block, u16 address, u8 value)
{
    // --------------------------------------------------------
    // Konami 8K mapper with SCC 
    //  Bank 1: 4000h - 5FFFh - mapped via writes to 5000h
    //  Bank 2: 6000h - 7FFFh - mapped via writes to 7000h
    //  Bank 3: 8000h - 9FFFh - mapped via writes to 9000h
    //  Bank 4: A000h - BFFFh - mapped via writes to B000h
    // --------------------------------------------------------
    if (bROMInSegment[1] && (address == 0x5000))
    {
        if (msx_last_block[0] != block)
        {
            MSXCartPtr[2] = (u8*)src;  // Main ROM
            MSXCartPtr[6] = (u8*)src;  // Mirror
            MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
            msx_last_block[0] = block;
        }
    }
    else if (bROMInSegment[1] && (address == 0x7000))
    {
        if (msx_last_block[1] != block)
        {
            MSXCartPtr[3] = (u8*)src;  // Main ROM
            MSXCartPtr[7] = (u8*)src;  // Mirror
            MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
            msx_last_block[1] = block;
        }
    }
    else if (bROMInSegment[2] && (address == 0x9000))
    {
        if ((value&0x3F) == 0x3F) {msx_scc_enable=true; return;}       // SCC sound - set a flag so we process this special sound chip

        if (msx_last_block[2] != block)
        {
            MSXCartPtr[4] = (u8*)src;  // Main ROM
            MSXCartPtr[0] = (u8*)src;  // Mirror
            MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
            msx_last_block[2] = block;
        }
    }
    else if (bROMInSegment[2] && (address == 0xB000))
    {
        if (msx_last_block[3] != block)
        {
            MSXCartPtr[5] = (u8*)src;  // Main ROM
            MSXCartPtr[1] = (u8*)src;  // Mirror
            MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
            msx_last_block[3] = block;
        }
    }
}


// -------------------------------------------------------------------------
// The ASCII 16K Mapper:
// 4000h~7FFFh  via writes to 6000h
// 8000h~BFFFh  via writes to 7000h or 77FFh
// -------------------------------------------------------------------------
ITCM_CODE void HandleAscii16K(u32* src, u8 block, u16 address)
{
    if (bROMInSegment[1] && (address >= 0x6000) && (address < 0x7000))
    {
        if (msx_last_block[0] != block)
        {
            MSXCartPtr[2] = (u8*)src;
            MSXCartPtr[3] = (u8*)src+0x2000;
            MemoryMap[2] = MSXCartPtr[2];
            MemoryMap[3] = MSXCartPtr[3];
            // Mirrors
            MSXCartPtr[6] = (u8*)src;
            MSXCartPtr[7] = (u8*)src+0x2000;
            if (bROMInSegment[3]) 
            {
                MemoryMap[6] = MSXCartPtr[6];
                MemoryMap[7] = MSXCartPtr[7];
            }
            msx_last_block[0] = block;
        }
    }
    else if (bROMInSegment[1] && (address >= 0x7000) && (address < 0x8000))
    {
        if ((file_crc == 0xfea70207) && (address != 0x7000)) return;  // Vaxol writes garbage to 7xxx so we ignore that
        
        if (msx_last_block[1] != block)
        {
            // ---------------------------------------------------------------------------------------------------------
            // Check if we have an SRAM capable game - those games (e.g. Hydlide II) use the block at 0x8000 for SRAM.
            // In theory this 2K or 8K of SRAM is mirrored but we don't worry about it - just allow writes.
            // ---------------------------------------------------------------------------------------------------------
            if (msx_sram_enabled && (block == msx_sram_enabled))
            {
                msx_sram_at_8000 = true;
            }
            else
            {
                msx_sram_at_8000 = false;
                MSXCartPtr[4] = (u8*)src;
                MSXCartPtr[5] = (u8*)src+0x2000;
                // Mirrors
                MSXCartPtr[0] = (u8*)src;
                MSXCartPtr[1] = (u8*)src+0x2000;
                if (bROMInSegment[2]) 
                {
                    MemoryMap[4] = MSXCartPtr[4];
                    MemoryMap[5] = MSXCartPtr[5];
                }
                if (bROMInSegment[0])
                {
                    MemoryMap[0] = MSXCartPtr[0];
                    MemoryMap[1] = MSXCartPtr[1];
                }
            }
            msx_last_block[1] = block;
        }
    }
}

void activision_pcb_write(u16 address)
{
  if ((address == 0xFF90) || (address == 0xFFA0) || (address == 0xFFB0))
  {
      MegaCartBankSwitch((address>>4) & romBankMask);
  }
  else
  {
      if (address == 0xFFC0) Write24XX(&EEPROM,EEPROM.Pins&~C24XX_SCL);
      if (address == 0xFFD0) Write24XX(&EEPROM,EEPROM.Pins|C24XX_SCL);
      if (address == 0xFFE0) Write24XX(&EEPROM,EEPROM.Pins&~C24XX_SDA);
      if (address == 0xFFF0) Write24XX(&EEPROM,EEPROM.Pins|C24XX_SDA);
  }
}

// ------------------------------------------------------------------
// Write memory handles both normal writes and bankswitched since
// write is much less common than reads...   We handle the MSX
// Konami 8K, SCC and ASCII 8K mappers directly here for max speed.
// ------------------------------------------------------------------
ITCM_CODE void cpu_writemem16(u8 value,u16 address) 
{
    // machine_mode will be non-zero for anything except the ColecoVision (handled further below)
    if (machine_mode)
    {
        // --------------------------------------------------------------
        // If the ADAM is enabled, we may be trying to write to AdamNet
        // or, more likely, to the various mapped RAM configurations...
        // --------------------------------------------------------------
        if (machine_mode & MODE_ADAM)
        {
            if (adam_ram_present[address >> 13]) // Is there RAM mapped in this 8K area?
            {
                *(MemoryMap[address>>13] + (address&0x1FFF)) = value;
                if (PCBTable[address]) WritePCB(address, value); // Check if we need to write to the PCB mapped area
            }
        }
        // -------------------------------------------------------------
        // If SG-1000 mode, we provide the Dhajee RAM expansion...
        // We aren't doing a lot of error/bounds checking here - we 
        // are going to assume well-behaved .sg ROMs as this is 
        // primarily a Colecovision emu with partial SG-1000 support.
        // -------------------------------------------------------------
        else if (machine_mode & MODE_SG_1000)
        {
            // -------------------------------------------------------
            // A few SG-1000 games use the SMSmapper. 
            // Most notably Loretta no Shouzou: Sherlock Holmes and 
            // the SG-1000 port of Prince of Persia.
            // $fffd 0 ($0000-$3fff)
            // $fffe 1 ($4000-$7fff)
            // $ffff 2 ($8000-$bfff)
            // -------------------------------------------------------
            if (sg1000_sms_mapper && (address >= 0xFFFD))
            {
                if      (address == 0xFFFD) memcpy(RAM_Memory+0x0000, ROM_Memory+((u32)(value&sg1000_sms_mapper)*16*1024), 0x4000);
                else if (address == 0xFFFE) memcpy(RAM_Memory+0x4000, ROM_Memory+((u32)(value&sg1000_sms_mapper)*16*1024), 0x4000);
                else if (address == 0xFFFF) memcpy(RAM_Memory+0x8000, ROM_Memory+((u32)(value&sg1000_sms_mapper)*16*1024), 0x4000);
            }

            // Allow normal SG-1000, SC-3000 writes, plus allow for 8K RAM Expanders...
            if ((address >= 0x8000) || (address >= 0x2000 && address < 0x4000))
            {
                RAM_Memory[address]=value;
            }
        }
        // ----------------------------------------------------------------------------------------------------------
        // For the MSX, we support a 64K main RAM machine plus some of the more common memory mappers...
        // ----------------------------------------------------------------------------------------------------------
        else if (machine_mode & MODE_MSX)
        {
            // -------------------------------------------------------
            // First see if this is a write to a RAM enabled slot...
            // -------------------------------------------------------
            if (bRAMInSegment[0] && (address < 0x4000))
            {
                RAM_Memory[address]=value;  // Allow write - this is a RAM mapped slot
            }
            else if (bRAMInSegment[1] && (address >= 0x4000) && (address <= 0x7FFF))
            {
                RAM_Memory[address]=value;  // Allow write - this is a RAM mapped slot
            }
            else if ((bRAMInSegment[2] || msx_sram_at_8000) && (address >= 0x8000) && (address <= 0xBFFF))
            {
                if (msx_sram_at_8000) 
                {
                    SRAM_Memory[address&0x3FFF] = value;   // Write SRAM area
                    write_NV_counter = 4;                  // This will back the EE in 4 seconds of non-activity on the SRAM
                }
                else RAM_Memory[address]=value;  // Allow write - this is a RAM mapped slot
            }
            else if ((bRAMInSegment[3] == 2) && (address >= 0xE000)) // A value of 2 here means this is an 8K machine
            {
                RAM_Memory[address]=value;  // Allow write - this is a RAM mapped slot
            }
            else if ((bRAMInSegment[3] == 1) && (address >= 0xC000)) // A value of 1 here means we can write to the entire 16K page
            {
                RAM_Memory[address]=value;  // Allow write - this is a RAM mapped slot
            }
            else    // Check for MSX Mappers Mappers
            {
                if (mapperMask)
                {
                    // -------------------------------------------------------------
                    // Compute the block and offset of the new memory and we 
                    // can map it into place... this is fast since we are just
                    // moving pointers around and not trying to copy memory blocks.
                    // -------------------------------------------------------------
                    u32 block = (value & mapperMask);
                    u32 msx_offset = block * msx_block_size;
                    u32 *src = (u32*)((u8*)ROM_Memory + msx_offset);

                    // ---------------------------------------------------------------------------------
                    // The Konami 8K Mapper without SCC:
                    // 4000h-5FFFh - fixed ROM area (not swappable)
                    // 6000h~7FFFh (mirror: E000h~FFFFh)    6000h (mirrors: 6001h~7FFFh)    1
                    // 8000h~9FFFh (mirror: 0000h~1FFFh)    8000h (mirrors: 8001h~9FFFh)    Random
                    // A000h~BFFFh (mirror: 2000h~3FFFh)    A000h (mirrors: A001h~BFFFh)    Random
                    // ---------------------------------------------------------------------------------
                    if (mapperType == KON8)
                    {
                        if (bROMInSegment[1] && (address == 0x4000))
                        {
                            if (msx_last_block[0] != block)
                            {
                                MSXCartPtr[2] = (u8*)src;  // Main ROM
                                MSXCartPtr[6] = (u8*)src;  // Mirror
                                MemoryMap[2] = (u8 *)(MSXCartPtr[2]);
                                msx_last_block[0] = block;
                            }
                        }
                        else if (bROMInSegment[1] && (address == 0x6000))
                        {
                            if (msx_last_block[1] != block)
                            {
                                MSXCartPtr[3] = (u8*)src;  // Main ROM
                                MSXCartPtr[7] = (u8*)src;  // Mirror
                                MemoryMap[3] = (u8 *)(MSXCartPtr[3]);
                                msx_last_block[1] = block;
                            }
                        }
                        else if (bROMInSegment[2] && (address == 0x8000))
                        {
                            if (msx_last_block[2] != block)
                            {
                                MSXCartPtr[4] = (u8*)src;  // Main ROM
                                MSXCartPtr[0] = (u8*)src;  // Mirror                            
                                MemoryMap[4] = (u8 *)(MSXCartPtr[4]);
                                msx_last_block[2] = block;
                            }
                        }
                        else if (bROMInSegment[2] && (address == 0xA000))
                        {
                            if (msx_last_block[3] != block)
                            {
                                MSXCartPtr[5] = (u8*)src;  // Main ROM
                                MSXCartPtr[1] = (u8*)src;  // Mirror       
                                MemoryMap[5] = (u8 *)(MSXCartPtr[5]);
                                msx_last_block[3] = block;
                            }
                        }
                    }
                    else if (mapperType == ASC8)
                    {
                        // -------------------------------------------------------------------------
                        // The ASCII 8K Mapper:
                        // 4000h~5FFFh (mirror: C000h~DFFFh)    6000h (mirrors: 6001h~67FFh)    0
                        // 6000h~7FFFh (mirror: E000h~FFFFh)    6800h (mirrors: 6801h~68FFh)    0
                        // 8000h~9FFFh (mirror: 0000h~1FFFh)    7000h (mirrors: 7001h~77FFh)    0
                        // A000h~BFFFh (mirror: 2000h~3FFFh)    7800h (mirrors: 7801h~7FFFh)    0     
                        // -------------------------------------------------------------------------
                        if (bROMInSegment[1] && (address >= 0x6000) && (address < 0x6800))
                        {
                            if (msx_last_block[0] != block)
                            {
                                MSXCartPtr[2] = (u8*)src;  // Main ROM
                                MSXCartPtr[6] = (u8*)src;  // Mirror
                                MemoryMap[2] = MSXCartPtr[2];
                                if (bROMInSegment[3])
                                {
                                    MemoryMap[6] = MSXCartPtr[6];
                                }
                                msx_last_block[0] = block;
                            }
                        }
                        else if (bROMInSegment[1] && (address >= 0x6800)  && (address < 0x7000))
                        {
                            if (msx_last_block[1] != block)
                            {
                                MSXCartPtr[3] = (u8*)src;  // Main ROM
                                MSXCartPtr[7] = (u8*)src;  // Mirror
                                MemoryMap[3] = MSXCartPtr[3];
                                if (bROMInSegment[3])
                                {
                                    MemoryMap[7] = MSXCartPtr[7];
                                }
                                msx_last_block[1] = block;
                            }
                        }
                        else if (bROMInSegment[1] && (address >= 0x7000)  && (address < 0x7800))
                        {
                            if (msx_last_block[2] != block)
                            {
                                if (msx_sram_enabled && (block == msx_sram_enabled))
                                {
                                    msx_sram_at_8000 = true;
                                }
                                else
                                {
                                    msx_sram_at_8000 = false;
                                    MSXCartPtr[4] = (u8*)src;  // Main ROM
                                    MSXCartPtr[0] = (u8*)src;  // Mirror    
                                    if (bROMInSegment[2])
                                    {
                                        MemoryMap[4] = MSXCartPtr[4];
                                    }
                                    if (bROMInSegment[0])
                                    {
                                        MemoryMap[0] = MSXCartPtr[0];
                                    }                            
                                }
                                msx_last_block[2] = block;
                            }
                        }
                        else if (bROMInSegment[1] && (address >= 0x7800) && (address < 0x8000))
                        {
                            if (msx_last_block[3] != block)
                            {
                                if (msx_sram_enabled && (block == msx_sram_enabled))
                                {
                                    msx_sram_at_8000 = true;
                                }
                                else
                                {
                                    msx_sram_at_8000 = false;
                                    MSXCartPtr[5] = (u8*)src;  // Main ROM
                                    MSXCartPtr[1] = (u8*)src;  // Mirror                            
                                    if (bROMInSegment[2]) 
                                    {
                                        MemoryMap[5] = MSXCartPtr[5];
                                    }
                                    if (bROMInSegment[0])
                                    {
                                        MemoryMap[1] = MSXCartPtr[1];
                                    }                            
                                }
                                msx_last_block[3] = block;
                            }
                        }
                    }
                    else if (mapperType == SCC8)
                    {
                        if ((address & 0x0FFF) != 0)
                        {
                            // ----------------------------------------------------
                            // Are we writing to the SCC chip memory mapped area?
                            // ----------------------------------------------------
                            if (msx_scc_enable && ((address & 0xFF00)==0x9800))
                            {
                                 SCCWrite(value, address, &mySCC);
                            }
                            return;    // It has to be one of the mapped addresses below - this will also short-circuit any SCC writes which are not yet supported
                        }
                        HandleKonamiSCC8(src, block, address, value);
                    }
                    else if (mapperType == ASC16)
                    {
                        HandleAscii16K(src, block, address);
                    }
                    else if (mapperType == ZEN8)
                    {
                        HandleZemina8K(src, block, address);
                    }
                    else if (mapperType == ZEN16)
                    {
                        HandleZemina16K(src, block, address);
                    }                
                    else if (mapperType == XBLAM)
                    {
                        if (address == 0x4045)
                        {
                            MSXCartPtr[4] = (u8*)src;          // Main ROM at 8000
                            MSXCartPtr[5] = (u8*)src+0x2000;   // Main ROM at A000                  
                            if (bROMInSegment[2]) 
                            {
                                MemoryMap[4] = MSXCartPtr[4];
                                MemoryMap[5] = MSXCartPtr[5];
                            }
                        }
                    }                
                }
            }
        }
        // ----------------------------------------------------------------------------------
        // For the Sord M5, RAM is at 0x7000 and we emulate the 32K RAM Expander above that
        // ----------------------------------------------------------------------------------
        else if (machine_mode & MODE_SORDM5)
        {
            if (address >= 0x7000)
            {
                RAM_Memory[address]=value;  // Allow pretty much anything above the base ROM area
            }
        }
        // ----------------------------------------------------------------------------------
        // For the Casio PV-2000, we call into a special routine to handle memory writes.
        // ----------------------------------------------------------------------------------
        else if (machine_mode & MODE_PV2000)
        {
            cpu_writemem_pv2000(value, address);
        }
        // ----------------------------------------------------------------------------------
        // For the Memotech MTX, we need to address through the MemoryMap[] pointers...
        // ----------------------------------------------------------------------------------
        else if (machine_mode & MODE_MEMOTECH)
        {
            if (address >= memotech_RAM_start)
            {
                *(MemoryMap[address>>13] + (address&0x1FFF)) = value;
            }
        }
        // -----------------------------------------------------------------------------------------
        // For the Einstien, allow full range - even if BIOS is installed, we still can write RAM.
        // -----------------------------------------------------------------------------------------
        else if (machine_mode & MODE_EINSTEIN)
        {
            RAM_Memory[address] = value; 
        }
        // ----------------------------------------------------------------------------------
        // For the Spectravideo SVI - we write into any area that is designated as RAM
        // ----------------------------------------------------------------------------------
        else if (machine_mode & MODE_SVI)
        {
            if (svi_RAMinSegment[(address & 0x8000) ? 1:0])  // Is there RAM in this 32K area?
            {
                RAM_Memory[address]=value;  // Allow pretty much anything above the base ROM area
            }
        }
        // -------------------------------------------------------------------------------------------------------------------------
        // For the Pencil II, we allow the full range. We should restrict it, but the two carts we have dumped are well behaved...
        // -------------------------------------------------------------------------------------------------------------------------
        else if (machine_mode & MODE_PENCIL2)
        {
            RAM_Memory[address] = value;
        }
        // ----------------------------------------------------------------------------------
        // For the ZX Spectrum we allow writes to any address that isn't in the BIOS area...
        // ----------------------------------------------------------------------------------
        else if (machine_mode & MODE_SPECCY)
        {
            if (address & 0xC000) // Must be above the 16K BIOS ROM area to allow write...
            {
                if (zx_128k_mode) // We might be pointing to expanded memory banks... 
                {
                    *(MemoryMap[address>>13] + (address&0x1FFF)) = value;
                }
                else RAM_Memory[address] = value; // 48K Spectrum just uses base 64K array
            }
        }
    }
    else // Colecovision Mode - optimized...
    {
        // --------------------------------------------------------------------
        // There are a few "hotspots" in the upper memory we have to look for.
        // --------------------------------------------------------------------
        if (address & 0x8000)
        {
            // A few carts have EEPROM at E000 (mostly Lord of the Dungeon)
            if (sRamAtE000_OK && (address >= 0xE000) && (address < 0xE800)) // Allow SRAM if cart doesn't extend this high...
            {
                RAM_Memory[address+0x800]=value;
            }
            /* "Activision" PCB boards, potentially containing EEPROM, use [1111 1111 10xx 0000] addresses for hotspot bankswitch */
            else if (bActivisionPCB)
            {
                activision_pcb_write(address);
            }
            else if (bSuperGameCart)
            {
                // Handle Super Game Cart
                SuperGameCartWrite(address, value);                
            }
            else if (address >= 0xFFC0) // MC = Mega Cart support
            {
                MegaCartBankSwitch(address & romBankMask);  // Handle Megacart Hot Spot writes (don't think anyone actually uses this but it's possible)
            }
            // Otherwise we shouldn't be writing in this region... ignore it.
        }
        else // Lower memory... normal RAM lives down here
        {
            // -----------------------------------------------------------
            // If the Super Game Module has been enabled, we have a much 
            // wider range of RAM that can be written (and no mirroring)
            // -----------------------------------------------------------
            if (sgm_enable)
            {
                if (address >= sgm_low_addr) RAM_Memory[address]=value;
            }    
            else if (address>0x5FFF) // Normal memory RAM write... with mirrors...
            {
                if (myConfig.mirrorRAM)
                {
                    address&=0x03FF;
                    RAM_Memory[0x6000|address]=RAM_Memory[0x6400|address]=RAM_Memory[0x6800|address]=RAM_Memory[0x6C00|address]=value;
                    RAM_Memory[0x7000|address]=RAM_Memory[0x7400|address]=RAM_Memory[0x7800|address]=RAM_Memory[0x7C00|address]=value;
                }
                else RAM_Memory[address] = value; // Mainly for the older DS hardware as the proper mirroring above chews up almost 10% of our CPU
            }
        }
    }
}

// -----------------------------------------------------------------
// Reset a few key variables needed for proper Z80 interface use...
// -----------------------------------------------------------------
void Z80_Interface_Reset(void) 
{
  last_mega_bank    = 199;
  CPU.CycleDeficit  = 0;
  msx_sram_at_8000  = 0;
  msx_scc_enable    = 0;
  msx_last_block[0] = 
  msx_last_block[1] =
  msx_last_block[2] =
  msx_last_block[3] = 199;
}

// -----------------------------------------------------------------
// Trap and report illegal opcodes to the ColecoDS debugger...
// -----------------------------------------------------------------
void Trap_Bad_Ops(char *prefix, byte I, word W)
{
    if (myGlobalConfig.debugger)
    {
        char tmp[32];
        sprintf(tmp, "ILLOP: %s %02X %04X", prefix, I, W);
        DSPrint(0,0,6, tmp);
    }
}

// End of file
