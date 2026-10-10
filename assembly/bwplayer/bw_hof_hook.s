@ 殿堂入りの主人公を「縮めない全身」にするフック（#20・src/bw_hof_pic.c）。

	.text
	.thumb
	.align	2
@ ★ armv4t に固定する（assembly/hoenn/townmap_hook.s の注記と同じ理由）
	.arch	armv4t

@ ================================================================
@ フック先 080F367C（素の Task_Hof_SpawnPlayerPic の中）
@
@   080F3678  BL   CreateTrainerPicSprite     → r0 = スプライトの番号、r5 = taskId
@   080F367C  LDR  r1, =0x030050D0 (gTasks)   ┐
@   080F367E  LSL  r4, r5, #2                 │ ここを潰す（8 バイト）
@   080F3680  ADD  r4, r4, r5                 │
@   080F3682  LSL  r4, r4, #3                 ┘
@   080F3684  ADD  r4, r4, r1
@   080F3686  STRH r0, [r4, #16]              → gTasks[taskId].data[4] = スプライト
@
@ ★ r1 は 080F367C で読み直されるので潰してよい（hooks の 3 列目 = 1）。
@ ★ 戻りに使う r2 は、080F3684 のあと読まれる前に入れ直される（080F3692 MOV r2,#0xD0）。
@ ★ C の関数の番地は +1 を自分で足す（このビルドのリンカは Thumb のビットを立てない）。
@ ================================================================
	.global	BwHofPicHook
	.thumb_func
BwHofPicHook:
	push	{r0}			@ スプライトの番号を守る（C が r0〜r3 を壊す）
	ldr	r1, =(BwHofPlayerPic + 1)
	bl	BwHof_BxR1		@ ★ 遠いので bl では届かない。bx で呼ぶ
	pop	{r0}
	ldr	r1, =0x030050D0		@ 素の 080F367C（gTasks）
	lsl	r4, r5, #2		@ 素の 080F367E
	add	r4, r4, r5		@ 素の 080F3680
	lsl	r4, r4, #3		@ 素の 080F3682
	ldr	r2, =0x080F3684 + 1
	bx	r2

BwHof_BxR1:
	bx	r1

	.pool
