@ 踏むとメタタイルが変わる仕掛けのフック。
@ tools/rom/build_hoenn_steps.py の説明と src/hoenn_steps.c を参照。
@
@ ★ .arch armv4t が必須（CFRU は -march を渡さないので、これが無いと
@   `ldr rX,=小さい値` が Thumb-2 の MOVW になり実機で暴走する）。
	.arch armv4t
	.text
	.thumb

.equ PerStepBody, 0x0806E00D		@ 潰した 4 命令の続き（0806E00C）

@ ---- 素 FRLG の「1 歩ごとのコールバック」を回すタスク（0806E004）----
@
@   ★★ 2026-09-18: 最初は DoGroundEffects_OnBeginStep（080680FC）に
@     置いていたが、**あれは動いている間しか呼ばれない**。
@     そのせいで丸太が途中の段で止まり、浮き上がらなくなった（実機）。
@     こちらは gTasks に積まれたタスクなので**毎フレーム**呼ばれる。
@
@   素の 8 バイトは
@       PUSH {lr} / LSL r0,#24 / LSR r0,#24 / LDR r2,=0x030050D0
@   ちょうど 8 バイトなので、フック（4 バイト + 飛び先 4 バイト）で
@   きれいに置き換わる。潰した 4 命令はこちらでやり直す。
@   ★ LDR は pc 相対なので、こちらでリテラルを作り直す。
@ ★ フックが潰すレジスタは 3。r0 は引数（taskId）で生きている。
	.global	HoennStepHook
	.thumb_func
HoennStepHook:
	push	{r0, lr}
	ldr	r3, =(HoennStepTick + 1)
	bl	HoennStepCall
	pop	{r0, r3}
	mov	lr, r3
	push	{lr}			@ 素の 0806E004
	lsl	r0, r0, #24		@ 素の 0806E006
	lsr	r0, r0, #24		@ 素の 0806E008
	ldr	r2, =0x030050D0		@ 素の 0806E00A
	ldr	r3, =PerStepBody
	bx	r3

HoennStepCall:
	bx	r3
