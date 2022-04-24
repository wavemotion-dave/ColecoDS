/** EMULib Emulation Library *********************************/
/**                                                         **/
/**                          C24XX.h                        **/
/**                                                         **/
/** This file contains emulation for the 24cXX series of    **/
/** serial EEPROMs. See C24XX.c for the actual code.        **/
/**                                                         **/
/** Copyright (C) Marat Fayzullin 2017-2021                 **/
/**     You are not allowed to distribute this software     **/
/**     commercially. Please, notify me, if you make any    **/
/**     changes to this file.                               **/
/*************************************************************/
#ifndef C24XX_H
#define C24XX_H
#ifdef __cplusplus
extern "C" {
#endif

#define C24XX_CHIP    0x0F
#define C24XX_24C01   0x00 // 128 bytes   (7bit, 8/page)
#define C24XX_24C02   0x01 // 256 bytes   (8bit, 8/page)
#define C24XX_24C04   0x02 // 512 bytes   (1+8bit, 16/page)
#define C24XX_24C08   0x03 // 1024 bytes  (2+8bit, 16/page)
#define C24XX_24C16   0x04 // 2048 bytes  (3+8bit, 16/page)
#define C24XX_24C32   0x05 // 4096 bytes  (12bit, 32/page)
#define C24XX_24C64   0x06 // 8192 bytes  (13bit, 32/page)
#define C24XX_24C128  0x07 // 16384 bytes (14bit, 64/page)
#define C24XX_24C256  0x08 // 32768 bytes (15bit, 64/page)
#define C24XX_DEBUG   0x10

/* Alternative chip names */
#define C24XX_128B    C24XX_24C01
#define C24XX_256B    C24XX_24C02
#define C24XX_512B    C24XX_24C04
#define C24XX_1kB     C24XX_24C08
#define C24XX_2kB     C24XX_24C16
#define C24XX_4kB     C24XX_24C32
#define C24XX_8kB     C24XX_24C64
#define C24XX_16kB    C24XX_24C128
#define C24XX_32kB    C24XX_24C256

#define C24XX_SDA     0x01
#define C24XX_SCL     0x02
#define C24XX_ADDR    0x1C
#define C24XX_A0      0x04
#define C24XX_A1      0x08
#define C24XX_A2      0x10

#ifndef BYTE_TYPE_DEFINED
#define BYTE_TYPE_DEFINED
typedef unsigned char byte;
#endif

#ifndef WORD_TYPE_DEFINED
#define WORD_TYPE_DEFINED
typedef unsigned short word;
#endif

#pragma pack(4)
typedef struct
{
  unsigned int Flags; /* Chip type, etc  */
  unsigned int Addr;  /* Current address */
  int  Rsrvd;         /* Reserved field  */
  byte State;         /* Current state   */
  byte Cmd;           /* Current command */
  word Bits;          /* In/out bits     */
  byte Pins;          /* Input pins      */
  byte Out;           /* Output pins     */
  byte Data[0x8000];  /* EEPROM data     */
} C24XX;
#pragma pack()

/** Reset24XX ************************************************/
/** Reset the 24xx chip.                                    **/
/*************************************************************/
void Reset24XX(C24XX *D,unsigned int Flags);

/** Write24XX ************************************************/
/** Write value V into the 24xx chip. Only bits 0,1 are     **/
/** used (see #defines).                                    **/
/*************************************************************/
byte Write24XX(C24XX *D,byte V);

/** Read24XX *************************************************/
/** Read value from the 24xx chip. Only bits 0,1 are used   **/
/** (see #defines).                                         **/
/*************************************************************/
byte Read24XX(C24XX *D);

/** Size24XX *************************************************/
/** Return the size of given chip model in bytes.           **/
/*************************************************************/
unsigned int Size24XX(C24XX *D);

#ifdef __cplusplus
}
#endif
#endif /* C24XX_H */
