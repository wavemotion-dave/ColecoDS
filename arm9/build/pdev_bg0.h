
//{{BLOCK(pdev_bg0)

//======================================================================
//
//	pdev_bg0, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 643 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 13528 + 1568 = 15608
//
//	Time-stamp: 2023-08-20, 06:54:14
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_PDEV_BG0_H
#define GRIT_PDEV_BG0_H

#define pdev_bg0TilesLen 13528
extern const unsigned int pdev_bg0Tiles[3382];

#define pdev_bg0MapLen 1568
extern const unsigned short pdev_bg0Map[784];

#define pdev_bg0PalLen 512
extern const unsigned short pdev_bg0Pal[256];

#endif // GRIT_PDEV_BG0_H

//}}BLOCK(pdev_bg0)
