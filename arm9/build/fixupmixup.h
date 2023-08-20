
//{{BLOCK(fixupmixup)

//======================================================================
//
//	fixupmixup, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 722 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 18392 + 1756 = 20660
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_FIXUPMIXUP_H
#define GRIT_FIXUPMIXUP_H

#define fixupmixupTilesLen 18392
extern const unsigned int fixupmixupTiles[4598];

#define fixupmixupMapLen 1756
extern const unsigned short fixupmixupMap[878];

#define fixupmixupPalLen 512
extern const unsigned short fixupmixupPal[256];

#endif // GRIT_FIXUPMIXUP_H

//}}BLOCK(fixupmixup)
