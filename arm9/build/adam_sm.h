
//{{BLOCK(adam_sm)

//======================================================================
//
//	adam_sm, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 789 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 19668 + 1900 = 22080
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_ADAM_SM_H
#define GRIT_ADAM_SM_H

#define adam_smTilesLen 19668
extern const unsigned int adam_smTiles[4917];

#define adam_smMapLen 1900
extern const unsigned short adam_smMap[950];

#define adam_smPalLen 512
extern const unsigned short adam_smPal[256];

#endif // GRIT_ADAM_SM_H

//}}BLOCK(adam_sm)
