#include "defines.h"
#include "hack_difficulty.h"
#include "Tables/hoenn_script_flags.h"
#include "../include/list_menu.h"
#include "../include/menu.h"
#include "../include/shop.h"
#include "../include/string_util.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/constants/items.h"
#include "../include/constants/hold_effects.h"
#include "../include/new/ram_locs.h"
#include "../include/event_data.h"
#include "../include/new/Vanilla_functions.h"
#include "Tables/mega_shop_items.h"

/*
    タマムシデパートのメガストーン売り場（2026-10-01）

    ユーザー「メガストーン専門の階層」「全メガストーン並べて 金額 XY くらい」「金額の表示枠も拡張した専門の売り場」
    →（AskUserQuestion）「リザードナイト Y・ライチュウナイト Y だけ 1 万円・ほかは 50 万円」「上におこづかい・下に広い一覧」。
    品物と値段は tools/megas/build_mega_shop.py が作る src/Tables/mega_shop_items.h。

    ★ 売り場の見分け方: 素の pokemart は表のポインタを gShopData.itemList に持つ。それが gMegaShopItems のときだけ
       ここでの処理を効かせる（状態を持たない・6.13）。ほかの売り場は 1 バイトも変わらない。
    ★ 値段: メガストーンの道具の表の値段は 0（u16 で 50 万円は入らない）。売り場の中だけ sMegaShopPrices を使う。
       値段を使う所は CFRU の src/item.c（わざマシンの 1 個買いの仕組み）を通る:
         一覧の値段 PrintTmPriceOrPurchased / 足りるか CheckBuyableTm / 確認の文 CheckTmPurchase / 個数 CheckSingleBagTm
       → それぞれの頭で MegaShop_* を呼ぶ。メガストーンは 1 個ずつ買う（個数を選ぶ画面は値段 0 で割るので通さない）。
    ★ 画面（素の窓は 0 おこづかい・4 一覧・5 説明。BPRJ の表は 0842DB8C）:
         上の段（行 0〜4）  左におこづかい（素と同じ）・右に「メガストーン うりば」の札
         下の段（行 4〜13） 画面いっぱいの一覧（窓 4 を 1,5 から 28 × 8 ＝ 4 行）。値段は 7 桁 ＋ 円
       枠の絵（bg1）は素の枠の部品を並べ直して作る（MegaShop_LoadFrameTilemap）。
       見える行の数（4）とスクロールの矢印の位置も合わせる。
*/

#define MEGA_LIST_ROWS 4

extern const u8 gText_MegaShopTitle[];
extern const u8 gText_ZCrystalShopTitle[];
extern const u8 gText_DangerLeafShopTitle[];
extern const u8 gText_MegaShopConfirm[];

/* 2026-10-01 追記: 2 人目の店員の Z クリスタルの売り場（gZCrystalShopItems・一律 5 万円）も同じ画面・同じ仕組み */
bool8 MegaShop_IsOpen(void)
{
	return gShopData.itemList == gMegaShopItems || gShopData.itemList == gZCrystalShopItems
	    || gShopData.itemList == gDangerLeafShopItems;   //★ 2026-10-09 あぶないはっぱ（6F の警備員）
}

/* ★ 2026-10-09 あぶないはっぱは買うたびに 1.5 倍（ユーザー「買うたびに値段高くできる？」→「1 個ごとに 1.5 倍」「1 回に 1 個だけ」・
   この売り場はもとから 1 個ずつ）。買った数は変数 0x50F6（もとミラクル交換の日付の上・使わなくなった予約）に 上 4 ビット 0xD の印つきで。
   値段はおこづかいの上限 9,999,999 で止める。売値は道具の表の 5000 円の半分のまま */
#define HACK_VAR_DANGER_LEAF_BOUGHT 0x50F6
#define DANGER_LEAF_TAG 0xD000
#define MONEY_MAX 9999999

//★ 2026-10-09 ユーザー「こなざとうも値段上がるように」: 買った数を 6 ビットずつ（下 = はっぱ・その上 = こなざとう）に分けた。
//  1.5 倍は 19 回で上限に届くので 63 回まで数えれば足りる（前の版の はっぱの数 0〜63 はそのまま読める）
#define BOUGHT_BITS 6
#define BOUGHT_MAX ((1 << BOUGHT_BITS) - 1)

