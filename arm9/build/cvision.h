
//{{BLOCK(cvision)

//======================================================================
//
//	cvision, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 782 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 19640 + 1880 = 22032
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_CVISION_H
#define GRIT_CVISION_H

#define cvisionTilesLen 19640
extern const unsigned int cvisionTiles[4910];

#define cvisionMapLen 1880
extern const unsigned short cvisionMap[940];

#define cvisionPalLen 512
extern const unsigned short cvisionPal[256];

#endif // GRIT_CVISION_H

//}}BLOCK(cvision)
