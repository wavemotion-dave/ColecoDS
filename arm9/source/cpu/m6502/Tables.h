/** M6502: portable 6502 emulator ****************************/
/**                                                         **/
/**                          Tables.h                       **/
/**                                                         **/
/** This file contains tables of used by 6502 emulation to  **/
/** compute NEGATIVE and ZERO flags. There are also timing  **/
/** tables for 6502 opcodes. This file is included from     **/
/** 6502.c.                                                 **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 1996-2007                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/   
/**     changes to this file.                               **/
/*************************************************************/

static const byte Cycles[256] __attribute__((section(".dtcm"))) =
{
  7,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6,  // 0x
  2,5,2,8,4,4,6,6,2,4,2,7,5,5,7,7,  // 1x
  6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6,  // 2x
  2,5,2,8,4,4,6,6,2,4,2,7,5,5,7,7,  // 3x
  6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6,  // 4x
  2,5,2,8,4,4,6,6,2,4,2,7,5,5,7,7,  // 5x
  6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6,  // 6x
  2,5,2,8,4,4,6,6,2,4,2,7,5,5,7,7,  // 7x
  2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,  // 8x
  2,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,  // 9x
  2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,  // Ax
  2,5,2,5,4,4,4,4,2,4,2,5,4,4,4,4,  // Bx
  2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,  // Cx
  2,5,2,8,4,4,6,6,2,4,2,7,5,5,7,7,  // Dx
  2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,  // Ex
  2,5,2,8,4,4,6,6,2,4,2,7,5,5,7,7   // Fx
};

static const byte ZNTable[256] __attribute__((section(".dtcm"))) =
{
  Z_FLAG,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
  N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,N_FLAG,
};
