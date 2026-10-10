@ タマムシデパートのメガストーン売り場（src/mega_shop.c・2026-10-01）の差し込み口。
@ どちらも素の売り場の関数の途中で C を呼び、素の続きへ戻る。売り場の見分けは C の中（gShopData.itemList == gMegaShopItems）。

.text
.thumb
.align 2

@ BuyMenuDecompressBgGraphics（0809AA9C）の中・0809AAC0 via r0
@   素: ldr r0,=08D85F08 / ldr r1,=gShopTilemapBuffer1 / ldr r1,[r1] / bl LZDecompressWram → 0809AAE6 へ
@   ここは 4 の倍数・潰すのは 0809AAC0〜0809AAC7。0809AABE の beq は 0809AADC へ飛ぶので潰す範囲に入らない
.global MegaShop_FrameHook
.thumb_func
MegaShop_FrameHook:
	bl MegaShop_LoadFrameTilemap
	ldr r0, =0x0809AAE6 | 1
	bx r0
.pool

@ BuyMenuBuildListMenuTemplate（0809ABA4）の終わり・0809ACFC via r1（r0 = gShopData）
@   素: ldr r1,=gMultiuseListMenuTemplate / ldrh r1,[r1,#14] / strh r1,[r0,#18] / movs r0,#1 → 0809AD04（pop）
@   潰すのは 0809ACFC〜0809AD03。0809AD04 は失敗のときの飛び先（0809ABD8）なので潰さない
.align 2
.global MegaShop_ListTemplateHook
.thumb_func
MegaShop_ListTemplateHook:
	bl MegaShop_FixListTemplate
	mov r0, #1
	ldr r1, =0x0809AD04 | 1
	bx r1
.pool

@ BuyMenuBuildListMenuTemplate（0809ABA4）の名前を書く所・0809ABE8 via r0（すべての売り場・2026-10-01）
@   素: movs r5,#0 → 名前 1 つ 9 バイトで書くループ → やめる → 0809AC32（ldr r5,=gMultiuseListMenuTemplate / str r1,[r5]）
@   RemoveShopModifierCheats が名前ごとに 16 バイトを確保し、C（Shop_BuildListStrings）が書く。戻りは r1 = 一覧の表。
@   ここへ来るのは 0809ABCC の bne と CFRU の RemoveShopModifierCheats（どちらも入口）。ループの中への分岐は使わなくなる
.align 2
.global Shop_BuildListStringsHook
.thumb_func
Shop_BuildListStringsHook:
	bl Shop_BuildListStrings
	mov r1, r0
	ldr r0, =0x0809AC32 | 1
	bx r0
.pool
