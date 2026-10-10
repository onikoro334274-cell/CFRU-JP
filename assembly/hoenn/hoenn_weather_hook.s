@ 天気 15 番（灰）を地図の天気番号として通す（#2・2026-09-24・src/hoenn_weather.c）。

	.text
	.thumb
	.align	2
@ ★ armv4t に固定する（assembly/hoenn/townmap_hook.s の注記と同じ理由）
	.arch	armv4t

@ ================================================================
@ 素の TranslateWeatherNum（0807A92C）は 0〜21 番を飛び先の表（0807A944）で分ける:
@
@   0807A92C  PUSH {lr}
@   0807A932  CMP  r0, #0x15 / BHI 0807A9F4（「天気なし」）
@   0807A938  LDR  r1, =0807A944 / ADD r0, r0, r1 / LDR r0, [r0] / MOV pc, r0
@   0807A99C  MOV  r0, #1 / B 0807A9F6 …（1〜14 番。どれも「その番号を返す」だけ）
@   0807A9F4  MOV  r0, #0                ← 0 番と 15〜19 番（天気なし）
@   0807A9F6  POP  {r1} / BX r1
@
@ 表の 15 番の行（0807A980）を repoints でここへ向け、15 を返す。
@ ★ MOV pc で飛んで来るので、番地は偶数のまま（.thumb_func を付けない・リンカも Thumb の
@   ビットを立てない）。戻り方は素と同じ POP {r1} / BX r1（入口の PUSH {lr} と対）。
@ ================================================================
	.global	HoennWeatherTranslateAsh
HoennWeatherTranslateAsh:
	mov	r0, #15
	pop	{r1}
	bx	r1
