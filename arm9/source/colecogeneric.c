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

#include <stdlib.h>
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "colecoDS.h"
#include "colecomngt.h"
#include "colecogeneric.h"
#include "cpu/z80/ctc.h"
#include "options.h"
#include "topscreen.h"
#include "C24XX.h"

#include "CRC32.h"
#include "printf.h"

typedef enum {FT_NONE,FT_FILE,FT_DIR} FILE_TYPE;

int countCV=0;
int ucGameAct=0;
int ucGameChoice = -1;
FICcoleco gpFic[MAX_ROMS];  
char szName[256];
char szFile[256];
u8 bForceMSXLoad = false;
u32 file_size = 0;
char strBuf[40];

struct Config_t AllConfigs[MAX_CONFIGS];
struct Config_t myConfig __attribute((aligned(4))) __attribute__((section(".dtcm")));
struct GlobalConfig_t myGlobalConfig;
extern u32 file_crc;

u8 option_table=0;

const char szKeyName[MAX_KEY_OPTIONS][18] = {
  "P1 JOY UP",
  "P1 JOY DOWN",
  "P1 JOY LEFT",
  "P1 JOY RIGHT",
  "P1 BTN 1 (L/YLW)",
  "P1 BTN 2 (R/RED)",
  "P1 BTN 3 (PURP)",
  "P1 BTN 4 (BLUE)",
  "P1 KEYPAD #1",        
  "P1 KEYPAD #2",
  "P1 KEYPAD #3",        
  "P1 KEYPAD #4",
  "P1 KEYPAD #5",
  "P1 KEYPAD #6",
  "P1 KEYPAD #7",
  "P1 KEYPAD #8",
  "P1 KEYPAD #9",
  "P1 KEYPAD ##",
  "P1 KEYPAD #0",
  "P1 KEYPAD #*",
    
  "P2 JOY UP",
  "P2 JOY DOWN",
  "P2 JOY LEFT",
  "P2 JOY RIGHT",
  "P2 BTN 1 (L/YLW)",
  "P2 BTN 2 (R/RED)",
  "P2 BTN 3 (PURP)",
  "P2 BTN 4 (BLUE)",
  "P2 KEYPAD #1",        
  "P2 KEYPAD #2",
  "P2 KEYPAD #3",        
  "P2 KEYPAD #4",
  "P2 KEYPAD #5",
  "P2 KEYPAD #6",
  "P2 KEYPAD #7",
  "P2 KEYPAD #8",
  "P2 KEYPAD #9",
  "P2 KEYPAD ##",
  "P2 KEYPAD #0",
  "P2 KEYPAD #*",
    
  "SAC SPIN X+",          
  "SAC SPIN X-",          
  "SAC SPIN Y+",
  "SAC SPIN Y-",
  
  "KEYBOARD A", //45
  "KEYBOARD B",
  "KEYBOARD C",
  "KEYBOARD D",
  "KEYBOARD E",
  "KEYBOARD F",
  "KEYBOARD G",
  "KEYBOARD H",
  "KEYBOARD I",
  "KEYBOARD J",
  "KEYBOARD K",
  "KEYBOARD L",
  "KEYBOARD M",
  "KEYBOARD N",
  "KEYBOARD O",
  "KEYBOARD P",
  "KEYBOARD Q", // 60
  "KEYBOARD R",
  "KEYBOARD S",
  "KEYBOARD T",
  "KEYBOARD U",
  "KEYBOARD V",
  "KEYBOARD W",
  "KEYBOARD X",
  "KEYBOARD Y",
  "KEYBOARD Z",
    
  "KEYBOARD 0", // 70
  "KEYBOARD 1",
  "KEYBOARD 2",
  "KEYBOARD 3",
  "KEYBOARD 4",
  "KEYBOARD 5",
  "KEYBOARD 6",
  "KEYBOARD 7",
  "KEYBOARD 8",
  "KEYBOARD 9",
    
  "KEYBOARD SHIFT",
  "KEYBOARD CTRL",
  "KEYBOARD CODE",
  "KEYBOARD GRAPH",
    
  "KEYBOARD SPACE",
  "KEYBOARD RETURN",
  "KEYBOARD ESC",
    
  "KEYBOARD HOME",
  "KEYBOARD UP",
  "KEYBOARD DOWN",
  "KEYBOARD LEFT",
  "KEYBOARD RIGHT",
    
  "KEYBOARD PERIOD",
  "KEYBOARD COMMA",
  "KEYBOARD COLON",
  "KEYBOARD SEMI",
  "KEYBOARD QUOTE",
  "KEYBOARD SLASH",
  "KEYBOARD BSLASH",
  "KEYBOARD PLUS",
  "KEYBOARD MINUS",
  "KEYBOARD LBRACKET",
  "KEYBOARD RBRACKET",
  "KEYBOARD CARET",
  "KEYBOARD ASTERISK",
  "KEYBOARD ATSIGN",
  "KEYBOARD BS",
  "KEYBOARD TAB",
  "KEYBOARD INS",
  "KEYBOARD DEL",
  "KEYBOARD CLEAR",
  "KEYBOARD UNDO",
  "KEYBOARD MOVE",
  "KEYBOARD WILDCARD",
  "KEYBOARD STORE",
  "KEYBOARD PRINT",
  "KEYBOARD STOP/BRK",
  "KEYBOARD F1 (I)",
  "KEYBOARD F2 (II)",
  "KEYBOARD F3 (III)",
  "KEYBOARD F4 (IV)",
  "KEYBOARD F5 (V)",
  "KEYBOARD F6 (VI)",
  "KEYBOARD F7",
  "KEYBOARD F8",    
};


/*********************************************************************************
 * Show A message with YES / NO
 ********************************************************************************/
u8 showMessage(char *szCh1, char *szCh2) {
  u16 iTx, iTy;
  u8 uRet=ID_SHM_CANCEL;
  u8 ucGau=0x00, ucDro=0x00,ucGauS=0x00, ucDroS=0x00, ucCho = ID_SHM_YES;
    
  BottomScreenOptions();

  DSPrint(16-strlen(szCh1)/2,10,6,szCh1);
  DSPrint(16-strlen(szCh2)/2,12,6,szCh2);
  DSPrint(8,14,6,("> YES <"));
  DSPrint(20,14,6,("  NO   "));
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  while (uRet == ID_SHM_CANCEL) 
  {
    WAITVBL;
    if (keysCurrent() & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ( (iTx>8*8) && (iTx<8*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucGauS) {
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
          ucGauS = 1;
          if (ucCho == ID_SHM_YES) {
            uRet = ucCho;
          }
          else {
            ucCho  = ID_SHM_YES;
          }
        }
      }
      else
        ucGauS = 0;
      if ( (iTx>20*8) && (iTx<20*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucDroS) {
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
          ucDroS = 1;
          if (ucCho == ID_SHM_NO) {
            uRet = ucCho;
          }
          else {
            ucCho = ID_SHM_NO;
          }
        }
      }
      else
        ucDroS = 0;
    }
    else {
      ucDroS = 0;
      ucGauS = 0;
    }
    
    if (keysCurrent() & KEY_LEFT){
      if (!ucGau) {
        ucGau = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      } 
    }
    else {
      ucGau = 0;
    }  
    if (keysCurrent() & KEY_RIGHT) {
      if (!ucDro) {
        ucDro = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho  = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      } 
    }
    else {
      ucDro = 0;
    }  
    if (keysCurrent() & KEY_A) {
      uRet = ucCho;
    }
  }
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);
  
  BottomScreenKeypad();  // Could be generic or overlay...
  
  return uRet;
}

void colecoDSModeNormal(void) {
  REG_BG3CNT = BG_BMP8_256x256;
  REG_BG3PA = (1<<8); 
  REG_BG3PB = 0;
  REG_BG3PC = 0;
  REG_BG3PD = (1<<8);
  REG_BG3X = 0;
  REG_BG3Y = 0;
}

//*****************************************************************************
// Put the top screen in refocused bitmap mode
//*****************************************************************************
void colecoDSInitScreenUp(void) {
  videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
  vramSetBankB(VRAM_B_MAIN_SPRITE);
  colecoDSModeNormal();
}

// ----------------------------------------------------------------------------
// This stuff handles the 'random' screen snapshot at the top screen...
// ----------------------------------------------------------------------------
void showRandomPreviewSnaps(void) {
  u16 *pusEcran=(u16*) bgGetMapPtr(bg1);
  u32 uX,uY;

  if (vusCptVBL>=5*60) {
    u8 uEcran = rand() % 6;
    vusCptVBL = 0;
    if (uEcran>2) {
      uEcran-=3;
      for (uY=24;uY<33;uY++) {
        for (uX=0;uX<12;uX++) {
          *(pusEcran + (14+uX) + ((10+uY-24)<<5)) = *(bgGetMapPtr(bg0) + (uY+uEcran*9)*32 + uX+12);
        }
      }
    }
    else
    {
      for (uY=24;uY<33;uY++) {
        for (uX=0;uX<12;uX++) {
          *(pusEcran + (14+uX) + ((10+uY-24)<<5)) = *(bgGetMapPtr(bg0) + (uY+uEcran*9)*32 + uX);
        }
      }
    }
  }
}

/*********************************************************************************
 * Show The 14 games on the list to allow the user to choose a new game.
 ********************************************************************************/
static char szName2[40];
void dsDisplayFiles(u16 NoDebGame, u8 ucSel) 
{
  u16 ucBcl,ucGame;
  u8 maxLen;
  
  DSPrint(30,8,0,(NoDebGame>0 ? "<" : " "));
  DSPrint(30,21,0,(NoDebGame+14<countCV ? ">" : " "));
  sprintf(szName,"%03d/%03d FILES AVAILABLE     ",ucSel+1+NoDebGame,countCV);
  DSPrint(2,6,0, szName);
  for (ucBcl=0;ucBcl<14; ucBcl++) 
  {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < countCV) 
    {
      maxLen=strlen(gpFic[ucGame].szName);
      strcpy(szName,gpFic[ucGame].szName);
      if (maxLen>28) szName[28]='\0';
      if (gpFic[ucGame].uType == DIRECT) 
      {
        szName[26] = 0; // Needs to be 2 chars shorter with brackets
        sprintf(szName2, "[%s]",szName);
        sprintf(szName,"%-28s",szName2);
        DSPrint(1,8+ucBcl,(ucSel == ucBcl ? 2 :  0),szName);
      }
      else 
      {
        sprintf(szName,"%-28s",strupr(szName));
        DSPrint(1,8+ucBcl,(ucSel == ucBcl ? 2 : 0 ),szName);
      }
    }
    else
    {
        DSPrint(1,8+ucBcl,(ucSel == ucBcl ? 2 : 0 ),"                            ");
    }
  }
}


