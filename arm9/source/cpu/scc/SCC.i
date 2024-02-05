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
#endif

							;@ SCC.s
	.struct 0
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

sccStateStart:
sccCh0Wave:		.space 32	;@ 0x00-0x1F
sccCh1Wave:		.space 32	;@ 0x20-0x3F
sccCh2Wave:		.space 32	;@ 0x40-0x5F
sccCh3Wave:		.space 32	;@ 0x60-0x7F Both Ch3 & Ch4
sccCh0Frq:		.short 0	;@ 0x80/0x90
sccCh1Frq:		.short 0	;@ 0x82/0x92
sccCh2Frq:		.short 0	;@ 0x84/0x94
sccCh3Frq:		.short 0	;@ 0x86/0x96
sccCh4Frq:		.short 0	;@ 0x88/0x98
sccCh0Volume:	.byte 0		;@ 0x8A/0x9A
sccCh1Volume:	.byte 0		;@ 0x8B/0x9B
sccCh2Volume:	.byte 0		;@ 0x8C/0x9C
sccCh3Volume:	.byte 0		;@ 0x8D/0x9D
sccCh4Volume:	.byte 0		;@ 0x8E/0x9E
sccChControl:	.byte 0		;@ 0x8F/0x9F

sccTestReg:		.byte 0		;@ 0xE0-0xFF
sccPadding:		.space 3
sccStateEnd:

sccSize:

;@----------------------------------------------------------------------------

