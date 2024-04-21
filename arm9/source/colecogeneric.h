#ifndef _colecoDS_GENERIC_H_
#define _colecoDS_GENERIC_H_

#define MAX_ROMS        1500
#define MAX_ROM_NAME    160

#define MAX_CONFIGS     1950
#define CONFIG_VER      0x0013

#define COLROM          0x01
#define DIRECT          0x02

#define ID_SHM_CANCEL   0x00
#define ID_SHM_YES      0x01
#define ID_SHM_NO       0x02

#define DPAD_NORMAL     0
#define DPAD_DIAGONALS  1

#define CPU_CLEAR_INT_ON_VDP_READ   0
#define CPU_CLEAR_INT_AUTOMATICALLY 1

#define COLECO_RAM_NO_MIRROR        0
#define COLECO_RAM_NORMAL_MIRROR    1

#define CV_MODE_NORMAL              0
#define CV_MODE_ADAM                1
#define CV_MODE_NOSGM               2
#define CV_MODE_ACTCART             3
#define CV_MODE_SUPERCART           4

typedef struct {
  char szName[MAX_ROM_NAME+1];
  u8 uType;
  u32 uCrc;
} FICcoleco;


struct __attribute__((__packed__)) GlobalConfig_t
{
    u16 config_ver;
    u32 bios_checksums;
    char szLastRom[MAX_ROM_NAME+1];
    char szLastPath[MAX_ROM_NAME+1];
    char reserved1[MAX_ROM_NAME+1];
    char reserved2[MAX_ROM_NAME+1];
    u8  showBiosInfo;
    u8  showFPS;
    u8  defaultMSX;
    u8  emuText;
    u8  msxCartOverlay;
    u8  defSprites;
    u8  diskSfxMute;
    u8  biosDelay;
    u8  global_09;
    u8  global_10;
    u8  global_11;
    u8  global_12;
    u8  global_13;
    u8  global_14;
    u8  debugger;
    u32 config_checksum;
};

struct __attribute__((__packed__)) Config_t
{
    u32 game_crc;
    u8  keymap[12];
    u8  frameSkip;
    u8  frameBlend;
    u8  msxMapper;
    u8  autoFire;
    u8  isPAL;
    u8  overlay;
    u8  maxSprites;
    u8  vertSync;
    u8  spinSpeed;
    u8  touchPad;
    u8  reserved0;
    u8  msxBios;
    u8  msxKey5;
    u8  dpad;
    u8  memWipe;
    u8  clearInt;
    u8  cvEESize;
    u8  adamnet;
    u8  mirrorRAM;
    u8  msxBeeper;
    u8  cvisionLoad;
    u8  gameSpeed;
    u8  keyMute;
    u8  ein_ctc3;
    u8  cvMode;
    u8  reserved2;
    u8  reserved3;
    u8  reserved4;
    u8  reserved5;
    u8  reserved6;
    u8  reserved7;
    u8  reserved8;
    u8  reserved9;
    u8  reserved10;
};
 

extern struct Config_t myConfig;
extern struct GlobalConfig_t myGlobalConfig;

extern u8 last_special_key;
extern u8 last_special_key_dampen;
extern char msx_rom_str[];
extern char msx_rom_str_short[];

extern void LoadConfig(void);

extern FICcoleco gpFic[MAX_ROMS];  
extern int uNbRoms;
extern int ucGameAct;
extern int ucGameChoice;

extern u16 msx_init;
extern u16 msx_basic;

extern u8 showMessage(char *szCh1, char *szCh2);
extern void colecoDSModeNormal(void);
extern void colecoDSInitScreenUp(void);
extern void colecoDSFindFiles(void);
extern void colecoDSChangeOptions(void);
extern void MSX_InitialMemoryLayout(u32 romSize);

extern void DSPrint(int iX,int iY,int iScr,char *szMessage);
extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);

extern void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait);
extern u8 colecoDSLoadFile(void);
extern void DisplayFileName(void);

#endif
