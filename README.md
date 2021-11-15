# ColecoDS
ColecoDS - An Emulator for the DS/DSi

To run requires a coleco.rom bios to 
be in the same directory as the emulator
or else in /roms/bios or /data/bios

Emulator for the Colecovision plus the
Super Game Module (SGM) and the MegaCart
for games larger than 32K.

This is a work-in-progress... more readme 
coming soon.

Copyright :
-----------------------
ColecoDS is Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)

Copying and distribution of this emulator, it's source code
and associated readme files, with or without modification, 
are permitted in any medium without royalty provided this 
copyright notice is used and wavemotion-dave (Phoenix-Edition),
Alekmaul (original port) and Marat Fayzullin (ColEM core) are 
thanked profusely.

The ColecoDS emulator is offered as-is, without any warranty.

Credits :
-----------------------
Thanks to Alekmaul who provided the 
baseline code to work with and to lobo
for the menu graphical design.

Thanks to Reesy for the DrZ80 core.

Special thanks to  Marat Fayzullin, as the 
author of ColEM which is the code for the 
core emulation (Z80, TMS9918 and SN76489).
I think the original port was circa ColEM 2.1
with some fixes incorproated from ColEM 2.9

Known Issues :
-----------------------
* Fathom won't render screen properly. Unknown cause.
* 64K Activision PCB carts have no EEPROM support (Black Onyx, Boxxle).
* Deep Dungeon Adventures won't run.
* Mario Bros. has graphical issues.
* Sudoku has graphical issues (still playable).
* Uridium won't run.
* Flappy Bird has graphical issues.
* Super Space Acer crashes after a few seconds of play.
* Pillars won't run.
* Vexxed won't run.
* Lord of the Dungeon doesn't play (need SRAM support).
* The original 2011 release of StarForce will crash - this is a known bug. There is a patched version of StarForce on Atariage.
* MegaCart games are limited to 512K (MegaCart supports up to 1MB)

Features :
-----------------------
A huge change is the new "blend mode" which I borrowed from my scheme on StellaDS. In this mode, 
two frames are blended together - this is really useful when playing games like Space Fury or Galaxian 
where the bullets on screen are only 1 pixel wide and the DSi LCD just doesn't hold onto the pixels 
long enough to be visible. These games were designed to run on an old tube TV with phosphor which 
decays slowly so your eye will see slight traces as the image fades. This emulates that (crudely).
On the DSi using this new mode renders those games really bright and visible.

The DSi XL/LL has a slower refresh on the LCD and it more closely approximates the old tube TVs... 
so blend mode is not needed for the XL/LL models.

However! Using blend mode comes at at 25% CPU cost!! The DSi can handle it... the DS-LITE/PHAT cannot.

So my recommendation is as follows:
* DSi non XL/LL - use Blend Mode for the games that benefit from it (Space Fury, Galaxian, etc).
* DSi XL/LL - don't bother... the XL/LL screen decay is slower and games look great on it.
* DS-LITE/PHAT - sorry, just not enough CPU to handle blending mode. Games will still play fine as-is.

To enable this new blend mode, when you pick your game use Y instead of A to select the game. I
I've added it to the game loading instructions to remind you.


