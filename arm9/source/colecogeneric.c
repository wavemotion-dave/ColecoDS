// =====================================================================================
// Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, it's source code and associated 
// readme files, with or without modification, are permitted in any medium without 
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty.
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

#include "sprMario.h"
#include "ecranBasSel.h"
#include "ecranHaut.h"

#include "CRC32.h"

typedef enum {FT_NONE,FT_FILE,FT_DIR} FILE_TYPE;

SpriteEntry OAMCopy[128];

int countCV=0;
int ucGameAct=0;
int ucGameChoice = -1;
FICcoleco gpFic[MAX_ROMS];  
char szName[256];

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
    
  "SPIN X+",          
  "SPIN X-",          
  "SPIN Y+",
  "SPIN Y-"
};


/*********************************************************************************
 * Show A message with YES / NO
 ********************************************************************************/
u8 showMessage(char *szCh1, char *szCh2) {
  u16 iTx, iTy;
  u8 uRet=ID_SHM_CANCEL;
  u8 ucGau=0x00, ucDro=0x00,ucGauS=0x00, ucDroS=0x00, ucCho = ID_SHM_YES;
  
  dmaCopy((void*) bgGetMapPtr(bg0b)+30*32*2,(void*) bgGetMapPtr(bg0b),32*24*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg0b)+24*32); 
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
  AffChaine(16-strlen(szCh1)/2,10,6,szCh1);
  AffChaine(16-strlen(szCh2)/2,12,6,szCh2);
  AffChaine(8,14,6,("> YES <"));
  AffChaine(20,14,6,("  NO   "));
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
          AffChaine(8,14,6,("> YES <"));
          AffChaine(20,14,6,("  NO   "));
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
          AffChaine(8,14,6,("  YES  "));
          AffChaine(20,14,6,("> NO  <"));
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
          AffChaine(8,14,6,("  YES  "));
          AffChaine(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          AffChaine(8,14,6,("> YES <"));
          AffChaine(20,14,6,("  NO   "));
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
          AffChaine(8,14,6,("  YES  "));
          AffChaine(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          AffChaine(8,14,6,("> YES <"));
          AffChaine(20,14,6,("  NO   "));
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
  
  InitBottomScreen();  // Could be generic or overlay...
  
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
// This stuff handles the 'random' Mario sprite walk around the top screen...
// ----------------------------------------------------------------------------
u8 uFlp=0;
signed char iDirAlex=2;
signed short iXAlex=0;
u8 uSprAlex=0,uYAlex=140;

void affMario(void) {
  u16 *pusEcran=(u16*) bgGetMapPtr(bg1);
  u32 uX,uY;

  uFlp++;
  if ((uFlp & 0x04)) {
    uFlp= 0;
    iXAlex += iDirAlex;
    uSprAlex = (uSprAlex == 4 ? 0 : 4);
    if (iXAlex<0) {
      iDirAlex = 2;
      uYAlex = rand() & 176;
    }
    if (iXAlex>256) {
      iDirAlex = -2;
      uYAlex = rand() & 176;
    }
    OAMCopy[0].attribute[0] = uYAlex | ATTR0_SQUARE | ATTR0_COLOR_16;  
    OAMCopy[0].attribute[1] = iXAlex | ATTR1_SIZE_16 | (iDirAlex > 0 ?  ATTR1_FLIP_X : 0);
    OAMCopy[0].attribute[2] = uSprAlex | ATTR2_PRIORITY(0) | ATTR2_PALETTE(0);
    for(uY= 0; uY< 128 * sizeof(SpriteEntry) / 4 ; uY++) {
      ((uint32*)OAM)[uY] = ((uint32*)OAMCopy)[uY];
    }
  }

  if (vusCptVBL>=5*60) {
    u8 uEcran = rand() % 6;
    vusCptVBL = 0;
    if (uEcran>2) {
      uEcran-=3;
      for (uY=24;uY<33;uY++) {
        for (uX=0;uX<12;uX++) {
          *(pusEcran + (14+uX) + ((10+uY-24)<<5)) = *(bgGetMapPtr(bg0) + (uY+uEcran*9)*32 + uX+12); //*((u16*) &ecranHaut_map[uY+uEcran*9][uX+12] );
        }
      }
    }
    else
    {
      for (uY=24;uY<33;uY++) {
        for (uX=0;uX<12;uX++) {
          *(pusEcran + (14+uX) + ((10+uY-24)<<5)) = *(bgGetMapPtr(bg0) + (uY+uEcran*9)*32 + uX); //*((u16*) &ecranHaut_map[uY+uEcran*9][uX] );
        }
      }
    }
  }
}

/*********************************************************************************
 * Show The 14 games on the list to allow the user to choose a new game.
 ********************************************************************************/
void dsDisplayFiles(u16 NoDebGame, u8 ucSel) 
{
  u16 ucBcl,ucGame;
  u8 maxLen;
  char szName[80];
  char szName2[80];
  
  AffChaine(30,8,0,(NoDebGame>0 ? "<" : " "));
  AffChaine(30,21,0,(NoDebGame+14<countCV ? ">" : " "));
  siprintf(szName,"%03d/%03d FILES AVAILABLE     ",ucSel+1+NoDebGame,countCV);
  AffChaine(2,6,0, szName);
  for (ucBcl=0;ucBcl<14; ucBcl++) {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < countCV) 
    {
      maxLen=strlen(gpFic[ucGame].szName);
      strcpy(szName,gpFic[ucGame].szName);
      if (maxLen>28) szName[28]='\0';
      if (gpFic[ucGame].uType == DIRECT) {
        siprintf(szName2, " %s]",szName);
        szName2[0]='[';
        siprintf(szName,"%-28s",szName2);
        AffChaine(1,8+ucBcl,(ucSel == ucBcl ? 2 :  0),szName);
      }
      else {
        siprintf(szName,"%-28s",strupr(szName));
        AffChaine(1,8+ucBcl,(ucSel == ucBcl ? 2 : 0 ),szName);
      }
    }
    else
    {
        AffChaine(1,8+ucBcl,(ucSel == ucBcl ? 2 : 0 ),"                            ");
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
  char szFile[256];
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
      if (!( (szFile[0] == '.') && (strlen(szFile) == 1))) 
      {
        strcpy(gpFic[uNbFile].szName,szFile);
        gpFic[uNbFile].uType = DIRECT;
        uNbFile++;
        countCV++;
      }
    }
    else {
      if ((strlen(szFile)>4) && (strlen(szFile)<(MAX_ROM_LENGTH-4)) ) {
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
  u32 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00,romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage;
  s32 uLenFic=0, ucFlip=0, ucFlop=0;

  // Show the menu...
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B))!=0);
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
  AffChaine(7,5,0,"A=SELECT,  B=EXIT");

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
        AffChaine(1,8+romSelected,2,szName);
      }
    }
    affMario();
    swiWaitForVBlank();
  }
    
  // Remet l'ecran du haut en mode bitmap
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B | KEY_R | KEY_L | KEY_UP | KEY_DOWN))!=0);
  
  return 0x01;
}

