;@
;@  AY38910.s
;@  AY-3-8910 / YM2149 sound chip emulator for arm32.
;@
;@  Created by Fredrik Ahlström on 2006-03-07.
;@  Copyright © 2006-2021 Fredrik Ahlström. All rights reserved.
;@
#ifdef __arm__

#include "AY38910.i"

	.global ay38910Reset
	.global ay38910SaveState
	.global ay38910LoadState
	.global ay38910GetStateSize
	.global ay38910Mixer
	.global ay38910IndexW
	.global ay38910DataW
	.global ay38910DataR

.equ NSEED,	0x10000				;@ Noise Seed
.equ WFEED,	0x12000				;@ White Noise Feedback, according to MAME.
.equ WFEED3, 0x14000			;@ White Noise Feedback for AY-3-8930, according to MAME.

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
;@ r0  = mix length.
;@ r1  = mixerbuffer.
;@ r2  = ayptr
;@ r3 -> r6 = pos+freq.
;@ r7  = noise generator.
;@ r8  = envelope freq
;@ r9  = envelope addr, ch disable, envelope type.
;@ r10 = pointer to attenuation table.
;@ r11 = calculatedVolumes.
;@ r12 = mixer reg/scrap
;@ lr  = envelope volume
;@----------------------------------------------------------------------------
ay38910Mixer:				;@ r0=len, r1=dest, ayptr=r2=pointer to struct
;@----------------------------------------------------------------------------
	stmfd sp!,{r4-r11,lr}
	ldmia r2,{r3-r11}			;@ Load freq,addr,rng
	tst r11,#0xff
	blne calculateVolumes
	add r2,r2,#ayCalculatedVolumes	;@ Change r2 to ayCalculatedVolumes ptr
;@----------------------------------------------------------------------------
mixLoop:
	adds r8,r8,#0x00010000
	subcs r8,r8,r8,lsl#16
	addcs r9,r9,#0x08000000
	tst r9,r9,lsl#15				;@ Envelope Hold
	bicmi r9,r9,#0x78000000
	and lr,r9,#0x78000000
	and r12,r9,r9,lsl#14			;@ Envelope Alternate (allready flipped from Hold)
	eors r12,r12,r9,lsl#13			;@ Envelope Attack
	eorpl lr,lr,#0x78000000

	adds r3,r3,#0x00100000
	subcs r3,r3,r3,lsl#20
	eorcs r9,r9,#0x02				;@ Channel A
	adds r4,r4,#0x00100000
	subcs r4,r4,r4,lsl#20
	eorcs r9,r9,#0x04				;@ Channel B
	adds r5,r5,#0x00100000
	subcs r5,r5,r5,lsl#20
	eorcs r9,r9,#0x08				;@ Channel C
	adds r6,r6,#0x00800000
	subcs r6,r6,r6,lsl#27
	orrcs r9,r9,#0x00000070			;@ Clear noise channel.
	movscs r7,r7,lsr#1
	eorcs r7,r7,#WFEED
	eorcs r9,r9,#0x00000070			;@ Noise channel.

	orr r12,r9,r9,lsr#9				;@ Channels disable.
	and r12,r12,r12,lsr#3			;@ Noise disable.
	and r12,r12,#0xE
	ldrh r11,[r2,r12]

	ands r12,r12,r9,lsr#6			;@ Check if any channels use envelope
	ldrne lr,[r10,lr,lsr#25]
	tst r12,#8
	addne r11,r11,lr
	tst r12,#4
	addne r11,r11,lr
	tst r12,#2
	addne r11,r11,lr

	subs r0,r0,#1
	strhpl r11,[r1],#2
	bhi mixLoop

	sub r2,r2,#ayCalculatedVolumes	;@ Restore r2/ayptr
	stmia r2,{r3-r9}				;@ Write back freq,addr,rng
	ldmfd sp!,{r4-r11,lr}
	bx lr

#ifdef NDS
	.section .dtcm					;@ For the NDS ARM9
	.align 2
#endif
;@----------------------------------------------------------------------------
attenuation:						;@ each step * 0.70710678 (-3dB?)
	.long 0x0000, 0x00AB, 0x00F1, 0x0155, 0x01E3, 0x02AB, 0x03C5, 0x0555, 0x078B, 0x0AAB, 0x0F16, 0x1555, 0x1E2B, 0x2AAB, 0x3C57, 0x5555
	.long 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
;@----------------------------------------------------------------------------

	.section .text
	.align 2
;@----------------------------------------------------------------------------
ay38910Reset:				;@ ayptr=r0=pointer to struct
	.type   ay38910Reset STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}

	mov r1,r0
	mov r0,#0
	mov r2,#aySize/4			;@ Clear AY38910 state
