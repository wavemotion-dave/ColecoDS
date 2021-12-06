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
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>
#include "colecoDS.h"
#include "colecogeneric.h"
#include "ecranBas.h"
#include "ecranBasSel.h"
#include "ecranHaut.h"

// ------------------------------------------------------------------------------
// With only 125 released games plus 25-50 prototypes and 50-ish homebrews,
// this 300 game limit should be more than enough to handle the entire library.
// ------------------------------------------------------------------------------
#define MAX_HS_GAMES    400
#define HS_VERSION      0x0005      // Changing this will wipe high scores on the next install

// --------------------------------------------------------------------------
// We allow sorting on various criteria. By default sorting is high-to-low.
// --------------------------------------------------------------------------
#define HS_OPT_SORTMASK  0x0003
#define HS_OPT_SORTLOW   0x0001
#define HS_OPT_SORTTIME  0x0002
#define HS_OPT_SORTASCII 0x0003

#pragma pack(1)     // Keep things tight...

void highscore_save(void);

// ---------------------------------------------------------
// Each score has this stuff... Initials, score and date.
// ---------------------------------------------------------
struct score_t 
{
    char    initials[4];        // With NULL this is only 3 ascii characters
    char    score[7];           // Six digits of score 
    char    reserved[5];        // For the future...
    u16  year;               // Date score was achieved. We'll auto-fill this from DS time
    u8   month;
    u8   day;
};

// -------------------------------------------------------------------------------------------
// We have up to 10 scores for each game... along with some notes and the sorting options...
// -------------------------------------------------------------------------------------------
struct highscore_t
{
    u32  crc;
    char    notes[21];
    u16  options;
    struct score_t scores[10];
};

// -----------------------------------------------------------------------------------
// We save up to 300 games worth of scores. We also have a spot for default initials 
// so we can re-use the last initials for the last high-score entered. Saves time 
// for most people who are always the ones using their DS system.
// -----------------------------------------------------------------------------------
struct highscore_full_t
{
    u16 version;
    char   last_initials[4];
    struct highscore_t highscore_table[MAX_HS_GAMES];
    u32 checksum;
} highscores;


// -----------------------------------------------------
// A single score entry and high-score line to edit...
// -----------------------------------------------------
struct score_t score_entry;
char hs_line[33];


// ------------------------------------------------------------------------------------
// Run through the entire highscores data and get a checksum. Mostly to make sure
// that it hasn't been tampered with or corrupted on disk.
// ------------------------------------------------------------------------------------
u32 highscore_checksum(void)
{
    char *ptr = (char *)&highscores;
    u32 sum = 0;
    
    for (int i=0; i<(int)sizeof(highscores) - 4; i++)
    {
           sum = *ptr++;
    }
    return sum;
}


// ------------------------------------------------------------------------------
// Read the high score file, if it exists. If it doesn't exist or if the file
// is not the right version and/or is corrupt (crc check), reset to defaults.
// ------------------------------------------------------------------------------
void highscore_init(void) 
{
    bool create_defaults = 0;
    FILE *fp;
    
    strcpy(highscores.last_initials, "   ");
    
    // --------------------------------------------------------------
    // See if the ColecoDS high score file exists... if so, read it!
    // --------------------------------------------------------------
    fp = fopen("/data/ColecoDS.hi", "rb");
    if (fp != NULL)
    {
        fread(&highscores, sizeof(highscores), 1, fp);
        fclose(fp);
        
        // --------------------------------------------
        // If the high score version is wrong or if 
        // the checksum is wrong, reset to defaults
        // --------------------------------------------
        if (highscores.version != HS_VERSION) create_defaults = 1;
        if (highscore_checksum() != highscores.checksum) create_defaults = 1;
    }
    else
    {
        create_defaults = 1;
    }
    
    if (create_defaults)  // Doesn't exist yet or is invalid... create defaults and save it...
    {
        strcpy(highscores.last_initials, "   ");
        
        for (int i=0; i<MAX_HS_GAMES; i++)
        {
            highscores.highscore_table[i].crc = 0x00000000;
            strcpy(highscores.highscore_table[i].notes, "                    ");
            highscores.highscore_table[i].options = 0x0000;
            for (int j=0; j<10; j++)
            {
                strcpy(highscores.highscore_table[i].scores[j].score, "000000");
                strcpy(highscores.highscore_table[i].scores[j].initials, "   ");
                strcpy(highscores.highscore_table[i].scores[j].reserved, "     ");                
                highscores.highscore_table[i].scores[j].year = 0;
                highscores.highscore_table[i].scores[j].month = 0;
                highscores.highscore_table[i].scores[j].day = 0;
            }
        }
        highscore_save();
    }
}