struct Config_t AllConfigs[MAX_CONFIGS];
struct Config_t myConfig;
extern u32 file_crc;

// ---------------------------------------------------------------------------
// Write out the ColecoDS.DAT configuration file to capture the settings for
// each game.  This one file contains global settings + 400 game settings.
// ---------------------------------------------------------------------------
void SaveConfig(bool bShow)
{
    FILE *fp;
    int slot = 0;
    
    if (bShow) dsPrintValue(6,0,0, (char*)"SAVING CONFIGURATION");

    // Set the global configuration version number...
    myConfig.config_ver = CONFIG_VER;

    // If there is a game loaded, save that into a slot... re-use the same slot if it exists
    myConfig.game_crc = file_crc;
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
    memcpy(&AllConfigs[slot], &myConfig, sizeof(struct Config_t));

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
        fwrite(&AllConfigs, sizeof(AllConfigs), 1, fp);
        fclose(fp);
    } else dsPrintValue(4,0,0, (char*)"ERROR SAVING CONFIG FILE");

    if (bShow) 
    {
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        dsPrintValue(4,0,0, (char*)"                        ");
    }
}

void SetDefaultGameConfig(void)
{
    myConfig.keymap[0]   = 0;    // NDS D-Pad mapped to CV Joystick UP
    myConfig.keymap[1]   = 1;    // NDS D-Pad mapped to CV Joystick DOWN
    myConfig.keymap[2]   = 2;    // NDS D-Pad mapped to CV Joystick LEFT
    myConfig.keymap[3]   = 3;    // NDS D-Pad mapped to CV Joystick RIGHT
    myConfig.keymap[4]   = 4;    // NDS A Button mapped to CV Button 1 (Yellow / Left Button)
    myConfig.keymap[5]   = 5;    // NDS B Button mapped to CV Button 2 (Red / Right Button)
    myConfig.keymap[6]   = 6;    // NDS X Button mapped to CV Button 3 (Purple / Super Action)
    myConfig.keymap[7]   = 7;    // NDS Y Button mapped to CV Button 4 (Blue / Super Action)
    
    myConfig.keymap[8]   = 10;   // NDS L      mapped to Keypad #3
    myConfig.keymap[9]   = 11;   // NDS R      mapped to Keypad #4
    myConfig.keymap[10]  = 8;    // NDS Start  mapped to Keypad #1
    myConfig.keymap[11]  = 9;    // NDS Select mapped to Keypad #2
    
    myConfig.showFPS     = 0;
    myConfig.frameSkip   = 0;
    myConfig.frameBlend  = 0;
    myConfig.fullSpeed   = 0;        
    myConfig.autoFire1   = 0;
    myConfig.autoFire2   = 0;
    myConfig.overlay     = 0;
    myConfig.maxSprites  = 0;
    myConfig.vertSync    = (isDSiMode() ? 1:0);
    myConfig.spinSpeed   = 0;    
    myConfig.touchPad    = 0;
    myConfig.cpuCore     = 0;    // Default is the faster DrZ80 core
    myConfig.reserved9   = 0;    
    myConfig.reservedA   = 0;    
    myConfig.reservedB   = 0;    
    myConfig.reservedC   = 0;    
    
    // And a few odd defaults 
    if (file_crc == 0xee530ad2) myConfig.maxSprites  = 1;  // QBiqs
    if (file_crc == 0x275c800e) myConfig.maxSprites  = 1;  // Antartic Adventure
    if (file_crc == 0xa66e5ed1) myConfig.maxSprites  = 1;  // Antartic Adventure Prototype  
    if (file_crc == 0x6af19e75) myConfig.maxSprites  = 1;  // Adventures in the Park    
    
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
    
    // ------------------------------------------------------------------------
    // These games all need the slower but higher compatibility CZ80 CPU Core
    // ------------------------------------------------------------------------
    if (
        (file_crc == 0xead5e824) ||     // Arno Dash
        (file_crc == 0x3b27ed05) ||     // Astro Storm
        (file_crc == 0x77900970) ||     // Deep Dungeon Adventures
        (file_crc == 0x5576dec3) ||     // Diamond Dash II
        (file_crc == 0x70d55091) ||     // Dungeon and Trolls
        (file_crc == 0xb3b767ae) ||     // Fathom (Imagic)        
        (file_crc == 0xf43b0b28) ||     // Frantic (homebrew)
        (file_crc == 0x27818d93) ||     // Hang-On 
        (file_crc == 0x278c5021) ||     // Klondike Solitaire
        (file_crc == 0x6ed6a2e1) ||     // Mahjong Solitaire
        (file_crc == 0x5cd9d34a) ||     // Minesweeper
        (file_crc == 0xdd730dbd) ||     // Missile Strike
        (file_crc == 0xb47377fd) ||     // Pegged
        (file_crc == 0x5a49b249) ||     // Pillars
        (file_crc == 0x50998610) ||     // Pitman
        (file_crc == 0xd8caac4c) ||     // Rip Cord
        (file_crc == 0x260cdf98) ||     // Super Pac-Mans
        (file_crc == 0xae209065) ||     // Super Space Acer
        (file_crc == 0xbc8320a0) ||     // Uridium
        (file_crc == 0x530c586f)        // Vexxed
        )
    {
        myConfig.cpuCore = 1;
    }
}

