# ColecoDS
ColecoDS - A Colecovision Emulator for the DS/DSi

To run requires a coleco.rom bios to 
be in the same directory as the emulator
or else in /roms/bios or /data/bios

_**Your Vision Is Our Vision...  COLECOVISION**_


Features :
-----------------------
* Super Game Module support including AY sound chip.
* Megacart Bankswitching support (up to 512K).
* Full Controller button mapping and touch-screen input.
* High-Score support - 10 scores per game.
* Save/Load Game State (one slot).
* Video Blend Mode (see below) and Vertical Sync.
* LCD Screen Swap (press and hold L+R+X during gameplay).
* Overlay support for the few games that need them.
* Super Action Controller, Spinner and Roller Controller (Trackball) mapping.
* Sega SG-1000 game support (.sg roms)
* Full speed, full sound and full frame-rate even on older hardware.

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

Credits :
-----------------------
Thanks to Alekmaul who provided the 
baseline code to work with and to lobo
for the menu graphical design.

Thanks to Reesy for the DrZ80 core.

Thanks to Flubba for the SN76496 sound core.

Special thanks to  Marat Fayzullin, as the 
author of ColEM which is the code for the 
core emulation (specifically TMS9918 VDP
and the CZ80 CPU core).  I think the original 
port was circa ColEM 2.1 with some fixes and 
updated Sprite/Line handling from ColEM 5.6

Known Issues :
-----------------------
* Borders are not correctly rendered - only a few games utilize them and are still fully playable without this.
* Games that utilize voice samples (Squish Em Sam, Wizard of Wor, etc) will not play the speech.
* 64K Activision PCB carts have no EEPROM support (Black Onyx, Boxxle - both playable without saves).
* The original 2011 release of StarForce will crash - this is a known bug. There is a patched version of the game StarForce on Atariage.

Controllers :
-----------------------
You can map buttons to either P1 or P2 controllers. 
There is full support for Spinner X (P1) and Spinner Y (P2) or map 
both of them to get support for trackball games. These also work
for games like Turbo steering.  You can change the spinner sensitivity
to one of five different settings (Normal, Fast, Fastest, Slow, Slowest).

Blend Mode (DSi) :
-----------------------
A huge change is the new "blend mode" which I borrowed from my scheme on StellaDS. In this mode, 
two frames are blended together - this is really useful when playing games like Space Fury or Galaxian 
where the bullets on screen are only 1 pixel wide and the DSi LCD just doesn't hold onto the pixels 
long enough to be visible. These games were designed to run on an old tube TV with phosphor which 
decays slowly so your eye will see slight traces as the image fades. This emulates that (crudely).
On the DSi using this new mode renders those games really bright and visible.

The DSi XL/LL has a slower refresh on the LCD and it more closely approximates the old tube TVs... 
so blend mode is not needed for the XL/LL models.

However! Using blend mode comes at at 25% CPU cost!! The DSi can handle it... the DS-LITE/PHAT might
struggle a bit on more complicated games. 

So my recommendation is as follows:
* DSi non XL/LL - use Blend Mode for the games that benefit from it (Space Fury, Galaxian, etc).
* DSi XL/LL - don't bother... the XL/LL screen decay is slower and games look great as-is.
* DS-LITE/PHAT - you can try it but the framerate might drop below 60 on some games.

To enable this new blend mode, pick your game and go into the "Game Options" sub-menu and turn it on.

Vertical Sync :
-----------------------
Vertical sync will force the update (refresh) of the screen when the DS goes into the vertical blank.
This reduces tearing and minor graphical artifacts but comes at a cost of speed. The DSi can handle
it for almost all games (Princess Quest is one game where you might turn it off) but the DS can only
handle it for the more simple games. So by default it's enabled for DSi and disabled for DS-LITE/PHAT.
You can toggle this in the "Game Options" (and START=SAVE it out as you wish). 