// ------------------------------------------------------------------------------------
// Save the high score file to disc. This gets saved in the /data directory and this
// directory is created if it doesn't exist (mostly likely does if using TWL++)
// ------------------------------------------------------------------------------------
void highscore_save(void) 
{
    FILE *fp;

    DIR* dir = opendir("/data");
    if (dir)
    {
        closedir(dir);  // Directory exists... close it out and move on.
    }
    else
    {
        mkdir("/data", 0777);   // Otherwise create the directory...
    }
    
    // --------------------------------------------------------
    // Set our current highscore file version and checksum...
    // --------------------------------------------------------
    highscores.version = HS_VERSION;
    highscores.checksum = highscore_checksum();

    // -------------------------------------------------------
    // Open file in binary mode... overwrite if it exists...
    // -------------------------------------------------------
    fp = fopen("/data/ColecoDS.hi", "wb+");
    if (fp != NULL)
    {
        // -----------------------------------------
        // And write the whole shebang!
        // -----------------------------------------
        fwrite(&highscores, sizeof(highscores), 1, fp);
        fclose(fp);
    }
}


// ------------------------------------------------------------------------
// We provide 4 different sorting options... show them for the user...
// Note: the default is high-to-low which does to show clarification text.
// ------------------------------------------------------------------------
void highscore_showoptions(u16 options)
{
    if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTLOW)
    {
        dsPrintValue(22,5,0, (char*)"[LOWSC]");
    }
    else if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTTIME)
    {
        dsPrintValue(22,5,0, (char*)"[TIME] ");
    }
    else if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
    {
        dsPrintValue(22,5,0, (char*)"[ALPHA]");
    }
    else
    {   
        dsPrintValue(22,5,0, (char*)"       ");    
    }
}

// -----------------------------------------------------
// Show the 10 scores for this game... 
// -----------------------------------------------------
void show_scores(short foundIdx, bool bShowLegend)
{
    dsPrintValue(3,5,0, (char*)highscores.highscore_table[foundIdx].notes);
    for (int i=0; i<10; i++)
    {
        if ((highscores.highscore_table[foundIdx].options & HS_OPT_SORTMASK) == HS_OPT_SORTTIME)
        {
            siprintf(hs_line, "%04d-%02d-%02d   %-3s   %c%c:%c%c.%c%c", highscores.highscore_table[foundIdx].scores[i].year, highscores.highscore_table[foundIdx].scores[i].month,highscores.highscore_table[foundIdx].scores[i].day, 
                                                             highscores.highscore_table[foundIdx].scores[i].initials, highscores.highscore_table[foundIdx].scores[i].score[0], highscores.highscore_table[foundIdx].scores[i].score[1],
                                                             highscores.highscore_table[foundIdx].scores[i].score[2], highscores.highscore_table[foundIdx].scores[i].score[3], highscores.highscore_table[foundIdx].scores[i].score[4],
                                                             highscores.highscore_table[foundIdx].scores[i].score[5]);
        }
        else
        {
            siprintf(hs_line, "%04d-%02d-%02d   %-3s   %-6s  ", highscores.highscore_table[foundIdx].scores[i].year, highscores.highscore_table[foundIdx].scores[i].month,highscores.highscore_table[foundIdx].scores[i].day, 
                                                               highscores.highscore_table[foundIdx].scores[i].initials, highscores.highscore_table[foundIdx].scores[i].score);
        }
        dsPrintValue(3,6+i, 0, hs_line);
    }
    
    if (bShowLegend)
    {
        dsPrintValue(2,16,0, (char*)"                             ");
        dsPrintValue(2,18,0, (char*)"PRESS X FOR NEW HI SCORE     ");
        dsPrintValue(2,19,0, (char*)"PRESS Y FOR NOTES/OPTIONS    ");
        dsPrintValue(2,20,0, (char*)"PRESS B TO EXIT              ");
        dsPrintValue(2,21,0, (char*)"SCORES AUTO SORT AFTER ENTRY ");
    }
    highscore_showoptions(highscores.highscore_table[foundIdx].options);
}

