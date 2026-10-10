@ ヒワマキジムの回転門の当たり判定（やりたいこと #2）
@
@   素 `GetCollisionAtCoords`(08062F6C) の **08062F78** に置くフック。
@   ここは
@
@       08062F6C  PUSH {r4-r7,lr} / MOV r7,r10 / MOV r6,r9 / MOV r5,r8
@       08062F74  PUSH {r5,r6,r7}        ← r5/r6/r7 はここで死ぬ
@       08062F76  ADD  r6,r0,#0          ← r6 = 物体
@       08062F78  LSL  r1,r1,#16         ← ★ ここから 8 バイトを潰す
@       08062F7A  LSL  r2,r2,#16
@       08062F7C  LSL  r3,r3,#24
@       08062F7E  LSR  r7,r3,#24
@       08062F80  ...
@
@   ・番地は 4 の倍数なので潰れるのは **8 バイト＝ 4 命令**（6.14 節）
@   ・踏み台が潰すのは **r5**。08062F74 の PUSH で退避済みなので安全（6.15 節）
@   ・r6 には物体（`struct EventObject*`）が入っている。r1/r2/r3 はまだ生の
@     x / y / 向き
@
@   ぶつかるときは素の `MOV r0,#1`（08062F9E）へ飛ぶ。
@   ＝ `COLLISION_IMPASSABLE` を返す道にそのまま乗る。

@ ★ .arch armv4t が必須（CFRU は -march を渡さないので、これが無いと
@   `ldr rX,=小さい値` が Thumb-2 の MOVW になり実機で暴走する）。
	.arch armv4t
	.text
	.align 2
	.thumb

	.global	HoennGateCollisionHook
	.thumb_func
HoennGateCollisionHook:
	push	{r0, r1, r2, r3, r4, lr}	@ 24 バイト（8 の倍数）
	mov	r0, r6				@ 物体
	lsl	r1, r1, #16
	asr	r1, r1, #16			@ x
	lsl	r2, r2, #16
	asr	r2, r2, #16			@ y
	lsl	r3, r3, #24
	lsr	r3, r3, #24			@ 向き
	ldr	r4, =(HoennGateCollisionCheck + 1)
	bl	HoennGateCallR4
	cmp	r0, #0
	pop	{r0, r1, r2, r3, r4}		@ POP は旗を変えない
	pop	{r5}
	mov	lr, r5
	bne	HoennGateCollisionBlocked
	lsl	r1, r1, #16			@ 素の 08062F78
	lsl	r2, r2, #16			@ 素の 08062F7A
	lsl	r3, r3, #24			@ 素の 08062F7C
	lsr	r7, r3, #24			@ 素の 08062F7E
	ldr	r5, =0x08062F81			@ 08062F80 へ戻る
	bx	r5

	.thumb_func
HoennGateCollisionBlocked:
	ldr	r5, =0x08062F9F			@ MOV r0,#1 → 出口
	bx	r5

	.thumb_func
HoennGateCallR4:
	bx	r4
