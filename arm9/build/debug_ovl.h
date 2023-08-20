
//{{BLOCK(debug_ovl)

//======================================================================
//
//	debug_ovl, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 152 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 1828 + 572 = 2912
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_DEBUG_OVL_H
#define GRIT_DEBUG_OVL_H

#define debug_ovlTilesLen 1828
extern const unsigned int debug_ovlTiles[457];

#define debug_ovlMapLen 572
extern const unsigned short debug_ovlMap[286];

#define debug_ovlPalLen 512
extern const unsigned short debug_ovlPal[256];

#endif // GRIT_DEBUG_OVL_H

//}}BLOCK(debug_ovl)
