;@
;@  SCC.s
;@  Konami SCC/K051649 sound chip emulator for arm32.
;@
;@  Created by Fredrik Ahlström on 2006-04-01.
;@  Copyright © 2006-2024 Fredrik Ahlström. All rights reserved.
;@
#ifdef __arm__

#include "SCC.i"

#define SCCDIVIDE 32
#define SCCADDITION 0x00004000*SCCDIVIDE

	.global SCCReset
	.global SCCMixer
	.global SCCWrite
	.global SCCRead


	.syntax unified
	.arm

	.section .itcm
	.align 2
;@----------------------------------------------------------------------------
;@ r0  = mix length.
;@ r1  = mixerbuffer.
;@ r2 -> r6 = pos+freq.
;@ r7 = sample reg/volume.
;@ r8 = mixer reg left.
;@ r9 = scrap
;@ r10= scrap
;@ r11= scrap
;@ r12= SCC ptr
;@ lr = return address.
;@----------------------------------------------------------------------------
SCCMix:
//IIIIIVCCCCCCCCCCCC10FFFFFFFFFFFF
//I=sampleindex, V=overflow, C=counter, F=frequency
;@----------------------------------------------------------------------------
sccMixLoop:
	add r2,r2,#SCCADDITION
	movs r9,r2,lsr#27
	mov r11,r2,lsl#18
	subcs r2,r2,r11,asr#4
vol0:
	movs r8,#0x00				;@ volume
	ldrsbne r11,[sccptr,r9]		;@ Channel 0
	mulne r8,r11,r8


	add r3,r3,#SCCADDITION
	movs r9,r3,lsr#27
	add r9,r9,#0x20
	mov r11,r3,lsl#18
	subcs r3,r3,r11,asr#4
vol1:
	movs r7,#0x00				;@ volume
	ldrsbne r11,[sccptr,r9]		;@ Channel 1
	mlane r8,r7,r11,r8


	add r4,r4,#SCCADDITION
	movs r9,r4,lsr#27
	add r9,r9,#0x40
	mov r11,r4,lsl#18
	subcs r4,r4,r11,asr#4
vol2:
	movs r7,#0x00				;@ volume
	ldrsbne r11,[sccptr,r9]		;@ Channel 2
	mlane r8,r7,r11,r8


	add r5,r5,#SCCADDITION
	movs r9,r5,lsr#27
	add r9,r9,#0x60
	mov r11,r5,lsl#18
	subcs r5,r5,r11,asr#4
vol3:
	movs r7,#0x00				;@ volume
	ldrsbne r11,[sccptr,r9]		;@ Channel 3
	mlane r8,r7,r11,r8


	add r6,r6,#SCCADDITION
	movs r9,r6,lsr#27
	add r9,r9,#0x60
	mov r11,r6,lsl#18
	subcs r6,r6,r11,asr#4
vol4:
	movs r7,#0x00				;@ volume
	ldrsbne r11,[sccptr,r9]		;@ Channel 4, same waveform as ch3
	mlane r8,r7,r11,r8


	subs r0,r0,#1
	strhpl r8,[r1],#2
	bhi sccMixLoop

	bx lr
;@----------------------------------------------------------------------------

	.section .text
	.align 2
