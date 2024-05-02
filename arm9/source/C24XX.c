/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                          C24XX.c                        **/
/**                                                         **/
/** This file contains emulation for the 24cXX series of    **/
/** serial EEPROMs. See C24XX.h for declarations.           **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 2017-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/

#include "C24XX.h"
#include <stdio.h>
#include <string.h>
#include "printf.h"

/** Internal Chip States *************************************/
#define RECV_CMD  0
#define RECV_ADDR 1
#define RECV_ADR2 2
#define RECV_DATA 3
#define SEND_DATA 4

/** Size24XX *************************************************/
/** Return the size of given chip model in bytes.           **/
/*************************************************************/
unsigned int Size24XX(C24XX *D)
{
  unsigned int Chip = D->Flags&C24XX_CHIP;
  return(Chip<=C24XX_24C256 ? (0x80<<Chip):0);
}

/** Reset24XX ************************************************/
/** Reset the 24xx chip.                                    **/
/*************************************************************/
void Reset24XX(C24XX *D,unsigned int Flags)
{
  D->Pins  = C24XX_SDA|C24XX_SCL;
  D->Out   = C24XX_SDA|C24XX_SCL;
  D->State = RECV_CMD;
  D->Bits  = 0x0001;
  D->Cmd   = 0x00;
  D->Addr  = 0;
  D->Flags = Flags;
  D->Rsrvd = 0;
  memset(D->Data, 0xFF, 0x8000);
}

/** Read24XX *************************************************/
/** Read value from the 24xx chip. Only bits 0,1 are used   **/
/** (see #defines).                                         **/
/*************************************************************/
byte Read24XX(C24XX *D)
{
  return(D->Out);
}

/** Write24XX ************************************************/
/** Write value V into the 24xx chip. Only bits 0,1 are     **/
/** used (see #defines).                                    **/
/*************************************************************/
byte Write24XX(C24XX *D,byte V)
{
  static const unsigned int PageSize[16] =
  { 8,8,16,16,16,32,32,64,64,128,128,256,256,256,256,256 };

  word J;
    
  /* When SDA line changes while SCL=1... */
  if(((D->Pins^V)&C24XX_SDA) && (D->Pins&V&C24XX_SCL))
  {
    if(V&C24XX_SDA)
    {
      /* SDA=1: STOP condition */
      D->State = RECV_CMD;
      D->Bits  = 0x0001;
    }
    else
    {
      /* SDA=0: START condition */
      D->State = RECV_CMD;
      D->Bits  = 0x0001;
    }
  }
  /* When SCL line goes up... */
  else if((D->Pins^V)&V&C24XX_SCL)
  {
    if((D->State==SEND_DATA) && (D->Bits!=0x8000))
    {
      /* Report current output bit */
      D->Out = (D->Out&~C24XX_SDA)|(D->Bits&0x8000? C24XX_SDA:0);
      /* Shift output bits */
      D->Bits<<=1;
    }
    else if((D->State!=SEND_DATA) && (D->Bits<0x0100))
    {
      /* Shift input bits */
      D->Bits = (D->Bits<<1)|(V&C24XX_SDA? 1:0);
    }
    else
    {
       extern unsigned char write_NV_counter;
      /* Depending on the state... */
      switch(D->State)
      {
        case RECV_CMD:
          D->Cmd   = D->Bits&0x00FF;
          D->State = (D->Cmd&0xF0)!=0xA0? RECV_CMD:D->Cmd&0x01? SEND_DATA:RECV_ADDR;
          break;
        case RECV_ADDR:
          D->Addr  = ((unsigned int)(D->Cmd&0x0E)<<7)+(D->Bits&0x00FF);
          D->Addr &= (0x80<<(D->Flags&C24XX_CHIP))-1;
          D->State = (D->Flags&C24XX_CHIP)>=C24XX_24C32? RECV_ADR2:RECV_DATA;
          break;
        case RECV_ADR2:
          D->Addr  = (D->Addr<<8)+(D->Bits&0x00FF);
          D->Addr &= (0x80<<(D->Flags&C24XX_CHIP))-1;
          D->State = RECV_DATA;
          break;
        case RECV_DATA:
          /* Write byte into EEPROM */
          D->Data[D->Addr] = D->Bits&0x00FF;
          /* Go to the next address inside N-byte page */
          J = PageSize[D->Flags&C24XX_CHIP]-1;
          D->Addr = ((D->Addr+1)&J)|(D->Addr&~J);
          write_NV_counter = 2;
          break;
        case SEND_DATA:
          /* See below */
          break;
        default:
          D->State = RECV_CMD;
          break;
      }
      /* Acknowledge received byte, clear bit buffer */
      D->Bits = 0x0001;
      D->Out  = C24XX_SCL;
      /* If sending the next byte... */
      if(D->State==SEND_DATA)
      {
        /* Read byte from EEPROM */
        D->Bits = ((word)D->Data[D->Addr]<<8)|0x0080;
        /* Go to the next address inside N-byte page */
        J = PageSize[D->Flags&C24XX_CHIP]-1;
        //D->Addr = ((D->Addr+1)&J)|(D->Addr&~J);
        // Sequential reads are not restricted by page size - just EE size
        D->Addr++;
        D->Addr &= (0x80<<(D->Flags&C24XX_CHIP))-1;
      }
    }
  }

  /* New pin values */
  D->Pins = V;

  /* Done */
  return(D->Out);
}
