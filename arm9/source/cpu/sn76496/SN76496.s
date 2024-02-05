;@
;@  SN76496.s
;@  SN76496/SN76489 sound chip emulator for arm32.
;@
;@  Created by Fredrik Ahlström on 2009-08-25.
;@  Copyright © 2009-2024 Fredrik Ahlström. All rights reserved.
;@
#ifdef __arm__

#include "SN76496.i"

	.global sn76496Reset
	.global sn76496SaveState
	.global sn76496LoadState
	.global sn76496GetStateSize
	.global sn76496Mixer
	.global sn76496W
								;@ These values are for the SMS/GG/MD vdp/sound chip.
	.equ PFEED_SMS,	0x8000		;@ Periodic Noise Feedback
	.equ WFEED_SMS,	0x9000		;@ White Noise Feedback

								;@ These values are for the SN76489/SN76496 sound chip.
	.equ PFEED_SN,	0x4000		;@ Periodic Noise Feedback
	.equ WFEED_SN,	0x6000		;@ White Noise Feedback

								;@ These values are for the NCR 8496 sound chip.
	.equ PFEED_NCR,	0x4000		;@ Periodic Noise Feedback
	.equ WFEED_NCR,	0x4400		;@ White Noise Feedback

#define SN_ADDITION 0x00400000

	.syntax unified
	.arm

#ifdef NDS
	.section .itcm						;@ For the NDS
#elif GBA
	.section .iwram, "ax", %progbits	;@ For the GBA
#else
	.section .text
#endif
	.align 2
;@----------------------------------------------------------------------------
;@ r0 = Mix length.
;@ r1 = Mixerbuffer.
;@ r2 = snptr.
;@ r3 -> r6 = pos+freq.
;@ r7 = currentBits + offset to calculated volumes.
;@ r8 = Noise generator.
;@ r9 = Noise feedback.
;@ lr = Mixer reg.
;@----------------------------------------------------------------------------
sn76496Mixer:				;@ r0=len, r1=dest, r2=snptr
	.type   sn76496Mixer STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{r4-r9,lr}
	ldmia r2,{r3-r9,lr}		;@ Load freq/addr0-3, currentBits, rng, noisefb, attChg
#ifdef SN_UPSHIFT
	mov r0,r0,lsl#SN_UPSHIFT
#endif
	tst lr,#0xff
	blne calculateVolumes
;@----------------------------------------------------------------------------
mixLoop:
	adds r3,r3,#SN_ADDITION
	subcs r3,r3,r3,lsl#16
	eorcs r7,r7,#0x02

	adds r4,r4,#SN_ADDITION
	subcs r4,r4,r4,lsl#16
	eorcs r7,r7,#0x04

	adds r5,r5,#SN_ADDITION
	subcs r5,r5,r5,lsl#16
	eorcs r7,r7,#0x08

	adds r6,r6,#SN_ADDITION		;@ 0x00200000?
	subcs r6,r6,r6,lsl#16
	biccs r7,r7,#0x10
	movscs r8,r8,lsr#1
	eorcs r8,r8,r9
	orrcs r7,r7,#0x10

#ifdef SN_UPSHIFT
	sub r0,r0,#1
	tst r0,#(1<<SN_UPSHIFT)-1
	bne mixLoop
	ldrh lr,[r2,r7]
	cmp r0,#0
#else
	ldrh lr,[r2,r7]
	subs r0,r0,#1
#endif
	strhpl lr,[r1],#2
	bhi mixLoop

	stmia r2,{r3-r8}			;@ Writeback freq,addr,currentBits,rng
	ldmfd sp!,{r4-r9,lr}
	bx lr
;@----------------------------------------------------------------------------

	.section .text
	.align 2
;@----------------------------------------------------------------------------
sn76496Reset:				;@ r0 = chiptype SMS/SN76496, snptr=r1=pointer to struct
	.type   sn76496Reset STT_FUNC
;@----------------------------------------------------------------------------

	cmp r0,#1
	ldr r3,=(WFEED_SMS<<16)+PFEED_SMS
	ldreq r3,=(WFEED_SN<<16)+PFEED_SN
	ldrhi r3,=(WFEED_NCR<<16)+PFEED_NCR

	mov r0,#0
	mov r2,#snSize/4			;@ 60/4=15
