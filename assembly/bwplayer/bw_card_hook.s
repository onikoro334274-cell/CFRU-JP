@ トレーナーカードの主人公を「縮めない全身」にするフック（#20・src/bw_card_pic.c）。

	.text
	.thumb
	.align	2
@ ★ armv4t に固定する（assembly/hoenn/townmap_hook.s の注記と同じ理由）
	.arch	armv4t

@ ================================================================
@ フック先 0810CE3C（素の CreateTrainerCardTrainerPicSprite の中）
@
@   0810CE3C  BL   CreateTrainerCardSprite (0810CB98)   ← 引数 9 個はそろっている
@   0810CE40  LSL  r0, r0, #16
@   0810CE42  LSR  r0, r0, #16
@   0810CE44  ADD  sp, #20 / POP {r4,r5} / POP {r1} / BX r1
@
@ ★ 潰す 8 バイトは上の 3 命令。自分でやり直して 0810CE44 へ戻る。
@ ★ r4 はこのあと POP で戻されるだけなので潰してよい（hooks の 3 列目 = 4）。
@ ★ スタックはいじらない。5 個目からの引数は [sp] にそのまま積まれている。
@ ================================================================
	.global	BwCardPicHook
	.thumb_func
BwCardPicHook:
	ldr	r4, =(BwCreateTrainerCardSprite + 1)	@ ★ +1 は自分で足す（steps_hook.s と同じ。リンカは Thumb のビットを立てない）
	bl	BwCardPic_BxR4		@ ★ 遠いので bl では届かない。bx で呼ぶ
	lsl	r0, r0, #16
	lsr	r0, r0, #16
	ldr	r4, =0x0810CE44 + 1
	bx	r4

BwCardPic_BxR4:
	bx	r4

	.pool