A Tale of Two Cores :
-----------------------
ColecoDS supports 2 different Z80 CPU cores. 
DrZ80 is very fast but is not 100% accurate so some games don't run right.
CZ80 is slower but is much closer to 100% accurate and games generally run great.
For the DSi and above, the CZ80 core is the default.
For the DS-LITE/PHAT, the DrZ80 core is the default.
You can toggle this (and save on a per-game basis) in GAME OPTIONS.
If you want to use the CZ80 core but need a bit more speed to make the
games playable - you can also play with Vertical Sync and Frame Skip settings.

The Need For Speed :
-----------------------
If a game just isn't running at the right speed or has periods of slowdown (not
attributed to the actual game), here are the things you can try in the order I 
would personally try them:
* Turn off Vertical Sync
* Turn on Frame Skip
* Set Max Sprites to 4
* Switch to the DrZ80 fast CPU core

Versions :
-----------------------
V4.8: 26-Dec-2021 by wavemotion-dave
* Preliminary support for SG-1000 games (.sg format)
* Cleanup across the board - a bit more speed gained.

V4.7: 23-Dec-2021 by wavemotion-dave
* Major speed improvements in the new CZ80 core. 
* Installed new CZ80 core as the default for DSi and above. 

V4.6: 22-Dec-2021 by wavemotion-dave
* New CZ80 core added to solve compatibility problems with the remaining games.

V4.5: 21-Dec-2021 by wavemotion-dave
* Full support for Spinner/Trackball. Map SpinX, SpinY in key settings.
* Five different sensitivities for the Spinner/Trackball in Game Options.
* Added ability to Quit Emulator (will return to TWL++ or power off depending on launcher).
* Unified handling of UI in Redefine Keys and Game Options so they work the same.
* Many small fixes and tweaks under the hood - Sudoku fixed.

V4.4: 18-Dec-2021 by wavemotion-dave
* Added option for "Max Sprites" to set to original HW limit of 4 vs 32.
* New Vertical Sync option (default ON for DSi and above) to reduce tearing.
* Fixed Save/Load state so it doesn't break on every new release.
* Slight adjustment to Z80 CPU timing for better accuracy.

V4.3: 16-Dec-2021 by wavemotion-dave
* More overlays added.
* Adjust CPU timing slightly to fix Spy Hunter (cars now spawn) and Frantic.

V4.2: 14-Dec-2021 by wavemotion-dave
* Allow mapping of Super Action Controller buttons.
* Overlay support - five games supported (for now) + Generic.
* Minor cleanups to VDP and better commenting.

V4.1: 11-Dec-2021 by wavemotion-dave
* VDP Timing fixes - Fathom now runs.
* Fixed loading of coleco.rom bios if in same directory as emulator.
* Minor Z80 cleanups for more accurate timing.

V4.0: 09-Dec-2021 by wavemotion-dave
* Fix GhostBlaster homebrew.
* Fix for graphical issues in Meteoric Shower
* Improved DrZ80 core from various web sources
* Improved VDP handling to more closely mimic real TMS9918a
* Many small touch-ups and improvements under the hood

V3.9: 06-Dec-2021 by wavemotion-dave
* Fix for Pitfall II Arcade Homebrew
* Improved memory management
* Shorter keyclick for more responsive keypad touches
* Improved UI key handling
* Other cleanups and improvements under the hood
* Saved states changed again ... sorry!

V3.8: 04-Dec-2021 by wavemotion-dave
* L+R+X to swap LCD Screens
* New light keyclick for feedback of touch controller.
* Other minor cleanups and improvements under the hood.

V3.7: 27-Nov-2021 by wavemotion-dave
* Super DK and Super DK Jr prototypes work now.
* Max Game ROM filename extended to 128 bytes.
* Tries to start in /roms or /roms/coleco if possible.
* Slight tweaks to main menu graphics to clean them up.
* Code cleanup and commenting of source files.

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