rLoop:
	subs r2,r2,#1
	strpl r0,[r1,r2,lsl#2]
	bhi rLoop

	strh r3,[r1,#rng]
	mov r2,r3,lsr#16
	strh r2,[r1,#noiseFB]
	str r3,[r1,#noiseType]
	mov r2,#calculatedVolumes
	str r2,[r1,#currentBits]
	mov r0,#0x8000
	strh r0,[r1,r2]

	bx lr

;@----------------------------------------------------------------------------
sn76496SaveState:			;@ In r0=destination, r1=snptr. Out r0=state size.
	.type   sn76496SaveState STT_FUNC
;@----------------------------------------------------------------------------
	mov r2,#snSize
	stmfd sp!,{r2,lr}

	bl memcpy

	ldmfd sp!,{r0,lr}
	bx lr
;@----------------------------------------------------------------------------
sn76496LoadState:			;@ In r0=snptr, r1=source. Out r0=state size.
	.type   sn76496LoadState STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,lr}

	mov r2,#snSize
	bl memcpy
	ldmfd sp!,{r0,lr}
	mov r1,#1
	strb r1,[r0,#snAttChg]
;@----------------------------------------------------------------------------
sn76496GetStateSize:		;@ Out r0=state size.
	.type   sn76496GetStateSize STT_FUNC
;@----------------------------------------------------------------------------
	mov r0,#snSize
	bx lr

;@----------------------------------------------------------------------------
sn76496W:					;@ r0 = value, r1 = struct-pointer
	.type   sn76496W STT_FUNC
;@----------------------------------------------------------------------------
	tst r0,#0x80
	andne r12,r0,#0x70
	strbne r12,[r1,#snLastReg]
	ldrbeq r12,[r1,#snLastReg]
	movs r12,r12,lsr#5
	add r2,r1,r12,lsl#2
	bcc setFreq
doVolume:
	and r0,r0,#0x0F
	ldrb r12,[r2,#ch0Att]
	eors r12,r12,r0
	strbne r0,[r2,#ch0Att]
	strbne r12,[r1,#snAttChg]
	bx lr

setFreq:
	cmp r12,#3					;@ Noise channel
	beq setNoiseFreq
	tst r0,#0x80
	andeq r0,r0,#0x3F
	movne r0,r0,lsl#4
	strbeq r0,[r2,#ch0Reg+1]
	strbne r0,[r2,#ch0Reg]
	ldrh r0,[r2,#ch0Reg]
	movs r0,r0,lsl#2
	cmp r0,#0x0180				;@ We set any value under 6 to 1 to fix aliasing.
	movmi r0,#0x0040			;@ Value zero is same as 1 on SMS.
	strh r0,[r2,#ch0Frq]

	cmp r12,#2					;@ Ch2
	ldrbeq r2,[r1,#ch3Reg]
	cmpeq r2,#3
	strheq r0,[r1,#ch3Frq]
	bx lr

setNoiseFreq:
	and r2,r0,#3
	strb r2,[r1,#ch3Reg]
	tst r0,#4
	ldr r0,[r1,#noiseType]
	strh r0,[r1,#rng]
	movne r0,r0,lsr#16			;@ White noise
	strh r0,[r1,#noiseFB]
	mov r12,#0x0400				;@ These values sound ok
	mov r12,r12,lsl r2
	cmp r2,#3
	ldrheq r12,[r1,#ch2Frq]
	strh r12,[r1,#ch3Frq]
	bx lr

;@----------------------------------------------------------------------------
calculateVolumes:
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,r1,r3-r6,lr}

	ldrb r3,[r2,#ch0Att]
	ldrb r4,[r2,#ch1Att]
	ldrb r5,[r2,#ch2Att]
	ldrb r6,[r2,#ch3Att]
	adr r1,attenuation
	ldr r3,[r1,r3,lsl#2]
	ldr r4,[r1,r4,lsl#2]
	ldr r5,[r1,r5,lsl#2]
	ldr r6,[r1,r6,lsl#2]

	mov lr,#0x8000
	add r12,r2,#calculatedVolumes
	mov r1,#0x1E
volLoop:
	movs r0,r1,lsl#30
	movmi r0,r3
	addcs r0,r0,r4
	teq r1,r1,lsl#28
	addmi r0,r0,r5
	addcs r0,r0,r6
	eor r0,lr,r0,lsr#2
	strh r0,[r12,r1]
	subs r1,r1,#2
	bne volLoop
	strb r1,[r2,#snAttChg]
	ldmfd sp!,{r0,r1,r3-r6,pc}

;@----------------------------------------------------------------------------
attenuation:				;@ each step * 0.79370053 (-2dB?)
	.long 0xFFFF,0xCB30,0xA145,0x8000,0x6598,0x50A3,0x4000,0x32CC
	.long 0x2851,0x2000,0x1966,0x1428,0x1000,0x0CB3,0x0A14,0x0000
;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
