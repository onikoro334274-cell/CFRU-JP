@ バッジの数をカントー＋ホウエンの合計にするフック（2026-09-29・src/badge_count.c）。
	.arch armv4t
	.text
	.thumb

@ ---- レポートの画面（素の SaveStatToString の SAVE_STAT_BADGES・080F9114）----
@   素: r5 = 書き込み先。旗 0x820〜0x827 を数えて「n こ EOS」を書き、080F9144（pop {r4-r7} / pop {r0} / bx r0）へ。
@   ここで r4・r6・r7 はまだ使っていない（素のこの先で初めて入れる）→ フックの飛び先のレジスタは r4
	.global	HoennSaveBadgesHook
	.thumb_func
HoennSaveBadgesHook:
	mov	r0, r5
	ldr	r1, =(SaveStatBadgesToString + 1)
	bl	HoennBadgesCallR1
	ldr	r0, =0x080F9145
	bx	r0

@ ---- 「つづきから」の画面（素の PrintBadgeCount・0800C9E4 の +4）----
@   素の 0800C9E8〜0800C9EF: movs r5,#0 / movs r4,#0x82 / lsls r4,#4 / add r6,sp,#32 → 数える輪（〜0800CA0B）。
@   合計を r5 に入れ、r6 = sp + 32 を作り直して、輪の次（0800CA0C）へ。r0〜r4 はこの先で入れ直される
	.global	HoennMainMenuBadgesHook
	.thumb_func
HoennMainMenuBadgesHook:
	ldr	r1, =(GetAllBadgeCount + 1)
	bl	HoennBadgesCallR1
	mov	r5, r0
	add	r6, sp, #32
	ldr	r0, =0x0800CA0D
	bx	r0

HoennBadgesCallR1:
	bx	r1

	.pool
