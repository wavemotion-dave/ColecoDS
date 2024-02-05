# SCC V0.2.3

Konami SCC/K051649 sound chip emulator for ARM32.

## Usage

First alloc chip struct, call SCCReset.
Call SCCMixer with length, destination and chip struct.
Produces signed 16bit mono.
You can define SCCMULT to a number, this is how many more times the internal
sampling is. You can add "-DSCCMULT=32" to the "make" file to make the
 internal clock speed 32 times higher. Default is 16.

The code uses self modifying code so you can only instantiate one chip at a
time.

## Projects that use this code

* https://github.com/FluBBaOfWard/S8DS
* https://github.com/wavemotion-dave/ColecoDS

## Credits

Fredrik Ahlström

X/Twitter @TheRealFluBBa

https://www.github.com/FluBBaOfWard
