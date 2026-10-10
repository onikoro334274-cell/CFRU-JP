@ タイトル画面（#45・2026-10-05）の差し込み口 2 つ。鳴き声をリザードンからガブリアスに／板の層を出す時期。
@ 素（SetTitleScreenScene_Cry・08078982〜）: movs r0,#6 / movs r1,#0 / bl PlayCry1 / ldrb r0,[r4,#12] → 0807898C
@ hooks で 08078984 via r1（4 の倍数・潰すのは 08078984〜0807898B）。この間へ飛んでくる分岐は無い。

.text
.thumb
.align 2

.global HackTitleCryHook
.thumb_func
HackTitleCryHook:
	@ SPECIES_GARCHOMP 0x1F2（DPE・include/species.h）= 0xF9 << 1。
	@ ★ ldr r0,=0x1F2 と書くと as が Thumb-2 の mov.w（ARMv4T に無い命令）に組むので使わない
	mov r0, #0xF9
	lsl r0, r0, #1
	mov r1, #0
	ldr r2, =PlayCry1
	bl HackTitleCry_CallR2
	ldrb r0, [r4, #12]
	ldr r1, =0x0807898C | 1
	bx r1

HackTitleCry_CallR2:
	bx r2
.pool

@ 板の層（BG2・PUSH START BUTTON）はパレット 15 が黒のまま出ていると、出始めの演出でガブリアスの下を黒く覆う。
@ 最初は隠し（bytereplacement 0807844C ShowBg → HideBg）、スライドでパレット 15 を戻す所で出す。
@ 素（Task_TitleScreen_SlideWin0 の case 3・08078B2A〜）: r0 = 0x8000 / r1 = 0 / 08078B30 movs r2,#0 /
@   bl BlendPalettes / 08078B36 movs r1,#240 → 08078B38 lsls r1,r1,#4。hooks で 08078B30 via r3（潰すのは 08078B30〜08078B37）
.align 2
.global HackTitleShowTextHook
.thumb_func
HackTitleShowTextHook:
	mov r2, #0
	ldr r3, =BlendPalettes
	bl HackTitle_CallR3
	mov r0, #2
	ldr r3, =ShowBg
	bl HackTitle_CallR3
	mov r1, #240
	ldr r3, =0x08078B38 | 1
	bx r3

HackTitle_CallR3:
	bx r3
.pool
