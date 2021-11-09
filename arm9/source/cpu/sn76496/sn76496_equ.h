;@ ASM header for the SN76496 emulator
;@

	snptr		.req r0

						;@ sn76496.s
	.equ sn_lastreg,	0
	.equ ggstereo,		sn_lastreg + 1
	.equ sn_dummy1,		ggstereo + 1
	.equ sn_dummy2,		sn_dummy1 + 1

	.equ ch0reg,		sn_dummy2 + 1
	.equ ch1reg,		ch0reg + 4
	.equ ch2reg,		ch1reg + 4
	.equ ch3reg,		ch2reg + 4

	.equ ch0freq,		ch3reg + 4
	.equ ch0addr,		ch0freq + 2
	.equ ch1freq,		ch0addr + 2
	.equ ch1addr,		ch1freq + 2
	.equ ch2freq,		ch1addr + 2
	.equ ch2addr,		ch2freq + 2
	.equ ch3freq,		ch2addr + 2
	.equ ch3addr,		ch3freq + 2

	.equ rng,			ch3addr + 2
	.equ noisefb,		rng + 4

	.equ ch0volume,		noisefb + 4
	.equ ch1volume,		ch0volume + 4
	.equ ch2volume,		ch1volume + 4
	.equ ch3volume,		ch2volume + 4

	.equ pcmptr,		ch3volume + 4
	.equ mixlength,		pcmptr + 4
	.equ mixrate,		mixlength + 4
	.equ freqconv,		mixrate + 4
	.equ freqtableptr,	freqconv + 4
	.equ sn_size,		freqtableptr + 4

;@----------------------------------------------------------------------------

