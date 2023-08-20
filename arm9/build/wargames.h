
//{{BLOCK(wargames)

//======================================================================
//
//	wargames, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 772 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 19124 + 1844 = 21480
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_WARGAMES_H
#define GRIT_WARGAMES_H

#define wargamesTilesLen 19124
extern const unsigned int wargamesTiles[4781];

#define wargamesMapLen 1844
extern const unsigned short wargamesMap[922];

#define wargamesPalLen 512
extern const unsigned short wargamesPal[256];

#endif // GRIT_WARGAMES_H

//}}BLOCK(wargames)
