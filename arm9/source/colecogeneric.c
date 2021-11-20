#include <nds.h>

#include <stdlib.h>
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "colecoDS.h"
#include "colecogeneric.h"

#include "sprMario.h"
#include "ecranBas.h"
#include "ecranBasSel.h"
#include "ecranHaut.h"

typedef enum {FT_NONE,FT_FILE,FT_DIR} FILE_TYPE;

SpriteEntry OAMCopy[128];

int countCV=0;
int ucGameAct=0;
int ucGameChoice = -1;
FICcoleco gpFic[MAX_ROMS];  

/*********************************************************************************
 * Show A message with YES / NO
 ********************************************************************************/
u8 showMessage(char *szCh1, char *szCh2) {
  u16 iTx, iTy;
  u8 uRet=ID_SHM_CANCEL;
  u8 ucGau=0x00, ucDro=0x00,ucGauS=0x00, ucDroS=0x00, ucCho = ID_SHM_YES;
  
  dmaCopy((void*) bgGetMapPtr(bg0b)+30*32*2,(void*) bgGetMapPtr(bg0b),32*24*2);
  unsigned short dmaVal = *(bgGetMapPtr(bg0b)+24*32); //ecranBas_map[24][0];
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
  AffChaine(16-strlen(szCh1)/2,10,6,szCh1);
  AffChaine(16-strlen(szCh2)/2,12,6,szCh2);
  AffChaine(8,14,6,(lgeEmul == 0 ? "> OUI <" : "> YES <"));
  AffChaine(20,14,6,(lgeEmul == 0 ? "  NON  " : "  NO   "));
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  while (uRet == ID_SHM_CANCEL) {
    if (keysCurrent() & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ( (iTx>8*8) && (iTx<8*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucGauS) {
          AffChaine(8,14,6,(lgeEmul == 0 ? "> OUI <" : "> YES <"));
          AffChaine(20,14,6,(lgeEmul == 0 ? "  NON  " : "  NO   "));
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
          AffChaine(8,14,6,(lgeEmul == 0 ? "  OUI  " : "  YES  "));
          AffChaine(20,14,6,(lgeEmul == 0 ? "> NON <" : "> NO  <"));
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
          AffChaine(8,14,6,(lgeEmul == 0 ? "  OUI  " : "  YES  "));
          AffChaine(20,14,6,(lgeEmul == 0 ? "> NON <" : "> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          AffChaine(8,14,6,(lgeEmul == 0 ? "> OUI <" : "> YES <"));
          AffChaine(20,14,6,(lgeEmul == 0 ? "  NON  " : "  NO   "));
        }
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
          AffChaine(8,14,6,(lgeEmul == 0 ? "  OUI  " : "  YES  "));
          AffChaine(20,14,6,(lgeEmul == 0 ? "> NON <" : "> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          AffChaine(8,14,6,(lgeEmul == 0 ? "> OUI <" : "> YES <"));
          AffChaine(20,14,6,(lgeEmul == 0 ? "  NON  " : "  NO   "));
        }
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
  decompress(ecranBasMap, (void*) bgGetMapPtr(bg0b), LZ77Vram);
  return uRet;
}

void colecoDSModeNormal(void) {
#ifdef NOCASH
  nocashMessage("colecoDSModeNormal");
#endif
#ifdef JMG16B
  REG_BG3CNT = BG_BMP16_256x256;
#else  
  REG_BG3CNT = BG_BMP8_256x256;
#endif
  REG_BG3PA = (1<<8); 
  REG_BG3PB = 0;
  REG_BG3PC = 0;
  REG_BG3PD = (1<<8);
  REG_BG3X = 0;
  REG_BG3Y = 0;
}

//*****************************************************************************
// Met l'ecran du haut en mode bitmap recentré
//*****************************************************************************
void colecoDSInitScreenUp(void) {
#ifdef NOCASH
  nocashMessage("colecoDSInitScreenUp");
#endif
  videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);// | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE );
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
  vramSetBankB(VRAM_B_MAIN_SPRITE);
  colecoDSModeNormal();
}

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
 * Show The 14 games on the list
 ********************************************************************************/
void dsDisplayFiles(u16 NoDebGame, u8 ucSel) {
//  u16 *pusEcran;
  u16 ucBcl,ucGame;
  u8 maxLen;
  char szName[80];
  char szName2[80];
  
  // Affichage des 14 jeux possibles
//  pusEcran=(u16*) SCREEN_BASE_BLOCK_SUB(31);
  AffChaine(30,8,0,(NoDebGame>0 ? "<" : " "));
  AffChaine(30,21,0,(NoDebGame+14<countCV ? ">" : " "));
  (lgeEmul == 0 ? sprintf(szName,"%03d/%03d FICHIERS DISPONIBLES",ucSel+1+NoDebGame,countCV) :  
                   sprintf(szName,"%03d/%03d FILES AVAILABLE     ",ucSel+1+NoDebGame,countCV));
  AffChaine(2,6,0, szName);
  for (ucBcl=0;ucBcl<14; ucBcl++) {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < countCV) 
    {
      maxLen=strlen(gpFic[ucGame].szName);
      strcpy(szName,gpFic[ucGame].szName);
      if (maxLen>28) szName[28]='\0';
      if (gpFic[ucGame].uType == DIRECT) {
        sprintf(szName2, " %s]",szName);
        szName2[0]='[';
        sprintf(szName,"%-28s",szName2);
        AffChaine(1,8+ucBcl,(ucSel == ucBcl ? 2 :  0),szName);
      }
      else {
        sprintf(szName,"%-28s",strupr(szName));
        AffChaine(1,8+ucBcl,(ucSel == ucBcl ? 2 : 0 ),szName);
      }
    }
    else
    {
        AffChaine(1,8+ucBcl,(ucSel == ucBcl ? 2 : 0 ),"                            ");
    }
  }
}


/*********************************************************************************
 * Find files (COL / ROM) available
 ********************************************************************************/
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

u8 bFullSpeed = false;
extern u8 bBlendMode;

//*****************************************************************************
// charge une rom
//*****************************************************************************
char szName[256];


u8 colecoDSLoadFile(void) 
{
  bool bDone=false;
  u32 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00,romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage;
  s32 uLenFic=0, ucFlip=0, ucFlop=0;

  // Show the menu...
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B))!=0);
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);// ecranBas_map[24][0];
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
  AffChaine(15-strlen(szLang[lgeEmul][20])/2,5,0,szLang[lgeEmul][20]);

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
        if (keysCurrent() & KEY_X) bFullSpeed = true; else bFullSpeed = false;
        if (keysCurrent() & KEY_Y) bBlendMode = true; else bBlendMode = false;          
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

//*****************************************************************************
// Affiche l'ecran CPC et change les touches
//*****************************************************************************
void affInfoTouches(u32 uY) 
{
  char szCha[64];

  sprintf(szCha," PAD UP    : %-17s",szKeyName[keyboard_JoyNDS[0]]);
  AffChaine(1, 7,(uY==  7 ? 2 : 0),szCha);
  sprintf(szCha," PAD DOWN  : %-17s",szKeyName[keyboard_JoyNDS[1]]);
  AffChaine(1, 8,(uY==  8 ? 2 : 0),szCha);
  sprintf(szCha," PAD LEFT  : %-17s",szKeyName[keyboard_JoyNDS[2]]);
  AffChaine(1, 9,(uY==  9 ? 2 : 0),szCha);
  sprintf(szCha," PAD RIGHT : %-17s",szKeyName[keyboard_JoyNDS[3]]);
  AffChaine(1,10,(uY== 10 ? 2 : 0),szCha);
  sprintf(szCha," KEY A     : %-17s",szKeyName[keyboard_JoyNDS[4]]);
  AffChaine(1,11,(uY== 11 ? 2 : 0),szCha);
  sprintf(szCha," KEY B     : %-17s",szKeyName[keyboard_JoyNDS[5]]);
  AffChaine(1,12,(uY== 12 ? 2 : 0),szCha);
  sprintf(szCha," KEY X     : %-17s",szKeyName[keyboard_JoyNDS[6]]);
  AffChaine(1,13,(uY== 13 ? 2 : 0),szCha);
  sprintf(szCha," KEY Y     : %-17s",szKeyName[keyboard_JoyNDS[7]]);
  AffChaine(1,14,(uY== 14 ? 2 : 0),szCha);
  sprintf(szCha," KEY R     : %-17s",szKeyName[keyboard_JoyNDS[8]]);
  AffChaine(1,15,(uY== 15 ? 2 : 0),szCha);
  sprintf(szCha," KEY L     : %-17s",szKeyName[keyboard_JoyNDS[9]]);
  AffChaine(1,16,(uY== 16 ? 2 : 0),szCha);
  sprintf(szCha," START     : %-17s",szKeyName[keyboard_JoyNDS[10]]);
  AffChaine(1,17,(uY== 17 ? 2 : 0),szCha);
  sprintf(szCha," SELECT    : %-17s",szKeyName[keyboard_JoyNDS[11]]);
  AffChaine(1,18,(uY== 18 ? 2 : 0),szCha);
}
void colecoDSChangeTouches(void) {
  u32 ucHaut=0x00, ucBas=0x00,ucL=0x00,ucR=0x00,ucA=0x00,ucY= 7, bOK=0, bTch, bIndTch;

  // Efface l'ecran pour mettre les touches
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);// ecranBas_map[24][0];
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
  AffChaine(9,5,0,(lgeEmul == 0 ? "=* TOUCHES *=" : "=*   KEYS  *="));
  AffChaine(4 ,20,0,(lgeEmul == 0 ? "    B : RETOUR AUX OPTIONS" : "    B : RETURN TO OPTIONS "));
  AffChaine(4 ,21,0,(lgeEmul == 0 ? "    A : CHOISIR TOUCHE    " : "    A : REDEFINE THIS KEY "));
  affInfoTouches(ucY);
  
  while ((keysCurrent() & (KEY_TOUCH | KEY_B | KEY_A | KEY_UP | KEY_DOWN))!=0)
      ;
 
  while (!bOK) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        affInfoTouches(32);
        ucY = (ucY == 7 ? 18 : ucY -1);
        ucHaut=0x01;
        affInfoTouches(ucY);
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
        affInfoTouches(32);
        ucY = (ucY == 18 ? 7 : ucY +1);
        ucBas=0x01;
        affInfoTouches(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      } 
    }
    else {
      ucBas = 0;
    }  
    if (keysCurrent() & KEY_A) {
      if (!ucA) {
        ucA = 0x01;
        AffChaine(4 ,20,0,(lgeEmul == 0 ? " GCHE/DRTE : CHANGE TOUCHE" : "LEFT/RIGHT : CHANGE KEY   "));
        AffChaine(4 ,21,0,(lgeEmul == 0 ? "         A : VALIDE CHOIX " : "         A : CONFIRM ENTRY  "));
        while (keysCurrent() & KEY_A);
        bTch = 0x00;
        bIndTch = keyboard_JoyNDS[ucY-7];
        while(!bTch) {
          affMario();
          swiWaitForVBlank();
          if (keysCurrent() & KEY_A) {
            bTch=1;
          }
          if (keysCurrent() & KEY_LEFT) {
            if (ucL == 0) {
              bIndTch = (bIndTch == 0 ? 17 : bIndTch-1);
              ucL=1;
              keyboard_JoyNDS[ucY-7] = bIndTch;
              affInfoTouches(ucY);
            }
            else {
              ucL++;
              if (ucL > 10) ucL = 0;
            }
          }
          else {
            ucL = 0;
          }
          if (keysCurrent() & KEY_RIGHT) {
            if (ucR == 0) {
              bIndTch = (bIndTch == 17 ? 0 : bIndTch+1);
              ucR=1;
              keyboard_JoyNDS[ucY-7] = bIndTch;
              affInfoTouches(ucY);
            }
            else {
              ucR++;
              if (ucR > 10) ucR = 0;
            }
          }
          else
            ucR  = 0;
        }
        AffChaine(4 ,20,0,(lgeEmul == 0 ? "    B : RETOUR AUX OPTIONS" : "    B : RETURN TO OPTIONS "));
        AffChaine(4 ,21,0,(lgeEmul == 0 ? "    A : CHOISIR TOUCHE    " : "    A : CHOOSE KEY        "));
        while (keysCurrent()  & KEY_A);
      }
    }
    else
      ucA = 0x00;
    if (keysCurrent() & KEY_B) {
      bOK = 1;
    }
    affMario();
    swiWaitForVBlank();
  }
  while (keysCurrent() & KEY_B);
}