// -------------------------------------------------------------------------
// Standard qsort routine for the coleco games - we sort all directory
// listings first and then a case-insenstive sort of all games.
// -------------------------------------------------------------------------
int colecoFilescmp (const void *c1, const void *c2) 
{
  FICcoleco *p1 = (FICcoleco *) c1;
  FICcoleco *p2 = (FICcoleco *) c2;

  if (p1->szName[0] == '.' && p2->szName[0] != '.')
      return -1;
  if (p2->szName[0] == '.' && p1->szName[0] != '.')
      return 1;
  if ((p1->uType == DIRECT) && !(p2->uType == DIRECT))
      return -1;
  if ((p2->uType == DIRECT) && !(p1->uType == DIRECT))
      return 1;
  return strcasecmp (p1->szName, p2->szName);        
}

/*********************************************************************************
 * Find files (COL / ROM) available - sort them for display.
 ********************************************************************************/
void colecoDSFindFiles(void) 
{
  u32 uNbFile;
  DIR *dir;
  struct dirent *pent;

  uNbFile=0;
  countCV=0;

  dir = opendir(".");
  while (((pent=readdir(dir))!=NULL) && (uNbFile<MAX_ROMS)) 
  {
    strcpy(szFile,pent->d_name);
      
    if(pent->d_type == DT_DIR) 
    {
      if (!((szFile[0] == '.') && (strlen(szFile) == 1))) 
      {
        // Do not include the [sav] directory
        if (strcasecmp(szFile, "sav") != 0)
        {
            strcpy(gpFic[uNbFile].szName,szFile);
            gpFic[uNbFile].uType = DIRECT;
            uNbFile++;
            countCV++;
        }
      }
    }
    else {
      if ((strlen(szFile)>4) && (strlen(szFile)<(MAX_ROM_NAME-4)) && (szFile[0] != '.') && (szFile[0] != '_'))  // For MAC don't allow underscore files
      {
        if ( (strcasecmp(strrchr(szFile, '.'), ".rom") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".col") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".bin") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".sg") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".sc") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".pv") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".m5") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".mtx") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".run") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".msx") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".cas") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".ddp") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".dsk") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".ein") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".pen") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".com") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".adm") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".cv") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
      }
    }
  }
  closedir(dir);
    
  // ----------------------------------------------
  // If we found any files, go sort the list...
  // ----------------------------------------------
  if (countCV)
  {
    qsort (gpFic, countCV, sizeof(FICcoleco), colecoFilescmp);
  }    
}


