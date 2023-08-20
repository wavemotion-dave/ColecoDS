
//{{BLOCK(adam_full)

//======================================================================
//
//	adam_full, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 575 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 8496 + 1688 = 10696
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_ADAM_FULL_H
#define GRIT_ADAM_FULL_H

#define adam_fullTilesLen 8496
extern const unsigned int adam_fullTiles[2124];

#define adam_fullMapLen 1688
extern const unsigned short adam_fullMap[844];

#define adam_fullPalLen 512
extern const unsigned short adam_fullPal[256];

#endif // GRIT_ADAM_FULL_H

//}}BLOCK(adam_full)
