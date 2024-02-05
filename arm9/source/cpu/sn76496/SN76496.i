;@
;@  SN76496.i
;@  SN76496/SN76489 sound chip emulator for arm32.
;@
;@  Created by Fredrik Ahlström on 2009-08-25.
;@  Copyright © 2009-2024 Fredrik Ahlström. All rights reserved.
;@
;@ ASM header for the SN76496 emulator
;@

#if !__ASSEMBLER__
	#error This header file is only for use in assembly files!
#endif

							;@ SN76496.s
	.struct 0
	ch0Frq:			.short 0
	ch0Cnt:			.short 0
	ch1Frq:			.short 0
	ch1Cnt:			.short 0
	ch2Frq:			.short 0
	ch2Cnt:			.short 0
	ch3Frq:			.short 0
	ch3Cnt:			.short 0

	currentBits:	.long 0

	rng:			.long 0
	noiseFB:		.long 0

	snAttChg:		.byte 0
	snLastReg:		.byte 0
	snPadding:		.space 2

	calculatedVolumes:	.space 16*2

	ch0Reg:			.short 0
	ch0Att:			.short 0
	ch1Reg:			.short 0
	ch1Att:			.short 0
	ch2Reg:			.short 0
	ch2Att:			.short 0
	ch3Reg:			.short 0
	ch3Att:			.short 0

	noiseType:		.long 0
	snSize:

;@----------------------------------------------------------------------------

