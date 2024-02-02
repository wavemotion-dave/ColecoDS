;@
;@  SCC.i
;@  Konami SCC/K051649 sound chip emulator for arm32.
;@
;@  Created by Fredrik Ahlström on 2006-04-01.
;@  Copyright © 2006-2024 Fredrik Ahlström. All rights reserved.
;@
;@ ASM header for the Konami SCC emulator
;@

#if !__ASSEMBLER__
	#error This header file is only for use in assembly files!
#endif	// !__ASSEMBLER__

	sccptr			.req r12

							;@ SCC.s
	.struct 0
	sccCh0Wave:		.space 32
	sccCh1Wave:		.space 32
	sccCh2Wave:		.space 32
	sccCh3Wave:		.space 32
	sccCh0Frq:		.short 0
	sccCh1Frq:		.short 0
	sccCh2Frq:		.short 0
	sccCh3Frq:		.short 0
	sccCh4Frq:		.short 0
	sccCh0Volume:	.byte 0
	sccCh1Volume:	.byte 0
	sccCh2Volume:	.byte 0
	sccCh3Volume:	.byte 0
	sccCh4Volume:	.byte 0
	sccChControl:	.byte 0
	sccTestReg:		.byte 0
	sccPadding:		.space 3

	sccCh0Freq:		.short 0
	sccCh0Addr:		.short 0
	sccCh1Freq:		.short 0
	sccCh1Addr:		.short 0
	sccCh2Freq:		.short 0
	sccCh2Addr:		.short 0
	sccCh3Freq:		.short 0
	sccCh3Addr:		.short 0
	sccCh4Freq:		.short 0
	sccCh4Addr:		.short 0

	sccSize:

;@----------------------------------------------------------------------------

