
//{{BLOCK(hal2010)

//======================================================================
//
//	hal2010, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 778 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 20420 + 1860 = 22792
//
//	Time-stamp: 2023-08-20, 06:54:14
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_HAL2010_H
#define GRIT_HAL2010_H

#define hal2010TilesLen 20420
extern const unsigned int hal2010Tiles[5105];

#define hal2010MapLen 1860
extern const unsigned short hal2010Map[930];

#define hal2010PalLen 512
extern const unsigned short hal2010Pal[256];

#endif // GRIT_HAL2010_H

//}}BLOCK(hal2010)
