# AY38910
An AY-3-8910 / YM2149 sound chip emulator for ARM32.

First alloc chip struct, call reset then set in/out function pointers.
Call AY38910Mixer with length, destination and chip struct.
Produces signed 16bit mono.
