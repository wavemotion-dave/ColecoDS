/** ColEm: portable Coleco emulator **************************/
/**                                                         **/
/**                       AdamNet.c                         **/
/**                                                         **/
/** This file contains implementation for the AdamNet I/O   **/
/** interface found in Coleco Adam home computer.           **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1994-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#include <string.h>
#include <stdio.h>

#include "colecoDS.h"
#include "AdamNet.h"
#include "FDIDisk.h"
#include "colecogeneric.h"
#include "colecomngt.h"
#include "cpu/tms9918a/tms9918a.h"
#include "cpu/z80/Z80_interface.h"
#include "printf.h"

FDIDisk Disks[MAX_DISKS] = { 0 };  /* Adam disk drives          */
FDIDisk Tapes[MAX_TAPES] = { 0 };  /* Adam tape drives          */

extern byte adam_ram_hi;
extern byte adam_ram_lo;

byte HoldingBuf[4096];
u8 io_busy= 0;
word savedBUF = 0;
word savedLEN = 0;
byte last_command_read=false;
byte io_show_status=0;

#define DELAY_IO    10

/** RAM Access Macro *****************************************/
#define RAM(A)         (RAM_Memory[A])

/** PCB Field Offsets ****************************************/
#define PCB_CMD_STAT   0
#define PCB_BA_LO      1
#define PCB_BA_HI      2
#define PCB_MAX_DCB    3
#define PCB_SIZE       4

/** DCB Field Offsets ****************************************/
#define DCB_CMD_STAT   0
#define DCB_BA_LO      1
#define DCB_BA_HI      2
#define DCB_BUF_LEN_LO 3
#define DCB_BUF_LEN_HI 4
#define DCB_SEC_NUM_0  5
#define DCB_SEC_NUM_1  6
#define DCB_SEC_NUM_2  7
#define DCB_SEC_NUM_3  8
#define DCB_DEV_NUM    9
#define DCB_RETRY_LO   14
#define DCB_RETRY_HI   15
#define DCB_ADD_CODE   16
#define DCB_MAXL_LO    17
#define DCB_MAXL_HI    18
#define DCB_DEV_TYPE   19
#define DCB_NODE_TYPE  20
#define DCB_SIZE       21

/** PCB Commands *********************************************/
#define CMD_PCB_IDLE   0x00
#define CMD_PCB_SYNC1  0x01
#define CMD_PCB_SYNC2  0x02
#define CMD_PCB_SNA    0x03
#define CMD_PCB_RESET  0x04
#define CMD_PCB_WAIT   0x05

/** DCB Commands *********************************************/
#define CMD_RESET      0x00
#define CMD_STATUS     0x01
#define CMD_ACK        0x02
#define CMD_CLEAR      0x03
#define CMD_RECEIVE    0x04
#define CMD_CANCEL     0x05
#define CMD_SEND       0x06 /* + SIZE_HI + SIZE_LO + DATA + CRC */
#define CMD_NACK       0x07

#define CMD_SOFT_RESET 0x02
#define CMD_WRITE      0x03
#define CMD_READ       0x04

/** Response Codes *******************************************/
#define RSP_STATUS     0x80 /* + SIZE_HI + SIZE_LO + TXCODE + STATUS + CRC */
#define RSP_ACK        0x90
#define RSP_CANCEL     0xA0
#define RSP_SEND       0xB0 /* + SIZE_HI + SIZE_LO + DATA + CRC */
#define RSP_NACK       0xC0

