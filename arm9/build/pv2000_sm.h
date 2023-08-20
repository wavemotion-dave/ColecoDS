
//{{BLOCK(pv2000_sm)

//======================================================================
//
//	pv2000_sm, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 782 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 19624 + 1880 = 22016
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_PV2000_SM_H
#define GRIT_PV2000_SM_H

#define pv2000_smTilesLen 19624
extern const unsigned int pv2000_smTiles[4906];

#define pv2000_smMapLen 1880
extern const unsigned short pv2000_smMap[940];

#define pv2000_smPalLen 512
extern const unsigned short pv2000_smPal[256];

#endif // GRIT_PV2000_SM_H

//}}BLOCK(pv2000_sm)