u32 affInfoReport(void) 
{
  return 0;
}

void DisplayFileName(void)
{
    char szName[64];
    sprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>30) szName[30]='\0';
    AffChaine((16 - (strlen(szName)/2)),22,0,szName);
}


//*****************************************************************************
// Affiche l'ecran de colecoDSlus et change les options 
//*****************************************************************************
void affInfoOptions(u32 uY) {
  AffChaine(2, 9,(uY== 9 ? 2 : 0),(lgeEmul == 0 ? "      CHARGEMENT JEU        " : "         LOAD  GAME         "));
  AffChaine(2,11,(uY==11 ? 2 : 0),(lgeEmul == 0 ? "  LANCEMENT DU JEU ACTUEL   " : "    EXECUTE ACTUAL GAME     "));
  AffChaine(2,13,(uY==13 ? 2 : 0),(lgeEmul == 0 ? "     REDEFINIR TOUCHES      " : "      REDEFINE   KEYS       "));
  AffChaine(2,15,(uY==15 ? 2 : 0),(lgeEmul == 0 ? "     LANGUE : FRANCAIS      " : "     LANGUAGE : ENGLISH     "));
  AffChaine(5,19,0,(lgeEmul == 0 ? "START : LANCER LE JEU " : "START : PLAY GAME     "));
  AffChaine(5,20,0,(lgeEmul == 0 ? "    A : CHOISIR OPTION" : "    A : CHOOSE OPTION "));
}
void colecoDSChangeOptions(void) {
  u32 ucHaut=0x00, ucBas=0x00,ucA=0x00,ucY= 9, bOK=0, bBcl;
  
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
  unsigned short dmaVal =  *(bgGetMapPtr(bg0) + 51*32);// ecranBas_map[24][0];
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
  //dmaCopy((void*) ecranBasSel_tiles,bgGetGfxPtr(bg0b),sizeof(ecranBasSel_tiles));
  //dmaCopy((void*) ecranBasSel_map,(void*) bgGetMapPtr(bg1b),32*24*2);
  dmaCopy((void*) ecranBasSelPal,(void*) BG_PALETTE_SUB,256*2);
  dmaVal = *(bgGetMapPtr(bg1b)+24*32); //ecranBasSel_map[24][0];
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
        ucY = (ucY == 9 ? 15 : ucY -2);
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
        ucY = (ucY == 15 ? 9 : ucY +2);
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
          case 9 :
            colecoDSLoadFile();
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
            if (ucGameChoice != -1) { 
                DisplayFileName();
            }
            ucY = 11;
            AffChaine(9,5,0,"=* OPTIONS *=");
            affInfoOptions(ucY);
            break;
          case 11 :
            if (ucGameChoice != -1) { 
              bOK = 1;
            }
            else {    
              while (keysCurrent()  & (KEY_START | KEY_A));
              dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
              AffChaine(5,10,0,(lgeEmul == 0 ? "PAS DE JEU SELECTIONNE" : "   NO GAME SELECTED   ")); 
              AffChaine(5,12,0,(lgeEmul == 0 ? "SVP, UTILISEZ L'OPTION" : "  PLEASE, USE OPTION  ")); 
              AffChaine(5,14,0,(lgeEmul == 0 ? "    CHARGEMENT JEU    " : "      LOAD  GAME      "));
              while (!(keysCurrent()  & (KEY_START | KEY_A)));
              while (keysCurrent()  & (KEY_START | KEY_A));
              dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
              AffChaine(9,5,0,"=* OPTIONS *=");
              affInfoOptions(ucY);
            }
            break;
          case 13 : 
            colecoDSChangeTouches();
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
            AffChaine(9,5,0,"=* OPTIONS *=");
            affInfoOptions(ucY);
            break;
          case 15 :
            lgeEmul = (lgeEmul == 1 ? 0 : 1);
            affInfoOptions(ucY);
            if (ucGameChoice != -1) { 
                DisplayFileName();
            }
            break;
        }
      }
    }
    else
      ucA = 0x00;
    if (keysCurrent()  & KEY_START) {
      if (ucGameChoice != -1) { 
        bOK = 1;
      }
      else {    
        while (keysCurrent()  & (KEY_START | KEY_A));
        dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
        AffChaine(5,10,0,(lgeEmul == 0 ? "PAS DE JEU SELECTIONNE" : "   NO GAME SELECTED   ")); 
        AffChaine(5,12,0,(lgeEmul == 0 ? "SVP, UTILISEZ L'OPTION" : "  PLEASE, USE OPTION  ")); 
        AffChaine(5,14,0,(lgeEmul == 0 ? "    CHARGEMENT JEU    " : "      LOAD  GAME      "));
        while (!(keysCurrent()  & (KEY_START | KEY_A)));
        while (keysCurrent()  & (KEY_START | KEY_A));
        dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
        AffChaine(9,5,0,"=* OPTIONS *=");
        affInfoOptions(ucY);
      }
    }
    affMario();
    swiWaitForVBlank();
  }
  while (keysCurrent()  & (KEY_START | KEY_A));
}

//*****************************************************************************
// Affiche un message sur l'ecran
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

/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by make_crc_table)
 */
unsigned int crc_table[256] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};

/* ========================================================================= */
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */
unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len) {
  if (buf == 0) return 0L;
  crc = crc ^ 0xffffffffL;
  while (len >= 8) {
    DO8(buf);
    len -= 8;
  }
  if (len) do {
    DO1(buf);
  } while (--len);
  return crc ^ 0xffffffffL;
}


/******************************************************************************
* Routine FadeToColor :
*  Fondu du fond vers le noir ou blanc
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

