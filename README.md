# ColecoDS
ColecoDS - An Emulator for the DS/DSi

To run requires a coleco.rom bios to 
be in the same directory as the emulator
or else in /roms/bios or /data/bios

Emulator for the Colecovision plus the
Super Game Module (SGM) and the MegaCart
for games larger than 32K.


Copyright :
-----------------------
ColecoDS Phoenix-Edition is Copyright (c) 2021 Dave Bernazzani (wavemotion-dave)

Copying and distribution of this emulator, it's source code
and associated readme files, with or without modification, 
are permitted in any medium without royalty provided this 
copyright notice is used and wavemotion-dave (Phoenix-Edition),
Alekmaul (original port) and Marat Fayzullin (ColEM core) are 
thanked profusely.

The ColecoDS emulator is offered as-is, without any warranty.

Versions :
-----------------------
V3.6: 25-Nov-2021 by wavemotion-dave
* New game options for frame skip, frame blend, auto fire, etc.
* Fixed pop noise on some of the SGM-AY games.
* Minor menu cleanup for better visual presentation.

V3.5: 24-Nov-2021 by wavemotion-dave
* Sound finally fixed with use of maxmod library!
* Updated CRC computation to match real crc32.
* High scores, save states and key options all changed - sorry!
* Lots of cleanups as timer permitted.

V3.4: 23-Nov-2021 by wavemotion-dave
* Save key map on a per-game basis.
* English is now the only language option.
* More cleanups and tweaks under the hood.

V3.3: 22-Nov-2021 by wavemotion-dave
* AY sound channels are now independent - for a CV total of 6 channels.
* Added MC/AY/SGM indicators on-screen for enhanced carts.
* Other cleanups and minor improvements across the board.

V3.2: 20-Nov-2021 by wavemotion-dave
* More AY sound improvements for the Super Game Module.
* Slight optmization of VDP rendering.
* Improved display of Game Titles - slower scroll and centered.
* Increase in contrast on game selection.
* Other cleanups and minor improvements across the board.

V3.1: 19-Nov-2021 by wavemotion-dave
* Fixed noise sound handling on AY/SGM games.
* Fixed audio pop going into first game.
* Optimized video rendering for speed improvement across the board.
* More robust VDP handling to avoid memory overflow.

V3.0: 18-Nov-2021 by wavemotion-dave
* Ressurected from the ashes - ported to the latest LIBNDS and DEVKIT PRO development tools.
* Sound core updated to latest SN76496.
* CPU core DrZ80 updated to latest.
* Added Super Game Module support with AY sound handling.
* Added MegaCart and Activision PCB cart support for larger games.
* Speed improvements and optmizations across the board.

Credits :
-----------------------
Thanks to Alekmaul who provided the 
baseline code to work with and to lobo
for the menu graphical design.

Thanks to Reesy for the DrZ80 core.

Thanks to Flubba for the SN76496 sound core.

Special thanks to  Marat Fayzullin, as the 
author of ColEM which is the code for the 
core emulation (specifically TMS9918 VDP).
I think the original port was circa ColEM 2.1
with some fixes incorproated from ColEM 2.9
and updated Sprite/Line handling from ColEM 5.6

Known Issues :
-----------------------
* Fathom won't render screen properly. Unknown cause.
* 64K Activision PCB carts have no EEPROM support (Black Onyx, Boxxle - both playable without saves).
* Deep Dungeon Adventures won't run.
* Sudoku has graphical issues (still playable).
* Uridium won't run.
* Super Pac Man has major graphical issues.
* Super Space Acer crashes after a few seconds of play.
* Pillars won't run.
* Vexxed won't run.
* Missile-Strike crashes about 10-15 seconds into the gameplay.
* Astrostorm crashes about 10-15 seconds into the gameplay.
* Arno Dash and Diamond Dash 2 glitch out soon after loading.
* Ghostblaster starts but controls don't work.
* The original 2011 release of StarForce will crash - this is a known bug. There is a patched version of StarForce on Atariage.
* Super DK and Super DK Jr prototypes are designed for the Adam which is not emulated (but they kinda play... somewhat)

Controllers :
-----------------------
Right now only the Player 1 (left) controller is emulated. 
For games that require other special controllers (Turbo, Slither, etc)
you can seek out "SCE" (Standard Controller Editions) which are fan-made
hacks that work with the standard controller. 

Features :
-----------------------
* Super Game Module support.
* Megacart Bankswitching support (up to 512K).
* Controller button mapping.
* High-Score support.
* Save/Load Game State (one slot).
* Blend Mode (see below).

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