// -------------------------------------------------------------------------
// Find the ColecoDS.DAT file and load it... if it doesn't exist, then
// default values will be used for the entire configuration database...
// -------------------------------------------------------------------------
void FindAndLoadConfig(void)
{
    FILE *fp;

    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();
    
    fp = fopen("/data/ColecoDS.DAT", "rb");
    if (fp != NULL)
    {
        fread(&AllConfigs, sizeof(AllConfigs), 1, fp);
        fclose(fp);
        
        if (AllConfigs[0].config_ver != CONFIG_VER)
        {
            memset(&AllConfigs, 0x00, sizeof(AllConfigs));
            SetDefaultGameConfig();
            SaveConfig(FALSE);
        }
        else
        {
            for (int slot=0; slot<MAX_CONFIGS; slot++)
            {
                if (AllConfigs[slot].game_crc == file_crc)  // Got a match?!
                {
                    memcpy(&myConfig, &AllConfigs[slot], sizeof(struct Config_t));
                    break;                           
                }
            }
        }
    }
    else    // Not found... init the entire database...
    {
        memset(&AllConfigs, 0x00, sizeof(AllConfigs));
        SetDefaultGameConfig();
        SaveConfig(FALSE);
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
    const char  *option[25];
    u8          *option_val;
    u8           option_max;
};

u8 dev_z80_cycles = 0;
const struct options_t Option_Table[] =
{
    {"OVERLAY",         {"GENERIC", "WARGAMES", "MOUSETRAP", "GATEWAY", "SPY HUNTER", "FIX UP MIX UP", "BOULDER DASH", "QUINTA ROO", "2010"},   &myConfig.overlay,    9},
    {"FPS",             {"OFF", "ON"},                                                                                                          &myConfig.showFPS,    2},
    {"FULL SPEED",      {"OFF", "ON"},                                                                                                          &myConfig.fullSpeed,  2},
    {"FRAME SKIP",      {"OFF", "ON"},                                                                                                          &myConfig.frameSkip,  2},
    {"FRAME BLEND",     {"OFF", "ON"},                                                                                                          &myConfig.frameBlend, 2},
    {"MAX SPRITES",     {"32",  "4"},                                                                                                           &myConfig.maxSprites, 2},
    {"VERT SYNC",       {"OFF", "ON"},                                                                                                          &myConfig.vertSync,   2},    
    {"AUTO FIRE B1",    {"OFF", "ON"},                                                                                                          &myConfig.autoFire1,  2},
    {"AUTO FIRE B2",    {"OFF", "ON"},                                                                                                          &myConfig.autoFire2,  2},
    {"TOUCH PAD",       {"PLAYER 1", "PLAYER 2"},                                                                                               &myConfig.touchPad,   2},    
    {"SPIN SPEED",      {"NORMAL", "FAST", "FASTEST", "SLOW", "SLOWEST"},                                                                       &myConfig.spinSpeed,  5},
    {"Z80 CPU CORE",    {"DRZ80 (Fast)", "CZ80 (Slow)"},                                                                                        &myConfig.cpuCore,    2},    
#if 0   // Developer use only   
    {"Z80 CYCLES!!",    {"NORMAL", "+1", "+2", "+3", "+4", "+5", "+6", "+7", "+8", "+9", "+10", "-1", "-2", "-3", "-4", "-5"},                  &dev_z80_cycles,     16},
#endif    
    {NULL,              {"",      ""},                                                                                                          NULL,                 1},
};              


// ------------------------------------------------------------------
// Display the current list of options for the user.
// ------------------------------------------------------------------
u8 display_options_list(bool bFullDisplay)
{
    char strBuf[35];
    int len=0;
    
    if (bFullDisplay)
    {
        while (true)
        {
            siprintf(strBuf, " %-12s : %-14s", Option_Table[len].label, Option_Table[len].option[*(Option_Table[len].option_val)]);
            dsPrintValue(1,7+len, (len==0 ? 2:0), strBuf); len++;
            if (Option_Table[len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<15; i++) 
        {
            dsPrintValue(1,7+i, 0, (char *)"                               ");
        }
    }

    dsPrintValue(2,21, 0, (char *)"USE DPAD. B=EXIT, START=SAVE");
    return len;    
}


//*****************************************************************************
// Change Game Options for the current game
//*****************************************************************************
void colecoDSGameOptions(void)
{
    extern s16 timingAdjustment;
    u8 optionHighlighted;
    u8 idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;
    char strBuf[35];
    
    // ----------------------------------------------------
    // Load up the timing adjustment for the Z80. This is
    // a bit of a "fudge factor" for the few games that
    // need help due to the slightly inaccurate Z80 core.
    // ----------------------------------------------------
    if      (timingAdjustment == -1) dev_z80_cycles = 11;
    else if (timingAdjustment == -2) dev_z80_cycles = 12;
    else if (timingAdjustment == -3) dev_z80_cycles = 13;
    else if (timingAdjustment == -4) dev_z80_cycles = 14;
    else if (timingAdjustment == -5) dev_z80_cycles = 15;
    else  dev_z80_cycles = timingAdjustment;

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
                siprintf(strBuf, " %-12s : %-14s", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,7+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                siprintf(strBuf, " %-12s : %-14s", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,7+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                siprintf(strBuf, " %-12s : %-14s", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,7+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                siprintf(strBuf, " %-12s : %-14s", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,7+optionHighlighted,2, strBuf);
            }

            if (keysCurrent() & KEY_RIGHT)  // Toggle option clockwise
            {
                *(Option_Table[optionHighlighted].option_val) = (*(Option_Table[optionHighlighted].option_val) + 1) % Option_Table[optionHighlighted].option_max;
                siprintf(strBuf, " %-12s : %-14s", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,7+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_LEFT)  // Toggle option counterclockwise
            {
                if ((*(Option_Table[optionHighlighted].option_val)) == 0)
                    *(Option_Table[optionHighlighted].option_val) = Option_Table[optionHighlighted].option_max -1;
                else
                    *(Option_Table[optionHighlighted].option_val) = (*(Option_Table[optionHighlighted].option_val) - 1) % Option_Table[optionHighlighted].option_max;
                siprintf(strBuf, " %-12s : %-14s", Option_Table[optionHighlighted].label, Option_Table[optionHighlighted].option[*(Option_Table[optionHighlighted].option_val)]);
                dsPrintValue(1,7+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_START)  // Save Options
            {
                SaveConfig(TRUE);
            }
            if ((keysCurrent() & KEY_B) || (keysCurrent() & KEY_A))  // Exit options
            {
                break;
            }
        }
        affMario();
        swiWaitForVBlank();
    }

    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }
    
    if      (dev_z80_cycles == 11) timingAdjustment = -1;
    else if (dev_z80_cycles == 12) timingAdjustment = -2;
    else if (dev_z80_cycles == 13) timingAdjustment = -3;
    else if (dev_z80_cycles == 14) timingAdjustment = -4;
    else if (dev_z80_cycles == 15) timingAdjustment = -5;
    else  timingAdjustment = dev_z80_cycles;
    
    return;
}

//*****************************************************************************
// Change Keymap Options for the current game
//*****************************************************************************
void DisplayKeymapName(u32 uY) 
{
  char szCha[34];

  siprintf(szCha," PAD UP    : %-17s",szKeyName[myConfig.keymap[0]]);
  AffChaine(1, 7,(uY==  7 ? 2 : 0),szCha);
  siprintf(szCha," PAD DOWN  : %-17s",szKeyName[myConfig.keymap[1]]);
  AffChaine(1, 8,(uY==  8 ? 2 : 0),szCha);
  siprintf(szCha," PAD LEFT  : %-17s",szKeyName[myConfig.keymap[2]]);
  AffChaine(1, 9,(uY==  9 ? 2 : 0),szCha);
  siprintf(szCha," PAD RIGHT : %-17s",szKeyName[myConfig.keymap[3]]);
  AffChaine(1,10,(uY== 10 ? 2 : 0),szCha);
  siprintf(szCha," KEY A     : %-17s",szKeyName[myConfig.keymap[4]]);
  AffChaine(1,11,(uY== 11 ? 2 : 0),szCha);
  siprintf(szCha," KEY B     : %-17s",szKeyName[myConfig.keymap[5]]);
  AffChaine(1,12,(uY== 12 ? 2 : 0),szCha);
  siprintf(szCha," KEY X     : %-17s",szKeyName[myConfig.keymap[6]]);
  AffChaine(1,13,(uY== 13 ? 2 : 0),szCha);
  siprintf(szCha," KEY Y     : %-17s",szKeyName[myConfig.keymap[7]]);
  AffChaine(1,14,(uY== 14 ? 2 : 0),szCha);
  siprintf(szCha," KEY R     : %-17s",szKeyName[myConfig.keymap[8]]);
  AffChaine(1,15,(uY== 15 ? 2 : 0),szCha);
  siprintf(szCha," KEY L     : %-17s",szKeyName[myConfig.keymap[9]]);
  AffChaine(1,16,(uY== 16 ? 2 : 0),szCha);
  siprintf(szCha," START     : %-17s",szKeyName[myConfig.keymap[10]]);
  AffChaine(1,17,(uY== 17 ? 2 : 0),szCha);
  siprintf(szCha," SELECT    : %-17s",szKeyName[myConfig.keymap[11]]);
  AffChaine(1,18,(uY== 18 ? 2 : 0),szCha);
}

// ------------------------------------------------------------------------------
// Allow the user to change the key map for the current game and give them
// the option of writing that keymap out to a configuration file for the game.
// ------------------------------------------------------------------------------
void colecoDSChangeKeymap(void) 
{
  u32 ucHaut=0x00, ucBas=0x00,ucL=0x00,ucR=0x00,ucY= 7, bOK=0, bIndTch=0;

  // ------------------------------------------------------
  // Clear the screen so we can put up Key Map infomation
  // ------------------------------------------------------
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
    
  // --------------------------------------------------
  // Give instructions to the user...
  // --------------------------------------------------
  AffChaine(9,5,0,("=*   KEYS  *="));
  AffChaine(1 ,20,0,("   D-PAD : CHANGE KEY MAP    "));
  AffChaine(1 ,21,0,("       B : RETURN MAIN MENU  "));
  AffChaine(1 ,22,0,("   START : SAVE KEYMAP       "));
  DisplayKeymapName(ucY);
  
  // -----------------------------------------------------------------------
  // Clear out any keys that might be pressed on the way in - make sure
  // NDS keys are not being pressed. This prevents the inadvertant A key
  // that enters this menu from also being acted on in the keymap...
  // -----------------------------------------------------------------------
  while ((keysCurrent() & (KEY_TOUCH | KEY_B | KEY_A | KEY_UP | KEY_DOWN))!=0)
      ;
  WAITVBL;
 
  while (!bOK) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        DisplayKeymapName(32);
        ucY = (ucY == 7 ? 18 : ucY -1);
        bIndTch = myConfig.keymap[ucY-7];
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
        ucY = (ucY == 18 ? 7 : ucY +1);
        bIndTch = myConfig.keymap[ucY-7];
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
          myConfig.keymap[ucY-7] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else {
          ucL++;
          if (ucL > 10) ucL = 0;
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
          myConfig.keymap[ucY-7] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else 
        {
          ucR++;
          if (ucR > 10) ucR = 0;
        }
    }
    else
    {
        ucR=0;
    }
    affMario();
    swiWaitForVBlank();
  }
  while (keysCurrent() & KEY_B);
}


// ----------------------------------------------------------------------------------
// At the bottom of the main screen we show the currently selected filename and CRC
// ----------------------------------------------------------------------------------
void DisplayFileName(void)
{
    char szName[64];
    siprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>30) szName[30]='\0';
    AffChaine((16 - (strlen(szName)/2)),22,0,szName);
#if 1 // Display CRC... for now 
    siprintf(szName, "      [%08X]      ", (int)file_crc);
    AffChaine(5,21,0,szName);
#endif    
}

//*****************************************************************************
// Display colecoDSlus screen and change options "main menu"
//*****************************************************************************
void affInfoOptions(u32 uY) 
{
    AffChaine(2, 8,(uY== 8 ? 2 : 0),("         LOAD  GAME         "));
    AffChaine(2,10,(uY==10 ? 2 : 0),("         PLAY  GAME         "));
    AffChaine(2,12,(uY==12 ? 2 : 0),("     REDEFINE  KEYS         "));
    AffChaine(2,14,(uY==14 ? 2 : 0),("        GAME   OPTIONS      "));
    AffChaine(2,16,(uY==16 ? 2 : 0),("        QUIT   EMULATOR     "));
    AffChaine(6,19,0,("USE D-PAD  A=SELECT"));
}

// --------------------------------------------------------------------
// Some main menu selections don't make sense without a game loaded.
// --------------------------------------------------------------------
void NoGameSelected(u32 ucY)
{
    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32); 
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    AffChaine(5,10,0,("   NO GAME SELECTED   ")); 
    AffChaine(5,12,0,("  PLEASE, USE OPTION  ")); 
    AffChaine(5,14,0,("      LOAD  GAME      "));
    while (!(keysCurrent()  & (KEY_START | KEY_A)));
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    AffChaine(9,5,0,"=* OPTIONS *=");
    affInfoOptions(ucY);
}

// --------------------------------------------------------------------
// Let the user select new options for the currently loaded game...
// --------------------------------------------------------------------
void colecoDSChangeOptions(void) 
{
  u32 ucHaut=0x00, ucBas=0x00,ucA=0x00,ucY= 8, bOK=0, bBcl;
  
  // Affiche l'ecran en haut
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_SPRITE_0x06400000);
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(ecranHautTiles, bgGetGfxPtr(bg0), LZ77Vram);
  decompress(ecranHautMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) ecranHautPal,(void*) BG_PALETTE,256*2);
  unsigned short dmaVal =  *(bgGetMapPtr(bg0) + 51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1),32*24*2);
  AffChaine(28,23,1,"V");AffChaine(29,23,1,VERSIONCLDS);

    // Init sprites
  for (bBcl=0;bBcl<128;bBcl++) {
    OAMCopy[bBcl].attribute[0] = ATTR0_DISABLED;  
  }
    // Init sprites
  dmaCopy((void*) sprMarioTiles,(void*) SPRITE_GFX,sizeof(sprMarioTiles));
  dmaCopy((void*) sprMarioPal,(void*) SPRITE_PALETTE,16*2);
  OAMCopy[0].attribute[0] = 192 | ATTR0_SQUARE | ATTR0_COLOR_16;  
  OAMCopy[0].attribute[1] = 0 | ATTR1_SIZE_16;
  OAMCopy[0].attribute[2] = 0 | ATTR2_PRIORITY(0) | ATTR2_PALETTE(0);
	for(bBcl= 0; bBcl< 128 * sizeof(SpriteEntry) / 4 ; bBcl++) {
		((uint32*)OAM)[bBcl] = ((uint32*)OAMCopy)[bBcl];
	}

  // Affiche le clavier en bas
  bg0b = bgInitSub(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1b = bgInitSub(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0b,1);bgSetPriority(bg1b,0);
  decompress(ecranBasSelTiles, bgGetGfxPtr(bg0b), LZ77Vram);
  decompress(ecranBasSelMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  dmaCopy((void*) ecranBasSelPal,(void*) BG_PALETTE_SUB,256*2);
  dmaVal = *(bgGetMapPtr(bg1b)+24*32); 
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b),32*24*2);

  AffChaine(9,5,0,"=* OPTIONS *=");
  affInfoOptions(ucY);
  
  if (ucGameChoice != -1) 
  { 
      DisplayFileName();
  }
  
  while (!bOK) {
    if (keysCurrent()  & KEY_UP) {
      if (!ucHaut) {
        affInfoOptions(32);
        ucY = (ucY == 8 ? 16 : ucY -2);
        ucHaut=0x01;
        affInfoOptions(ucY);
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
        affInfoOptions(32);
        ucY = (ucY == 16 ? 8 : ucY +2);
        ucBas=0x01;
        affInfoOptions(ucY);
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
          case 8 :      // LOAD GAME
            colecoDSLoadFile();
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
            if (ucGameChoice != -1) 
            { 
                getfile_crc(gpFic[ucGameChoice].szName);
                FindAndLoadConfig();    // Try to find keymap for this file...
                DisplayFileName();
            }
            ucY = 10;
            AffChaine(9,5,0,"=* OPTIONS *=");
            affInfoOptions(ucY);
            break;
          case 10 :     // PLAY GAME
            if (ucGameChoice != -1) 
            { 
              bOK = 1;
            }
            else 
            {    
                NoGameSelected(ucY);
            }
            break;
          case 12 :     // REDEFINE KEYS
            if (ucGameChoice != -1) 
            { 
                colecoDSChangeKeymap();
                dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
                AffChaine(9,5,0,"=* OPTIONS *=");
                affInfoOptions(ucY);
                DisplayFileName();
            }
            else 
            { 
                NoGameSelected(ucY);
            }
            break;
          case 14 :     // GAME OPTIONS
            if (ucGameChoice != -1) 
            { 
                colecoDSGameOptions();
                dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
                AffChaine(9,5,0,"=* OPTIONS *=");
                affInfoOptions(ucY);
                DisplayFileName();
            }
            else 
            {    
               NoGameSelected(ucY);
            }
            break;                
                
          case 16 :     // QUIT EMULATOR
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
    affMario();
    swiWaitForVBlank();
  }
  while (keysCurrent()  & (KEY_START | KEY_A));
}

//*****************************************************************************
// Displays a message on the screen
//*****************************************************************************

void dsPrintValue(int iX,int iY,int iScr,char *szMessage)
{
    AffChaine(iX,iY,iScr,szMessage);
}

void AffChaine(int iX,int iY,int iScr,char *szMessage) {
  u16 *pusEcran,*pusMap;
  u16 usCharac;
  char szTexte[128],*pTrTxt=szTexte;
  
  strcpy(szTexte,szMessage);
  strupr(szTexte);
  pusEcran=(u16*) (iScr != 1 ? bgGetMapPtr(bg1b) : bgGetMapPtr(bg1))+iX+(iY<<5);
  pusMap=(u16*) (iScr != 1 ? (iScr == 6 ? bgGetMapPtr(bg0b)+24*32 : (iScr == 0 ? bgGetMapPtr(bg0b)+24*32 : bgGetMapPtr(bg0b)+26*32 )) : bgGetMapPtr(bg0)+51*32 );
    
  while((*pTrTxt)!='\0' )
  {
    char ch = *pTrTxt;
    if (ch >= 'a' && ch <= 'z') ch -= 32; // Faster than strcpy/strtoupper
    usCharac=0x0000;
    if ((ch) == '|')
      usCharac=*(pusMap);
    else if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');
    else
      usCharac=*(pusMap+32+(ch)-'@');
    *pusEcran++=usCharac;
    pTrTxt++;
  }
}

/******************************************************************************
* Routine FadeToColor :  Fade from background to black or white
******************************************************************************/
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait) {
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out vers le noir
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

// End of file