// ----------------------------------------------------------------
// Let the user select a new game (rom) file and load it up!
// ----------------------------------------------------------------
u8 colecoDSLoadFile(void) 
{
  bool bDone=false;
  u16 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00, romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage;
  s16 uLenFic=0, ucFlip=0, ucFlop=0;

  // Show the menu...
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B))!=0);
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
  DSPrint(7,5,0,"A=SELECT,  B=EXIT");

  colecoDSFindFiles();
    
  ucGameChoice = -1;

  nbRomPerPage = (countCV>=14 ? 14 : countCV);
  uNbRSPage = (countCV>=5 ? 5 : countCV);
  
  if (ucGameAct>countCV-nbRomPerPage)
  {
    firstRomDisplay=countCV-nbRomPerPage;
    romSelected=ucGameAct-countCV+nbRomPerPage;
  }
  else
  {
    firstRomDisplay=ucGameAct;
    romSelected=0;
  }
  dsDisplayFiles(firstRomDisplay,romSelected);
    
  // -----------------------------------------------------
  // Until the user selects a file or exits the menu...
  // -----------------------------------------------------
  while (!bDone)
  {
    if (keysCurrent() & KEY_UP)
    {
      if (!ucHaut)
      {
        ucGameAct = (ucGameAct>0 ? ucGameAct-1 : countCV-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=countCV-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {

        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;     
    }
    else
    {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN)
    {
      if (!ucBas) {
        ucGameAct = (ucGameAct< countCV-1 ? ucGameAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<countCV-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;     
    }
    else {
      ucBas = 0;
    }
      
    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_RIGHT)
    {
      if (!ucSBas)
      {
        ucGameAct = (ucGameAct< countCV-nbRomPerPage ? ucGameAct+nbRomPerPage : countCV-nbRomPerPage);
        if (firstRomDisplay<countCV-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = countCV-nbRomPerPage; }
        if (ucGameAct == countCV-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;     
    }
    else {
      ucSBas = 0;
    }
      
    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_LEFT)
    {
      if (!ucSHaut)
      {
        ucGameAct = (ucGameAct> nbRomPerPage ? ucGameAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucGameAct == 0) romSelected = 0;
        if (romSelected > ucGameAct) romSelected = ucGameAct;          
        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;     
    }
    else {
      ucSHaut = 0;
    }
    
    // -------------------------------------------------------------------------
    // They B key will exit out of the ROM selection without picking a new game
    // -------------------------------------------------------------------------
    if ( keysCurrent() & KEY_B )
    {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }
      
    // -------------------------------------------------------------------
    // Any of these keys will pick the current ROM and try to load it...
    // -------------------------------------------------------------------
    if (keysCurrent() & KEY_A || keysCurrent() & KEY_Y || keysCurrent() & KEY_X)
    {
      if (gpFic[ucGameAct].uType != DIRECT)
      {
        bDone=true;
        ucGameChoice = ucGameAct;
        bForceMSXLoad = false;
        if (keysCurrent() & KEY_X) bForceMSXLoad=true;
        WAITVBL;
      }
      else
      {
        chdir(gpFic[ucGameAct].szName);
        colecoDSFindFiles();
        ucGameAct = 0;
        nbRomPerPage = (countCV>=14 ? 14 : countCV);
        uNbRSPage = (countCV>=5 ? 5 : countCV);
        if (ucGameAct>countCV-nbRomPerPage) {
          firstRomDisplay=countCV-nbRomPerPage;
          romSelected=ucGameAct-countCV+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucGameAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }
    
    // --------------------------------------------
    // If the filename is too long... scroll it.
    // --------------------------------------------
    if (strlen(gpFic[ucGameAct].szName) > 29) 
    {
      ucFlip++;
      if (ucFlip >= 25) 
      {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+28)>strlen(gpFic[ucGameAct].szName)) 
        {
          ucFlop++;
          if (ucFlop >= 15) 
          {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,gpFic[ucGameAct].szName+uLenFic,28);
        szName[28] = '\0';
        DSPrint(1,8+romSelected,2,szName);
      }
    }
    showRandomPreviewSnaps();
    swiWaitForVBlank();
  }
    
  // Wait for some key to be pressed before returning
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B | KEY_R | KEY_L | KEY_UP | KEY_DOWN))!=0);
  
  return 0x01;
}


// ---------------------------------------------------------------------------
// Write out the ColecoDS.DAT configuration file to capture the settings for
// each game.  This one file contains global settings + 400 game settings.
// ---------------------------------------------------------------------------
void SaveConfig(bool bShow)
{
    FILE *fp;
    int slot = 0;
    
    if (bShow) DSPrint(6,0,0, (char*)"SAVING CONFIGURATION");

    // Set the global configuration version number...
    myGlobalConfig.config_ver = CONFIG_VER;

    // If there is a game loaded, save that into a slot... re-use the same slot if it exists
    myConfig.game_crc = file_crc;
    
    if (myConfig.gameSpeed)  myConfig.vertSync = 0;      // If game speed isn't 100%, we can't sync to the DS 60Hz
    
    // Find the slot we should save into...
    for (slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == myConfig.game_crc)  // Got a match?!
        {
            break;                           
        }
        if (AllConfigs[slot].game_crc == 0x00000000)  // Didn't find it... use a blank slot...
        {
            break;                           
        }
    }

    // --------------------------------------------------------------------------
    // Copy our current game configuration to the main configuration database...
    // --------------------------------------------------------------------------
    if (myConfig.game_crc != 0x00000000)
    {
        memcpy(&AllConfigs[slot], &myConfig, sizeof(struct Config_t));
    }

    // --------------------------------------------------
    // Now save the config file out o the SD card...
    // --------------------------------------------------
    DIR* dir = opendir("/data");
    if (dir)
    {
        closedir(dir);  // Directory exists.
    }
    else
    {
        mkdir("/data", 0777);   // Doesn't exist - make it...
    }
    fp = fopen("/data/ColecoDS.DAT", "wb+");
    if (fp != NULL)
    {
        fwrite(&myGlobalConfig, sizeof(myGlobalConfig), 1, fp); // Write the global config
        fwrite(&AllConfigs, sizeof(AllConfigs), 1, fp);         // Write the array of all configurations
        fclose(fp);
    } else DSPrint(4,0,0, (char*)"ERROR SAVING CONFIG FILE");

    if (bShow) 
    {
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,0,0, (char*)"                        ");
    }
}

void MapPlayer1(void)
{
    myConfig.keymap[0]   = 0;    // NDS D-Pad mapped to CV Joystick UP
    myConfig.keymap[1]   = 1;    // NDS D-Pad mapped to CV Joystick DOWN
    myConfig.keymap[2]   = 2;    // NDS D-Pad mapped to CV Joystick LEFT
    myConfig.keymap[3]   = 3;    // NDS D-Pad mapped to CV Joystick RIGHT
    myConfig.keymap[4]   = 4;    // NDS A Button mapped to CV Button 1 (Yellow / Left Button)
    myConfig.keymap[5]   = 5;    // NDS B Button mapped to CV Button 2 (Red / Right Button)
    myConfig.keymap[6]   = 6;    // NDS X Button mapped to CV Button 3 (Purple / Super Action)
    myConfig.keymap[7]   = 7;    // NDS Y Button mapped to CV Button 4 (Blue / Super Action)
    myConfig.keymap[8]   = 81;   // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;   // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 8;    // NDS Start  mapped to Keypad #1
    myConfig.keymap[11]  = 9;    // NDS Select mapped to Keypad #2
}

void MapPlayer2(void)
{
    myConfig.keymap[0]   = 20;    // NDS D-Pad mapped to CV Joystick UP
    myConfig.keymap[1]   = 21;    // NDS D-Pad mapped to CV Joystick DOWN
    myConfig.keymap[2]   = 22;    // NDS D-Pad mapped to CV Joystick LEFT
    myConfig.keymap[3]   = 23;    // NDS D-Pad mapped to CV Joystick RIGHT
    myConfig.keymap[4]   = 24;    // NDS A Button mapped to CV Button 1 (Yellow / Left Button)
    myConfig.keymap[5]   = 25;    // NDS B Button mapped to CV Button 2 (Red / Right Button)
    myConfig.keymap[6]   = 26;    // NDS X Button mapped to CV Button 3 (Purple / Super Action)
    myConfig.keymap[7]   = 27;    // NDS Y Button mapped to CV Button 4 (Blue / Super Action)
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 28;    // NDS Start  mapped to Keypad #1
    myConfig.keymap[11]  = 29;    // NDS Select mapped to Keypad #2
}

void MapQAOP(void)
{
    myConfig.keymap[0]   = 60;    // Q
    myConfig.keymap[1]   = 44;    // A
    myConfig.keymap[2]   = 58;    // O
    myConfig.keymap[3]   = 59;    // P
    myConfig.keymap[4]   = 84;    // Space
    myConfig.keymap[5]   = 84;    // Space
    myConfig.keymap[6]   = 92;    // Period 
    myConfig.keymap[7]   = 92;    // Period
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}

void MapWASD(void)
{
    myConfig.keymap[0]   = 66;    // W
    myConfig.keymap[1]   = 44;    // A
    myConfig.keymap[2]   = 62;    // S
    myConfig.keymap[3]   = 47;    // D
    myConfig.keymap[4]   = 84;    // Space
    myConfig.keymap[5]   = 84;    // Space
    myConfig.keymap[6]   = 84;    // Space
    myConfig.keymap[7]   = 84;    // Space
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}

void MapZCPeriod(void)
{
    myConfig.keymap[0]   = 60;    // Q
    myConfig.keymap[1]   = 44;    // A
    myConfig.keymap[2]   = 69;    // Z
    myConfig.keymap[3]   = 46;    // C
    myConfig.keymap[4]   = 92;    // Period
    myConfig.keymap[5]   = 92;    // Period
    myConfig.keymap[6]   = 84;    // Space
    myConfig.keymap[7]   = 84;    // Space
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}

void MapZXSpace(void)
{
    myConfig.keymap[0]   = 84;    // Space
    myConfig.keymap[1]   = 92;    // Period
    myConfig.keymap[2]   = 69;    // Z
    myConfig.keymap[3]   = 67;    // X
    myConfig.keymap[4]   = 84;    // Space
    myConfig.keymap[5]   = 84;    // Space
    myConfig.keymap[6]   = 85;    // Return
    myConfig.keymap[7]   = 85;    // Return
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}



void MapArrows(void)
{
    myConfig.keymap[0]   = 88;    // UP Arrow
    myConfig.keymap[1]   = 89;    // Down Arrow
    myConfig.keymap[2]   = 90;    // Left Arrow
    myConfig.keymap[3]   = 91;    // Right Arrow
    myConfig.keymap[4]   = 84;    // Space
    myConfig.keymap[5]   = 84;    // Space
    myConfig.keymap[6]   = 84;    // Space
    myConfig.keymap[7]   = 84;    // Space
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}


void SetDefaultGlobalConfig(void)
{
    // A few global defaults...
    memset(&myGlobalConfig, 0x00, sizeof(myGlobalConfig));
    myGlobalConfig.showBiosInfo   = 1;    // Show BIOS info at startup by default
    myGlobalConfig.showFPS        = 0;    // Don't show FPS counter by default
    myGlobalConfig.defaultMSX     = 1;    // Default to the MSX.ROM if available
    myGlobalConfig.emuText        = 1;    // Default is to show Emulator Text
    myGlobalConfig.msxCartOverlay = 1;    // Default is to show Keyboard for CART games
    myGlobalConfig.defSprites     = 0;    // Default is to show 32 sprites (real hardware is 4 per line)
    myGlobalConfig.diskSfxMute    = 0;    // By default Disk/DDP loading sounds are enabled... 1=Mute
    myGlobalConfig.biosDelay      = 0;    // Normal BIOS delay
}

void SetDefaultGameConfig(void)
{
    myConfig.game_crc    = 0;    // No game in this slot yet
    
    MapPlayer1();                // Default to Player 1 mapping
    
    myConfig.frameSkip   = (isDSiMode() ? 0:1);         // For DSi we don't need FrameSkip, but for older DS-LITE we turn on light frameskip
    myConfig.frameBlend  = 0;                           // No frame blending needed for most games
    myConfig.msxMapper   = GUESS;                       // MSX mapper takes its best guess
    myConfig.autoFire    = 0;                           // Default to no auto-fire on either button
    myConfig.isPAL       = 0;                           // Default to NTSC
    myConfig.overlay     = 0;                           // Default to normal CV overlay
    myConfig.maxSprites  = myGlobalConfig.defSprites;   // 0 means allow 32 sprites... 1 means limit to the original 4 sprites of the VDP
    myConfig.vertSync    = (isDSiMode() ? 1:0);         // Default is Vertical Sync ON for DSi and OFF for DS-LITE
    myConfig.spinSpeed   = 0;                           // Default spin speed is normal
    myConfig.touchPad    = 0;                           // Nothing special about the touch-pad by default
    myConfig.reserved0   = 1;                           // Repurpose (was CPU core)
    myConfig.msxBios     = (bMSXBiosFound ? myGlobalConfig.defaultMSX:0); // Default to real MSX bios unless we can't find it
    myConfig.msxKey5     = 0;                           // Default key map for MSX key 5 (question mark)
    myConfig.dpad        = DPAD_NORMAL;                 // Normal DPAD use - mapped to joystick
    myConfig.memWipe     = 0;                           // Default to RANDOM memory
    myConfig.clearInt    = CPU_CLEAR_INT_AUTOMATICALLY; // By default clear VDP interrupts automatically
    myConfig.cvEESize    = EEPROM_NONE;                 // Default CV EEPROM size is NONE
    myConfig.adamnet     = 0;                           // Adamnet is FAST by default
    myConfig.mirrorRAM   = COLECO_RAM_NORMAL_MIRROR;    // By default use the normal Colecovision (and CreatiVision) memory mirrors
    myConfig.msxBeeper   = 0;                           // Assume no MSX beeper required - only a few games need this
    myConfig.cvisionLoad = 0;                           // Default to normal Legacy A/B load for CreatiVision games
    myConfig.gameSpeed   = 0;                           // Default is 100% game speed
    myConfig.keyMute     = 0;                           // Default is no mute (key click heard)
    myConfig.ein_ctc3    = 0;                           // Default is normal CTC3 handling for Einstein (no fudge factor)
    myConfig.cvMode      = CV_MODE_NORMAL;              // Default is normal detect of Coleco Cart with possible SGM 
    myConfig.soundDriver = SND_DRV_NORMAL;              // Default is normal sound driver (not Wave Direct)
    myConfig.reserved3   = 0;    
    myConfig.reserved4   = 0;    
    myConfig.reserved5   = 0;    
    myConfig.reserved6   = 0;
    myConfig.reserved7   = 0;
    myConfig.reserved8   = 0;
    myConfig.reserved9   = 0xA5;    // So it's easy to spot on an "upgrade" and we can re-default it
    myConfig.reserved10  = 0xA5;    // So it's easy to spot on an "upgrade" and we can re-default it
  
    // ----------------------------------------------------------------------------------
    // A few games don't want more than 4 max sprites (they pull tricks that rely on it)
    // ----------------------------------------------------------------------------------
    if (file_crc == 0xee530ad2) myConfig.maxSprites  = 1;  // QBiqs
    if (file_crc == 0x275c800e) myConfig.maxSprites  = 1;  // Antartic Adventure
    if (file_crc == 0xa66e5ed1) myConfig.maxSprites  = 1;  // Antartic Adventure Prototype  
    if (file_crc == 0x6af19e75) myConfig.maxSprites  = 1;  // Adventures in the Park    
    if (file_crc == 0xbc8320a0) myConfig.maxSprites  = 1;  // Uridium 
    
    // ----------------------------------------------------------------------------------
    // Set some of the common games to use their proper Colecovision Graphical Overlays
    // ----------------------------------------------------------------------------------
    if (file_crc == 0xc575a831)  myConfig.overlay = OVL_2010;
    if (file_crc == 0x8c7b7803)  myConfig.overlay = OVL_BLACKJACK;
    if (file_crc == 0x9b547ba8)  myConfig.overlay = OVL_BOULDERDASH;
    if (file_crc == 0x109699e2)  myConfig.overlay = OVL_FIXUPMIXUP;
    if (file_crc == 0xfdb75be6)  myConfig.overlay = OVL_GATEWAY;
    if (file_crc == 0xde47c29f)  myConfig.overlay = OVL_MOUSETRAP;
    if (file_crc == 0xeec81c42)  myConfig.overlay = OVL_QUINTAROO;
    if (file_crc == 0xbb0f6678)  myConfig.overlay = OVL_SPACESHUTTLE;
    if (file_crc == 0x0a90ba65)  myConfig.overlay = OVL_UTOPIA;
    if (file_crc == 0xfd25adb3)  myConfig.overlay = OVL_WARGAMES;
    if (file_crc == 0x36478923)  myConfig.overlay = OVL_SPYHUNTER;
    if (file_crc == 0x261b7d56)  myConfig.overlay = OVL_WARROOM;
    
    
    // -------------------------------------------
    // Turbo needs the Driving Module
    // -------------------------------------------
    if (file_crc == 0xbd6ab02a)      // Turbo (Sega)
    {
        myConfig.touchPad    = 1;    // Map the on-screen touch Keypad to P2 for Turbo
        myConfig.keymap[0]   = 20;   // NDS D-Pad mapped to P2 UP
        myConfig.keymap[1]   = 21;   // NDS D-Pad mapped to P2 DOWN 
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
    }
    
    
    // -------------------------------------------
    // Destructor needs the Driving Module...
    // -------------------------------------------
    if (file_crc == 0x56c358a6)      // Destructor (Coleco)
    {
        myConfig.touchPad    = 1;    // Map the on-screen touch Keypad to P2 for Destructor
        myConfig.keymap[0]   = 20;   // NDS D-Pad mapped to P2 UP
        myConfig.keymap[1]   = 21;   // NDS D-Pad mapped to P2 DOWN 
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
    }
    
    // -------------------------------------------
    // Dukes of Hazzard needs the Driving Module
    // -------------------------------------------
    if (file_crc == 0x4025ac94)      // Dukes of Hazzard (Coleco)
    {
        myConfig.touchPad    = 1;    // Map the on-screen touch Keypad to P2 for Dukes of Hazzard
        myConfig.keymap[0]   = 20;   // NDS D-Pad mapped to P2 UP
        myConfig.keymap[1]   = 21;   // NDS D-Pad mapped to P2 DOWN 
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[6]   = 23;   // NDS X Button mapped to P2 RIGHT for gear shift
        myConfig.keymap[7]   = 22;   // NDS Y Button mapped to P2 LEFT for gear shift
    }
    
    // -------------------------------------------
    // Slither needs Trackball Support (SpinX/Y)
    // -------------------------------------------
    if (file_crc == 0x53d2651c)      // Slither (Century)
    {
        myConfig.keymap[0]   = 42;   // NDS D-Pad mapped to Spinner Y
        myConfig.keymap[1]   = 43;   // NDS D-Pad mapped to Spinner Y
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
    }
    
    // ---------------------------------------------------------
    // Victory needs Trackball Support (SpinX/Y) plus Buttons
    // ---------------------------------------------------------
    if (file_crc == 0x70142655)      // Victory (Exidy)
    {
        myConfig.keymap[0]   = 42;   // NDS D-Pad mapped to Spinner Y
        myConfig.keymap[1]   = 43;   // NDS D-Pad mapped to Spinner Y
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
        
        myConfig.keymap[4]   = 4;    // NDS A Button mapped to P1 Button 1
        myConfig.keymap[5]   = 5;    // NDS B Button mapped to P1 Button 2 
        myConfig.keymap[6]   = 24;   // NDS X Button mapped to P2 Button 1
        myConfig.keymap[7]   = 25;   // NDS Y Button mapped to P2 Button 2         
    }
    
    if ((file_crc == 0xeec68527) ||     // SVI Crazy Teeth
        (file_crc == 0x1748aed7))       // SVI Burkensoft Game Pak 14 with MEGALONE
    {
        MapPlayer2();               // These games want P2 mapping...
    }
    
	// --------------------------------------------------------------------------
	// There are a few games that don't want the SGM or ADAM... Check those now.
	// --------------------------------------------------------------------------
	if (file_crc == 0xef25af90) myConfig.cvMode = CV_MODE_NOSGM; // Super DK Prototype - ignore any SGM/Adam Writes (this disables SGM)
	if (file_crc == 0xc2e7f0e0) myConfig.cvMode = CV_MODE_NOSGM; // Super DK JR Prototype - ignore any SGM/Adam Writes (this disables SGM)
    if (file_crc == 0x846faf14) myConfig.cvMode = CV_MODE_NOSGM; // Cavern Fighter - ignore any SGM/Adam Writes (this disables SGM)    
    
    if (file_crc == 0x987491ce) myConfig.memWipe = 1;    // The Heist for Colecovision is touchy about RAM (known issue with game) so force clear

	// --------------------------------------------------------------------------
	// And a few games that are known to use the Super Cart boards...
	// --------------------------------------------------------------------------
    if (IsSuperGameCart(file_crc))
    {
        myConfig.cvMode = CV_MODE_SUPERCART;
    }
    
    // -----------------------------------------------------------
    // And two carts that are multicarts for the Colecovision...
    // -----------------------------------------------------------
    if (file_crc == 0x3f4ebe58) myConfig.cvMode = CV_MODE_31IN1;        // 31 in 1 multicart
    if (file_crc == 0xd821cad4) myConfig.cvMode = CV_MODE_31IN1;        // 63 in 1 multicart
   
    // -----------------------------------------------------------------------------
    // A few carts want the 'Wave Direct' sound so they can handle digitized speech
    // -----------------------------------------------------------------------------
    if (file_crc == 0x4591f393) myConfig.soundDriver = SND_DRV_WAVE;        // Sewer Sam
    if (file_crc == 0xac92862d) myConfig.soundDriver = SND_DRV_WAVE;        // Squish'em Featuring Sam
    if (file_crc == 0xd9207f30) myConfig.soundDriver = SND_DRV_WAVE;        // Wizard of Wor SGM
    if (file_crc == 0x91326103) myConfig.soundDriver = SND_DRV_WAVE;        // Bosconian SGM
    if (file_crc == 0x6328ffc1) myConfig.soundDriver = SND_DRV_WAVE;        // Berzerk (2022)
    if (file_crc == 0xa7a8d25e) myConfig.soundDriver = SND_DRV_WAVE;        // Vanguard    
    if (file_crc == 0x2e4c28e2) myConfig.soundDriver = SND_DRV_WAVE;        // Side Trak
    if (file_crc == 0x69e3c673) myConfig.soundDriver = SND_DRV_WAVE;        // Jeepers Creepers - 30th Anniversary Edition    
    
    // -----------------------------------------------------------
    // If we are DS-PHAT or DS-LITE running on slower CPU, we 
    // need to help the processor out a bit by turning off RAM
    // mirrors for the games that don't need them.
    // -----------------------------------------------------------
    if (!isDSiMode())
    {
        myConfig.mirrorRAM = COLECO_RAM_NO_MIRROR;      // For the older hardware, disable the mirror by default
        
        // Except for these games that need the Mirror to work...
        if (file_crc == 0xf84622d2) myConfig.mirrorRAM = COLECO_RAM_NORMAL_MIRROR; // Super Cobra
        if (file_crc == 0xc48db4ce) myConfig.mirrorRAM = COLECO_RAM_NORMAL_MIRROR; // Jump Land
        if (file_crc == 0x644124f6) myConfig.mirrorRAM = COLECO_RAM_NORMAL_MIRROR; // Donkey Kong Junior - Super Game (needs mirrors for SGM detection to work)
        if (file_crc == 0xb3e62471) myConfig.mirrorRAM = COLECO_RAM_NORMAL_MIRROR; // Donkey Kong - Super Game (needs mirrors for SGM detection to work)
        if (file_crc == 0xeac71b43) myConfig.mirrorRAM = COLECO_RAM_NORMAL_MIRROR; // Subroc - Super Game (needs mirrors for SGM detection to work)
        
        // ---------------------------------------------------------------------
        // Set the spinner configuration automatically for games that want it.
        // ---------------------------------------------------------------------
        myConfig.spinSpeed = 5;                                                 // Turn off Spinner... except for these games
        if (file_crc == 0x53d2651c)                 myConfig.spinSpeed = 0;     // Slither
        if (file_crc == 0xbd6ab02a)                 myConfig.spinSpeed = 0;     // Turbo
        if (file_crc == 0xd0110137)                 myConfig.spinSpeed = 0;     // Front Line
        if (file_crc == 0x56c358a6)                 myConfig.spinSpeed = 0;     // Destructor
        if (file_crc == 0x70142655)                 myConfig.spinSpeed = 0;     // Victory    
    }

    // --------------------------------------------------------------------
    // Set various keyboard/keypad overlays for various emulated modes...
    // --------------------------------------------------------------------
    if (sg1000_mode == 2)                       myConfig.overlay = 1;  // SC-3000 uses the full keyboard
    if (sg1000_mode == 2)                       myConfig.vertSync= 0;  // SC-3000 does not use vertical sync
    if (msx_mode == 1)                          myConfig.overlay = (myGlobalConfig.msxCartOverlay ? 1:0);  // MSX cart-based games follows the global default
    if (msx_mode == 2)                          myConfig.overlay = 1;  // MSX with .cas defaults to full keyboard
    if (msx_mode == 3)                          myConfig.overlay = 1;  // MSX with .dsk defaults to full keyboard
    if (msx_mode == 2)                          myConfig.msxBios = myGlobalConfig.defaultMSX;  // If loading cassette, must have real MSX bios
    if (adam_mode)                              myConfig.memWipe = 1;  // Adam defaults to clearing memory to a specific pattern.
    if (adam_mode)                              myConfig.mirrorRAM = 0;// Adam does not mirror RAM as a Colecovision would
    if (msx_mode && (file_size >= (64*1024)))   myConfig.vertSync= 0;  // For bankswiched MSX games, disable VSync to gain speed
    if (memotech_mode)                          myConfig.overlay = 1;  // Memotech MTX default to full keyboard
    if (einstein_mode)                          myConfig.overlay = 1;  // Tatung Einstein defaults to full keyboard
    if (svi_mode)                               myConfig.overlay = 1;  // SVI default to full keyboard
    if (sordm5_mode)                            myConfig.overlay = 1;  // Sord M5 default to full keyboard
    
    if (einstein_mode)                          myConfig.isPAL   = 1;  // Tatung Einstein defaults to PAL machine
    if (memotech_mode)                          myConfig.isPAL   = 1;  // Memotech defaults to PAL machine
    if (creativision_mode)                      myConfig.isPAL   = 1;  // Creativision defaults to PAL machine
    if (creativision_mode)                      myConfig.vertSync= 0;  // Creativision defaults to no vert sync
    
    // ----------------------------------------------------
    // Some special BASIC carts that want full keyboards
    // ----------------------------------------------------
    if (file_crc == 0x69a92b72)                 myConfig.overlay = 1;  // PV-2000 BASIC uses keyboard
    if (file_crc == 0x4891613b)                 myConfig.overlay = 2;  // Hanimex Pencil II BASIC uses simplified keyboard
    if (file_crc == 0x0c497839)                 myConfig.overlay = 2;  // Hanimex Pencil II BASIC [a] uses simplified keyboard    

    // ----------------------------------------------------------------------------------
    // Some CreatiVision games that want the new CV overlay keypad/keybaord
    // ----------------------------------------------------------------------------------
    if (file_crc == 0x4aee923e)                 myConfig.overlay = 1; // BASIC 82A
    if (file_crc == 0x1849efd0)                 myConfig.overlay = 1; // BASIC 82B
    if (file_crc == 0x10409a1d)                 myConfig.overlay = 1; // BASIC 83A
    if (file_crc == 0x044adbe8)                 myConfig.overlay = 1; // BASIC 83C
    if (file_crc == 0x8258ee6c)                 myConfig.overlay = 1; // BASIC 83H
    if (file_crc == 0x8375203e)                 myConfig.overlay = 1; // CSL BIOS A
    if (file_crc == 0x77afd38b)                 myConfig.overlay = 1; // CSL BIOS B
    if (file_crc == 0x9e584ce2)                 myConfig.overlay = 1; // DIAG A
    if (file_crc == 0x4d92ff4e)                 myConfig.overlay = 1; // DIAG B
    if (file_crc == 0xadb11067)                 myConfig.overlay = 1; // DIAG DEMO
    if (file_crc == 0xc2ba6a99)                 myConfig.overlay = 1; // WERBENE
    if (file_crc == 0xf8383d33)                 myConfig.overlay = 1; // MUSIC MAKER
    
    // ---------------------------------------------------------------
    // Check for CP/M disks which want memory and full keyboard.
    // Also check for T-DOS disks which want a faster cache flush.
    // ---------------------------------------------------------------
    if (adam_mode)
    {
        for (int i=0; i<0x2000; i++)
        {
            if ((ROM_Memory[i] == 'C') && (ROM_Memory[i+1] == 'P') && (ROM_Memory[i+2] == '/') && (ROM_Memory[i+3] == 'M')) // Look for CP/M
            {
                myConfig.overlay = 1;  // And most CPM games are going to want a full keyboard
                break;
            }
            if ((ROM_Memory[i] == 'T') && (ROM_Memory[i+1] == '-') && (ROM_Memory[i+2] == 'D') && (ROM_Memory[i+3] == 'O') && (ROM_Memory[i+4] == 'S')) // Look for T-DOS
            {
                myConfig.overlay = 1;  // And most T-DOS games are going to want a full keyboard
                break;
            }
            if ((ROM_Memory[i] == 'B') && (ROM_Memory[i+1] == 'A') && (ROM_Memory[i+2] == 'S') && (ROM_Memory[i+3] == 'I') && (ROM_Memory[i+4] == 'C')) // Look for Smart Basic packs
            {
                myConfig.overlay = 1;  // Most SmartBasic packs are going to want a full keyboard
                break;
            }
            if ((ROM_Memory[i] == 'L') && (ROM_Memory[i+1] == 'O') && (ROM_Memory[i+2] == 'G') && (ROM_Memory[i+3] == 'O')) // Look for Logo packs
            {
                myConfig.overlay = 1;  // Most SmartBasic packs are going to want a full keyboard
                break;
            }
        }
    }
    
    // ----------------------------------------------------------------------------
    // For these machines, we default to clearing interrupts only on VDP read...
    // ----------------------------------------------------------------------------
    if (msx_mode || einstein_mode || svi_mode || memotech_mode) myConfig.clearInt = CPU_CLEAR_INT_ON_VDP_READ;
    
    // ---------------------------------------------------------------------------------
    // A few games don't work well with the clearing of interrupts on VDP and run 
    // better with auto-clear.  So we adjust those here...
    // ---------------------------------------------------------------------------------
    if (file_crc == 0xef339b82)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Ninja Kun - Bouken
    if (file_crc == 0xc9bcbe5a)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Ghostbusters
    if (file_crc == 0x9814c355)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Ghostbusters    
    if (file_crc == 0x90530889)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Soul of a Robot
    if (file_crc == 0x33221ad9)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Time Bandits    
    if (file_crc == 0x9dbdd4bc)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX GP World (Sega)    
    if (file_crc == 0x7820e86c)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX GP World (Sega)   
    if (file_crc == 0x6e8bb5fa)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Seleniak - Mark II
    if (file_crc == 0xb8ca3108)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Quazzia
    if (file_crc == 0xbd285566)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Caves of Orb
    if (file_crc == 0xe30fb8f7)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Pac Manor Rescue
    if (file_crc == 0xa2db030e)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Revenge of the Chamberoids    
    if (file_crc == 0x3c8500af)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Target Zone
    if (file_crc == 0xbde21de8)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Target Zone
    if (file_crc == 0x8f9f902e)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Target Zone    
    if (file_crc == 0xe3f495c4)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MAGROM
    if (file_crc == 0x98240ee9)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MAGROM
    
    
    // ---------------------------------------------------------------------------------
    // A few of the ZX Spectrum ports actually use the MSX beeper for sound. Go figure!
    // ---------------------------------------------------------------------------------
    if (file_crc == 0x1b8873ca)                 myConfig.msxBeeper = 1;     // MSX Avenger uses beeper
    if (file_crc == 0x111fc33b)                 myConfig.msxBeeper = 1;     // MSX Avenger uses beeper    
    if (file_crc == 0x690f9715)                 myConfig.msxBeeper = 1;     // MSX Batman (the movie) uses beeper
    if (file_crc == 0x3571f5d4)                 myConfig.msxBeeper = 1;     // MSX Master of the Universe uses beeper
    if (file_crc == 0x2142bd10)                 myConfig.msxBeeper = 1;     // MSX Future Knight uses beeper
    
    if (file_crc == 0x094db111)                 myConfig.msxBeeper = 1;     // Einstein Druid uses beeper
    if (file_crc == 0x94073b5c)                 myConfig.msxBeeper = 1;     // Einstein Alien 8 uses beeper

    // ------------------------------------------------------
    // A few games really want diagonal inputs enabled...
    // ------------------------------------------------------
    if (file_crc == 0x9d8fa05f)                 myConfig.dpad = DPAD_DIAGONALS;  // QOGO  needs diagonals
    if (file_crc == 0x9417ec36)                 myConfig.dpad = DPAD_DIAGONALS;  // QOGO2 needs diagonals    
    if (file_crc == 0x154702b2)                 myConfig.dpad = DPAD_DIAGONALS;  // QOGS2 needs diagonals    
    if (file_crc == 0x71600e71)                 myConfig.dpad = DPAD_DIAGONALS;  // QOGO  needs diagonals    
    if (file_crc == 0xf9934809)                 myConfig.dpad = DPAD_DIAGONALS;  // Reveal needs diagonals    
    
    // ----------------------------------------------------------
    // Sort out a handful of games that are clearly PAL or NTSC
    // ----------------------------------------------------------
    if (file_crc == 0x0084b239)                 myConfig.isPAL = 1;     // Survivors Multi-Cart is PAL
    if (file_crc == 0x76a3d2e2)                 myConfig.isPAL = 1;     // Survivors MEGA-Cart is PAL
    
    if (file_crc == 0x07056b00)                 myConfig.isPAL   = 0;   // Memotech Pacman is an NTSC conversion
    if (file_crc == 0x8b28101a)                 myConfig.isPAL   = 0;   // Memotech Pacman is an NTSC conversion    
    if (file_crc == 0x87b9b54e)                 myConfig.isPAL   = 0;   // Memotech PowerPac is an NTSC conversion
    if (file_crc == 0xb8ed9f9e)                 myConfig.isPAL   = 0;   // Memotech PowerPac is an NTSC conversion    
    if (file_crc == 0xcac1f237)                 myConfig.isPAL   = 0;   // Memotech Telebunny is an NTSC conversion
    if (file_crc == 0xbd0e4513)                 myConfig.isPAL   = 0;   // Memotech Telebunny is an NTSC conversion
    if (file_crc == 0x24ae8ac0)                 myConfig.isPAL   = 0;   // Memotech Hustle Chummy is an NTSC conversion
    if (file_crc == 0x31ff229b)                 myConfig.isPAL   = 0;   // Memotech Hustle Chummy is an NTSC conversion
    if (file_crc == 0x025e77dc)                 myConfig.isPAL   = 0;   // Memotech OldMac is an NTSC conversion
    if (file_crc == 0x95e71c67)                 myConfig.isPAL   = 0;   // Memotech OldMac is an NTSC conversion
    if (file_crc == 0xc10a6e96)                 myConfig.isPAL   = 1;   // Sord M5 Master Chess is PAL
    if (file_crc == 0xca2cd257)                 myConfig.isPAL   = 1;   // Sord M5 Reversi is PAL


    // ------------------------------------------------------------
    // Some special cart types - for known Activision PCB carts...
    // ------------------------------------------------------------
    if (file_crc == 0xdddd1396)
    {
        myConfig.cvMode = CV_MODE_ACTCART;  // Black Onyx is Activision PCB
        myConfig.cvEESize = EEPROM_256B;    // Black Onyx EEPROM is 256 bytes
    }
    
    if (file_crc == 0x62dacf07)
    {
        myConfig.cvMode = CV_MODE_ACTCART;  // Boxxle is Activision PCB
        myConfig.cvEESize = EEPROM_32KB;    // Boxxle EEPROM is 32K bytes
    }
    
    if (file_crc == 0x9f74b0e9)
    {
        myConfig.cvMode = CV_MODE_ACTCART;  // Jewel Panic is Activision PCB
        myConfig.cvEESize = EEPROM_256B;    // Jewel Panic EEPROM is 256 bytes
    }
    
    // Acromage is also an Activision PCB and is thought to be 256 bytes of EE
    
    if (file_crc == 0x767a1f38)                 myConfig.maxSprites = 1;    // CreatiVision Sonic Invaders needs 4 sprites max
    if (file_crc == 0x011899cf)                 myConfig.maxSprites = 1;    // CreatiVision Sonic Invaders needs 4 sprites max (32K version)
    
    if (file_crc == 0x532f61ba)                 myConfig.vertSync = 0;      // Colecovision Q-Bert will struggle with vertical sync on stage clear
    if (myConfig.isPAL)                         myConfig.vertSync = 0;      // If we are PAL, we can't sync to the DS 60Hz
    if (myConfig.gameSpeed)                     myConfig.vertSync = 0;      // If game speed isn't 100%, we can't sync to the DS 60Hz
}

// ----------------------------------------------------------
// Load configuration into memory where we can use it. 
// The configuration is stored in ColecoDS.DAT 
// ----------------------------------------------------------
void LoadConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();
    
    if (ReadFileCarefully("/data/ColecoDS.DAT", (u8*)&myGlobalConfig, sizeof(myGlobalConfig), 0))  // Read Global Config
    {
        ReadFileCarefully("/data/ColecoDS.DAT", (u8*)&AllConfigs, sizeof(AllConfigs), sizeof(myGlobalConfig)); // Read the full game array of configs
        
        // We auto-update rev 12 to rev 13 configuration
        if (myGlobalConfig.config_ver == 0x0012)
        {
            for (u16 slot=0; slot<MAX_CONFIGS; slot++)
            {
                AllConfigs[slot].cvMode   = CV_MODE_NORMAL;
                AllConfigs[slot].cvEESize = EEPROM_NONE;
            }
            myGlobalConfig.config_ver = CONFIG_VER;
            SaveConfig(FALSE);
        }

        if (myGlobalConfig.config_ver != CONFIG_VER)
        {
            memset(&AllConfigs, 0x00, sizeof(AllConfigs));
            SetDefaultGameConfig();
            SetDefaultGlobalConfig();
            SaveConfig(FALSE);
        }
    }
    else    // Not found... init the entire database...
    {
        memset(&AllConfigs, 0x00, sizeof(AllConfigs));
        SetDefaultGameConfig();
        SetDefaultGlobalConfig();
        SaveConfig(FALSE);
    }}

