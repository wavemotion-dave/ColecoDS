
//{{BLOCK(alpha_kbd)

//======================================================================
//
//	alpha_kbd, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 401 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 5748 + 1332 = 7592
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_ALPHA_KBD_H
#define GRIT_ALPHA_KBD_H

#define alpha_kbdTilesLen 5748
extern const unsigned int alpha_kbdTiles[1437];

#define alpha_kbdMapLen 1332
extern const unsigned short alpha_kbdMap[666];

#define alpha_kbdPalLen 512
extern const unsigned short alpha_kbdPal[256];

#endif // GRIT_ALPHA_KBD_H

//}}BLOCK(alpha_kbd)