;@----------------------------------------------------------------------------
SCCReset:					;@ r0=pointer to SCC struct
	.type   SCCReset STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,lr}
	mov r1,#0
	mov r2,#sccSize				;@ 144
	bl memset					;@ clear variables
	ldmfd sp!,{r0,lr}
	mov r1,#0x20
	strb r1,[r0,#sccCh0Freq+1]	;@ counters
	strb r1,[r0,#sccCh1Freq+1]	;@ counters
	strb r1,[r0,#sccCh2Freq+1]	;@ counters
	strb r1,[r0,#sccCh3Freq+1]	;@ counters
	strb r1,[r0,#sccCh4Freq+1]	;@ counters
	bx lr
;@----------------------------------------------------------------------------
SCCMixer:					;@ r0=len, r1=dest, r2=pointer to SCC struct
	.type   SCCMixer STT_FUNC
;@----------------------------------------------------------------------------
	;@ update DMA buffer for PCM

	mov sccptr,r2
	stmfd sp!,{r4-r11,lr}

;@--------------------------
	adr r2,SCCVolume
	ldrb r3,[sccptr,#sccChControl]

	ands r4,r3,#0x01
	ldrbne r4,[sccptr,#sccCh0Volume]
	ands r4,r4,#0x0F
	ldrbne r4,[r2,r4]
	ldr r5,=vol0
	strb r4,[r5]

	ands r4,r3,#0x02
	ldrbne r4,[sccptr,#sccCh1Volume]
	ands r4,r4,#0x0F
	ldrbne r4,[r2,r4]
	strb r4,[r5,#vol1-vol0]

	ands r4,r3,#0x04
	ldrbne r4,[sccptr,#sccCh2Volume]
	ands r4,r4,#0x0F
	ldrbne r4,[r2,r4]
	strb r4,[r5,#vol2-vol0]

	ands r4,r3,#0x08
	ldrbne r4,[sccptr,#sccCh3Volume]
	ands r4,r4,#0x0F
	ldrbne r4,[r2,r4]
	strb r4,[r5,#vol3-vol0]

	ands r4,r3,#0x10
	ldrbne r4,[sccptr,#sccCh4Volume]
	ands r4,r4,#0x0F
	ldrbne r4,[r2,r4]
	strb r4,[r5,#vol4-vol0]


	add r7,sccptr,#sccCh0Freq	;@ counters
	ldmia r7,{r2-r6}
;@--------------------------
	ldrh r10,[sccptr,#sccCh0Frq]
	bic r10,r10,#0xF000
	mov r2,r2,lsr#12
	orr r2,r10,r2,lsl#12
;@--------------------------
	ldrh r10,[sccptr,#sccCh1Frq]
	bic r10,r10,#0xF000
	mov r3,r3,lsr#12
	orr r3,r10,r3,lsl#12
;@--------------------------
	ldrh r10,[sccptr,#sccCh2Frq]
	bic r10,r10,#0xF000
	mov r4,r4,lsr#12
	orr r4,r10,r4,lsl#12
;@--------------------------
	ldrh r10,[sccptr,#sccCh3Frq]
	bic r10,r10,#0xF000
	mov r5,r5,lsr#12
	orr r5,r10,r5,lsl#12
;@--------------------------
	ldrh r10,[sccptr,#sccCh4Frq]
	bic r10,r10,#0xF000
	mov r6,r6,lsr#12
	orr r6,r10,r6,lsl#12
;@--------------------------

	bl SCCMix

	add r7,sccptr,#sccCh0Freq	;@ counters
	stmia r7,{r2-r6}

	ldmfd sp!,{r4-r11,lr}
	bx lr
;@----------------------------------------------------------------------------
SCCVolume:
	.byte 0,3,7,10,14,17,20,24,27,31,34,37,41,44,48,51
;@----------------------------------------------------------------------------
SCCRead:					;@ 0x9800-0x9FFF, r0=adr, r1=SCC
	.type   SCCRead STT_FUNC
;@----------------------------------------------------------------------------
	movs r0,r0,lsl#24
	ldrbpl r0,[r1,r0,lsr#24]
	movmi r0,#0xFF
	bx lr
;@----------------------------------------------------------------------------
SCCWrite:					;@ 0x9800-0x9FFF, r0=val, r1=adr, r2=SCC
	.type   SCCWrite STT_FUNC
;@----------------------------------------------------------------------------
	and r1,r1,#0xFF				;@ 0x00-0x7F wave ram.
	cmp r1,#0x90				;@ 0x80-0x8F registers, 0x90-0x9F mirror.
	subpl r1,r1,#0x10			;@ 0xE0-0xFF test register, all mirrors.
	cmp r1,#0x90
	strbmi r0,[r2,r1]
	strbpl r0,[r2,#sccTestReg]
	bx lr

;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
