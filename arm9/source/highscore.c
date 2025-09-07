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
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>
#include "colecoDS.h"
#include "colecogeneric.h"
#include "printf.h"

extern u8 *ROM_Memory;

// ------------------------------------------------------------------------------------
// We need to put a practical limit on the size of the high scores... 1475 games it is!
// ------------------------------------------------------------------------------------
#define MAX_HS_GAMES    1475        // Fits just barely into 256K which is all we want to use
#define HS_VERSION      0x0009      // Changing this will wipe high scores on the next install

// --------------------------------------------------------------------------
// We allow sorting on various criteria. By default sorting is high-to-low.
// --------------------------------------------------------------------------
#define HS_OPT_SORTMASK  0x03
#define HS_OPT_SORTLOW   0x01
#define HS_OPT_SORTTIME  0x02
#define HS_OPT_SORTASCII 0x03

#pragma pack(1)     // Keep things tight...

#define HS_FILE     "/data/ColecoDS.hi"

void highscore_save(void);

// ---------------------------------------------------------
// Each score has this stuff... Initials, score and date.
// ---------------------------------------------------------
struct score_t
{
    char    initials[4];        // With NULL this is only 3 ascii characters
    char    score[7];           // Six digits of score
    char    reserved;           // For the future...
    u16     year;               // Date score was achieved. We'll auto-fill this from DS time
    u8      month;
    u8      day;
};


// -----------------------------------------------------------------------------------
// The header has the version number and default 'initials' so we can re-use the
// last initials for the last high-score entered. Saves time for most people who 
// are always the ones using their DS system.
// -----------------------------------------------------------------------------------
struct highscore_header_t
{
    u16    version;
    char   last_initials[4];
    u32    checksum;
    u16    dirty_flag;
} highscore_header;

// -------------------------------------------------------------------------------------------
// We have up to 10 scores for each game... along with some notes and the sorting options...
// -------------------------------------------------------------------------------------------
struct highscore_t
{
    u32  crc;
    char notes[11];
    u8   options;
    struct score_t scores[10];
} highscore;

u32 hs_file_offset = 0; // So we know where to write-back the highscore entry

// -----------------------------------------------------
// A single score entry and high-score line to edit...
// -----------------------------------------------------
struct score_t score_entry;
char hs_line[33];

// ------------------------------------------------------------------------------
// Read the high score file, if it exists. If it doesn't exist or if the file
// is not the right version and/or is corrupt (crc check), reset to defaults.
// ------------------------------------------------------------------------------
void highscore_init(void)
{
    u8 create_defaults = 0;

    strcpy(highscore_header.last_initials, "   ");

    // --------------------------------------------------------------
    // See if the ColecoDS high score file exists... if so, read it!
    // --------------------------------------------------------------
    FILE *fp = fopen(HS_FILE, "rb");
    
    if (fp) // Make sure the file exists - otherwise we create it...
    {
        fread(&highscore_header, sizeof(highscore_header), 1, fp);
        
        // -----------------------------------------------------------
        // For version 8, we will auto-update to the current version.
        // We have expanded the database to have more highscore slots
        // and we are now only reading one record at a time to save 
        // as it was a bit outrageous to read in all highscore slots
        // when only one is used (saves almost 125K of RAM).
        // -----------------------------------------------------------
        if (highscore_header.version == 0x0008)
        {
            // Old file entries started at offset 6 just past the header
            fseek(fp, 6, SEEK_SET);
            fread(ROM_Memory, sizeof(highscore), 744, fp);
            fclose(fp);

            highscore_header.version = HS_VERSION;
            highscore_header.dirty_flag = 0;
            
            FILE *fp = fopen(HS_FILE, "wb");
            fwrite(&highscore_header, sizeof(highscore_header), 1, fp);
            for (int i=0; i<MAX_HS_GAMES; i++)
            {
                if (i < 744)
                {
                    memcpy(&highscore, ROM_Memory + (i * sizeof(highscore)), sizeof(highscore));
                }
                fwrite(&highscore, sizeof(highscore), 1, fp);
            }            
        }

        // -------------------------------------------------------
        // If the high score version is wrong, reset to defaults.
        // -------------------------------------------------------
        if (highscore_header.version != HS_VERSION)
        {
            create_defaults = 1;
        }
        
        fclose(fp);  // Close the highscore file - if we need to rewrite it, we do so below.
    }
    else
    {
        create_defaults = 1;
    }

    if (create_defaults)  // Doesn't exist yet or is invalid... create defaults and save it...
    {
        DIR* dir = opendir("/data");
        if (dir)
        {
            closedir(dir);  // Directory exists... close it out and move on.
        }
        else
        {
            mkdir("/data", 0777);   // Otherwise create the directory...
        }
        
        strcpy(highscore_header.last_initials, "   ");
        highscore_header.version = HS_VERSION;
        highscore_header.dirty_flag = 0;
        highscore_header.checksum = 0x00000000;
                
        highscore.crc = 0x00000000;
        highscore.options = 0x0000;
        strcpy(highscore.notes, "          ");
        for (int j=0; j<10; j++)
        {
                strcpy(highscore.scores[j].score, "000000");
                strcpy(highscore.scores[j].initials, "   ");
                highscore.scores[j].reserved = 0;
                highscore.scores[j].year = 0;
                highscore.scores[j].month = 0;
                highscore.scores[j].day = 0;
        }

        FILE *fp = fopen(HS_FILE, "wb");
        
        fwrite(&highscore_header, sizeof(highscore_header), 1, fp);
        
        for (int i=0; i<MAX_HS_GAMES; i++)
        {
            fwrite(&highscore, sizeof(highscore), 1, fp);
        }
        
        fclose(fp);        
    }
}


