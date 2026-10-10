@ 主人公の姿を切り替えたときのパレットの枠を動的 OW パレットに合わせるフック（#20・src/bw_player_pal.c）。

	.text
	.thumb
	.align	2
@ ★ armv4t に固定する（assembly/hoenn/townmap_hook.s の注記と同じ理由）
	.arch	armv4t

@ ================================================================
@ フック先 0805EA40（素の ObjectEventSetGraphicsId の中・paletteNum を決めたあと）
@
@   0805EA3C  BL   SetSpritePosToMapCoords (080633DC)
@   0805EA40  LDRH r0, [r5, #8]      ← ここから 8 バイト（4 命令）を潰す
@   0805EA42  LSL  r0, r0, #16
@   0805EA44  ASR  r0, r0, #17
@   0805EA46  NEG  r0, r0
@   0805EA48  ADD  r1, r4, #0        ← ここへ戻る
@
@ ★ r4 = スプライト / r5 = 絵の情報 / r6 = 物体（関数の頭で決まり、最後まで変わらない）。
@ ★ r1 は 0805EA48 で書き直されるので潰してよい（hooks の 3 列目 = 1）。
@ ★ r0〜r3 は直前の BL で壊れたあとなので、C を呼んで壊してよい。r4〜r8 は C が守る。
@ ================================================================
	.global	BwPlayerPalHook
	.thumb_func
BwPlayerPalHook:
	mov	r0, r4
	mov	r1, r5
	ldr	r3, =(BwFixSetGraphicsIdPalette + 1)	@ ★ +1 は自分で足す（リンカは Thumb のビットを立てない）
	bl	BwPlayerPal_BxR3
	ldrh	r0, [r5, #8]
	lsl	r0, r0, #16
	asr	r0, r0, #17
	neg	r0, r0
	ldr	r1, =0x0805EA48 + 1
	bx	r1

BwPlayerPal_BxR3:
	bx	r3

	.pool
