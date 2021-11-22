;@ AY-3-8910 / YM2149 sound chip emulator (for MSX).
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

.equ NSEED,	0x00001				;@ Noise Seed
.equ WFEED,	0x12000				;@ White Noise Feedback, according to MAME.

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
;@ r2 -> r5 = pos+freq.
;@ r6  = noise generator.
;@ r7 = envelope freq
;@ r8 = envelope addr, ch disable, envelope type.
;@ r9 = pointer to attenuation table.
;@ r10= calculatedVolumes.
;@ r11= mixer reg/scrap
;@ r12= ayptr
;@ lr = envelope volume
;@----------------------------------------------------------------------------
ay38910Mixer:				;@ r0=len, r1=dest, ayptr=r12=pointer to struct
    .type   ay38910Mixer STT_FUNC
    mov r12,r2
;@----------------------------------------------------------------------------
	stmfd sp!,{r4-r11,lr}
	ldmia ayptr,{r2-r10}			;@ Load freq,addr,rng
	tst r10,#0xff
	blne calculateVolumes
	add r10,ayptr,#ayCalculatedVolumes
;@----------------------------------------------------------------------------
mixLoop:
	adds r7,r7,#0x00010000
	subcs r7,r7,r7,lsl#16
	addcs r8,r8,#0x08000000
	tst r8,r8,lsl#15				;@ Envelope Hold
	bicmi r8,r8,#0x78000000
	and lr,r8,#0x78000000
	and r11,r8,r8,lsl#14			;@ Envelope Alternate (allready flipped from Hold)
	eors r11,r11,r8,lsl#13			;@ Envelope Attack
	eorpl lr,lr,#0x78000000

	ldr lr,[r9,lr,lsr#25]

	adds r2,r2,#0x00200000
	subcs r2,r2,r2,lsl#20
	eorcs r8,r8,#0x01				;@ Channel A
	adds r3,r3,#0x00200000
	subcs r3,r3,r3,lsl#20
	eorcs r8,r8,#0x02				;@ Channel B
	adds r4,r4,#0x00200000
	subcs r4,r4,r4,lsl#20
	eorcs r8,r8,#0x04				;@ Channel C
	adds r5,r5,#0x00200000
	subcs r5,r5,r5,lsl#21
	orrcs r8,r8,#0x00000038			;@ Clear noise channel.
	movscs r6,r6,lsr#1
	eorcs r6,r6,#WFEED
	eorcs r8,r8,#0x00000038			;@ Noise channel.

	orr r11,r8,r8,lsr#8				;@ Channels disable.
	and r11,r11,r11,lsr#3			;@ Noise disable.
	and r11,r11,#7
	mov r11,r11,lsl#1
	ldrh r11,[r10,r11]
	add r11,r11,lr

	subs r0,r0,#1
	strhpl r11,[r1],#2
	bhi mixLoop

	stmia ayptr,{r2-r8}				;@ Write back freq,addr,rng
	ldmfd sp!,{r4-r11,lr}
	bx lr

#ifdef NDS
	.section .dtcm					;@ For the NDS ARM9
	.align 2
#endif
;@----------------------------------------------------------------------------
attenuation0:
	.long 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000
attenuation:						;@ each step * 0.70710678 (-3dB?)
	.long 0x0000, 0x00AB, 0x00F1, 0x0155, 0x01E3, 0x02AB, 0x03C5, 0x0555, 0x078B, 0x0AAB, 0x0F16, 0x1555, 0x1E2B, 0x22AB, 0x2857, 0x3555
attenuation2:
	.long 0x0000, 0x0155, 0x01E3, 0x02AB, 0x03C5, 0x0555, 0x078B, 0x0AAB, 0x0F16, 0x1555, 0x1E2B, 0x2AAB, 0x3C57, 0x5555, 0x78AE, 0xAAAA
attenuation3:
	.long 0x0000, 0x0200, 0x02D4, 0x0400, 0x05A8, 0x0800, 0x0B50, 0x1000, 0x16A1, 0x2000, 0x2D41, 0x4000, 0x5A82, 0x8000, 0xB505, 0xFFFF
;@----------------------------------------------------------------------------

	.section .text
	.align 2
;@----------------------------------------------------------------------------
ay38910Reset:				;@ ayptr=r12=pointer to struct
    .type   ay38910Reset STT_FUNC
    mov r12,r0
;@----------------------------------------------------------------------------
	ldr r0,=attenuation0
	str r0,[ayptr,#ayEnvVolumePtr]

	adr r0,dummyOutFunc
	str r0,[ayptr,#ayPortAOutFptr]
	str r0,[ayptr,#ayPortBOutFptr]
	ldr r0,=portAInDummy
	str r0,[ayptr,#ayPortAInFptr]
	ldr r0,=portBInDummy
	str r0,[ayptr,#ayPortBInFptr]
    
	mov r0,#0x8000
	strh r0,[ayptr,#ayCalculatedVolumes]

	mov r0,#0xFF
	strb r0,[ayptr,#ayPortAIn]
	strb r0,[ayptr,#ayPortBIn]
	mov r0,#NSEED
	str r0,[ayptr,#ayRng]
    bx lr

dummyOutFunc:
	bx lr
;@----------------------------------------------------------------------------
updateAllRegisters:
;@----------------------------------------------------------------------------
	stmfd sp!,{lr}
	mov r3,#0
regLoop:
	mov r0,r3
	bl ay38910IndexW
	add r1,ayptr,#ayRegs
	ldrb r0,[r1,r3]
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
	mov ayptr,r4
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
ay38910IndexW:
    .type   ay38910IndexW STT_FUNC
    mov r12,r1
	tst r0,#0xF0
	strbeq r0,[ayptr,#ayRegIndex]
	bx lr

;@----------------------------------------------------------------------------
ay38910DataW:
    .type   ay38910DataW STT_FUNC
    mov r12,r1
	ldrb r1,[ayptr,#ayRegIndex]
	adr r2,regMask
	ldrb r2,[r2,r1]
	and r0,r0,r2
	add r2,ayptr,#ayRegs
	strb r0,[r2,r1]
	ldr pc,[pc,r1,lsl#2]
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
ay38910DataR:
    .type   ay38910DataR STT_FUNC
    mov r12,r0
	ldrb r1,[ayptr,#ayRegIndex]
	cmp r1,#0xE
	beq ay38910RegER
	bhi ay38910RegFR
	add r0,ayptr,#ayRegs
	ldrb r0,[r0,r1]
	bx lr
;@----------------------------------------------------------------------------
regMask:
	.byte 0xFF,0x0F,0xFF,0x0F,0xFF,0x0F,0x1F,0xFF, 0x1F,0x1F,0x1F,0xFF,0xFF,0x0F,0xFF,0xFF
;@----------------------------------------------------------------------------
ay38910Reg1W:
ay38910Reg3W:
ay38910Reg5W:
	bic r1,r1,#1
;@----------------------------------------------------------------------------
ay38910Reg0W:
ay38910Reg2W:
ay38910Reg4W:
	ldrh r0,[r2,r1]
	cmp r0,#0
	moveq r0,#1
	add r2,ayptr,r1,lsl#1
	strh r0,[r2,#ayCh0Freq]
	bx lr
;@----------------------------------------------------------------------------
ay38910Reg6W:
	cmp r0,#0
	moveq r0,#1
	strh r0,[ayptr,#ayCh3Freq]
//	mov r0,#NSEED
//	str r0,[ayptr,#ayRng]
	bx lr
;@----------------------------------------------------------------------------
ay38910Reg7W:
	strb r0,[ayptr,#ayChDisable]
	bx lr
;@----------------------------------------------------------------------------
ay38910Reg8W:
ay38910Reg9W:
ay38910RegAW:
	strb r1,[ayptr,#ayAttChg]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegBW:
ay38910RegCW:
	ldrb r0,[ayptr,#ayRegs+0xB]
	ldrb r1,[ayptr,#ayRegs+0xC]
	orrs r0,r0,r1,lsl#8
	moveq r0,#1
	strh r0,[ayptr,#ayEnvFreq]
//	mov r0,#0
//	strb r0,[ayptr,#ayEnvAddr]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegDW:
	cmp r0,#4
	movmi r0,#9
	cmp r0,#8
	movmi r0,#0xF
	tst r0,#1					;@ ALT ^= Hold
	eorne r0,r0,#2
	strh r0,[ayptr,#ayEnvType]	;@ Also clear Envelope addr
	bx lr
;@----------------------------------------------------------------------------
ay38910RegEW:
	strb r0,[ayptr,#ayPortAOut]
	ldrb r1,[ayptr,#ayChDisable]
	tst r1,#0x40
	ldrne pc,[ayptr,#ayPortAOutFptr]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegFW:
	strb r0,[ayptr,#ayPortBOut]
	ldrb r1,[ayptr,#ayChDisable]
	tst r1,#0x80
	ldrne pc,[ayptr,#ayPortBOutFptr]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegER:
	ldrb r1,[ayptr,#ayChDisable]
	tst r1,#0x40
	ldrbne r0,[ayptr,#ayPortAOut]
	bxne lr
	ldr pc,[ayptr,#ayPortAInFptr]
;@-------------------------------
portAInDummy:
	ldrb r0,[ayptr,#ayPortAIn]
	bx lr
;@----------------------------------------------------------------------------
ay38910RegFR:
	ldrb r1,[ayptr,#ayChDisable]
	tst r1,#0x80
	ldrbne r0,[ayptr,#ayPortBOut]
	bxne lr
	ldr pc,[ayptr,#ayPortBInFptr]
;@-------------------------------
portBInDummy:
	ldrb r0,[ayptr,#ayPortBIn]
	bx lr
;@----------------------------------------------------------------------------
calculateVolumes:
;@----------------------------------------------------------------------------
	stmfd sp!,{r0-r5,lr}

	mov r2,#0					;@ Used to calculate how many channels use the envelope.
	ldrb r0,[ayptr,#ayRegs+0x8]
	ands r3,r0,#0x10
	andeq r3,r0,#0xF
	addne r2,r2,#1
	ldrb r0,[ayptr,#ayRegs+0x9]
	ands r4,r0,#0x10
	andeq r4,r0,#0xF
	addne r2,r2,#1
	ldrb r0,[ayptr,#ayRegs+0xA]
	ands r5,r0,#0x10
	andeq r5,r0,#0xF
	addne r2,r2,#1

	ldr r1,=attenuation
	sub r9,r1,#0x40				;@ Point to attenutation0
	add r9,r9,r2,lsl#6
	str r9,[ayptr,#ayEnvVolumePtr]
	ldr r3,[r1,r3,lsl#2]
	ldr r4,[r1,r4,lsl#2]
	ldr r5,[r1,r5,lsl#2]

	add r2,ayptr,#ayCalculatedVolumes
	mov r1,#0x0E
volLoop:
	ands r0,r1,#0x02
	movne r0,r3
	tst r1,#0x04
	addne r0,r0,r4
	tst r1,#0x08
	addne r0,r0,r5
	eor r0,r0,#0x8000
	strh r0,[r2,r1]
	subs r1,r1,#2
	bne volLoop
	strb r1,[ayptr,#ayAttChg]
	ldmfd sp!,{r0-r5,pc}

;@----------------------------------------------------------------------------
	.end
#endif // #ifdef __arm__