static u8 BoughtShift(u16 item)
{
	return (item == ITEM_DANGER_SUGAR) ? BOUGHT_BITS : 0;
}

static u16 DangerLeafBought(u16 item)
{
	u16 v = VarGet(HACK_VAR_DANGER_LEAF_BOUGHT);
	if ((v & 0xF000) != DANGER_LEAF_TAG)
		return 0;
	return (v >> BoughtShift(item)) & BOUGHT_MAX;
}

static u32 DangerLeafPrice(u16 item, u32 base)
{
	if (item != ITEM_DANGER_LEAF && item != ITEM_DANGER_SUGAR)
		return base;
	u32 n = DangerLeafBought(item), p = base;
	while (n-- != 0 && p < MONEY_MAX)
		p = p * 3 / 2;
	return (p > MONEY_MAX) ? MONEY_MAX : p;
}

bool8 MegaShop_AfterPurchase(u16 item)   //TRUE なら一覧の値段が変わった（呼ぶ側が書き直す）
{
	u16 v, n;
	u8 sh;

	if (!MegaShop_IsOpen() || (item != ITEM_DANGER_LEAF && item != ITEM_DANGER_SUGAR))
		return FALSE;
	v = VarGet(HACK_VAR_DANGER_LEAF_BOUGHT);
	if ((v & 0xF000) != DANGER_LEAF_TAG)
		v = DANGER_LEAF_TAG;
	sh = BoughtShift(item);
	n = (v >> sh) & BOUGHT_MAX;
	if (n < BOUGHT_MAX)
		++n;
	v = (v & ~(BOUGHT_MAX << sh)) | (n << sh);
	VarSet(HACK_VAR_DANGER_LEAF_BOUGHT, DANGER_LEAF_TAG | (v & 0x0FFF));
	return TRUE;
}

u32 MegaShop_Price(u16 item)
{
	u32 i;

	if (gShopData.itemList == gMegaShopItems)
	{
		for (i = 0; i < MEGA_SHOP_COUNT; ++i)
		{
			if (gMegaShopItems[i] == item)
				return HackDifficulty_BuyPrice(sMegaShopPrices[i], VarGet(HOENN_VAR_DIFFICULTY));
		}
	}
	else if (gShopData.itemList == gZCrystalShopItems)
	{
		for (i = 0; i < Z_CRYSTAL_SHOP_COUNT; ++i)
		{
			if (gZCrystalShopItems[i] == item)
				return HackDifficulty_BuyPrice(sZCrystalShopPrices[i], VarGet(HOENN_VAR_DIFFICULTY));
		}
	}
	else if (gShopData.itemList == gDangerLeafShopItems)
	{
		for (i = 0; i < DANGER_LEAF_SHOP_COUNT; ++i)
		{
			if (gDangerLeafShopItems[i] == item)
				return HackDifficulty_BuyPrice(DangerLeafPrice(item, sDangerLeafShopPrices[i]), VarGet(HOENN_VAR_DIFFICULTY));
		}
	}
	return ItemId_GetPrice(item);
}

/* ---- 窓 ---- */
static const struct WindowTemplate sMegaShopWindows[] =
{
	{0, 1, 1, 8, 3, 15, 0x27},     /* 0 おこづかい（素と同じ） */
	{0, 1, 11, 13, 2, 15, 0x3F},   /* 1（素と同じ・この売り場では使わない） */
	{0, 2, 15, 26, 4, 14, 0x59},   /* 2 会話（素と同じ） */
	{0, 17, 9, 12, 4, 14, 0xC1},   /* 3 個数（素と同じ・この売り場では使わない） */
	{0, 1, 5, 28, 8, 14, 0xF1},    /* 4 一覧（画面いっぱい・4 行） */
	{0, 4, 14, 26, 6, 15, 0x1D1},  /* 5 説明（素は 5,14 の 23 マス＝184 画素。メガストーンの説明は 1 行 198 画素まであり x 4 から描くので 4,14 の 26 マスに広げる） */
	{0, 13, 1, 15, 2, 15, 0x26D},  /* 6 札「メガストーン うりば」／「Zクリスタル うりば」 */
	DUMMY_WIN_TEMPLATE,
};

