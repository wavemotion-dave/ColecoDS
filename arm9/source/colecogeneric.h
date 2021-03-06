#ifndef _colecoDS_GENERIC_H_
#define _colecoDS_GENERIC_H_

#define MAX_ROMS        1500
#define MAX_ROM_LENGTH  160

#define MAX_CONFIGS      1360
#define CONFIG_VER       0x000A
#define CONFIG_VER_OLD1  0x0008
#define CONFIG_VER_OLD2  0x0008

#define COLROM        0x01
#define DIRECT        0x02

#define ID_SHM_CANCEL 0x00
#define ID_SHM_YES    0x01
#define ID_SHM_NO     0x02

#define DPAD_NORMAL     0
#define DPAD_DIAGONALS  1

#define CPU_CLEAR_INT_ON_VDP_READ   0
#define CPU_CLEAR_INT_AUTOMATICALLY 1

#define COLECO_RAM_NORMAL_MIRROR   0
#define COLECO_RAM_NO_MIRROR       1

typedef struct {
  char szName[MAX_ROM_LENGTH];
  u8 uType;
  u32 uCrc;
} FICcoleco;

struct __attribute__((__packed__)) Config_t
{
    u16 config_ver;
    u32 game_crc;
    u8  keymap[12];
    u8  showFPS;
    u8  frameSkip;
    u8  frameBlend;
    u8  msxMapper;
    u8  autoFire1;
    u8  isPAL;
    u8  overlay;
    u8  maxSprites;
    u8  vertSync;
    u8  spinSpeed;
    u8  touchPad;
    u8  cpuCore;
    u8  msxBios;
    u8  msxKey5;
    u8  dpad;
    u8  memWipe;
    u8  clearInt;
    u8  cvEESize;
    u8  ayEnvelope;
    u8  colecoRAM;
    u8  msxBeeper;
    u8  reservedA3;
    u8  reservedB0;
    u8  reservedB1;
    u8  reservedB2;
    u8  reservedB3;
    u32 reservedC;
};
 

extern struct Config_t myConfig;

extern void FindAndLoadConfig(void);

extern FICcoleco gpFic[MAX_ROMS];  
extern int uNbRoms;
extern int ucGameAct;
extern int ucGameChoice;

extern u8 showMessage(char *szCh1, char *szCh2);
extern void colecoDSModeNormal(void);
extern void colecoDSInitScreenUp(void);
extern void colecoDSFindFiles(void);
extern void colecoDSChangeOptions(void);
extern void MSX_InitialMemoryLayout(u32 iSSize);

extern void AffChaine(int iX,int iY,int iScr,char *szMessage);
extern void dsPrintValue(int iX,int iY,int iScr,char *szMessage);
extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);

extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern u8 colecoDSLoadFile(void);
extern void DisplayFileName(void);

#endif
