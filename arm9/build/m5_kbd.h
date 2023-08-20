
//{{BLOCK(m5_kbd)

//======================================================================
//
//	m5_kbd, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 543 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 7588 + 1544 = 9644
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_M5_KBD_H
#define GRIT_M5_KBD_H

#define m5_kbdTilesLen 7588
extern const unsigned int m5_kbdTiles[1897];

#define m5_kbdMapLen 1544
extern const unsigned short m5_kbdMap[772];

#define m5_kbdPalLen 512
extern const unsigned short m5_kbdPal[256];

#endif // GRIT_M5_KBD_H

//}}BLOCK(m5_kbd)