/** Key Codes with SHIFT and CTRL ****************************/
/** Shifted Key Codes ****************************************/
static const byte ShiftKey[256] =
{
  /* 0x00 */
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0xB8,0xB9,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x22,0x28,0x29,0x2A,0x3D,0x3C,0x60,0x3E,0x3F,
  0x29,0x21,0x40,0x23,0x24,0x25,0x5F,0x26,0x2A,0x28,0x3A,0x3A,0x3C,0x2B,0x3E,0x3F,
  /* 0x40 */
  0x40,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,
  0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x5F,
  0x7E,0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,
  0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x7B,0x7C,0x7D,0x7E,0x7F,
  /* 0x80 */
  0x80,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
  0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
  0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
  0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
  /* 0xC0 */
  0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
  0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
  0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
  0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

static const byte CtrlKey[256] =
{
  /* 0x00 */
  0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,
  0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F,
  0x30,0x31,0x00,0x33,0x34,0x35,0x1F,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
  /* 0x40 */
  0x40,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x5F,
  0x60,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
  0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x7F,
  /* 0x80 */
  0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F,
  0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x7F,0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,
  0xA4,0xA5,0xA6,0xA7,0xA4,0xA5,0xA6,0xA7,0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,
  0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF,
  /* 0xC0 */
  0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7,0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF,
  0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7,0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF,
  0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7,0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF,
  0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7,0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
};

extern byte Port60;

byte PCBTable[0x10000];

word PCBAddr;
byte DiskID;
byte KBDStatus;
byte LastKey;

extern byte RAM_Memory[];


/** GetDCB() *************************************************/
/** Get DCB byte at given offset.                           **/
/*************************************************************/
static byte GetDCB(byte Dev,byte Offset)
{
  word A = (PCBAddr+PCB_SIZE+Dev*DCB_SIZE+Offset)&0xFFFF;
  return(RAM_Memory[A]);
}

static word GetDCBBase(byte Dev)
{
  return(GetDCB(Dev,DCB_BA_LO)+((word)GetDCB(Dev,DCB_BA_HI)<<8));
}

static word GetDCBLen(byte Dev)
{
  return(GetDCB(Dev,DCB_BUF_LEN_LO)+((word)GetDCB(Dev,DCB_BUF_LEN_HI)<<8));
}

static unsigned int GetDCBSector(byte Dev)
{
  return(
    GetDCB(Dev,DCB_SEC_NUM_0)
  + ((unsigned int)GetDCB(Dev,DCB_SEC_NUM_1)<<8)
  + ((unsigned int)GetDCB(Dev,DCB_SEC_NUM_2)<<16)
  + ((unsigned int)GetDCB(Dev,DCB_SEC_NUM_3)<<24)
  );
}

/** GetPCB() *************************************************/
/** Get PCB byte at given offset.                           **/
/*************************************************************/
static byte GetPCB(word Offset)
{
  word A = (PCBAddr+Offset)&0xFFFF;
  return(RAM_Memory[A]);
}

static word GetPCBBase(void)
{
  return(GetPCB(PCB_BA_LO)+((word)GetPCB(PCB_BA_HI)<<8));
}

static word GetMaxDCB(void)
{
  return(GetPCB(PCB_MAX_DCB));
}

/** SetDCB() *************************************************/
/** Set DCB byte at given offset.                           **/
/*************************************************************/
static void SetDCB(byte Dev,byte Offset,byte Value)
{
  word A = (PCBAddr+PCB_SIZE+Dev*DCB_SIZE+Offset)&0xFFFF;
    
  RAM_Memory[A] = Value;
}

/** SetPCB() *************************************************/
/** Set PCB byte at given offset.                           **/
/*************************************************************/
static void SetPCB(word Offset,byte Value)
{
  word A = (PCBAddr+Offset)&0xFFFF;
  RAM_Memory[A] = Value;
}

/** IsPCB() **************************************************/
/** Return 1 if given address belongs to PCB, 0 otherwise.  **/
/*************************************************************/
static int IsPCB(word A)
{
  /* Quick check for PCB presence */
  if(!PCBTable[A]) return(0);
  /* Check if PCB is mapped in */
  if((A<0x2000) && ((Port60&0x03)!=1)) return(0);
  if((A<0x8000) && ((Port60&0x03)!=1) && ((Port60&0x03)!=3)) return(0);
  if((A>=0x8000) && (Port60&0x0C)) return(0);

  /* Check number of active devices */
  if(A>=PCBAddr+PCB_SIZE+GetMaxDCB()*DCB_SIZE) return(0);

  /* This address belongs to AdamNet */
  return(1);
}

/** MovePCB() ************************************************/
/** Move PCB and related DCBs to a new address.             **/
/*************************************************************/
static void MovePCB(word NewAddr,byte MaxDCB)
{
  int J;

  PCBTable[PCBAddr] = 0;
  for(J=0;J<15*DCB_SIZE;J+=DCB_SIZE)
    PCBTable[(PCBAddr+PCB_SIZE+J)&0xFFFF] = 0;

  PCBTable[NewAddr] = 1;
  for(J=0;J<15*DCB_SIZE;J+=DCB_SIZE)
    PCBTable[(NewAddr+PCB_SIZE+J)&0xFFFF] = 1;

  PCBAddr = NewAddr;
  SetPCB(PCB_BA_LO,   NewAddr&0xFF);
  SetPCB(PCB_BA_HI,   NewAddr>>8);
  SetPCB(PCB_MAX_DCB, MaxDCB);

  for(J=0;J<=MaxDCB;++J)
  {
    SetDCB(J,DCB_DEV_NUM,0);
    SetDCB(J,DCB_ADD_CODE,J);
  }
}

/** ReportDevice() *******************************************/
/** Reply to STATUS command with device parameters.         **/
/*************************************************************/
static void ReportDevice(byte Dev,word MsgSize,byte IsBlock)
{
  SetDCB(Dev,DCB_CMD_STAT, RSP_STATUS);
  SetDCB(Dev,DCB_MAXL_LO,  MsgSize&0xFF);
  SetDCB(Dev,DCB_MAXL_HI,  MsgSize>>8);
  SetDCB(Dev,DCB_DEV_TYPE, IsBlock? 0x01:0x00);
}

/** PutKBD() *************************************************/
/** Add a new key to the keyboard buffer.                   **/
/*************************************************************/
void PutKBD(unsigned int Key)
{
  unsigned int Mode;

  Mode = Key & ~0xFF;
  Key  = Key & 0xFF;
  Key  = (Key>='A')&&(Key<='Z')? Key+'a'-'A':Key;
  Key  = (Mode&ADAM_KEY_CONTROL || key_ctrl) && (CtrlKey[Key]!=Key)?  CtrlKey[Key]
       : (Mode&ADAM_KEY_SHIFT || key_shift)  && (ShiftKey[Key]!=Key)? ShiftKey[Key]
       : Key;
  Key  = (Mode&ADAM_KEY_CAPS)&&(Key>='a')&&(Key<='z')? Key+'A'-'a':Key;

  LastKey = Key;
}

static byte GetKBD()
{
  byte Result = LastKey;
  LastKey = 0x00;
  return(Result);
}

static void UpdateKBD(byte Dev,int V)
{
  int J,N;
  word A;

  switch(V)
  {
    case -1:
      SetDCB(Dev,DCB_CMD_STAT,KBDStatus);
      break;
    case CMD_STATUS:
    case CMD_SOFT_RESET:
      /* Character-based device, single character buffer */
      ReportDevice(Dev,0x0001,0);
      KBDStatus = RSP_STATUS;
      LastKey   = 0x00;
      break;
    case CMD_WRITE:
      SetDCB(Dev,DCB_CMD_STAT,RSP_ACK+0x0B);
      KBDStatus = RSP_STATUS;
      break;
    case CMD_READ:
      SetDCB(Dev,DCB_CMD_STAT,0x00);
      A = GetDCBBase(Dev);
      N = GetDCBLen(Dev);
      for(J=0 ; (J<N) && (V=GetKBD()) ; ++J, A=(A+1)&0xFFFF)
      {
        RAM_Memory[A] = V;
      }
      KBDStatus = RSP_STATUS+(J<N? 0x0C:0x00);
      break;
  }
}

static void UpdatePRN(byte Dev,int V)
{
  int N;
  word A;

  switch(V)
  {
    case CMD_STATUS:
    case CMD_SOFT_RESET:
      /* Character-based device, single character buffer */
      ReportDevice(Dev,0x0001,0);
      break;
    case CMD_READ:
      SetDCB(Dev,DCB_CMD_STAT,RSP_ACK+0x0B);
      break;
    case CMD_WRITE:
      SetDCB(Dev,DCB_CMD_STAT,0x00);
      A = GetDCBBase(Dev);
      N = GetDCBLen(Dev);
      (void)A;
      (void)N;
      //for(J=0 ; J<N ; ++J, A=(A+1)&0xFFFF)
        //Printer(RAM(A));
      break;
    default:
      SetDCB(Dev,DCB_CMD_STAT,RSP_STATUS);
      break;
  }
}

static void AdamFlushCache(void)
{
  for (word i=0; i<savedLEN; i++)
  {
      // Copy data from holding buffer...
      RAM_Memory[savedBUF] = HoldingBuf[i];
      savedBUF++;
  }
}

static void UpdateDSK(byte N,byte Dev,int V)
{
  static const byte InterleaveTable[8]= { 0,5,2,7,4,1,6,3 };
  int I,J,K,LEN,SEC;
  word BUF;
  byte *Data;

  /* We have limited number of disks */
  if(N>=MAX_DISKS) return;

  /* If reading DCB status, stop here */
  if(V<0)
  {
      if (io_busy)
      {
          io_busy--;
          SetDCB(Dev,DCB_CMD_STAT,0x00);
          
          if (io_busy == 0 && last_command_read)
          {
              last_command_read=0;
              AdamFlushCache();
          }
      }
      else
      {
         SetDCB(Dev,DCB_CMD_STAT,RSP_STATUS);
      }
    return;
    return;
  }

  /* Reset errors, report missing disks */
  SetDCB(Dev,DCB_NODE_TYPE,(GetDCB(Dev,DCB_NODE_TYPE)&0xF0) | (Disks[N].Data? 0x00:0x03));

  /* Depending on the command... */
  switch(V)
  {
    case CMD_STATUS:
      /* Block-based device, 1kB buffer */
      ReportDevice(Dev,0x0400,1);
      break;

    case CMD_SOFT_RESET:
      SetDCB(Dev,DCB_CMD_STAT,RSP_STATUS);
      break;

    case CMD_WRITE:
    case CMD_READ:
      io_show_status = (V==CMD_READ) ? 1:2;
      if (io_show_status == 2) disk_unsaved_data = 1;
      /* Busy status by default */
      SetDCB(Dev,DCB_CMD_STAT,0x00);
      io_busy = DELAY_IO;
      /* If no disk, stop here */
      if(!Disks[N].Data) break;
      /* Determine buffer address, length, block number */
      BUF = GetDCBBase(Dev);
      LEN = GetDCBLen(Dev);
      LEN = LEN<0x0400? LEN:0x0400;
      SEC = GetDCBSector(Dev);
      savedBUF = BUF;
      savedLEN = LEN;
      /* For each 512-byte sector... */
      for(I=0, SEC<<=1 ; I<LEN ; ++SEC, I+=0x200)
      {
        /* Remap sector number via interleave table */
        K = (SEC&~7) | InterleaveTable[SEC&7];
        /* Get pointer to sector data on disk */
        Data = LinearFDI(&Disks[N],K);
        /* If wrong sector number, stop here */
        if(!Data)
        {
          SetDCB(Dev,DCB_NODE_TYPE,GetDCB(Dev,DCB_NODE_TYPE)|0x02);
          LEN = 0;
          break;
        }
        /* Read or write sectors */
        K = I+0x200>LEN? LEN-I:0x200;
        if(V==CMD_READ)
        {
            last_command_read = true;
            for(J=0;J<K;++J,++BUF) 
            {
                HoldingBuf[I+J] = Data[J];
            }
        }
        else
        {
          last_command_read = false;
          for(J=0;J<K;++J,++BUF) 
          {
              Data[J] = RAM_Memory[BUF];
          }
        }
        /* If disk access failed, stop here */
        if(J<K)
        {
          SetDCB(Dev,DCB_NODE_TYPE,GetDCB(Dev,DCB_NODE_TYPE)|0x06);
          LEN = 0;
          break;
        }
      }
      /* Done */
      break;
  }
}

static void UpdateTAP(byte N,byte Dev,int V)
{
  int I,J,K,LEN,SEC;
  word BUF;
  byte *Data;

  /* If reading DCB status, stop here */
  if(V<0)
  {
      if (io_busy)
      {
          io_busy--;
          SetDCB(Dev,DCB_CMD_STAT,0x00);
          if (io_busy == 0 && last_command_read)
          {
              last_command_read = 0;
              AdamFlushCache();
          }
      }
      else
      {
        SetDCB(Dev,DCB_CMD_STAT,RSP_STATUS);
      }
    return;
  }

  /* Reset errors, report missing tapes */
  SetDCB(Dev,DCB_NODE_TYPE,(Tapes[N&2].Data? 0x00:0x03)|(Tapes[(N&2)+1].Data? 0x00:0x30));

  /* Depending on the command... */
  switch(V)
  {
    case CMD_STATUS:
      /* Block-based device, 1kB buffer */
      ReportDevice(Dev,0x0400,1);
      break;
 
    case CMD_SOFT_RESET:
      SetDCB(Dev,DCB_CMD_STAT,RSP_STATUS);
      break;

    case CMD_WRITE:
    case CMD_READ:
      io_show_status = (V==CMD_READ) ? 1:2;
      if (io_show_status == 2) disk_unsaved_data = 1;
      /* Busy status by default */
      SetDCB(Dev,DCB_CMD_STAT,0x00);
      io_busy = DELAY_IO;
      /* If no tape, stop here */
      if(!Tapes[N].Data) break;
      /* Determine buffer address, length, block number */
      BUF = GetDCBBase(Dev);
      LEN = GetDCBLen(Dev);
      LEN = LEN<0x0400? LEN:0x0400;
      SEC = GetDCBSector(Dev);
      savedBUF = BUF;
      savedLEN = LEN;
      
      /* For each 512-byte sector... */
      for(I=0, SEC<<=1 ; I<LEN ; ++SEC, I+=0x200)
      {
        /* Get pointer to sector data on tape */
        Data = LinearFDI(&Tapes[N],SEC);
        /* If wrong sector number, stop here */
        if(!Data)
        {
          SetDCB(Dev,DCB_NODE_TYPE,GetDCB(Dev,DCB_NODE_TYPE)|0x02);
          LEN = 0;
          break;
        }
        /* Read or write sectors */
        K = I+0x200>LEN? LEN-I:0x200;
        if(V==CMD_READ)
        {
          last_command_read = true;
          for(J=0;J<K;++J,++BUF) 
          {
              HoldingBuf[I+J] = Data[J];
          }
        }
        else
        {
          last_command_read = false;
          for(J=0;J<K;++J,++BUF) Data[J] = RAM(BUF);
        }
        /* If disk access failed, stop here */
        if(J<K)
        {
          SetDCB(Dev,DCB_NODE_TYPE,GetDCB(Dev,DCB_NODE_TYPE)|0x06);
          LEN = 0;
          break;
        }
      }
      /* Done */
      break;
  }
}

static void UpdateDCB(byte Dev,int V)
{
  byte DevID;

  /* When writing, ignore invalid commands */
  if(!V || (V>=0x80)) return;

  /* Compute device ID */
  DevID = (GetDCB(Dev,DCB_DEV_NUM)<<4) + (GetDCB(Dev,DCB_ADD_CODE)&0x0F);

  /* Depending on the device ID... */
  switch(DevID)
  {
    case 0x01: UpdateKBD(Dev,V);break;
    case 0x02: UpdatePRN(Dev,V);break;
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07: UpdateDSK(DiskID=DevID-4,Dev,V);break;
    case 0x08:
    case 0x09:
    case 0x18:
    case 0x19: UpdateTAP((DevID>>4)+((DevID&1)<<1),Dev,V);break;
    case 0x52: UpdateDSK(DiskID,Dev,-2);break;

    default:
      SetDCB(Dev,DCB_CMD_STAT,RSP_ACK+0x0B);
      break;
  }
}

/** ReadPCB() ************************************************/
/** Read value from a given PCB or DCB address.             **/
/*************************************************************/
void ReadPCB(word A)
{
  if(!IsPCB(A)) return;

  /* Compute offset within PCB/DCB */
  A -= PCBAddr;

  /* If reading a PCB status... */
  if(A==PCB_CMD_STAT)
  {
    /* Do nothing */
  }
  /* If reading status from a device... */
  else if(!((A-PCB_SIZE)%DCB_SIZE))
  {
    byte Dev = (A-PCB_SIZE)/DCB_SIZE;
    if(Dev<=GetMaxDCB()) UpdateDCB(Dev,-1);
  }
}

/** WritePCB() ***********************************************/
/** Write value to a given PCB or DCB address.              **/
/*************************************************************/
void WritePCB(word A,byte V)
{
  if(!IsPCB(A)) return;

  /* Compute offset within PCB/DCB */
  A -= PCBAddr;

  /* If writing a PCB command... */
  if(A==PCB_CMD_STAT)
  {
    switch(V)
    {
      case CMD_PCB_SYNC1: /* Sync Z80 */
        SetPCB(PCB_CMD_STAT,RSP_STATUS|V);
        break;
      case CMD_PCB_SYNC2: /* Sync master 6801 */
        SetPCB(PCB_CMD_STAT,RSP_STATUS|V);
        break;
      case CMD_PCB_SNA: /* Rellocate PCB */
        MovePCB(GetPCBBase(),GetMaxDCB());
        SetPCB(PCB_CMD_STAT,RSP_STATUS|V);
        break;
      case CMD_PCB_IDLE:
      case CMD_PCB_WAIT:
        break;
      case CMD_PCB_RESET:
        memset(PCBTable,0,sizeof(PCBTable));
        break;
      default:
        memset(PCBTable,0,sizeof(PCBTable));
        break;
    }
  }
  /* If writing a DCB command... */
  else if(!((A-PCB_SIZE)%DCB_SIZE))
  {
    byte Dev = (A-PCB_SIZE)/DCB_SIZE;
    if(Dev<=GetMaxDCB()) UpdateDCB(Dev,V);
  }
}

/** ResetPCB() ***********************************************/
/** Reset PCB and attached hardware.                        **/
/*************************************************************/
void ResetPCB(void)
{
  /* PCB/DCB not mapped yet */
  memset(PCBTable,0,sizeof(PCBTable));

  /* Set starting PCB address */
  PCBAddr = 0x0000;
  MovePCB(0xFEC0,15);

  /* @@@ Reset tape and disk here */
  KBDStatus = RSP_STATUS;
  LastKey   = 0x00;
}

/** ChangeTape() *********************************************/
/** Change tape image in a given drive. Closes current tape **/
/** image if Name=0 was given. Creates a new tape image if  **/
/** Name="" was given. Returns 1 on success or 0 on failure.**/
/*************************************************************/
byte ChangeTape(byte N,const char *FileName)
{
  byte *P;

  /* We only have MAX_TAPES drives */
  if(N>=MAX_TAPES) return(0);

  /* Eject disk if requested */
  if(!FileName) { EjectFDI(&Tapes[N]);return(1); }

  /* If FileName not empty, try loading tape image */
  if(*FileName && LoadFDI(&Tapes[N],FileName,FMT_DDP))
  {
    /* Done */
    return(1);
  }

  /* If no existing file, create a new 256kB tape image */
  P = FormatFDI(&Tapes[N],FMT_DDP);
  return(!!P);
}

/** ChangeDisk() *********************************************/
/** Change disk image in a given drive. Closes current disk **/
/** image if Name=0 was given. Creates a new disk image if  **/
/** Name="" was given. Returns 1 on success or 0 on failure.**/
/*************************************************************/
byte ChangeDisk(byte N,const char *FileName)
{
  byte *P;

  /* We only have MAX_DISKS drives */
  if(N>=MAX_DISKS) return(0);

  /* Eject disk if requested */
  if(!FileName) { EjectFDI(&Disks[N]);return(1); }

  /* If FileName not empty, try loading disk image */
  if(*FileName && LoadFDI(&Disks[N],FileName,FMT_ADMDSK))
  {
    /* Done */
    return(1);
  }

  /* If no existing file, create a new 160kB disk image */
  P = FormatFDI(&Disks[N],FMT_ADMDSK);
  return(!!P);
}