rLoop:
	subs r2,r2,#1
	strpl r0,[r1,r2,lsl#2]
	bhi rLoop

	bl updateAllRegisters

	ldr r0,=attenuation
	str r0,[r1,#ayEnvVolumePtr]
	adr r0,dummyOutFunc
	str r0,[r1,#ayPortAOutFptr]
	str r0,[r1,#ayPortBOutFptr]
	ldr r0,=portAInDummy
	str r0,[r1,#ayPortAInFptr]
	ldr r0,=portBInDummy
	str r0,[r1,#ayPortBInFptr]
	mov r0,#0x8000
	strh r0,[r1,#ayCalculatedVolumes]

	mov r0,#0xFF
	strb r0,[r1,#ayPortAIn]
	strb r0,[r1,#ayPortBIn]
	mov r0,#NSEED
	str r0,[r1,#ayRng]

	ldmfd sp!,{lr}
dummyOutFunc:
	bx lr
;@----------------------------------------------------------------------------
updateAllRegisters:			;@ In r1=ayptr
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}
	mov r3,#0
regLoop:
	mov r0,r3
	bl ay38910IndexW
	add r2,r1,#ayRegs
	ldrb r0,[r2,r3]
	bl ay38910DataW
	add r3,r3,#1
	cmp r3,#0x10
	bne regLoop
	ldmfd sp!,{pc}
;@----------------------------------------------------------------------------
ay38910SaveState:			;@ In r0=destination, r1=ayptr. Out r0=state size.
	.type   ay38910SaveState STT_FUNC
;@----------------------------------------------------------------------------
	mov r2,#0x10
	stmfd sp!,{r2,lr}
	add r1,r1,#ayRegs
	bl memcpy
	ldmfd sp!,{r0,lr}
	bx lr
;@----------------------------------------------------------------------------
ay38910LoadState:			;@ In r0=ayptr, r1=source. Out r0=state size.
	.type   ay38910LoadState STT_FUNC
;@----------------------------------------------------------------------------
	stmfd sp!,{r4,lr}
	mov r4,r0				;@ Store ayptr (r0)
	add r0,r0,#ayRegs
	mov r2,#0x10
	bl memcpy
	mov r1,r4
	bl updateAllRegisters
	ldmfd sp!,{r4,lr}
;@----------------------------------------------------------------------------
ay38910GetStateSize:		;@ Out r0=state size.
	.type   ay38910GetStateSize STT_FUNC
;@----------------------------------------------------------------------------
	mov r0,#0x10
	bx lr
;@----------------------------------------------------------------------------
#ifdef GBA
	.section .ewram,"ax"
	.align 2
#endif
;@----------------------------------------------------------------------------
ay38910IndexW:			;@ In r0=value, r1=ayptr
	.type   ay38910IndexW STT_FUNC
;@----------------------------------------------------------------------------
	tst r0,#0xF0
	strbeq r0,[r1,#ayRegIndex]
	bx lr
;@----------------------------------------------------------------------------
ay38910DataW:			;@ In r0=value, r1=ayptr
	.type   ay38910DataW STT_FUNC
;@----------------------------------------------------------------------------
	ldrb r2,[r1,#ayRegIndex]
	adr r12,regMask
	ldrb r12,[r12,r2]
	and r0,r0,r12
	add r12,r1,#ayRegs
	strb r0,[r12,r2]
	ldr pc,[pc,r2,lsl#2]
	.long 0
ayTable:
	.long ay38910Reg0W
	.long ay38910Reg1W
	.long ay38910Reg2W
	.long ay38910Reg3W
	.long ay38910Reg4W
	.long ay38910Reg5W
	.long ay38910Reg6W
	.long ay38910Reg7W
	.long ay38910Reg8W
	.long ay38910Reg9W
	.long ay38910RegAW
	.long ay38910RegBW
	.long ay38910RegCW
	.long ay38910RegDW
	.long ay38910RegEW
	.long ay38910RegFW
;@----------------------------------------------------------------------------
ay38910DataR:			;@ In r0=ayptr
	.type   ay38910DataR STT_FUNC
;@----------------------------------------------------------------------------
	ldrb r1,[r0,#ayRegIndex]
	cmp r1,#0xE
	beq ay38910RegER
	bhi ay38910RegFR
	add r1,r1,#ayRegs
	ldrb r0,[r0,r1]
	bx lr
;@----------------------------------------------------------------------------
regMask:
	.byte 0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0x1F,0xFF, 0x1F,0x1F,0x1F,0xFF,0xFF,0x0F,0xFF,0xFF
;@----------------------------------------------------------------------------
ay38910Reg1W:			;@ Frequency coarse
ay38910Reg3W:
ay38910Reg5W:
	bic r2,r2,#1
;@----------------------------------------------------------------------------
ay38910Reg0W:			;@ Frequency fine
ay38910Reg2W:
ay38910Reg4W:
	ldrh r0,[r12,r2]
	cmp r0,#0
	moveq r0,#1
	add r12,r1,r2,lsl#1
	strh r0,[r12,#ayCh0Freq]
	bx lr
;@----------------------------------------------------------------------------
ay38910Reg6W:			;@ Frequency coarse noise
	cmp r0,#0
	moveq r0,#1
	strh r0,[r1,#ayCh3Freq]
	bx lr
;@----------------------------------------------------------------------------
ay38910Reg7W:			;@ Channel disable
	ldrb r2,[r1,#ayChDisable]
	and r2,r2,#3
	orr r0,r2,r0,lsl#2
	strb r0,[r1,#ayChDisable]
	bx lr
;@----------------------------------------------------------------------------
ay38910Reg8W:			;@ Attenuation
ay38910Reg9W:
ay38910RegAW:
	strb r2,[r1,#ayAttChg]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegCW:			;@ Envelope frequency
	ldrb r0,[r1,#ayRegs+0xB]
ay38910RegBW:
	ldrb r2,[r1,#ayRegs+0xC]
	orrs r0,r0,r2,lsl#8
	moveq r0,#1
	strh r0,[r1,#ayEnvFreq]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegDW:			;@ Envelope type
	cmp r0,#4
	movmi r0,#9
	cmp r0,#8
	movmi r0,#0xF
	tst r0,#1					;@ ALT ^= Hold
	eorne r0,r0,#2
	strh r0,[r1,#ayEnvType]		;@ Also clear Envelope addr
	bx lr
;@----------------------------------------------------------------------------
ay38910RegEW:
	strb r0,[r1,#ayPortAOut]
	ldrb r2,[r1,#ayRegs+7]		;@ ayChDisable
	tst r2,#0x40
	ldrne pc,[r1,#ayPortAOutFptr]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegFW:
	strb r0,[r1,#ayPortBOut]
	ldrb r2,[r1,#ayRegs+7]		;@ ayChDisable
	tst r2,#0x80
	ldrne pc,[r1,#ayPortBOutFptr]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegER:
	ldrb r1,[r0,#ayRegs+7]		;@ ayChDisable
	tst r1,#0x40
	ldrbne r0,[r0,#ayPortAOut]
	bxne lr
	ldr pc,[r0,#ayPortAInFptr]
;@-------------------------------
portAInDummy:
	ldrb r0,[r0,#ayPortAIn]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegFR:
	ldrb r1,[r0,#ayRegs+7]		;@ ayChDisable
	tst r1,#0x80
	ldrbne r0,[r0,#ayPortBOut]
	bxne lr
	ldr pc,[r0,#ayPortBInFptr]
;@-------------------------------
portBInDummy:
	ldrb r0,[r0,#ayPortBIn]
	bx lr
;@----------------------------------------------------------------------------
calculateVolumes:			;@ r2 = ayptr, r10 = attenuation
;@----------------------------------------------------------------------------
	stmfd sp!,{r0,r1,r3-r6,lr}

	bic r9,r9,#0x0380			;@ Bits used to show which channels use the envelope.
	ldrb r0,[r2,#ayRegs+0x8]
	movs r3,r0,lsl#27
	ldrne r3,[r10,r3,lsr#25]
	orrmi r9,r9,#0x0080
	ldrb r0,[r2,#ayRegs+0x9]
	movs r4,r0,lsl#27
	ldrne r4,[r10,r4,lsr#25]
	orrmi r9,r9,#0x0100
	ldrb r0,[r2,#ayRegs+0xA]
	movs r5,r0,lsl#27
	ldrne r5,[r10,r5,lsr#25]
	orrmi r9,r9,#0x0200

	add r6,r2,#ayCalculatedVolumes
	mov r1,#0x0E
volLoop:
	ands r0,r1,#0x02
	movne r0,r3
	tst r1,#0x04
	addne r0,r0,r4
	tst r1,#0x08
	addne r0,r0,r5
	eor r0,r0,#0x8000
	strh r0,[r6,r1]
	subs r1,r1,#2
	bne volLoop
	strb r1,[r2,#ayAttChg]
	ldmfd sp!,{r0,r1,r3-r6,pc}

;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
