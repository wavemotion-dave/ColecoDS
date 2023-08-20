
//{{BLOCK(colecovision)

//======================================================================
//
//	colecovision, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 778 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 19528 + 1872 = 21912
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_COLECOVISION_H
#define GRIT_COLECOVISION_H

#define colecovisionTilesLen 19528
extern const unsigned int colecovisionTiles[4882];

#define colecovisionMapLen 1872
extern const unsigned short colecovisionMap[936];

#define colecovisionPalLen 512
extern const unsigned short colecovisionPal[256];

#endif // GRIT_COLECOVISION_H

//}}BLOCK(colecovision)
