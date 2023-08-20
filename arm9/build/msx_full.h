
//{{BLOCK(msx_full)

//======================================================================
//
//	msx_full, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 599 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 8640 + 1720 = 10872
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_MSX_FULL_H
#define GRIT_MSX_FULL_H

#define msx_fullTilesLen 8640
extern const unsigned int msx_fullTiles[2160];

#define msx_fullMapLen 1720
extern const unsigned short msx_fullMap[860];

#define msx_fullPalLen 512
extern const unsigned short msx_fullPal[256];

#endif // GRIT_MSX_FULL_H

//}}BLOCK(msx_full)
