@ ミラクル交換（src/hack_wonder_trade.c・2026-10-07）: 交換の場面の文字を用意する所のフック。元は pkhk_BeGiNNeR さんの wonder_trade_hooks.s。
@ 素の BufferTradeSceneStrings（080506E0・push {r4, r5, lr} / sub sp, #0x14）の「ゲーム内交換」側（08050770〜）を
@ BufferInGameTradeSceneStrings に置き換え、素の終わり（080507AC）と同じ後始末をして戻る。
@ 日本語版で確かめた: 08050770 は 4 の倍数・ゲーム内交換の表 0822D2F8（1 件 0x3C・親の名前 +0x2B）・終わりは add sp,#20 / pop {r4,r5} / pop {r0} / bx r0。

.text
.thumb
.align 2

.global WonderTradeSceneStringsHook
WonderTradeSceneStringsHook:
	bl BufferInGameTradeSceneStrings
	add sp, #0x14
	pop {r4, r5}
	pop {r0}
	bx r0

.pool
