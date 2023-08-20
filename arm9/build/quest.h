
//{{BLOCK(quest)

//======================================================================
//
//	quest, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 764 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 18972 + 1844 = 21328
//
//	Time-stamp: 2023-08-20, 06:54:14
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_QUEST_H
#define GRIT_QUEST_H

#define questTilesLen 18972
extern const unsigned int questTiles[4743];

#define questMapLen 1844
extern const unsigned short questMap[922];

#define questPalLen 512
extern const unsigned short questPal[256];

#endif // GRIT_QUEST_H

//}}BLOCK(quest)
