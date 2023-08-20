
//{{BLOCK(sc3000_kbd)

//======================================================================
//
//	sc3000_kbd, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 534 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 8132 + 1552 = 10196
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_SC3000_KBD_H
#define GRIT_SC3000_KBD_H

#define sc3000_kbdTilesLen 8132
extern const unsigned int sc3000_kbdTiles[2033];

#define sc3000_kbdMapLen 1552
extern const unsigned short sc3000_kbdMap[776];

#define sc3000_kbdPalLen 512
extern const unsigned short sc3000_kbdPal[256];

#endif // GRIT_SC3000_KBD_H

//}}BLOCK(sc3000_kbd)
