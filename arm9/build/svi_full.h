
//{{BLOCK(svi_full)

//======================================================================
//
//	svi_full, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 599 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 8544 + 1728 = 10784
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_SVI_FULL_H
#define GRIT_SVI_FULL_H

#define svi_fullTilesLen 8544
extern const unsigned int svi_fullTiles[2136];

#define svi_fullMapLen 1728
extern const unsigned short svi_fullMap[864];

#define svi_fullPalLen 512
extern const unsigned short svi_fullPal[256];

#endif // GRIT_SVI_FULL_H

//}}BLOCK(svi_full)
