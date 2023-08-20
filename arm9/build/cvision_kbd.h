
//{{BLOCK(cvision_kbd)

//======================================================================
//
//	cvision_kbd, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 566 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 7828 + 1712 = 10052
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_CVISION_KBD_H
#define GRIT_CVISION_KBD_H

#define cvision_kbdTilesLen 7828
extern const unsigned int cvision_kbdTiles[1957];

#define cvision_kbdMapLen 1712
extern const unsigned short cvision_kbdMap[856];

#define cvision_kbdPalLen 512
extern const unsigned short cvision_kbdPal[256];

#endif // GRIT_CVISION_KBD_H

//}}BLOCK(cvision_kbd)