// -------------------------------------------------------------------------
// Try to match our loaded game to a configuration my matching CRCs
// -------------------------------------------------------------------------
void FindConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();
    
    for (u16 slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == file_crc)  // Got a match?!
        {
            memcpy(&myConfig, &AllConfigs[slot], sizeof(struct Config_t));
            break;                           
        }
    }
}


// ------------------------------------------------------------------------------
// Options are handled here... we have a number of things the user can tweak
// and these options are applied immediately. The user can also save off 
// their option choices for the currently running game into the NINTV-DS.DAT
// configuration database. When games are loaded back up, NINTV-DS.DAT is read
// to see if we have a match and the user settings can be restored for the game.
// ------------------------------------------------------------------------------
struct options_t
{
    const char  *label;
    const char  *option[37];
    u8          *option_val;
    u8           option_max;
};

const struct options_t Option_Table[3][20] =
{
    // Page 1
    {
        {"OVERLAY",        {"GENERIC", "FULL KEYBOARD", "ALPHA KEYBOARD", "WARGAMES", "MOUSETRAP", "GATEWAY", "SPY HUNTER", "FIX UP MIX UP", "BOULDER DASH", "QUINTA ROO", "2010", "SPACE SHUTTLE", "UTOPIA", "BLACKJACK", "WAR ROOM"}, &myConfig.overlay,  15},
        {"FRAME SKIP",     {"OFF", "SHOW 3/4", "SHOW 1/2"},                                                                                                                                     &myConfig.frameSkip,  3},
        {"FRAME BLEND",    {"OFF", "ON"},                                                                                                                                                       &myConfig.frameBlend, 2},
        {"VIDEO TYPE",     {"NTSC", "PAL"},                                                                                                                                                     &myConfig.isPAL,      2},
        {"MAX SPRITES",    {"32",  "4"},                                                                                                                                                        &myConfig.maxSprites, 2},
        {"VERT SYNC",      {"OFF", "ON"},                                                                                                                                                       &myConfig.vertSync,   2},    
        {"AUTO FIRE",      {"OFF", "B1 ONLY", "B2 ONLY", "BOTH"},                                                                                                                               &myConfig.autoFire,   4},
        {"TOUCH PAD",      {"PLAYER 1", "PLAYER 2"},                                                                                                                                            &myConfig.touchPad,   2},    
        {"JOYSTICK",       {"NORMAL", "DIAGONALS"},                                                                                                                                             &myConfig.dpad,       2},
        {"SPIN SPEED",     {"NORMAL", "FAST", "FASTEST", "SLOW", "SLOWEST", "OFF"},                                                                                                             &myConfig.spinSpeed,  6},
        {"MSX MAPPER",     {"GUESS","KONAMI 8K","ASCII 8K","KONAMI SCC","ASCII 16K","ZEMINA 8K","ZEMINA 16K","CROSSBLAIM","RESERVED","AT 0000H","AT 4000H","AT 8000H","64K LINEAR"},            &myConfig.msxMapper,  13},
        {"MSX BIOS",       {"C-BIOS 64K", "MSX.ROM 64K", "CF2700.ROM 64K", "CX5M.ROM 32K", "HX-10.ROM 64K", "HB-10.ROM 16K", "FS1300.ROM 64K", "PV-7.ROM  8K"},                                 &myConfig.msxBios,    8},
        {"RAM WIPE",       {"RANDOM", "CLEAR"},                                                                                                                                                 &myConfig.memWipe,    2},
        {"COLECO RAM",     {"NO MIRROR", "MIRRORED"},                                                                                                                                           &myConfig.mirrorRAM,  2},
        {"COLECO MODE",    {"NORMAL","FORCE ADAM","SGM DISABLE","ACTIVISION PCB","SUPERCART", "31 IN 1"},                                                                                       &myConfig.cvMode,     6},
        {"COLECO NVRAM",   {"128B", "256B", "512B", "1KB", "2KB", "4KB", "8KB", "16KB", "32KB", "NONE"},                                                                                        &myConfig.cvEESize,   10},
        {NULL,             {"",      ""},                                                                                                                                                       NULL,                 1},
    },
    // Page 2
    {
        {"CPU INT",        {"CLEAR ON VDP", "AUTO CLEAR"},                                                                                                                                      &myConfig.clearInt,   2},
        {"KEY CLICK",      {"ON", "OFF"},                                                                                                                                                       &myConfig.keyMute,    2},
        {"SOUND DRIVER",   {"NORMAL", "WAVE DIRECT"},                                                                                                                                           &myConfig.soundDriver,2},
        {"MSX BEEPER",     {"OFF", "ON"},                                                                                                                                                       &myConfig.msxBeeper,  2},        
        {"MSX KEY ?",      {"DEFAULT","SHIFT","CTRL","ESC","F4","F5","6","7","8","9","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"},  &myConfig.msxKey5,    36},
        {"GAME SPEED",     {"100%", "110%", "120%", "130%", "90%"},                                                                                                                             &myConfig.gameSpeed,  5},
        {"CVISION LOAD",   {"LEGACY (A/B)", "LINEAR", "32K BANKSWAP", "BIOS"},                                                                                                                  &myConfig.cvisionLoad,4},
        {"EINSTEIN CTC",   {"NORMAL", "+1 (SLOWER)", "+2 (SLOWER)", "+3 (SLOWER)", "+5 (SLOWER)", "+10 (SLOWER)", 
                            "+20 (SLOWER)", "-1 (FASTER)", "-2 (FASTER)", "-3 (FASTER)", "-5 (FASTER)", "-10 (FASTER)", "-20 (FASTER)"},                                                        &myConfig.ein_ctc3,  13},
        {"ADAMNET",        {"FAST", "SLOWER", "SLOWEST"},                                                                                                                                       &myConfig.adamnet,    3},
        {NULL,             {"",      ""},                                                                                                                                                       NULL,                 1},
    },
    // Global Options
    {
        {"FPS",            {"OFF", "ON", "ON FULLSPEED"},                                                                                                                                       &myGlobalConfig.showFPS,        3},
        {"EMU TEXT",       {"OFF",  "ON"},                                                                                                                                                      &myGlobalConfig.emuText,        2},
        {"BIOS INFO",      {"HIDE ON BOOT", "SHOW ON BOOT"},                                                                                                                                    &myGlobalConfig.showBiosInfo,   2},
        {"DEFAULT MSX",    {"C-BIOS 64K", "MSX.ROM 64K", "CF2700.ROM 64K", "CX5M.ROM 32K", "HX-10.ROM 64K", "HB-10.ROM 16K", "FS1300.ROM 64K", "PV-7.ROM  8K"},                                 &myGlobalConfig.defaultMSX,     8},
        {"MSX CART USE",   {"JOYPAD OVERLAY", "KEYBOARD OVL"},                                                                                                                                  &myGlobalConfig.msxCartOverlay, 2},
        {"DEF SPRITES",    {"32", "4"},                                                                                                                                                         &myGlobalConfig.defSprites,     2},
        {"DISK / DDP",     {"SOUND ON", "SOUND OFF"},                                                                                                                                           &myGlobalConfig.diskSfxMute,    2},
        {"BIOS DELAY",     {"NORMAL", "SHORT"},                                                                                                                                                 &myGlobalConfig.biosDelay,      2},        
        
        {"DEBUGGER",       {"OFF", "BAD OPS", "DEBUG", "FULL DEBUG"},                                                                                                                           &myGlobalConfig.debugger,       4},
        
        {NULL,             {"",      ""},                                                                                                                                                       NULL,                           1},
    }
};


