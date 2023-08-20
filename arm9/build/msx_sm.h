
//{{BLOCK(msx_sm)

//======================================================================
//
//	msx_sm, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 785 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 19740 + 1880 = 22132
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_MSX_SM_H
#define GRIT_MSX_SM_H

#define msx_smTilesLen 19740
extern const unsigned int msx_smTiles[4935];

#define msx_smMapLen 1880
extern const unsigned short msx_smMap[940];

#define msx_smPalLen 512
extern const unsigned short msx_smPal[256];

#endif // GRIT_MSX_SM_H

//}}BLOCK(msx_sm)
