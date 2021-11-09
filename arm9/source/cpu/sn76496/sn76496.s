#include "sn76496_equ.h"

	.global SN76496_init
	.global SN76496_reset
	.global SN76496_set_mixrate
	.global SN76496_set_frequency
	.global SN76496_mixer
	.global SN76496_w
	.global SN76496_GG_w
								;@ These values are for the SMS/GG/MD vdp sound.
.equ NSEED_SMS,	0x8000			;@ Noise Seed
.equ WFEED_SMS,	0x9000			;@ White Noise Feedback
.equ PFEED_SMS,	0x8000			;@ Periodic Noise Feedback

								;@ These values are for the SN76496 sound chip.
.equ NSEED_SN,	0x4000			;@ Noise Seed
.equ WFEED_SN,	0x6000			;@ White Noise Feedback
.equ PFEED_SN,	0x4000			;@ Periodic Noise Feedback

	.arm
	.align 4
	.section .itcm
;@----------------------------------------------------------------------------
;@ r0  = mixer reg.
;@ r1 -> r4 = pos+freq.
;@ r5  = noise generator.
;@ r6  = noise feedback.
;@ r7  = ch0 volumes.
;@ r8  = ch1 volumes.
;@ r9  = ch2 volumes.
;@ r10 = ch3 volumes.
;@ r11 = mixerbuffer.
;@ r12 = mix length.
;@ lr  = return address.
;@----------------------------------------------------------------------------
mixer:
;@----------------------------------------------------------------------------
mixloop:

	adds r4,r4,r4,lsl#16
	movcss r5,r5,lsr#1
	eorcs r5,r5,r6

	mov r0,#0
	adds r1,r1,r1,lsl#16
	addpl r0,r0,r7

	adds r2,r2,r2,lsl#16
	addpl r0,r0,r8

	adds r3,r3,r3,lsl#16
	addpl r0,r0,r9

	tst r5,#1
	addne r0,r0,r10
	eor r0,r0,#0x00008000
	eor r0,r0,#0x80000000
	str r0,[r11],#4

	subs r12,r12,#1
	bhi mixloop

	bx lr
;@----------------------------------------------------------------------------

	.arm
	.align 4
	.section .text

;@----------------------------------------------------------------------------
SN76496_init:				;@ snptr=r0=pointer to struct, r1=FREQTABLE
;@----------------------------------------------------------------------------
	stmfd sp!,{snptr,lr}
	bl frequency_calculate
	ldmfd sp!,{snptr,lr}
	bx lr
;@----------------------------------------------------------------------------
SN76496_reset:				;@ snptr=r0=pointer to struct, r1=SMS/SN76496
;@----------------------------------------------------------------------------

	cmp r1,#0
	adr r1,SMS_feedback
	addne r1,r1,#8
	ldmia r1,{r2-r3}
	adr r1,Noise_feedback
	str r2,[r1],#8
	str r3,[r1]

	mov r1,#0
	mov r2,#13					;@ 52/4=13