#define sShopBuyMenuWindowTemplatesNormal ((const struct WindowTemplate*) 0x0842DB8C)
#define sShopBuyMenuWindowTemplatesTM     ((const struct WindowTemplate*) 0x0842DBC4)
#define LoadUserWindowGfx_         ((void (*)(u8, u16, u8)) (0x081530E0 | 1))
#define LoadMenuMessageWindowGfx_  ((void (*)(u8, u16, u8)) (0x08152F70 | 1))
#define LoadStdWindowGfx_          ((void (*)(u8, u16, u8)) (0x08152FF0 | 1))
void __attribute__((long_call)) DeactivateAllTextPrinters(void);

/* 素の BuyMenuInitWindows（0813FD90）の置き換え（functionrewrites） */
void MegaShop_BuyMenuInitWindows(bool32 isSellingTM)
{
	bool8 mega = MegaShop_IsOpen();

	if (mega)
		InitWindows(sMegaShopWindows);
	else if (isSellingTM != TRUE)
		InitWindows(sShopBuyMenuWindowTemplatesNormal);
	else
		InitWindows(sShopBuyMenuWindowTemplatesTM);
	DeactivateAllTextPrinters();
	LoadUserWindowGfx_(0, 0x1, 0xD0);
	LoadMenuMessageWindowGfx_(0, 0x13, 0xE0);
	LoadStdWindowGfx_(0, 0xA, 0xF0);
	PutWindowTilemap(0);
	PutWindowTilemap(4);
	PutWindowTilemap(5);
	if (isSellingTM == TRUE && !mega)
		PutWindowTilemap(6);

	if (mega)
	{
		PutWindowTilemap(6);
		DrawStdFrameWithCustomTileAndPalette(6, FALSE, 0xA, 15);
		FillWindowPixelBuffer(6, PIXEL_FILL(1));
		AddTextPrinterParameterized(6, 2, (gShopData.itemList == gZCrystalShopItems) ? gText_ZCrystalShopTitle
		: (gShopData.itemList == gDangerLeafShopItems) ? gText_DangerLeafShopTitle : gText_MegaShopTitle, 0, 1, 0xFF, NULL);
		CopyWindowToVram(6, COPYWIN_BOTH);
	}
}

/* ---- 枠の絵（bg1 の 32 × 32 の並び。素は gShopTilemapBuffer1 に 08D85F08 を展開する） ---- */
#define sShopTilemapBuffer1 (*((u16**) 0x020398A0))
#define gBuyMenuFrame_Tilemap ((const void*) 0x08D85F08)

/* 素の BuyMenuDecompressBgGraphics の中（0809AAC0・hooks）から呼ぶ。素の枠を展開してから並べ直す */
void MegaShop_LoadFrameTilemap(void)
{
	u16 *buf = sShopTilemapBuffer1;
	u16 src[14][30];
	u32 r, c;

	LZDecompressWram(gBuyMenuFrame_Tilemap, buf);
	if (!MegaShop_IsOpen())
		return;

	for (r = 0; r < 14; ++r)
		for (c = 0; c < 30; ++c)
			src[r][c] = buf[r * 32 + c];

	/* 素の枠は 10〜29 列 × 0〜13 行（左の縁 10・中 11〜27・右の縁 28〜29・上の縁 0・下の縁 13・点線は 3/5/7/9/11 行）。
	   新しい枠は 0〜29 列 × 4〜13 行。行 r ← 素の行（4 → 0・5〜12 → 1〜8・13 → 13）、
	   列 c ← 素の列（0 → 10・1 → 11・2〜27 → 15（点線の続く列）・28 → 28・29 → 29） */
	for (r = 0; r < 14; ++r)
	{
		for (c = 0; c < 30; ++c)
		{
			u32 sr, sc;

			if (r < 4)
			{
				buf[r * 32 + c] = (c < 10) ? src[r][c] : 0; /* 上の段は野外の絵を見せる（左はおこづかいの下の素のまま） */
				continue;
			}
			sr = (r == 4) ? 0 : (r == 13) ? 13 : r - 4;
			sc = (c == 0) ? 10 : (c == 1) ? 11 : (c >= 28) ? c : 15;
			buf[r * 32 + c] = src[sr][sc];
		}
	}
}

