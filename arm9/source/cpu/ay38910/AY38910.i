;@
;@  AY38910.i
;@  ASM header for the AY-3-8910 / YM2149 sound chip emulator for arm32.
;@
;@  Created by Fredrik Ahlström on 2006-03-07.
;@  Copyright © 2006-2021 Fredrik Ahlström. All rights reserved.
;@

							;@ AY38910.s
	.struct 0
	ayCh0Freq:		.short 0
	ayCh0Addr:		.short 0
	ayCh1Freq:		.short 0
	ayCh1Addr:		.short 0
	ayCh2Freq:		.short 0
	ayCh2Addr:		.short 0
	ayCh3Freq:		.short 0
	ayCh3Addr:		.short 0

	ayRng:			.long 0
	ayEnvFreq:		.long 0
	ayChState:		.byte 0
	ayChDisable:	.byte 0
	ayEnvType:		.byte 0
	ayEnvAddr:		.byte 0
	ayEnvVolumePtr:	.long 0

	ayAttChg:		.byte 0
	ayRegIndex:		.byte 0
	ayPadding1:		.space 2
	ayPortAOut:		.byte 0
	ayPortBOut:		.byte 0
	ayPortAIn:		.byte 0
	ayPortBIn:		.byte 0
	ayRegs:			.space 16
	ayPortAInFptr:	.long 0
	ayPortBInFptr:	.long 0
	ayPortAOutFptr:	.long 0
	ayPortBOutFptr:	.long 0

	ayCalculatedVolumes:
					.space 8*2
	aySize:

;@----------------------------------------------------------------------------

