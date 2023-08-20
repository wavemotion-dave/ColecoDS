
//{{BLOCK(topscreen)

//======================================================================
//
//	topscreen, 256x512@8, 
//	+ palette 256 entries, not compressed
//	+ 910 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x64 
//	Total size: 512 + 22552 + 2444 = 25508
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_TOPSCREEN_H
#define GRIT_TOPSCREEN_H

#define topscreenTilesLen 22552
extern const unsigned int topscreenTiles[5638];

#define topscreenMapLen 2444
extern const unsigned short topscreenMap[1222];

#define topscreenPalLen 512
extern const unsigned short topscreenPal[256];

#endif // GRIT_TOPSCREEN_H

//}}BLOCK(topscreen)
