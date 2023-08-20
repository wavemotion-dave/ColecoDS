
//{{BLOCK(boulder)

//======================================================================
//
//	boulder, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 657 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 16572 + 1612 = 18696
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_BOULDER_H
#define GRIT_BOULDER_H

#define boulderTilesLen 16572
extern const unsigned int boulderTiles[4143];

#define boulderMapLen 1612
extern const unsigned short boulderMap[806];

#define boulderPalLen 512
extern const unsigned short boulderPal[256];

#endif // GRIT_BOULDER_H

//}}BLOCK(boulder)