/* ---- 一覧 ---- */
/* 素の BuyMenuBuildListMenuTemplate の終わり（0809ACFC・hooks）から呼ぶ。見える行の数を窓の高さに合わせる */
void MegaShop_FixListTemplate(void)
{
	if (MegaShop_IsOpen() && gMultiuseListMenuTemplate->maxShowed > MEGA_LIST_ROWS)
		gMultiuseListMenuTemplate->maxShowed = MEGA_LIST_ROWS;
	gShopData.field12 = gMultiuseListMenuTemplate->maxShowed;
}

/* 素の BuyMenuAddScrollIndicatorArrows（0809B080）の置き換え（functionrewrites） */
void MegaShop_BuyMenuAddScrollIndicatorArrows(void)
{
	u8 taskId;
	s32 x = 160, top = 8, bottom = (gShopData.martType == 1) ? 88 : 104;

	if (MegaShop_IsOpen())
	{
		x = 120;     /* 一覧の窓（x 8〜232）の真ん中 */
		top = 40;    /* 窓の上の端（行 5） */
		bottom = 104;
	}
	taskId = AddScrollIndicatorArrowPairParameterized(2, x, top, bottom, gShopData.itemCount - gShopData.field12 + 1, 110, 110, &gShopData.scrollOffset);
	gShopData.unk16_11 = taskId;
}

/* 一覧の値段（CFRU の PrintTmPriceOrPurchased の頭から）。7 桁 ＋ 円 を右寄せ（小さい字は 1 字 8 画素） */
bool8 MegaShop_PrintPrice(u8 windowId, u16 item, u8 y)
{
	s32 x;
	u8 *loc;

	if (!MegaShop_IsOpen())
		return FALSE;

	ConvertIntToDecimalStringN(gStringVar1, MegaShop_Price(item), 0, 7);
	x = 7 - StringLength(gStringVar1);
	loc = gStringVar4;
	while (x-- > 0)
		*loc++ = 0;      /* 空白（8 画素） */
	StringExpandPlaceholders(loc, (void*) 0x083DDDAB);   /* 「{STR_VAR_1}円」 */
	BuyMenuPrint(windowId, 0, gStringVar4, 28 * 8 - 8 * 8 - 4, y, 0, 0, 0xFF, 1);
	return TRUE;
}

/* 確認の文（CFRU の CheckTmPurchase の頭から）。1 個ずつ買う */
bool8 MegaShop_CheckPurchase(u16 item, u8 taskId)
{
	if (!MegaShop_IsOpen())
		return FALSE;

	CopyItemName(item, gStringVar1);
	ConvertIntToDecimalStringN(gStringVar2, MegaShop_Price(item), 3, 8);
	BuyMenuDisplayMessage(taskId, gText_MegaShopConfirm, BuyMenuConfirmPurchase);
	return TRUE;
}

/* ---- エレベーター（tools/kanto/build_mega_floor.py の EventScript_MegaElevator から callasm） ---- */
/* 素の GetElevatorFloor は 1F〜5F しか知らない（6F から乗ると 1F＝4 のまま）。6F（群 79 の 0 番）から乗ったときだけ 9 にする */
#define VAR_ELEVATOR_FLOOR 0x403A
#define MEGA_FLOOR_GROUP 79
#define MEGA_FLOOR_NUM 0

void MegaFloor_FixElevatorFloor(void)
{
	if (gSaveBlock1->dynamicWarp.mapGroup == MEGA_FLOOR_GROUP && gSaveBlock1->dynamicWarp.mapNum == MEGA_FLOOR_NUM)
		VarSet(VAR_ELEVATOR_FLOOR, 9);
}

/* ---- 品物の名前の置き場（すべての売り場・2026-10-01） ----
   素の BuyMenuBuildListMenuTemplate は名前を 1 つ 9 バイトの置き場に書く（CFRU の RemoveShopModifierCheats も 9 バイトで取る）。
   9 文字の名前（ガブリアスナイトZ など）は終わりの 0xFF を入れて 10 バイトになり、次の名前の頭に上書きされて
   「ガブリアスナイトZルカリオナイト」とつながった（ユーザー実機 2026-10-01）。最後の 1 つは置き場の外を 1 バイト壊していた。
   0809ABE8（hooks）から、置き場を 1 つ 16 バイトで取り直して名前とやめるを書く。素の続き（0809AC32）は r1 = 一覧の表で戻る */
