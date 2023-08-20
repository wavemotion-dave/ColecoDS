
//{{BLOCK(mousetrap)

//======================================================================
//
//	mousetrap, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 796 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 19940 + 1896 = 22348
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_MOUSETRAP_H
#define GRIT_MOUSETRAP_H

#define mousetrapTilesLen 19940
extern const unsigned int mousetrapTiles[4985];

#define mousetrapMapLen 1896
extern const unsigned short mousetrapMap[948];

#define mousetrapPalLen 512
extern const unsigned short mousetrapPal[256];

#endif // GRIT_MOUSETRAP_H

//}}BLOCK(mousetrap)