// -------------------------------------------------------------------------------
// We need to sort the scores according to the sorting options. We are using a
// very simple bubblesort which is very slow but with only 10 scores, this is
// still blazingly fast on the NDS.
// -------------------------------------------------------------------------------
char cmp1[21];
char cmp2[21];
void highscore_sort(short foundIdx)
{
    // Bubblesort!!
    for (int i=0; i<9; i++)
    {
        for (int j=0; j<9; j++)
        {
            if (((highscores.highscore_table[foundIdx].options & HS_OPT_SORTMASK) == HS_OPT_SORTLOW) || ((highscores.highscore_table[foundIdx].options & HS_OPT_SORTMASK) == HS_OPT_SORTTIME))
            {
                if (strcmp(highscores.highscore_table[foundIdx].scores[j+1].score, "000000") == 0)
                     strcpy(cmp1, "999999");
                else 
                    strcpy(cmp1, highscores.highscore_table[foundIdx].scores[j+1].score);
                if (strcmp(highscores.highscore_table[foundIdx].scores[j].score, "000000") == 0)
                     strcpy(cmp2, "999999");
                else 
                    strcpy(cmp2, highscores.highscore_table[foundIdx].scores[j].score);
                if (strcmp(cmp1, cmp2) < 0)
                {
                    // Swap...
                    memcpy(&score_entry, &highscores.highscore_table[foundIdx].scores[j], sizeof(score_entry));
                    memcpy(&highscores.highscore_table[foundIdx].scores[j], &highscores.highscore_table[foundIdx].scores[j+1], sizeof(score_entry));
                    memcpy(&highscores.highscore_table[foundIdx].scores[j+1], &score_entry, sizeof(score_entry));
                }
            }
            else if ((highscores.highscore_table[foundIdx].options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
            {
                if (strcmp(highscores.highscore_table[foundIdx].scores[j+1].score, "000000") == 0)
                     strcpy(cmp1, "------");
                else 
                    strcpy(cmp1, highscores.highscore_table[foundIdx].scores[j+1].score);
                if (strcmp(highscores.highscore_table[foundIdx].scores[j].score, "000000") == 0)
                     strcpy(cmp2, "------");
                else 
                    strcpy(cmp2, highscores.highscore_table[foundIdx].scores[j].score);
                
                if (strcmp(cmp1, cmp2) > 0)
                {
                    // Swap...
                    memcpy(&score_entry, &highscores.highscore_table[foundIdx].scores[j], sizeof(score_entry));
                    memcpy(&highscores.highscore_table[foundIdx].scores[j], &highscores.highscore_table[foundIdx].scores[j+1], sizeof(score_entry));
                    memcpy(&highscores.highscore_table[foundIdx].scores[j+1], &score_entry, sizeof(score_entry));
                }
            }
            else
            {
                if (strcmp(highscores.highscore_table[foundIdx].scores[j+1].score, highscores.highscore_table[foundIdx].scores[j].score) > 0)
                {
                    // Swap...
                    memcpy(&score_entry, &highscores.highscore_table[foundIdx].scores[j], sizeof(score_entry));
                    memcpy(&highscores.highscore_table[foundIdx].scores[j], &highscores.highscore_table[foundIdx].scores[j+1], sizeof(score_entry));
                    memcpy(&highscores.highscore_table[foundIdx].scores[j+1], &score_entry, sizeof(score_entry));
                }
            }
        }
    }    
}


// -------------------------------------------------------------------------
// Let the user enter a new highscore. We look for up/down and other entry
// keys and show the new score on the screen. This is old-school up/down to
// "dial-in" the score by moving from digit to digit. Much like the Arcade.
// -------------------------------------------------------------------------
void highscore_entry(short foundIdx, u32 crc)
{
    char bEntryDone = 0;
    char blink=0;
    unsigned short entry_idx=0;
    char dampen=0;
    time_t unixTime = time(NULL);
    struct tm* timeStruct = gmtime((const time_t *)&unixTime);
    
    dsPrintValue(2,19,0, (char*)"UP/DN/LEFT/RIGHT ENTER SCORE");
    dsPrintValue(2,20,0, (char*)"PRESS START TO SAVE SCORE   ");
    dsPrintValue(2,21,0, (char*)"PRESS SELECT TO CANCEL      ");
    dsPrintValue(2,22,0, (char*)"                            ");

    strcpy(score_entry.score, "000000");
    strcpy(score_entry.initials, highscores.last_initials);
    score_entry.year  = timeStruct->tm_year +1900;
    score_entry.month = timeStruct->tm_mon+1;
    score_entry.day   = timeStruct->tm_mday;
    while (!bEntryDone)
    {
        swiWaitForVBlank();
        if (keysCurrent() & KEY_SELECT) {bEntryDone=1;}

        if (keysCurrent() & KEY_START) 
        {
            strcpy(highscores.last_initials, score_entry.initials);
            memcpy(&highscores.highscore_table[foundIdx].scores[9], &score_entry, sizeof(score_entry));
            highscores.highscore_table[foundIdx].crc = crc;
            highscore_sort(foundIdx);
            highscore_save();
            bEntryDone=1;
        }

        if (dampen == 0)
        {
            if ((keysCurrent() & KEY_RIGHT) || (keysCurrent() & KEY_A))
            {
                if (entry_idx < 8) entry_idx++; 
                blink=25;
                dampen=15;
            }

            if (keysCurrent() & KEY_LEFT)  
            {
                if (entry_idx > 0) entry_idx--; 
                blink=25;
                dampen=15;
            }

            if (keysCurrent() & KEY_UP)
            {
                if (entry_idx < 3) // This is the initials
                {
                    if (score_entry.initials[entry_idx] == ' ')
                        score_entry.initials[entry_idx] = 'A';
                    else if (score_entry.initials[entry_idx] == 'Z')
                        score_entry.initials[entry_idx] = ' ';
                    else score_entry.initials[entry_idx]++;
                }
                else    // This is the score...
                {
                    if ((highscores.highscore_table[foundIdx].options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
                    {
                        if (score_entry.score[entry_idx-3] == ' ')
                            score_entry.score[entry_idx-3] = 'A';
                        else if (score_entry.score[entry_idx-3] == 'Z')
                            score_entry.score[entry_idx-3] = '0';
                        else if (score_entry.score[entry_idx-3] == '9')
                            score_entry.score[entry_idx-3] = ' ';
                        else score_entry.score[entry_idx-3]++;
                    }
                    else
                    {
                        score_entry.score[entry_idx-3]++;
                        if (score_entry.score[entry_idx-3] > '9') score_entry.score[entry_idx-3] = '0';
                    }
                }
                blink=0;
                dampen=10;
            }

            if (keysCurrent() & KEY_DOWN)
            {
                if (entry_idx < 3) // // This is the initials
                {
                    if (score_entry.initials[entry_idx] == ' ')
                        score_entry.initials[entry_idx] = 'Z';
                    else if (score_entry.initials[entry_idx] == 'A')
                        score_entry.initials[entry_idx] = ' ';
                    else score_entry.initials[entry_idx]--;
                }
                else   // This is the score...
                {
                    if ((highscores.highscore_table[foundIdx].options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
                    {
                        if (score_entry.score[entry_idx-3] == ' ')
                            score_entry.score[entry_idx-3] = '9';
                        else if (score_entry.score[entry_idx-3] == '0')
                            score_entry.score[entry_idx-3] = 'Z';
                        else if (score_entry.score[entry_idx-3] == 'A')
                            score_entry.score[entry_idx-3] = ' ';
                        else score_entry.score[entry_idx-3]--;
                    }
                    else
                    {
                        score_entry.score[entry_idx-3]--;
                        if (score_entry.score[entry_idx-3] < '0') score_entry.score[entry_idx-3] = '9';
                    }
                }
                blink=0;
                dampen=10;
            }
        }
        else
        {
            dampen--;
        }

        siprintf(hs_line, "%04d-%02d-%02d   %-3s   %-6s", score_entry.year, score_entry.month, score_entry.day, score_entry.initials, score_entry.score);
        if ((++blink % 60) > 30)
        {
            if (entry_idx < 3)
                hs_line[13+entry_idx] = '_';
            else
                hs_line[16+entry_idx] = '_';
        }
        dsPrintValue(3,16, 0, (char*)hs_line);
    }
    
    show_scores(foundIdx, true);
}

// ----------------------------------------------------------------
// Let the user enter options and notes for the current game...
// ----------------------------------------------------------------
void highscore_options(short foundIdx, u32 crc)
{
    u16 options = 0x0000;
    char notes[21];
    char bEntryDone = 0;
    char blink=0;
    unsigned short entry_idx=0;
    char dampen=0;
    
    dsPrintValue(3,16,0, (char*)"NOTE: ");
    dsPrintValue(3,19,0, (char*)"UP/DN/LEFT/RIGHT ENTER NOTES");
    dsPrintValue(3,20,0, (char*)"X=TOGGLE SORT, L+R=CLR SCORE");
    dsPrintValue(3,21,0, (char*)"PRESS START TO SAVE OPTIONS ");
    dsPrintValue(3,22,0, (char*)"PRESS SELECT TO CANCEL      ");

    strcpy(notes, highscores.highscore_table[foundIdx].notes);
    options = highscores.highscore_table[foundIdx].options;
    
    while (!bEntryDone)
    {
        swiWaitForVBlank();
        if (keysCurrent() & KEY_SELECT) {bEntryDone=1;}

        if (keysCurrent() & KEY_START) 
        {
            strcpy(highscores.highscore_table[foundIdx].notes, notes);
            highscores.highscore_table[foundIdx].options = options;
            highscores.highscore_table[foundIdx].crc = crc;
            highscore_sort(foundIdx);
            highscore_save();
            bEntryDone=1;
        }

        if (dampen == 0)
        {
            if ((keysCurrent() & KEY_RIGHT) || (keysCurrent() & KEY_A))
            {
                if (entry_idx < 19) entry_idx++; 
                blink=25;
                dampen=15;
            }

            if (keysCurrent() & KEY_LEFT)  
            {
                if (entry_idx > 0) entry_idx--; 
                blink=25;
                dampen=15;
            }

            if (keysCurrent() & KEY_UP)
            {
                if (notes[entry_idx] == ' ')
                    notes[entry_idx] = 'A';
                else if (notes[entry_idx] == 'Z')
                    notes[entry_idx] = '0';
                else if (notes[entry_idx] == '9')
                    notes[entry_idx] = ' ';
                else notes[entry_idx]++;
                blink=0;
                dampen=10;
            }

            if (keysCurrent() & KEY_DOWN)
            {
                if (notes[entry_idx] == ' ')
                    notes[entry_idx] = '9';
                else if (notes[entry_idx] == '0')
                    notes[entry_idx] = 'Z';
                else if (notes[entry_idx] == 'A')
                    notes[entry_idx] = ' ';
                else notes[entry_idx]--;
                blink=0;
                dampen=10;
            }

            if (keysCurrent() & KEY_X)  
            {
                if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTLOW)
                {
                    options &= (u16)~HS_OPT_SORTMASK;
                    options |= HS_OPT_SORTTIME;
                }
                else if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTTIME)
                {
                    options &= (u16)~HS_OPT_SORTMASK;
                    options |= HS_OPT_SORTASCII;
                }
                else if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
                {
                    options &= (u16)~HS_OPT_SORTMASK;
                }
                else
                {
                    options |= (u16)HS_OPT_SORTLOW;
                }
                highscore_showoptions(options);
                dampen=15;
            }
            
            // Clear the entire game of scores... 
            if ((keysCurrent() & KEY_L) && (keysCurrent() & KEY_R))
            {
                highscores.highscore_table[foundIdx].crc = 0x00000000;
                highscores.highscore_table[foundIdx].options = 0x0000;
                strcpy(highscores.highscore_table[foundIdx].notes, "                    ");
                strcpy(notes, "                    ");                
                for (int j=0; j<10; j++)
                {
                    strcpy(highscores.highscore_table[foundIdx].scores[j].score, "000000");
                    strcpy(highscores.highscore_table[foundIdx].scores[j].initials, "   ");
                    strcpy(highscores.highscore_table[foundIdx].scores[j].reserved, "    ");
                    highscores.highscore_table[foundIdx].scores[j].year = 0;
                    highscores.highscore_table[foundIdx].scores[j].month = 0;
                    highscores.highscore_table[foundIdx].scores[j].day = 0;
                }
                show_scores(foundIdx, false);
                highscore_save();                    
            }            
        }
        else
        {
            dampen--;
        }

        siprintf(hs_line, "%-20s", notes);
        if ((++blink % 60) > 30)
        {
            hs_line[entry_idx] = '_';
        }
        dsPrintValue(9,16, 0, (char*)hs_line);
    }
    
    show_scores(foundIdx, true);
}

// ------------------------------------------------------------------------
// Entry point for the high score table. We are passed in the crc of the 
// current game. We use the crc to check the high score database and see
// if there is already saved highscore data for this game.  At the point
// where this is called, the high score init has already been called.
// ------------------------------------------------------------------------
void highscore_display(u32 crc) 
{
    short foundIdx = -1;
    short firstBlank = -1;
    char bDone = 0;

      dmaCopy((void*) bgGetMapPtr(bg0b)+30*32*2,(void*) bgGetMapPtr(bg0b),32*24*2);
      unsigned short dmaVal = *(bgGetMapPtr(bg0b)+24*32); //ecranBas_map[24][0];
      dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);

    swiWaitForVBlank();

    // ---------------------------------------------------------------------------------
    // Check if the current CRC32 is in our High Score database...
    // ---------------------------------------------------------------------------------
    for (int i=0; i<MAX_HS_GAMES; i++)
    {
        if (firstBlank == -1)
        {
            if ((highscores.highscore_table[i].crc) == 0)
            {
                firstBlank = i;
            }
        }

        if (highscores.highscore_table[i].crc == crc)
        {
            foundIdx = i;
            break;
        }
    }
    
    if (foundIdx == -1)
    {
        foundIdx = firstBlank;   
    }
    
    show_scores(foundIdx, true);

    while (!bDone)
    {
        if (keysCurrent() & KEY_A) bDone=1;
        if (keysCurrent() & KEY_B) bDone=1;
        if (keysCurrent() & KEY_X) highscore_entry(foundIdx, crc);
        if (keysCurrent() & KEY_Y) highscore_options(foundIdx, crc);
    }
    
    InitBottomScreen();
}

// End of file