// ------------------------------------------------------------------
// Display the current list of options for the user.
// ------------------------------------------------------------------
u8 display_options_list(bool bFullDisplay)
{
    s16 len=0;
    
    DSPrint(1,21, 0, (char *)"                              ");
    if (bFullDisplay)
    {
        while (true)
        {
            sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][len].label, Option_Table[option_table][len].option[*(Option_Table[option_table][len].option_val)]);
            DSPrint(1,5+len, (len==0 ? 2:0), strBuf); len++;
            if (Option_Table[option_table][len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<16; i++) 
        {
            DSPrint(1,5+i, 0, (char *)"                               ");
        }
    }

    DSPrint(1,22, 0, (char *)"  B=EXIT, X=MORE, START=SAVE  ");
    return len;    
}


//*****************************************************************************
// Change Game Options for the current game
//*****************************************************************************
void colecoDSGameOptions(bool bIsGlobal)
{
    u8 optionHighlighted;
    u8 idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;

    option_table = (bIsGlobal ? 2:0);
    
    idx=display_options_list(true);
    optionHighlighted = 0;
    while (keysCurrent() != 0)
    {
        WAITVBL;
    }
    while (!bDone)
    {
        keys_pressed = keysCurrent();
        if (keys_pressed != last_keys_pressed)
        {
            last_keys_pressed = keys_pressed;
            if (keysCurrent() & KEY_UP) // Previous option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }

            if (keysCurrent() & KEY_RIGHT)  // Toggle option clockwise
            {
                *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) + 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_LEFT)  // Toggle option counterclockwise
            {
                if ((*(Option_Table[option_table][optionHighlighted].option_val)) == 0)
                    *(Option_Table[option_table][optionHighlighted].option_val) = Option_Table[option_table][optionHighlighted].option_max -1;
                else
                    *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) - 1) % Option_Table[option_table][optionHighlighted].option_max;
                sprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_START)  // Save Options
            {
                SaveConfig(TRUE);
            }
            if (keysCurrent() & (KEY_X)) // Toggle Table
            {
                option_table = (bIsGlobal ? 2: ((option_table + 1) % 2));
                idx=display_options_list(true);
                optionHighlighted = 0;
                while (keysCurrent() != 0)
                {
                    WAITVBL;
                }
            }
            if ((keysCurrent() & KEY_B) || (keysCurrent() & KEY_A))  // Exit options
            {
                option_table = 0;   // Reset for next time
                break;
            }
        }
        showRandomPreviewSnaps();
        swiWaitForVBlank();
    }

    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }
    
    // We can't support PAL with Vertical Sync 
    if (myConfig.isPAL) myConfig.vertSync = 0;
    
    return;
}

