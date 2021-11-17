;@ ASM header for the SN76496 emulator
;@

	snptr			.req r12

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
	ggStereo:		.byte 0
	snPadding:		.space 1

	ch0Reg:			.short 0
	ch0Att:			.short 0
	ch1Reg:			.short 0
	ch1Att:			.short 0
	ch2Reg:			.short 0
	ch2Att:			.short 0
	ch3Reg:			.short 0
	ch3Att:			.short 0

	snPadding2:		.space 4*4
	calculatedVolumes:	.space 16*2*2

	snSize:

;@----------------------------------------------------------------------------

