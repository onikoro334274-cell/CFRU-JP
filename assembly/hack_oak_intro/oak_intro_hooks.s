@ はじめのオーキド博士の話の鳴き声を ニドラン♀ → フカマル に（2026-10-08・絵は src/hack_oak_intro.c）。
@ 素（Task_OakSpeech_ の 0813034C〜）: movs r0,#29 / movs r1,#0 / bl 08071594（PlayCry1）/ 08130354 add sp,#16 → 08130354。
@ hooks で 0813034C via r1（4 の倍数・潰すのは 0813034C〜08130353）。r1 はここで決める → 空いている。この間へ飛んでくる分岐は無い。

.text
.thumb
.align 2

.global HackOakIntroCryHook
.thumb_func
HackOakIntroCryHook:
	@ SPECIES_GIBLE 0x1F0（DPE・include/species.h）= 0xF8 << 1。ldr r0,=小さい定数 は使わない（6.63）
	mov r0, #0xF8
	lsl r0, r0, #1
	mov r1, #0
	ldr r2, =PlayCry1
	bl HackOakIntroCry_CallR2
	ldr r1, =0x08130354 | 1
	bx r1

HackOakIntroCry_CallR2:
	bx r2
.pool
