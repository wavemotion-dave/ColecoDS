
//{{BLOCK(msx_japan)

//======================================================================
//
//	msx_japan, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 590 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 8308 + 1728 = 10548
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_MSX_JAPAN_H
#define GRIT_MSX_JAPAN_H

#define msx_japanTilesLen 8308
extern const unsigned int msx_japanTiles[2077];

#define msx_japanMapLen 1728
extern const unsigned short msx_japanMap[864];

#define msx_japanPalLen 512
extern const unsigned short msx_japanPal[256];

#endif // GRIT_MSX_JAPAN_H

//}}BLOCK(msx_japan)