// ------------------------------------------------------------------------------------
// Save the high score file to the SD card. This gets saved in the /data directory and
// this directory is created if it doesn't exist (mostly likely does if using TWL++)
// ------------------------------------------------------------------------------------
void highscore_save(void)
{
    FILE *fp = fopen(HS_FILE, "rb+");   // Open for read/write
    if (highscore_header.dirty_flag)
    {
        highscore_header.dirty_flag = 0;
        fwrite(&highscore_header, sizeof(highscore_header), 1, fp);
    }
    fseek(fp, hs_file_offset, SEEK_SET);
    fwrite(&highscore, sizeof(highscore), 1, fp);
    fclose(fp);
}


// ------------------------------------------------------------------------
// We provide 4 different sorting options... show them for the user...
// Note: the default is high-to-low which does not show clarification text.
// ------------------------------------------------------------------------
void highscore_showoptions(u16 options)
{
    if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTLOW)
    {
        DSPrint(22,5,0, (char*)"[LOWSC]");
    }
    else if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTTIME)
    {
        DSPrint(22,5,0, (char*)"[TIME] ");
    }
    else if ((options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
    {
        DSPrint(22,5,0, (char*)"[ALPHA]");
    }
    else
    {
        DSPrint(22,5,0, (char*)"       ");
    }
}

// -----------------------------------------------------
// Show the 10 scores for this game...
// -----------------------------------------------------
void show_scores(bool bShowLegend)
{
    DSPrint(3,5,0, (char*)highscore.notes);
    for (int i=0; i<10; i++)
    {
        if ((highscore.options & HS_OPT_SORTMASK) == HS_OPT_SORTTIME)
        {
            sprintf(hs_line, "%04d-%02d-%02d   %-3s   %c%c:%c%c.%c%c", highscore.scores[i].year, highscore.scores[i].month,highscore.scores[i].day,
                                                             highscore.scores[i].initials, highscore.scores[i].score[0], highscore.scores[i].score[1],
                                                             highscore.scores[i].score[2], highscore.scores[i].score[3], highscore.scores[i].score[4],
                                                             highscore.scores[i].score[5]);
        }
        else
        {
            sprintf(hs_line, "%04d-%02d-%02d   %-3s   %-6s  ", highscore.scores[i].year, highscore.scores[i].month,highscore.scores[i].day,
                                                               highscore.scores[i].initials, highscore.scores[i].score);
        }
        DSPrint(3,6+i, 0, hs_line);
    }

    if (bShowLegend)
    {
        DSPrint(1,16,0, (char*)"                              ");
        DSPrint(1,18,0, (char*)" PRESS X FOR NEW HI SCORE     ");
        DSPrint(1,19,0, (char*)" PRESS Y FOR NOTES/OPTIONS    ");
        DSPrint(1,20,0, (char*)" PRESS B TO EXIT              ");
        DSPrint(1,21,0, (char*)" SCORES AUTO SORT AFTER ENTRY ");
    }
    highscore_showoptions(highscore.options);
}

// -------------------------------------------------------------------------------
// We need to sort the scores according to the sorting options. We are using a
// very simple bubblesort which is very slow but with only 10 scores, this is
// still blazingly fast on the NDS.
// -------------------------------------------------------------------------------
char cmp1[21];
char cmp2[21];
void highscore_sort(void)
{
    // Bubblesort!! There are only 10 entries here, so this is plenty fast.
    for (int i=0; i<9; i++)
    {
        for (int j=0; j<9; j++)
        {
            if (((highscore.options & HS_OPT_SORTMASK) == HS_OPT_SORTLOW) || ((highscore.options & HS_OPT_SORTMASK) == HS_OPT_SORTTIME))
            {
                if (strcmp(highscore.scores[j+1].score, "000000") == 0)
                     strcpy(cmp1, "999999");
                else
                    strcpy(cmp1, highscore.scores[j+1].score);
                if (strcmp(highscore.scores[j].score, "000000") == 0)
                     strcpy(cmp2, "999999");
                else
                    strcpy(cmp2, highscore.scores[j].score);
                if (strcmp(cmp1, cmp2) < 0)
                {
                    // Swap...
                    memcpy(&score_entry, &highscore.scores[j], sizeof(score_entry));
                    memcpy(&highscore.scores[j], &highscore.scores[j+1], sizeof(score_entry));
                    memcpy(&highscore.scores[j+1], &score_entry, sizeof(score_entry));
                }
            }
            else if ((highscore.options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
            {
                if (strcmp(highscore.scores[j+1].score, "000000") == 0)
                     strcpy(cmp1, "------");
                else
                    strcpy(cmp1, highscore.scores[j+1].score);
                if (strcmp(highscore.scores[j].score, "000000") == 0)
                     strcpy(cmp2, "------");
                else
                    strcpy(cmp2, highscore.scores[j].score);

                if (strcmp(cmp1, cmp2) > 0)
                {
                    // Swap...
                    memcpy(&score_entry, &highscore.scores[j], sizeof(score_entry));
                    memcpy(&highscore.scores[j], &highscore.scores[j+1], sizeof(score_entry));
                    memcpy(&highscore.scores[j+1], &score_entry, sizeof(score_entry));
                }
            }
            else
            {
                if (strcmp(highscore.scores[j+1].score, highscore.scores[j].score) > 0)
                {
                    // Swap...
                    memcpy(&score_entry, &highscore.scores[j], sizeof(score_entry));
                    memcpy(&highscore.scores[j], &highscore.scores[j+1], sizeof(score_entry));
                    memcpy(&highscore.scores[j+1], &score_entry, sizeof(score_entry));
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
void highscore_entry(u32 crc)
{
    char bEntryDone = 0;
    char blink=0;
    unsigned short entry_idx=0;
    char dampen=0;
    time_t unixTime = time(NULL);
    struct tm* timeStruct = gmtime((const time_t *)&unixTime);

    DSPrint(2,19,0, (char*)"UP/DN/LEFT/RIGHT ENTER SCORE");
    DSPrint(2,20,0, (char*)"PRESS START TO SAVE SCORE   ");
    DSPrint(2,21,0, (char*)"PRESS SELECT TO CANCEL      ");
    DSPrint(2,22,0, (char*)"                            ");

    strcpy(score_entry.score, "000000");
    strcpy(score_entry.initials, highscore_header.last_initials);
    score_entry.year  = timeStruct->tm_year +1900;
    score_entry.month = timeStruct->tm_mon+1;
    score_entry.day   = timeStruct->tm_mday;
    while (!bEntryDone)
    {
        swiWaitForVBlank();
        if (keysCurrent() & KEY_SELECT) {bEntryDone=1;}

        if (keysCurrent() & KEY_START)
        {
            // If last initials changed... force it to write on next save
            if (strcmp(highscore_header.last_initials, score_entry.initials) != 0)
            {
                strcpy(highscore_header.last_initials, score_entry.initials);
                highscore_header.dirty_flag = 1;
            }
            memcpy(&highscore.scores[9], &score_entry, sizeof(score_entry));
            highscore.crc = crc;
            highscore_sort();
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
                    if ((highscore.options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
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
                    if ((highscore.options & HS_OPT_SORTMASK) == HS_OPT_SORTASCII)
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

        sprintf(hs_line, "%04d-%02d-%02d   %-3s   %-6s", score_entry.year, score_entry.month, score_entry.day, score_entry.initials, score_entry.score);
        if ((++blink % 60) > 30)
        {
            if (entry_idx < 3)
                hs_line[13+entry_idx] = '_';
            else
                hs_line[16+entry_idx] = '_';
        }
        DSPrint(3,16, 0, (char*)hs_line);
    }

    show_scores(true);
}

// ----------------------------------------------------------------
// Let the user enter options and notes for the current game...
// ----------------------------------------------------------------
void highscore_options(u32 crc)
{
    u16 options = 0x0000;
    static char notes[11];
    char bEntryDone = 0;
    char blink=0;
    unsigned short entry_idx=0;
    char dampen=0;

    DSPrint(2,16,0, (char*)" NOTE: ");
    DSPrint(2,19,0, (char*)" UP/DN/LEFT/RIGHT ENTER NOTES");
    DSPrint(2,20,0, (char*)" X=TOGGLE SORT, L+R=CLR SCORE");
    DSPrint(2,21,0, (char*)" PRESS START TO SAVE OPTIONS ");
    DSPrint(2,22,0, (char*)" PRESS SELECT TO CANCEL      ");

    strcpy(notes, highscore.notes);
    options = highscore.options;

    while (!bEntryDone)
    {
        swiWaitForVBlank();
        if (keysCurrent() & KEY_SELECT) {bEntryDone=1;}

        if (keysCurrent() & KEY_START)
        {
            strcpy(highscore.notes, notes);
            highscore.options = options;
            highscore.crc = crc;
            highscore_sort();
            highscore_save();
            bEntryDone=1;
        }

        if (dampen == 0)
        {
            if ((keysCurrent() & KEY_RIGHT) || (keysCurrent() & KEY_A))
            {
                if (entry_idx < 9) entry_idx++;
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
                highscore.crc = 0x00000000;
                highscore.options = 0x0000;
                strcpy(highscore.notes, "          ");
                strcpy(notes, "          ");
                for (int j=0; j<10; j++)
                {
                    strcpy(highscore.scores[j].score, "000000");
                    strcpy(highscore.scores[j].initials, "   ");
                    highscore.scores[j].reserved = 0;
                    highscore.scores[j].year = 0;
                    highscore.scores[j].month = 0;
                    highscore.scores[j].day = 0;
                }
                show_scores(false);
                highscore_save();
            }
        }
        else
        {
            dampen--;
        }

        sprintf(hs_line, "%-10s", notes);
        if ((++blink % 60) > 30)
        {
            hs_line[entry_idx] = '_';
        }
        DSPrint(9,16, 0, (char*)hs_line);
    }

    show_scores(true);
}

// ------------------------------------------------------------------------
// Entry point for the high score table. We are passed in the crc of the
// current game. We use the crc to check the high score database and see
// if there is already saved highscore data for this game.  At the point
// where this is called, the high score init has already been called.
// ------------------------------------------------------------------------
void highscore_display(u32 crc)
{
    char bDone = 0;

    // ---------------------------------------------
    // Setup lower screen for High Score dispay...
    // ---------------------------------------------
    BottomScreenOptions();

    // ---------------------------------------------------------------------------------
    // Check if the current CRC32 is in our High Score database...
    // ---------------------------------------------------------------------------------
    FILE *fp = fopen(HS_FILE, "rb");
    
    fread(&highscore_header, sizeof(highscore_header), 1, fp);
    hs_file_offset = sizeof(highscore_header);
    
    for (int i=0; i<MAX_HS_GAMES; i++)
    {
        fread(&highscore, sizeof(highscore), 1, fp);
        if (highscore.crc == crc)
        {
            break;  // Found the game CRC - use this slot
        }
        if (highscore.crc == 0x00000000)
        {
            break;  // First blank entry can be used
        }
        
        hs_file_offset += sizeof(highscore); // Tells us where to write this entry back to the high score file
    }
    fclose(fp);

    show_scores(true);

    while (!bDone)
    {
        if (keysCurrent() & KEY_A) bDone=1;
        if (keysCurrent() & KEY_B) bDone=1;
        if (keysCurrent() & KEY_X) highscore_entry(crc);
        if (keysCurrent() & KEY_Y) highscore_options(crc);
    }

    BottomScreenKeypad();
}

// End of file