//*****************************************************************************
// Change Keymap Options for the current game
//*****************************************************************************
char szCha[34];
void DisplayKeymapName(u32 uY) 
{
  sprintf(szCha," PAD UP    : %-17s",szKeyName[myConfig.keymap[0]]);
  DSPrint(1, 6,(uY==  6 ? 2 : 0),szCha);
  sprintf(szCha," PAD DOWN  : %-17s",szKeyName[myConfig.keymap[1]]);
  DSPrint(1, 7,(uY==  7 ? 2 : 0),szCha);
  sprintf(szCha," PAD LEFT  : %-17s",szKeyName[myConfig.keymap[2]]);
  DSPrint(1, 8,(uY==  8 ? 2 : 0),szCha);
  sprintf(szCha," PAD RIGHT : %-17s",szKeyName[myConfig.keymap[3]]);
  DSPrint(1, 9,(uY== 9 ? 2 : 0),szCha);
  sprintf(szCha," KEY A     : %-17s",szKeyName[myConfig.keymap[4]]);
  DSPrint(1,10,(uY== 10 ? 2 : 0),szCha);
  sprintf(szCha," KEY B     : %-17s",szKeyName[myConfig.keymap[5]]);
  DSPrint(1,11,(uY== 11 ? 2 : 0),szCha);
  sprintf(szCha," KEY X     : %-17s",szKeyName[myConfig.keymap[6]]);
  DSPrint(1,12,(uY== 12 ? 2 : 0),szCha);
  sprintf(szCha," KEY Y     : %-17s",szKeyName[myConfig.keymap[7]]);
  DSPrint(1,13,(uY== 13 ? 2 : 0),szCha);
  sprintf(szCha," KEY R     : %-17s",szKeyName[myConfig.keymap[8]]);
  DSPrint(1,14,(uY== 14 ? 2 : 0),szCha);
  sprintf(szCha," KEY L     : %-17s",szKeyName[myConfig.keymap[9]]);
  DSPrint(1,15,(uY== 15 ? 2 : 0),szCha);
  sprintf(szCha," START     : %-17s",szKeyName[myConfig.keymap[10]]);
  DSPrint(1,16,(uY== 16 ? 2 : 0),szCha);
  sprintf(szCha," SELECT    : %-17s",szKeyName[myConfig.keymap[11]]);
  DSPrint(1,17,(uY== 17 ? 2 : 0),szCha);
}

u8 keyMapType = 0;
void SwapKeymap(void)
{
    keyMapType = (keyMapType+1) % 7;
    switch (keyMapType)
    {
        case 0: MapPlayer1();  break;
        case 1: MapPlayer2();  break;
        case 2: MapQAOP();     break;
        case 3: MapWASD();     break;
        case 4: MapZCPeriod(); break;
        case 5: MapZXSpace();  break;
        case 6: MapArrows();   break;
    }
}


