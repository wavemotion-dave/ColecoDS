
//{{BLOCK(gateway)

//======================================================================
//
//	gateway, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 733 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 17972 + 1784 = 20268
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_GATEWAY_H
#define GRIT_GATEWAY_H

#define gatewayTilesLen 17972
extern const unsigned int gatewayTiles[4493];

#define gatewayMapLen 1784
extern const unsigned short gatewayMap[892];

#define gatewayPalLen 512
extern const unsigned short gatewayPal[256];

#endif // GRIT_GATEWAY_H

//}}BLOCK(gateway)
