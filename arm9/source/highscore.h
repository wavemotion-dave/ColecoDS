// =====================================================================================
// Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, it's source code and associated 
// readme files, with or without modification, are permitted in any medium without 
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty.
// =====================================================================================

#ifndef __HIGHSCORE_H
#define __HIGHSCORE_H

#include <nds.h>

extern void highscore_init(void);
extern void highscore_save(void);
extern void highscore_display(u32 crc);

#endif