// ------------------------------------------------------------------------------
// Allow the user to change the key map for the current game and give them
// the option of writing that keymap out to a configuration file for the game.
// ------------------------------------------------------------------------------
void colecoDSChangeKeymap(void) 
{
  u32 ucHaut=0x00, ucBas=0x00,ucL=0x00,ucR=0x00,ucY= 6, bOK=0, bIndTch=0;

  // ------------------------------------------------------
  // Clear the screen so we can put up Key Map infomation
  // ------------------------------------------------------
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
    
  // --------------------------------------------------
  // Give instructions to the user...
  // --------------------------------------------------
  DSPrint(1 ,19,0,("   D-PAD : CHANGE KEY MAP    "));
  DSPrint(1 ,20,0,("       B : RETURN MAIN MENU  "));
  DSPrint(1 ,21,0,("       X : SWAP KEYMAP TYPE  "));
  DSPrint(1 ,22,0,("   START : SAVE KEYMAP       "));
  DisplayKeymapName(ucY);
  
  // -----------------------------------------------------------------------
  // Clear out any keys that might be pressed on the way in - make sure
  // NDS keys are not being pressed. This prevents the inadvertant A key
  // that enters this menu from also being acted on in the keymap...
  // -----------------------------------------------------------------------
  while ((keysCurrent() & (KEY_TOUCH | KEY_B | KEY_A | KEY_X | KEY_UP | KEY_DOWN))!=0)
      ;
  WAITVBL;
 
  while (!bOK) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        DisplayKeymapName(32);
        ucY = (ucY == 6 ? 17 : ucY -1);
        bIndTch = myConfig.keymap[ucY-6];
        ucHaut=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      } 
    }
    else {
      ucHaut = 0;
    }  
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        DisplayKeymapName(32);
        ucY = (ucY == 17 ? 6 : ucY +1);
        bIndTch = myConfig.keymap[ucY-6];
        ucBas=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      } 
    }
    else {
      ucBas = 0;
    }  
      
    if (keysCurrent() & KEY_START) 
    {
        SaveConfig(true); // Save options
    }
      
    if (keysCurrent() & KEY_B) 
    {
      bOK = 1;  // Exit menu
    }
      
    if (keysCurrent() & KEY_LEFT) 
    {
        if (ucL == 0) {
          bIndTch = (bIndTch == 0 ? (MAX_KEY_OPTIONS-1) : bIndTch-1);
          ucL=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else {
          ucL++;
          if (ucL > 7) ucL = 0;
        }
    }
    else 
    {
        ucL = 0;
    }
      
    if (keysCurrent() & KEY_RIGHT) 
    {
        if (ucR == 0) 
        {
          bIndTch = (bIndTch == (MAX_KEY_OPTIONS-1) ? 0 : bIndTch+1);
          ucR=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else 
        {
          ucR++;
          if (ucR > 7) ucR = 0;
        }
    }
    else
    {
        ucR=0;
    }
      
    // Swap Player 1 and Player 2 keymap
    if (keysCurrent() & KEY_X)
    {
        SwapKeymap();
        bIndTch = myConfig.keymap[ucY-6];
        DisplayKeymapName(ucY);
        while (keysCurrent() & KEY_X) 
            ;
        WAITVBL
    }
    showRandomPreviewSnaps();
    swiWaitForVBlank();
  }
  while (keysCurrent() & KEY_B);
}


// -----------------------------------------------------------------------------------------
// At the bottom of the main screen we show the currently selected filename, size and CRC32
// -----------------------------------------------------------------------------------------
void DisplayFileName(void)
{
    sprintf(szName, "[%d K] [CRC: %08X]", file_size/1024, file_crc);
    DSPrint((16 - (strlen(szName)/2)),19,0,szName);
    
    sprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>30) szName[30]='\0';
    DSPrint((16 - (strlen(szName)/2)),21,0,szName);
    if (strlen(gpFic[ucGameChoice].szName) >= 35)   // If there is more than a few characters left, show it on the 2nd line
    {
        sprintf(szName,"%s",gpFic[ucGameChoice].szName+30);
        for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
        if (strlen(szName)>30) szName[30]='\0';
        DSPrint((16 - (strlen(szName)/2)),22,0,szName);
    }
}

//*****************************************************************************
// Display colecoDS info screen and change options "main menu"
//*****************************************************************************
void dispInfoOptions(u32 uY) 
{
    DSPrint(2, 7,(uY== 7 ? 2 : 0),("         LOAD  GAME         "));
    DSPrint(2, 9,(uY== 9 ? 2 : 0),("         PLAY  GAME         "));
    DSPrint(2,11,(uY==11 ? 2 : 0),("     REDEFINE  KEYS         "));
    DSPrint(2,13,(uY==13 ? 2 : 0),("         GAME  OPTIONS      "));
    DSPrint(2,15,(uY==15 ? 2 : 0),("       GLOBAL  OPTIONS      "));
    DSPrint(2,17,(uY==17 ? 2 : 0),("         QUIT  EMULATOR     "));
}

// --------------------------------------------------------------------
// Some main menu selections don't make sense without a game loaded.
// --------------------------------------------------------------------
void NoGameSelected(u32 ucY)
{
    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32); 
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    DSPrint(5,10,0,("   NO GAME SELECTED   ")); 
    DSPrint(5,12,0,("  PLEASE, USE OPTION  ")); 
    DSPrint(5,14,0,("      LOAD  GAME      "));
    while (!(keysCurrent()  & (KEY_START | KEY_A)));
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    dispInfoOptions(ucY);
}

/*********************************************************************************
 * Look for MSX 'AB' header in the ROM file or possibly 0xF331 for SVI ROMs
 ********************************************************************************/
void CheckRomHeaders(char *szGame)
{
    // ------------------------------------------------------------------------------------------
    // MSX Header Bytes:
    //  0 DEFB "AB" ; expansion ROM header
    //  2 DEFW initcode ; start of the init code, 0 if no initcode
    //  4 DEFW callstat; pointer to CALL statement handler, 0 if no such handler
    //  6 DEFW device; pointer to expansion device handler, 0 if no such handler
    //  8 DEFW basic ; pointer to the start of a tokenized basicprogram, 0 if no basicprogram
    // ------------------------------------------------------------------------------------------
    
    // ---------------------------------------------------------------------
    // Do some auto-detection for game ROM. MSX games have 'AB' in their
    // header and we also want to track the INIT address for those ROMs
    // so we can take a better guess at mapping them into our Slot1 memory
    // ---------------------------------------------------------------------
    msx_init = 0x4000;
    msx_basic = 0x0000;
    if ((ROM_Memory[0] == 'A') && (ROM_Memory[1] == 'B'))
    {
        msx_mode = 1;      // MSX roms start with AB (might be in bank 0)
        msx_init = ROM_Memory[2] | (ROM_Memory[3]<<8);
        if (msx_init == 0x0000) msx_basic = ROM_Memory[8] | (ROM_Memory[8]<<8);
        if (msx_init == 0x0000)   // If 0, check for 2nd header... this might be a dummy
        {
            if ((ROM_Memory[0x4000] == 'A') && (ROM_Memory[0x4001] == 'B'))  
            {
                msx_init = ROM_Memory[0x4002] | (ROM_Memory[0x4003]<<8);
                if (msx_init == 0x0000) msx_basic = ROM_Memory[0x4008] | (ROM_Memory[0x4009]<<8);
            }
        }
    }
    else if ((ROM_Memory[0x4000] == 'A') && (ROM_Memory[0x4001] == 'B'))  
    {
        msx_mode = 1;      // MSX roms start with AB (might be in bank 1)
        msx_init = ROM_Memory[0x4002] | (ROM_Memory[0x4003]<<8);
        if (msx_init == 0x0000) msx_basic = ROM_Memory[0x4008] | (ROM_Memory[0x4009]<<8);
    }
    // Check for Spectravideo SVI Cart Header...
    else if ((ROM_Memory[0] == 0xF3) && (ROM_Memory[1] == 0x31))
    {
        if ((strstr(gpFic[ucGameChoice].szName, ".rom")) || (strstr(gpFic[ucGameChoice].szName, ".ROM")))  svi_mode = 2;       // Detected SVI Cartridge header...
    }
}


