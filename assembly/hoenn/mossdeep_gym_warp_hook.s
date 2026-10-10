@ トクサネジムの中のワープで、動かした人（回る床の上の石像・トレーナー）が元に戻らないようにする（やりたいこと #2）
@   2026-09-29 ユーザー「トクサネジムのギミックがワープで元に戻ってしまうので他の場所に戻れなくなる詰み状態になる」
@
@   エメラルドはジムの中のワープ床で DoMossdeepGymWarp を使い、SKIP_OBJECT_EVENT_LOAD で
@   地図の人の置き場（gSaveBlock1->objectEventTemplates）を読み直さない（pokeemerald src/field_screen_effect.c・
@   src/overworld.c LoadMapFromWarp）。回る床の仕掛けは動かした先をこの置き場に書く（src/hoenn_gym_puzzles.c）ので、
@   読み直すと元の位置に戻る。素 FRLG の LoadMapFromWarp にはこの印が無く、必ず読み直す（pokefirered src/overworld.c:793）。
@
@   素 LoadObjEventTemplatesFromHeader（**08054828**・呼び元は 0805514C / 080551E6 / 0805526E の 3 か所で、どれも入口を指す）の
@   入口に置く。ここは
@
@       08054828  PUSH {r4-r7,lr}        ← ★ ここから 8 バイトを潰す（4 の倍数なので 4 命令・6.14 節）
@       0805482A  MOV  r7,r10
@       0805482C  MOV  r6,r9
@       0805482E  MOV  r5,r8
@       08054830  PUSH {r5,r6,r7}        ← 素の続き
@       08054834  MOV  r0,#0             ← r0 は入口では使っていない（引数なし）
@
@   ・踏み台が潰すのは r0（引数なし・08054834 で上書きされる）
@   ・HoennMossdeepGym_KeepObjTemplates が TRUE（トクサネジム → トクサネジムのワープ）なら何もしないで戻る。
@     それ以外は潰した 4 命令をやり直して 08054830 へ

@ ★ .arch armv4t が必須（gates_hook.s と同じ理由）
	.arch armv4t
	.text
	.align 2
	.thumb

	.global	HoennMossdeepGym_LoadObjTemplates
	.thumb_func
HoennMossdeepGym_LoadObjTemplates:
	push	{lr}
	ldr	r1, =(HoennMossdeepGym_KeepObjTemplates + 1)
	bl	HoennMossdeepGymCallR1
	pop	{r1}
	mov	lr, r1
	cmp	r0, #0
	beq	HoennMossdeepGym_LoadVanilla
	bx	lr

	.thumb_func
HoennMossdeepGym_LoadVanilla:
	push	{r4, r5, r6, r7, lr}		@ 素の 08054828
	mov	r7, r10				@ 素の 0805482A
	mov	r6, r9				@ 素の 0805482C
	mov	r5, r8				@ 素の 0805482E
	ldr	r0, =0x08054831			@ 08054830 へ戻る
	bx	r0

	.thumb_func
HoennMossdeepGymCallR1:
	bx	r1

	.pool
