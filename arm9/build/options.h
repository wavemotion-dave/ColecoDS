
//{{BLOCK(options)

//======================================================================
//
//	options, 256x256@8, 
//	+ palette 256 entries, not compressed
//	+ 328 tiles (t|f reduced) lz77 compressed
//	+ regular map (in SBBs), lz77 compressed, 32x32 
//	Total size: 512 + 5856 + 968 = 7336
//
//	Time-stamp: 2023-08-20, 06:54:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_OPTIONS_H
#define GRIT_OPTIONS_H

#define optionsTilesLen 5856
extern const unsigned int optionsTiles[1464];

#define optionsMapLen 968
extern const unsigned short optionsMap[484];

#define optionsPalLen 512
extern const unsigned short optionsPal[256];

#endif // GRIT_OPTIONS_H

//}}BLOCK(options)