void ReadFileCRCAndConfig(void)
{    
    u8 checkCOM = 0;
    u8 checkROM = 0;
    u8 cas_load = 0;
    
    // Reset the mode related vars...
    sg1000_mode = 0;
    pv2000_mode = 0;
    sordm5_mode = 0;
    memotech_mode = 0;
    pencil2_mode = 0;
    einstein_mode = 0;
    msx_mode = 0;
    svi_mode = 0;
    adam_mode = 0;
    creativision_mode = 0;
    coleco_mode = 0;
    keyMapType = 0;

    // ----------------------------------------------------------------------------------
    // Clear the entire ROM buffer[] - fill with 0xFF to emulate non-responsive memory
    // ----------------------------------------------------------------------------------
    memset(ROM_Memory, 0xFF, (MAX_CART_SIZE * 1024));

    // Grab the all-important file CRC - this also loads the file into ROM_Memory[]
    getfile_crc(gpFic[ucGameChoice].szName);
    
    if (strstr(gpFic[ucGameChoice].szName, ".sg") != 0) sg1000_mode = 1;    // SG-1000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".SG") != 0) sg1000_mode = 1;    // SG-1000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".sc") != 0) sg1000_mode = 2;    // SC-3000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".SC") != 0) sg1000_mode = 2;    // SC-3000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".pv") != 0) pv2000_mode = 2;    // PV-2000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".PV") != 0) pv2000_mode = 2;    // PV-2000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".CV") != 0) creativision_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".cv") != 0) creativision_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".m5") != 0) sordm5_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".M5") != 0) sordm5_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".mtx") != 0) memotech_mode = 2;
    if (strstr(gpFic[ucGameChoice].szName, ".MTX") != 0) memotech_mode = 2;
    if (strstr(gpFic[ucGameChoice].szName, ".run") != 0) memotech_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".RUN") != 0) memotech_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".msx") != 0) msx_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".MSX") != 0) msx_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".cas") != 0) cas_load = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".CAS") != 0) cas_load = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".ddp") != 0) adam_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".DDP") != 0) adam_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".dsk") != 0) {if (file_size/1024 == 210) einstein_mode = 2; else if (file_size/1024 == 720 || file_size/1024 == 360) msx_mode = 3; else adam_mode = 1;}
    if (strstr(gpFic[ucGameChoice].szName, ".DSK") != 0) {if (file_size/1024 == 210) einstein_mode = 2; else if (file_size/1024 == 720 || file_size/1024 == 360) msx_mode = 3; else adam_mode = 1;}
    if (strstr(gpFic[ucGameChoice].szName, ".adm") != 0) adam_mode = 3;
    if (strstr(gpFic[ucGameChoice].szName, ".ADM") != 0) adam_mode = 3;
    if (strstr(gpFic[ucGameChoice].szName, ".ein") != 0) einstein_mode = 2;
    if (strstr(gpFic[ucGameChoice].szName, ".EIN") != 0) einstein_mode = 2;
    if (strstr(gpFic[ucGameChoice].szName, ".pen") != 0) pencil2_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".PEN") != 0) pencil2_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".com") != 0) checkCOM = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".COM") != 0) checkCOM = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".rom") != 0) checkROM = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".ROM") != 0) checkROM = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".col") != 0) checkROM = 1;  // Coleco types - check if MSX or SVI
    if (strstr(gpFic[ucGameChoice].szName, ".COL") != 0) checkROM = 1;  // Coleco types - check if MSX or SVI
    
    if (checkROM) CheckRomHeaders(gpFic[ucGameChoice].szName);   // See if we've got an MSX or SVI cart - this may set msx_mode=1 or svi_mode=2
    
    if (checkCOM)   // COM is usually Einstein... but we also support it for MTX for some games
    {
        if ( (file_crc == 0xb35a8beb)  ||  // SGM2M - alt
             (file_crc == 0x8e1dd825)  ||  // 1STLETTR.COM
             (file_crc == 0xefd652df)  ||  // ALPHA.COM
             (file_crc == 0x2fa8a871)  ||  // ANGLE.COM
             (file_crc == 0x640efa8d)  ||  // ASTROPAC.COM
             (file_crc == 0x1a50a1e3)  ||  // BUCK.COM
             (file_crc == 0xeb809665)  ||  // CSCPM.COM
             (file_crc == 0xd79a86b7)  ||  // DEFENDER.COM
             (file_crc == 0xac7195d4)  ||  // DT.COM
             (file_crc == 0xcd43d48d)  ||  // H-CHUMY.COM
             (file_crc == 0x62dfae2d)  ||  // L9ADV.COM
             (file_crc == 0xb52a7312)  ||  // NEMO.COM
             (file_crc == 0x7ad2b90e)  ||  // OBLOIDS.COM
             (file_crc == 0x36549adc)  ||  // OLDMAC.COM
             (file_crc == 0x2f3e6416)  ||  // ORBCPM.COM
             (file_crc == 0x8af83858)  ||  // PACMAN.COM
             (file_crc == 0x585478f4)  ||  // POWERPAC.COM
             (file_crc == 0x542051ce)  ||  // QUASAR.COM
             (file_crc == 0x5ed7b7d8)  ||  // REV.COM
             (file_crc == 0x1464e8b0)  ||  // RUN.COM
             (file_crc == 0x6b2d4eb9)  ||  // SASA.COM
             (file_crc == 0x7812bb8c)  ||  // SMG2M.COM
             (file_crc == 0x02fb1412)  ||  // SMG.COM
             (file_crc == 0xcbd19c59)  ||  // SPECTRON.COM
             (file_crc == 0x9f4b067d)  ||  // STAR.COM
             (file_crc == 0x6b67dd68)  ||  // TBUNNY.COM
             (file_crc == 0xcab321fc)  ||  // TLOAD.COM
             (file_crc == 0x8ff4ef96)  ||  // TURBO.COM
             (file_crc == 0x9db0a5d7)  ||  // TV.COM
             (file_crc == 0x46a242b0) )    // ZOMBNEAR.COM
        {
            memotech_mode = 3;  // Memotech MTX .COM file
        }
        else
        {
            einstein_mode = 1;  // Tatung Einstein .COM file
        }
    }
    
    // --------------------------------------------------------------------------
    // If a .cas file is picked, we need to figure out what machine it's for...
    // --------------------------------------------------------------------------
    if (cas_load)
    {
        for (u8 i=0; i<30; i++)
        {
            if ((ROM_Memory[i] == 0x55) && (ROM_Memory[i+2] == 0x55) && (ROM_Memory[i+2] == 0x55))
            {
                svi_mode = 1;
                break;
            }
        }
        if (svi_mode == 0) msx_mode = 2;        // if not SVI, assume MSX
    }
    
    // -----------------------------------------------------------------------
    // If Adam Mode, we need to see if the .ddp or .dsk is a CP/M game so 
    // we check now in the ROM_Memory[] buffer previously read-in.
    // -----------------------------------------------------------------------
    if (adam_mode)
    {
        if (adam_mode == 3) // If we are a .adm file, we need to look at the first byte to tell us if we are perhaps an ADAM expansion ROM
        {
            if (ROM_Memory[0] == 0x66) adam_mode = 2; // 0x6699 at the start indicates we are an ADAM expansion ROM. Otherwise assume normal ADAM cart
        }        
    }
    
    FindConfig();    // Try to find keymap and config for this file...
    
    // --------------------------------------------
    // A few special cases for the CreatiVision
    // --------------------------------------------
    if (file_crc == 0x8375203e) myConfig.cvisionLoad = 3;  // Special load of 16K CSL BIOS at C000-FFFF
    if (file_crc == 0x77afd38b) myConfig.cvisionLoad = 3;  // Special load of 16K CSL BIOS at C000-FFFF
    
    // ------------------------------------------------------------------------
    // And if the cart type is specifically set to ADAM, force that driver in.
    // ------------------------------------------------------------------------
    if (myConfig.cvMode == CV_MODE_ADAM)  adam_mode = 3;
}


// ----------------------------------------------------------------------
// Read file twice and ensure we get the same CRC... if not, do it again
// until we get a clean read. Return the filesize to the caller...
// ----------------------------------------------------------------------
u32 ReadFileCarefully(char *filename, u8 *buf, u32 buf_size, u32 buf_offset)
{
    u32 crc1 = 0;
    u32 crc2 = 1;
    u32 fileSize = 0;
    
    // --------------------------------------------------------------------------------------------
    // I've seen some rare issues with reading files from the SD card on a DSi so we're doing
    // this slow and careful - we will read twice and ensure that we get the same CRC both times.
    // --------------------------------------------------------------------------------------------
    do
    {
        // Read #1
        crc1 = 0xFFFFFFFF;
        FILE* file = fopen(filename, "rb");
        if (file)
        {
            if (buf_offset) fseek(file, buf_offset, SEEK_SET);
            fileSize = fread(buf, 1, buf_size, file);
            crc1 = getCRC32(buf, buf_size);
            fclose(file);
        }

        // Read #2
        crc2 = 0xFFFFFFFF;
        FILE* file2 = fopen(filename, "rb");
        if (file2)
        {
            if (buf_offset) fseek(file2, buf_offset, SEEK_SET);
            fread(buf, 1, buf_size, file2);
            crc2 = getCRC32(buf, buf_size);
            fclose(file2);
        }
   } while (crc1 != crc2); // If the file couldn't be read, file_size will be 0 and the CRCs will both be 0xFFFFFFFF
   
   return fileSize;
}

// --------------------------------------------------------------------
// Let the user select new options for the currently loaded game...
// --------------------------------------------------------------------
void colecoDSChangeOptions(void) 
{
  u16 ucHaut=0x00, ucBas=0x00,ucA=0x00,ucY= 7, bOK=0;
  
  // Upper Screen Background
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_SPRITE_0x06400000);
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(topscreenTiles, bgGetGfxPtr(bg0), LZ77Vram);
  decompress(topscreenMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) topscreenPal,(void*) BG_PALETTE,256*2);
  unsigned short dmaVal =  *(bgGetMapPtr(bg0) + 51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1),32*24*2);
  DSPrint(27,23,1,VERSIONCLDS);

  // Lower Screen Background
  BottomScreenOptions();

  dispInfoOptions(ucY);
  
  if (ucGameChoice != -1) 
  { 
      DisplayFileName();
  }
  
  while (!bOK) {
    if (keysCurrent()  & KEY_UP) {
      if (!ucHaut) {
        dispInfoOptions(32);
        ucY = (ucY == 7 ? 17 : ucY -2);
        ucHaut=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      } 
    }
    else {
      ucHaut = 0;
    }  
    if (keysCurrent()  & KEY_DOWN) {
      if (!ucBas) {
        dispInfoOptions(32);
        ucY = (ucY == 17 ? 7 : ucY +2);
        ucBas=0x01;
        dispInfoOptions(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      } 
    }
    else {
      ucBas = 0;
    }  
    if (keysCurrent()  & KEY_A) {
      if (!ucA) {
        ucA = 0x01;
        switch (ucY) {
          case 7 :      // LOAD GAME
            colecoDSLoadFile();
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
            if (ucGameChoice != -1) 
            { 
                ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
                DisplayFileName();      // And put up the filename on the bottom screen
            }
            ucY = 9;
            dispInfoOptions(ucY);
            break;
          case 9 :     // PLAY GAME
            if (ucGameChoice != -1) 
            { 
                bOK = 1;
            }
            else 
            {    
                NoGameSelected(ucY);
            }
            break;
          case 11 :     // REDEFINE KEYS
            if (ucGameChoice != -1) 
            { 
                colecoDSChangeKeymap();
                dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else 
            { 
                NoGameSelected(ucY);
            }
            break;
          case 13 :     // GAME OPTIONS
            if (ucGameChoice != -1) 
            { 
                colecoDSGameOptions(false);
                dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
                dispInfoOptions(ucY);
                DisplayFileName();
            }
            else 
            {    
               NoGameSelected(ucY);
            }
            break;                
                
          case 15 :     // GLOBAL OPTIONS
            colecoDSGameOptions(true);
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
            dispInfoOptions(ucY);
            DisplayFileName();
            break;
                
          case 17 :     // QUIT EMULATOR
            exit(1);
            break;
        }
      }
    }
    else
      ucA = 0x00;
    if (keysCurrent()  & KEY_START) {
      if (ucGameChoice != -1) 
      {
        bOK = 1;
      }
      else 
      {
        NoGameSelected(ucY);
      }
    }
    showRandomPreviewSnaps();
    swiWaitForVBlank();
  }
  while (keysCurrent()  & (KEY_START | KEY_A));
}

//*****************************************************************************
// Displays a message on the screen
//*****************************************************************************
void DSPrint(int iX,int iY,int iScr,char *szMessage) 
{
  u16 *pusScreen,*pusMap;
  u16 usCharac;
  char *pTrTxt=szMessage;
  
  pusScreen=(u16*) (iScr != 1 ? bgGetMapPtr(bg1b) : bgGetMapPtr(bg1))+iX+(iY<<5);
  pusMap=(u16*) (iScr != 1 ? (iScr == 6 ? bgGetMapPtr(bg0b)+24*32 : (iScr == 0 ? bgGetMapPtr(bg0b)+24*32 : bgGetMapPtr(bg0b)+26*32 )) : bgGetMapPtr(bg0)+51*32 );
    
  while((*pTrTxt)!='\0' )
  {
    char ch = *pTrTxt++;
    if (ch >= 'a' && ch <= 'z') ch -= 32;   // Faster than strcpy/strtoupper
    
    if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);                   // Will render as a vertical bar
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');          // Number from 0-9 or punctuation
    else
      usCharac=*(pusMap+32+(ch)-'@');       // Character from A-Z
    *pusScreen++=usCharac;
  }
}

/******************************************************************************
* Routine FadeToColor :  Fade from background to black or white
******************************************************************************/
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait) {
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out to black
  if (ucScr & 0x01) REG_BLDCNT=ucBG;
  if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
  if (ucSens == 1) {
    for(ucFade=0;ucFade<valEnd;ucFade++) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
  else {
    for(ucFade=16;ucFade>valEnd;ucFade--) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
}

void _putchar(char character) {}

// End of file
