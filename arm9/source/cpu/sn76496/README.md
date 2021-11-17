# SN76496
SN76496/SN76489, SMS sound chip plus GG stereo extension for ARM32.

First alloc chip struct, call init with chip type.
Call SN76496Mixer with chip struct, length and destination.
Produces signed 16bit interleaved stereo.
