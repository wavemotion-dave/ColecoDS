# AY38910
An AY-3-8910 / YM2149 sound chip emulator for ARM32.

First alloc chip struct, call init then set in/out function pointers.
Call AY38910Mixer with chip struct, length and destination.
Produces signed 16bit mono.