#define sShopMenuListMenu    (*((struct ListMenuItem**) 0x020398B0))
#define sShopMenuItemStrings (*((u8**) 0x020398B4))
#define SHOP_NAME_STRIDE 16
#define PokeMartWriteNameAndIdAt ((void (*)(struct ListMenuItem*, u16, u8*)) (0x0809AD14 | 1))
#define gText_ShopQuit ((const u8*) 0x083DD805)

struct ListMenuItem *Shop_BuildListStrings(void)
{
	struct ListMenuItem *list = sShopMenuListMenu;
	u32 count = gShopData.itemCount;
	u8 *strs = sShopMenuItemStrings;
	u32 i;

	/* RemoveShopModifierCheats allocates the final stride and handles failure
	 * before entering this function. BuyMenuFreeMemory owns that allocation. */

	for (i = 0; i < count; ++i)
		PokeMartWriteNameAndIdAt(&list[i], gShopData.itemList[i], strs + i * SHOP_NAME_STRIDE);
	StringCopy(strs + count * SHOP_NAME_STRIDE, gText_ShopQuit);
	list[count].name = strs + count * SHOP_NAME_STRIDE;
	list[count].id = -2;   /* LIST_CANCEL */
	return list;
}

/* ---- メガストーンを売れるように（2026-10-03 ユーザー「メガストーン売却できるように」→「どれも3万でリザードナイトYなどは半値で」）----
   素の ItemId_GetPrice（0809A30C・functionrewrites）の置き換え。売値は「値段の半分」（素の売る処理のまま）。
   メガストーンは道具の表の値段が 0（売り場の 50 万円は u16 に入らないので売り場の中だけの表）で、値段 0 は売れない。
   → 表の値段が 0 のメガストーンだけ 60000（売値 3 万円）、売り場で 1 万円のもの（リザードナイト Y など）は 10000（売値 5 千円）。
   ★ 売り場で買うときは MegaShop_Price（売り場の中だけの表）を通るので、ここの値段は使われない */
#define MEGA_SELL_TABLE_PRICE 60000
extern const struct Item gItemData[];

/* ★ 2026-10-05 シルフカンパニー 1F のマスターボール（assembly/kanto/kanto_silph_master_ball.s）:
   ユーザー「お店の画面のまま・値段だけ売り場で」。道具の表の値段は 0 のまま（売れない）で、
   この売り場の買う画面（CB2_BuyMenu）のときだけ 65535 円。売る画面（バッグ）では表の 0 のまま */
#define SILPH_MASTER_BALL_PRICE 65535
extern const u16 KantoSilph_MasterBallItems[];

static u16 ItemIdGetBasePrice(u16 item)
{
	u32 i;

	if (item == ITEM_MASTER_BALL && gShopData.itemList == KantoSilph_MasterBallItems && (gMain.callback2 == CB2_BuyMenu || gMain.callback2 == CB2_InitBuyMenu))
		return SILPH_MASTER_BALL_PRICE;
	if (item >= ITEMS_COUNT)
		item = ITEM_NONE;
	if (gItemData[item].price != 0 || gItemData[item].holdEffect != ITEM_EFFECT_MEGA_STONE)
		return gItemData[item].price;
	for (i = 0; i < MEGA_SHOP_COUNT; ++i)
	{
		if (gMegaShopItems[i] == item && sMegaShopPrices[i] < MEGA_SELL_TABLE_PRICE)
			return sMegaShopPrices[i];
	}
	return MEGA_SELL_TABLE_PRICE;
}

u16 MegaShop_ItemIdGetPrice(u16 item)
{
	u16 price = ItemIdGetBasePrice(item);
	if (gMain.callback2 == CB2_BuyMenu || gMain.callback2 == CB2_InitBuyMenu)
		return HackDifficulty_BuyPrice(price, VarGet(HOENN_VAR_DIFFICULTY));
	return price;
}
