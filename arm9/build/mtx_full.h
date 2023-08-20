
//{{BLOCK(mtx_full)

//======================================================================
//
//	mtx_full, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 548 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 8228 + 1640 = 10380
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_MTX_FULL_H
#define GRIT_MTX_FULL_H

#define mtx_fullTilesLen 8228
extern const unsigned int mtx_fullTiles[2057];

#define mtx_fullMapLen 1640
extern const unsigned short mtx_fullMap[820];

#define mtx_fullPalLen 512
extern const unsigned short mtx_fullPal[256];

#endif // GRIT_MTX_FULL_H

//}}BLOCK(mtx_full)
