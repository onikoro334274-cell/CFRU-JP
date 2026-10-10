@ 全滅したときの戻り先をホウエン・ジョウト・シンオウのポケセンにも（#2・2026-09-30）。
@ ★ 詳細は docs/research/2026-09-30_region-fly-heal.md / 処理は src/region_fly.c
@
@ 素の SetWhiteoutRespawnWarpAndHealerNpc（080C0EA0〜）は「最後の回復地点」の群・番号を
@ GetHealLocationIndexFromMapGroupAndNum（080C0E20）で引くが、見るのは素の 20 件だけ（080C0E4C の CMP r3,#0x13）。
@ 見つからないと 0 が返り、戻り先の表（083B59E4）の [-1] を読んでいた。
@
@ フック先 080C0F08（hooks: RegionWhiteoutHook 080C0F08 2）。潰すのは 8 バイト:
@   ASR r1,#24 / LSL r1,#16 / LSR r1,#16（ここまで写す）/ BL 080C0E20 の前半
@ ★ LSL r1,#24 は手前の 080C0F06 で済んでいる（入口の r1 = 番号 << 24）。もう一度 LSL すると 0 になる
@ ★ 080C0F14 は CFRU の DynamicWhiteoutMap のフック（そこへは r6 = 索引で戻る・素の ADD r6,r0,#0 の代わり）
@ ★ r4 = 戻り先（struct WarpData *）。関数の出口 080C0FBA は POP {r4-r7} / POP {r0} / BX r0

	.text
	.thumb
	.align	2

	.global	RegionWhiteoutHook
	.thumb_func
RegionWhiteoutHook:
	asr	r1, r1, #24
	lsl	r1, r1, #16
	lsr	r1, r1, #16
	ldr	r3, =0x080C0E20 + 1		@ GetHealLocationIndexFromMapGroupAndNum（素の 20 件）
	bl	RegionWhiteout_BxR3
	cmp	r0, #0
	bne	RegionWhiteout_Vanilla
	mov	r0, r4
	bl	RegionWhiteoutRespawn		@ C: 0 = 決めた / 1 = 見つからない
	cmp	r0, #0
	bne	RegionWhiteout_Vanilla		@ 素の 1 番（マサラタウンの家）で続ける
	ldr	r0, =0x080C0FBA + 1		@ 関数の出口
	bx	r0
RegionWhiteout_Vanilla:
	mov	r6, r0
	ldr	r1, =0x080C0F14 + 1		@ CFRU の DynamicWhiteoutMap へ
	bx	r1
RegionWhiteout_BxR3:
	bx	r3
	.align	2
	.pool
