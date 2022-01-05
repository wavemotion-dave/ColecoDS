#ifndef _colecoDS_GENERIC_H_
#define _colecoDS_GENERIC_H_

#define MAX_ROMS        1024
#define MAX_ROM_LENGTH  160

#define COLROM 0x01
#define DIRECT 0x02

#define ID_SHM_CANCEL 0x00
#define ID_SHM_YES    0x01
#define ID_SHM_NO     0x02

typedef struct {
  char szName[MAX_ROM_LENGTH];
  u8 uType;
  u32 uCrc;
} FICcoleco;

#define MAX_CONFIGS 700
#define CONFIG_VER  0x0004

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
    u8  autoFire2;
    u8  overlay;
    u8  maxSprites;
    u8  vertSync;
    u8  spinSpeed;
    u8  touchPad;
    u8  cpuCore;
    u32 reserved9;
    u32 reservedA;
    u32 reservedB;
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

extern void AffChaine(int iX,int iY,int iScr,char *szMessage);
extern void dsPrintValue(int iX,int iY,int iScr,char *szMessage);
extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);

extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);

#endif
