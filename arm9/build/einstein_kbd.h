
//{{BLOCK(einstein_kbd)

//======================================================================
//
//	einstein_kbd, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 588 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 9692 + 1648 = 11852
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_EINSTEIN_KBD_H
#define GRIT_EINSTEIN_KBD_H

#define einstein_kbdTilesLen 9692
extern const unsigned int einstein_kbdTiles[2423];

#define einstein_kbdMapLen 1648
extern const unsigned short einstein_kbdMap[824];

#define einstein_kbdPalLen 512
extern const unsigned short einstein_kbdPal[256];

#endif // GRIT_EINSTEIN_KBD_H

//}}BLOCK(einstein_kbd)