r_loop:
	subs r2,r2,#1
	strpl r1,[snptr,r2,lsl#2]
	bhi r_loop
	mov r1,#0xFF
	strb r1,[snptr,#ggstereo]

	bx lr

;@----------------------------------------------------------------------------
SMS_feedback:
	mov r3,#PFEED_SMS			;@ Periodic noise
	movne r3,#WFEED_SMS			;@ White noise
SN_feedback:
	mov r3,#PFEED_SN			;@ Periodic noise
	movne r3,#WFEED_SN			;@ White noise
;@----------------------------------------------------------------------------
SN76496_set_mixrate:		;@ snptr=r0=pointer to struct, r1 in. 0 = low, 1 = high
;@----------------------------------------------------------------------------
	cmp r1,#0
	moveq r1,#924				;@ low,  18157Hz
	movne r1,#532				;@ high, 31536Hz
	str r1,[snptr,#mixrate]
	moveq r1,#304				;@ low
	movne r1,#528				;@ high
	str r1,[snptr,#mixlength]
	bx lr
;@----------------------------------------------------------------------------
SN76496_set_frequency:		;@ snptr=r0=pointer to struct, r1=frequency of chip.
;@----------------------------------------------------------------------------
	ldr r2,[snptr,#mixrate]
	mul r1,r2,r1
	mov r1,r1,lsr#12
	str r1,[snptr,#freqconv]	;@ Frequency conversion (SN76496freq*mixrate)/4096
	bx lr
;@----------------------------------------------------------------------------
frequency_calculate:		;@ snptr=r0=pointer to struct, r1=FREQTABLE
;@----------------------------------------------------------------------------
	stmfd sp!,{r4-r6,lr}
	str r1,[snptr,#freqtableptr]
	mov r5,r1					;@ Destination
	ldr r6,[snptr,#freqconv]	;@ (sn76496/gba)*4096
	mov r4,#2048
frqloop2:
	mov r0,r6
	mov r1,r4
	swi 0x090000				;@ BIOS Div, r0/r1.
	cmp r4,#7*2
	movmi r0,#0					;@ to remove real high tones.
	subs r4,r4,#2
	strh r0,[r5,r4]
	bhi frqloop2

	ldmfd sp!,{r4-r6,lr}
	bx lr

;@----------------------------------------------------------------------------
SN76496_mixer:				;@ snptr = r0 = struct-pointer
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,r4-r11,lr}

	add r12,snptr,#ch0freq
	ldmia r12,{r1-r11,r12}	;@ load freq,addr,rng, noisefb, vol0-3, ptr & len
    
;@--------------------------
	bl mixer
;@--------------------------
	ldmfd sp!,{r0}
	add r12,snptr,#ch0freq
	stmia r12,{r1-r5}			;@ writeback freq,addr,rng

	ldmfd sp!,{r4-r11,lr}
	bx lr

;@----------------------------------------------------------------------------
SN76496_w:					;@ snptr = r0 = struct-pointer, r1 = value
;@----------------------------------------------------------------------------
	tst r1,#0x80
	andne r2,r1,#0x70
	strneb r2,[snptr,#sn_lastreg]
	ldreqb r2,[snptr,#sn_lastreg]
	movs r2,r2,lsr#5
	bcc SetFreq
DoVolume:
	and r1,r1,#0x0F
	adr r3,Attenuation			;@ This might be possible to optimise.
	add r3,r3,r1
	ldrh r1,[r3,r1]
	orr r1,r1,r1,lsl#16
	add r3,snptr,r2,lsl#2
	str r1,[r3,#ch0volume]
	bx lr

SetFreq:
	cmp r2,#3					;@ noise channel
	beq SetNoiseF
	tst r1,#0x80
	add r3,snptr,r2,lsl#2
	andeq r1,r1,#0x3F
	movne r1,r1,lsl#4
	streqb r1,[r3,#ch0reg+1]
	strneb r1,[r3,#ch0reg]
	ldr r1,[r3,#ch0reg]
	mov r1,r1,lsr#3

	ldr r12,[snptr,#freqtableptr]
	ldrh r1,[r12,r1]
	strh r1,[r3,#ch0freq]
	cmp r2,#2					;@ ch2
	ldreq r3,[snptr,#ch3reg]
	andeq r3,r3,#3
	cmpeq r3,#3
	streqh r1,[snptr,#ch3freq]
	bx lr

SetNoiseF:
	str r1,[snptr,#ch3reg]
	tst r1,#4
Noise_feedback:
	mov r3,#PFEED_SMS			;@ Periodic noise
	str r3,[snptr,#rng]
	movne r3,#WFEED_SMS			;@ White noise
	str r3,[snptr,#noisefb]
	ldr r3,[snptr,#freqconv]
	ands r1,r1,#3
	moveq r2,r3,lsr#5			;@ These values sound ok
	movne r2,r3,lsr#6
	cmp r1,#2
	moveq r2,r3,lsr#7
	ldrhih r2,[snptr,#ch2freq]
	strh r2,[snptr,#ch3freq]
	bx lr

;@----------------------------------------------------------------------------
SN76496_GG_w:
;@----------------------------------------------------------------------------
	strb r1,[snptr,#ggstereo]
	bx lr
;@----------------------------------------------------------------------------

  
Attenuation:
	.hword 0x3FFF,0x32D5,0x2861,0x2013,0x197A,0x143D,0x1013,0x0CC5,0x0A25,0x080E,0x0666,0x0515,0x040A,0x0335,0x028C,0x0000
;@----------------------------------------------------------------------------
