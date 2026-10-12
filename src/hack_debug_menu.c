#include "defines.h"
#include "defines_battle.h"
#include "../include/bg.h"
#include "../include/event_data.h"
#include "../include/field_screen_effect.h"
#include "../include/item.h"
#include "../include/constants/items.h"
#include "../include/main.h"
#include "../include/menu.h"
#include "../include/money.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/pokemon.h"
#include "../include/script.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/string_util.h"
#include "../include/new/learn_move.h"
#include "../include/new/battle_strings.h"
#include "../include/malloc.h"
#include "../include/script_pokemon_80F8.h"
#include "../include/new/Vanilla_functions.h"
#include "map_region.h"
#include "hack_debug_menu.h"
#include "hack_mon_id.h"

/*
	デバッグメニュー（2026-10-11 ユーザー「https://github.com/pm-start/debug-menu これ取り込んで」「ビルドオプションで切り替えできるように」）。

	元は pm-start/debug-menu（作者 liru さん・806579e）: 素の日本語版 FR に IPS で当てる、フラグ 0x8FF で開けるメニュー。
	こちらは CFRU の C に書き直した:
	  - ビルドの指定 `build_rom.py --debug-menu`（デバッグメニュー付きビルド.bat）で -DHACK_DEBUG_MENU を渡したときだけ入る。
	    指定なしのビルドとデモ版（--demo とは一緒に使えない）には処理が入らない（文字列 strings/hack_debug_menu.string だけは CFRU の build.py が
	    .string を全部組むので入る・数百バイト）。入れたビルドでは常に開ける（元の 0x8FF・0x8FD は使わない）
	  - 開き方は「フィールドで B を押しながら Select」（元は A＋Select・2026-10-12 ネット交換（src/hack_network_trade.c・pm-start/enchanted-network-trade）が
	    A＋Select を使うので、ユーザー「競合するなら取り込みを優先」で B＋Select に移した）。素の処理にフックを足さず、野外の毎フレーム（src/overworld_speedup.c）で
	    gMain.callback1 を HackDebugMenu_CB1（見てから素の CB1_Overworld）に差し替える。開いたフレームの Select は newKeys から消す（べんりボタンを使わない）
	  - 捕獲 100%: ON/OFF を予約フラグ HACK_FLAG_DEBUG_CATCH に保つ。捕獲率の式（src/hack_difficulty.c の HackDifficulty_CatchOdds・patches/0326 の差し込み口）
	    が 0xFF を返す＝通常の野生の捕獲だけ（トレーナー戦・ゴースト・チュートリアルは前の分岐のまま）。CFRU の FLAG_ALWAYS_CATCHABLE は戦いのたびに下ろされるので使わない
	  - アイテムの番号は 1〜ITEMS_COUNT-1・個数 1〜999。所持金はこちらの上限 9,999,999 まで足す（AddMoney が頭打ちにする）
	  - ワープは全部の地図（2026-10-11 ユーザー「ワープのマップ全域対応」）: 群と地図の番号を選び、地名（地方ごとの番号＝MapRegion_GetName）を出す。
	    地図にワープの入口があればその 0 番、無ければ地図の真ん中へ。初めは今いる地図。群の数は DEBUG_MAP_GROUP_COUNT、群ごとの地図の数は
	    ROM の表から数える（MapCountInGroup）。フラグ編集は 1〜0x18FF
	  - 「デバッグをむこうに」は外した（ビルドで入れるので要らない）
	  - 2026-10-12 ユーザー「デバッグメニュー追加」（萌えもん化と色違い化／野生に合わない／手持ち編集／欲しいポケモンが手に入る／無敵／一撃で倒す／
	    任意のショッピングへ／Lv200／トレーナー無視／図鑑コンプリート）:
	      いちばん上の一覧は 19 行のスクロール（9 行見せる）。切り替え 4 つは旗 0xF0E〜0xF11（hack_debug_menu.h・セーブに残る・ふだんの版では読まない）。
	      効き目は上流の .c の #ifdef HACK_DEBUG_MENU の中から聞く（patches/0342）: やせいなし＝wild_encounter.c の Standard／SweetScent／
	      DoesFishBite／RockSmash と dexnav.c の InitDexNavHUD の頭・トレーナーむし＝overworld.c の CheckForTrainersWantingBattle の頭・
	      むてき／いちげき＝general_bs_commands.c の atk0C_datahpupdate の頭（HackDebugMenu_AdjustDamage・通信では効かない）。
	      萌えもん化／色違い化は手持ちを選んで「当たり ⇔ もとに」を入れ替える（src/hack_mon_id.c の HackMonId_SetMoemon／SetShiny＝裏 ID・表 ID だけ・6.5）。
	      手持ち編集は 21 の欄を左右で直してすぐ書く（性格はミント natureMint・特性は枠 0/1/2・Lv は経験値の表・個体値は hyperTraining を触らない）。
	      ショップは道具の番号を 50 個ずつのページ（名前が「？」と値段 0 の道具は外す）にして、RAM に組んだ小さな台本（callasm → waitstate → release）で
	      素の CreatePokemartMenu（0809BB54）を開く（閉じたら台本が主人公を放す）。台本と品の表は Malloc のまま（開くたびに約 110 バイト・デバッグ版だけ）
	状態はタスクの data に持つ（6.13: 書き換えられる静的変数は使えない）。
*/

#ifdef HACK_DEBUG_MENU

#define gHelpSystemEnabled (*((bool8 *)0x03005F1C))   //L/R のヘルプ（src/bw_start_menu.c と同じ）

#define DEBUG_FLAG_MAX 0x18FF
#define DEBUG_ITEM_COUNT_MAX 999
#define DEBUG_MAP_GROUP_COUNT 81     //群 0〜80（素 0〜42・ホウエン 43〜76・シンオウ 77・ジョウト 78・カントーの追加 79・シンオウの建物 80＝src/map_region.h）
#define MAP_GROUPS_LITERAL 0x08054B0C   //素の Overworld_GetMapHeaderByGroupAndId（08054AF8）の定数＝gMapGroups の番地（tools/rom/verify_hoenn_wild.py と同じ）
#define DEBUG_MONEY_GIVE 9999999

#define TOP_ROWS 9                   //いちばん上の一覧の見せる行（y 16〜112）
#define EDIT_ROWS 8                  //手持ち編集の見せる行
#define EDIT_FIELDS 21               //種族・Lv・性格・特性の枠・持ち物・技 4・個体値 6・努力値 6
#define SHOP_PAGE 50                 //ショップの 1 ページの道具の数
#define CREATE_POKEMART_MENU ((void (*)(const u16*)) (0x0809BB54 | 1))   //素の CreatePokemartMenu（ScrCmd_pokemart 0806BBA0 から）
#define NATURE_NAMES ((const u8* const*) 0x0842D1EC)                      //性格名 25 本（素の日本語版・src/bw_summary_screen.c と同じ）
#define DEX_FLAG_SET_SEEN 2
#define DEX_FLAG_SET_CAUGHT 3

enum
{
	MODE_TOP,
	MODE_ITEM,
	MODE_WARP,
	MODE_FLAG,
	MODE_PICK,     //手持ちを選ぶ（萌えもん化・色違い化・手持ち編集）
	MODE_EDIT,
	MODE_GETMON,
	MODE_SHOP,
};

enum
{
	ITEM_TOP_CATCH,
	ITEM_TOP_ITEM,
	ITEM_TOP_WARP,
	ITEM_TOP_HEAL,
	ITEM_TOP_MONEY,
	ITEM_TOP_BADGES,
	ITEM_TOP_FLAG,
	ITEM_TOP_NOWILD,
	ITEM_TOP_INVINCIBLE,
	ITEM_TOP_OHKO,
	ITEM_TOP_NOTRAINER,
	ITEM_TOP_MOEMON,
	ITEM_TOP_SHINY,
	ITEM_TOP_EDIT,
	ITEM_TOP_GETMON,
	ITEM_TOP_SHOP,
	ITEM_TOP_LV200,
	ITEM_TOP_DEX,
	ITEM_TOP_CLOSE,
	ITEM_TOP_COUNT,
};

//タスクの data
#define tWindow   data[0]
#define tCursor   data[1]
#define tMode     data[2]
#define tValue    data[3]   //アイテムの番号・町・フラグの番号
#define tCount    data[4]   //アイテムの個数
#define tField    data[5]   //アイテムの画面で 0 = 番号・1 = 個数
#define tResult   data[6]   //アイテム: 1 もらった・2 失敗
#define tScroll   data[7]   //いちばん上の一覧の先頭の行
#define tSlot     data[8]   //手持ちの何番目
#define tPurpose  data[9]   //手持ちを選ぶ画面の用事（ITEM_TOP_MOEMON など）
#define tEScroll  data[10]  //手持ち編集の先頭の行
#define tMsg      data[11]  //下の行に出す文の番号（0 = 操作の説明）
#define tHelp     data[14]  //開く前の gHelpSystemEnabled
#define tWaitA    data[15]  //開いた B（2026-10-12 まで A）を離すまで待つ

extern const u8 gText_HackDebug_Title[], gText_HackDebug_Cursor[];
extern const u8 gText_HackDebug_Catch[], gText_HackDebug_Item[], gText_HackDebug_Warp[], gText_HackDebug_Heal[];
extern const u8 gText_HackDebug_Money[], gText_HackDebug_Badges[], gText_HackDebug_Flag[], gText_HackDebug_Close[];
extern const u8 gText_HackDebug_CatchOn[], gText_HackDebug_CatchOff[];
extern const u8 gText_HackDebug_ItemNumber[], gText_HackDebug_ItemNumberSel[], gText_HackDebug_ItemCount[], gText_HackDebug_ItemCountSel[];
extern const u8 gText_HackDebug_ItemUnused[], gText_HackDebug_HelpUpDown[], gText_HackDebug_HelpStep[], gText_HackDebug_HelpBig[];
extern const u8 gText_HackDebug_ItemGot[], gText_HackDebug_ItemFail[], gText_HackDebug_ItemHelp[];
extern const u8 gText_HackDebug_WarpTitle[], gText_HackDebug_WarpHelp[];
extern const u8 gText_HackDebug_FlagTitle[], gText_HackDebug_On[], gText_HackDebug_Off[], gText_HackDebug_FlagHelp[];
extern const u8 gText_HackDebug_Healed[], gText_HackDebug_MoneyDone[], gText_HackDebug_BadgesDone[];
extern const u8 gText_HackDebug_WarpGroup[], gText_HackDebug_WarpGroupSel[], gText_HackDebug_WarpMap[], gText_HackDebug_WarpMapSel[];
extern const u8 gText_HackDebug_WarpNoName[], gText_HackDebug_WarpHelpUD[];
extern const u8 gText_HackDebug_NoWild[], gText_HackDebug_Invincible[], gText_HackDebug_Ohko[], gText_HackDebug_NoTrainer[];
extern const u8 gText_HackDebug_Moemon[], gText_HackDebug_Shiny[], gText_HackDebug_Edit[], gText_HackDebug_GetMon[];
extern const u8 gText_HackDebug_Shop[], gText_HackDebug_Lv200[], gText_HackDebug_Dex[], gText_HackDebug_TopHelp[];
extern const u8 gText_HackDebug_PickHelp[], gText_HackDebug_Lv[], gText_HackDebug_MarkShiny[], gText_HackDebug_MarkMoemon[];
extern const u8 gText_HackDebug_Changed[], gText_HackDebug_EditHelp[], gText_HackDebug_GetMonHelp[];
extern const u8 gText_HackDebug_GotParty[], gText_HackDebug_GotBox[], gText_HackDebug_GotFail[];
extern const u8 gText_HackDebug_ShopRange[], gText_HackDebug_ShopHelp[], gText_HackDebug_ShopEmpty[];
extern const u8 gText_HackDebug_Lv200Done[], gText_HackDebug_DexDone[], gText_HackDebug_NoMon[];
extern const u8 gText_HackDebug_ESpecies[], gText_HackDebug_ELevel[], gText_HackDebug_ENature[], gText_HackDebug_EAbility[], gText_HackDebug_EItem[];
extern const u8 gText_HackDebug_EMove1[], gText_HackDebug_EMove2[], gText_HackDebug_EMove3[], gText_HackDebug_EMove4[];
extern const u8 gText_HackDebug_EIvHp[], gText_HackDebug_EIvAtk[], gText_HackDebug_EIvDef[], gText_HackDebug_EIvSpe[], gText_HackDebug_EIvSpa[], gText_HackDebug_EIvSpd[];
extern const u8 gText_HackDebug_EEvHp[], gText_HackDebug_EEvAtk[], gText_HackDebug_EEvDef[], gText_HackDebug_EEvSpe[], gText_HackDebug_EEvSpa[], gText_HackDebug_EEvSpd[];
extern u8 gMoveNames[][MOVE_NAME_LENGTH + 1];

static const struct WindowTemplate sDebugWindow = {0, 1, 1, 28, 18, 15, 0x100};    //元と同じ

static const u8* const sTopLabels[ITEM_TOP_COUNT] =
{
	gText_HackDebug_Catch, gText_HackDebug_Item, gText_HackDebug_Warp, gText_HackDebug_Heal,
	gText_HackDebug_Money, gText_HackDebug_Badges, gText_HackDebug_Flag,
	gText_HackDebug_NoWild, gText_HackDebug_Invincible, gText_HackDebug_Ohko, gText_HackDebug_NoTrainer,
	gText_HackDebug_Moemon, gText_HackDebug_Shiny, gText_HackDebug_Edit, gText_HackDebug_GetMon,
	gText_HackDebug_Shop, gText_HackDebug_Lv200, gText_HackDebug_Dex, gText_HackDebug_Close,
};

//切り替えの行の旗（0 = 切り替えではない）
static const u16 sTopFlags[ITEM_TOP_COUNT] =
{
	[ITEM_TOP_CATCH] = HACK_FLAG_DEBUG_CATCH,
	[ITEM_TOP_NOWILD] = HACK_FLAG_DEBUG_NOWILD,
	[ITEM_TOP_INVINCIBLE] = HACK_FLAG_DEBUG_INVINCIBLE,
	[ITEM_TOP_OHKO] = HACK_FLAG_DEBUG_OHKO,
	[ITEM_TOP_NOTRAINER] = HACK_FLAG_DEBUG_NOTRAINER,
};

static const u8* const sEditLabels[EDIT_FIELDS] =
{
	gText_HackDebug_ESpecies, gText_HackDebug_ELevel, gText_HackDebug_ENature, gText_HackDebug_EAbility, gText_HackDebug_EItem,
	gText_HackDebug_EMove1, gText_HackDebug_EMove2, gText_HackDebug_EMove3, gText_HackDebug_EMove4,
	gText_HackDebug_EIvHp, gText_HackDebug_EIvAtk, gText_HackDebug_EIvDef, gText_HackDebug_EIvSpe, gText_HackDebug_EIvSpa, gText_HackDebug_EIvSpd,
	gText_HackDebug_EEvHp, gText_HackDebug_EEvAtk, gText_HackDebug_EEvDef, gText_HackDebug_EEvSpe, gText_HackDebug_EEvSpa, gText_HackDebug_EEvSpd,
};

//下の行の文（tMsg の番号）
static const u8* const sMessages[] =
{
	NULL, gText_HackDebug_Changed, gText_HackDebug_GotParty, gText_HackDebug_GotBox, gText_HackDebug_GotFail,
	gText_HackDebug_ShopEmpty, gText_HackDebug_NoMon,
};
enum { MSG_NONE, MSG_CHANGED, MSG_GOT_PARTY, MSG_GOT_BOX, MSG_GOT_FAIL, MSG_SHOP_EMPTY, MSG_NO_MON };


static void Task_HackDebugMenu(u8 taskId);

static void Line(u8 window, u8 y, const u8* str)
{
	AddTextPrinterParameterized(window, 2, str, 8, y, 0xFF, NULL);
}

static void Number(u8 window, u8 y, u32 n)
{
	u8 s[8];
	u32 power = 10000;
	u32 j;

	for (j = 0; j < 5; ++j)
	{
		s[j] = 0xA1;            //「0」
		while (n >= power)
		{
			n -= power;
			s[j]++;
		}
		power /= 10;
	}
	s[5] = EOS;
	Line(window, y, s);
}

static void Message(u8 window, const u8* str)
{
	Line(window, 128, str);
	CopyWindowToVram(window, 2);
}

static void LineAt(u8 window, u8 x, u8 y, const u8* str)
{
	AddTextPrinterParameterized(window, 2, str, x, y, 0xFF, NULL);
}

static void NumberAt(u8 window, u8 x, u8 y, u32 n, u8 digits)
{
	u8 s[12];
	ConvertIntToDecimalStringN(s, n, STR_CONV_MODE_LEADING_ZEROS, digits);
	LineAt(window, x, y, s);
}

static bool8 IsUsableMon(u8 slot)
{
	return slot < PARTY_SIZE && GetMonData(&gPlayerParty[slot], MON_DATA_SPECIES, NULL) != SPECIES_NONE
	    && !GetMonData(&gPlayerParty[slot], MON_DATA_IS_EGG, NULL);
}

/* ---- 手持ち編集（2026-10-12）---- */

static s32 EditGet(struct Pokemon* mon, u8 f)
{
	switch (f)
	{
		case 0: return GetMonData(mon, MON_DATA_SPECIES, NULL);
		case 1: return GetMonData(mon, MON_DATA_LEVEL, NULL);
		case 2: return mon->natureMint != 0 ? mon->natureMint - 1 : GetNature(mon);
		case 3: return mon->hiddenAbility ? 2 : GetMonData(mon, MON_DATA_ALT_ABILITY, NULL);
		case 4: return GetMonData(mon, MON_DATA_HELD_ITEM, NULL);
	}
	if (f < 9)
		return GetMonData(mon, MON_DATA_MOVE1 + f - 5, NULL);
	if (f < 15)
		return GetMonData(mon, MON_DATA_HP_IV + f - 9, NULL);
	return GetMonData(mon, MON_DATA_HP_EV + f - 15, NULL);
}

static s32 EditMax(u8 f)
{
	switch (f)
	{
		case 0: return NUM_SPECIES - 1;
		case 1: return MAX_LEVEL;
		case 2: return NUM_NATURES - 1;
		case 3: return 2;
		case 4: return ITEMS_COUNT - 1;
	}
	if (f < 9)
		return MOVES_COUNT - 1;
	if (f < 15)
		return 31;
	return EV_CAP;
}

static s32 EditMin(u8 f)
{
	return (f <= 1) ? 1 : 0;
}

//欄 f を v に（すぐ書いて能力を計算し直す）。性格値は触らない（性格はミント・特性は枠の番号）
static void EditSet(struct Pokemon* mon, u8 f, s32 v)
{
	u32 x = v;
	u16 species;

	switch (f)
	{
		case 0:
			SetMonData(mon, MON_DATA_SPECIES, &x);
			break;
		case 1:
			species = GetMonData(mon, MON_DATA_SPECIES, NULL);
			x = gExperienceTables[gBaseStats[species].growthRate][v];
			SetMonData(mon, MON_DATA_EXP, &x);
			break;
		case 2:
			mon->natureMint = v + 1;
			break;
		case 3:
			x = (v == 1);
			mon->hiddenAbility = (v == 2);
			SetMonData(mon, MON_DATA_ALT_ABILITY, &x);
			break;
		case 4:
			SetMonData(mon, MON_DATA_HELD_ITEM, &x);
			break;
		default:
			if (f < 9)
				SetMonMoveSlot(mon, v, f - 5);
			else if (f < 15)
				SetMonData(mon, MON_DATA_HP_IV + f - 9, &x);
			else
				SetMonData(mon, MON_DATA_HP_EV + f - 15, &x);
			break;
	}
	species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	if (mon->hiddenAbility && gBaseStats[species].hiddenAbility == ABILITY_NONE)   //5e: 種族を変えて特性の枠が無くなったら 1 つ目に
		mon->hiddenAbility = FALSE;
	CalculateMonStats(mon);
}

//欄の横に出す名前（無ければ NULL）
static const u8* EditName(struct Pokemon* mon, u8 f, s32 v, u8* buf)
{
	switch (f)
	{
		case 0: return gSpeciesNames[v];
		case 2: return NATURE_NAMES[v];
		case 3: return GetAbilityName(GetMonAbility(mon));
		case 4:
			if (v == 0)
				return NULL;
			CopyItemName(v, buf);
			return buf;
	}
	if (f >= 5 && f < 9 && v != 0)
		return gMoveNames[v];
	return NULL;
}

/* ---- ショップ（道具の番号の 50 個ずつ・2026-10-12 5e「案 A」）---- */

void HackDebugMenu_OpenShop(void)          //RAM に組んだ台本の callasm から: Var8004／Var8005 が品の表の番地
{
	CREATE_POKEMART_MENU((const u16*) (Var8004 | ((u32) Var8005 << 16)));
}

//品の表と台本を組む（流すのはメニューを閉じてから）。品が 1 つも無ければ NULL
static u8* BuildShop(u16 page)
{
	u8* buf = Malloc(8 + 2 * (SHOP_PAGE + 1));
	u16* list;
	u32 fn = (u32) HackDebugMenu_OpenShop | 1;
	u16 id, n = 0;
	u8 name[64];

	if (buf == NULL)
		return NULL;
	list = (u16*) (buf + 8);
	for (id = 1 + page * SHOP_PAGE; id < ITEMS_COUNT && id < 1 + (page + 1) * SHOP_PAGE; ++id)
	{
		CopyItemName(id, name);
		if (name[0] != 0xAC && ItemId_GetPrice(id) != 0)       //「？」の名前（使っていない番号）と値段 0 は外す（6.59）
			list[n++] = id;
	}
	list[n] = ITEM_NONE;
	if (n == 0)
	{
		Free(buf);
		return NULL;
	}
	buf[0] = 0x23;                          //callasm（callnative）
	buf[1] = fn;
	buf[2] = fn >> 8;
	buf[3] = fn >> 16;
	buf[4] = fn >> 24;
	buf[5] = 0x27;                          //waitstate（CreatePokemartMenu が閉じたら EnableBothScriptContexts）
	buf[6] = 0x6C;                          //release
	buf[7] = 0x02;                          //end
	Var8004 = (u32) list;
	Var8005 = (u32) list >> 16;
	return buf;
}

/* ---- 効き目（上流の .c の #ifdef HACK_DEBUG_MENU から・patches/0342）---- */

bool8 HackDebugMenu_NoWild(void)
{
	return FlagGet(HACK_FLAG_DEBUG_NOWILD);
}

bool8 HackDebugMenu_NoTrainer(void)
{
	return FlagGet(HACK_FLAG_DEBUG_NOTRAINER);
}

//atk0C_datahpupdate の頭: 守る側が主人公の側なら 0（むてき）・主人公の側の技で相手の側なら今の HP 全部（いちげき）。通信では効かない
void HackDebugMenu_AdjustDamage(u8 bank)
{
	if (gBattleTypeFlags & BATTLE_TYPE_LINK || gBattleMoveDamage <= 0)
		return;
	if (SIDE(bank) == B_SIDE_PLAYER)
	{
		if (FlagGet(HACK_FLAG_DEBUG_INVINCIBLE))
			gBattleMoveDamage = 0;
	}
	else if (FlagGet(HACK_FLAG_DEBUG_OHKO) && SIDE(gBankAttacker) == B_SIDE_PLAYER
	      && !(gHitMarker & HITMARKER_NON_ATTACK_DMG) && gBattleMons[bank].hp > 0)
		gBattleMoveDamage = gBattleMons[bank].hp;
}

/* ---- 全部の地図（ワープ）---- */

static bool8 IsRomPtr(const void* p)
{
	u32 a = (u32) p;
	return a >= 0x08000000 && a < 0x0A000000 && !(a & 3);
}

static bool8 IsRomText(const u8* p)    //文字列は 4 の倍数に置かれるとは限らない
{
	u32 a = (u32) p;
	return a >= 0x08000000 && a < 0x0A000000 && *p != EOS;
}

static const struct MapHeader* const* const* MapGroups(void)
{
	return *(const struct MapHeader* const* const* const*) MAP_GROUPS_LITERAL;
}

//群の中の地図の数: 次に始まる群の表の手前まで、かつ地図の頭として正しい間（番地が ROM・地形・イベントも ROM）。
//群の地図の数は ROM のどこにも持っていないので、表の並び（群の表は前から続けて置かれる）と中身で数える
static u16 MapCountInGroup(u8 group)
{
	const struct MapHeader* const* const* groups = MapGroups();
	const struct MapHeader* const* list = groups[group];
	u32 limit = 256;
	u32 g, n;

	if (!IsRomPtr(list))
		return 0;
	for (g = 0; g < DEBUG_MAP_GROUP_COUNT; ++g)
	{
		u32 other = (u32) groups[g];
		if (other > (u32) list && (other - (u32) list) / 4 < limit)
			limit = (other - (u32) list) / 4;
	}
	for (n = 0; n < limit; ++n)
	{
		const struct MapHeader* h = list[n];
		if (!IsRomPtr(h) || !IsRomPtr(h->mapLayout) || !IsRomPtr(h->events) || h->mapLayoutId == 0)
			break;
	}
	return n;
}

static const struct MapHeader* DebugMapHeader(u8 group, u8 num)
{
	return MapGroups()[group][num];
}

static void DebugWarpTo(u8 group, u8 num)
{
	const struct MapHeader* h = DebugMapHeader(group, num);

	if (h->events->warpCount != 0)
		SetWarpDestination(group, num, 0, -1, -1);     //ワープの入口の 0 番
	else
		SetWarpDestination(group, num, -1, h->mapLayout->width / 2, h->mapLayout->height / 2);  //入口が無い地図は真ん中
	DoWarp();
}

static void Draw(u8 taskId)
{
	s16* data = gTasks[taskId].data;
	u8 w = tWindow;
	u32 j;

	FillWindowPixelBuffer(w, 0x11);
	Line(w, 0, gText_HackDebug_Title);

	if (tMode == MODE_TOP)
	{
		for (j = 0; j < TOP_ROWS && tScroll + j < ITEM_TOP_COUNT; ++j)
		{
			u32 k = tScroll + j;
			Line(w, 16 + j * 12, sTopLabels[k]);
			if (sTopFlags[k])
				LineAt(w, 176, 16 + j * 12, FlagGet(sTopFlags[k]) ? gText_HackDebug_On : gText_HackDebug_Off);
			if (k == (u32) tCursor)
				AddTextPrinterParameterized(w, 2, gText_HackDebug_Cursor, 0, 16 + j * 12, 0xFF, NULL);
		}
		Line(w, 128, tMsg ? sMessages[tMsg] : gText_HackDebug_TopHelp);
	}
	else if (tMode == MODE_PICK)
	{
		Line(w, 14, sTopLabels[tPurpose]);
		for (j = 0; j < PARTY_SIZE; ++j)
		{
			u8 y = 30 + j * 14;
			if (!IsUsableMon(j))
				continue;
			LineAt(w, 16, y, gSpeciesNames[GetMonData(&gPlayerParty[j], MON_DATA_SPECIES, NULL)]);
			LineAt(w, 100, y, gText_HackDebug_Lv);
			NumberAt(w, 116, y, GetMonData(&gPlayerParty[j], MON_DATA_LEVEL, NULL), 3);
			if (HackMonId_IsShinyBox((struct BoxPokemon*) &gPlayerParty[j]))
				LineAt(w, 150, y, gText_HackDebug_MarkShiny);
			if (HackMonId_IsMoemonHit((struct BoxPokemon*) &gPlayerParty[j]))
				LineAt(w, 186, y, gText_HackDebug_MarkMoemon);
			if (j == (u32) tSlot)
				AddTextPrinterParameterized(w, 2, gText_HackDebug_Cursor, 4, y, 0xFF, NULL);
		}
		Line(w, 128, tMsg ? sMessages[tMsg] : gText_HackDebug_PickHelp);
	}
	else if (tMode == MODE_EDIT)
	{
		struct Pokemon* mon = &gPlayerParty[tSlot];
		u8 buf[64];
		for (j = 0; j < EDIT_ROWS && tEScroll + j < EDIT_FIELDS; ++j)
		{
			u8 f = tEScroll + j, y = 14 + j * 14;
			s32 v = EditGet(mon, f);
			const u8* name = EditName(mon, f, v, buf);
			Line(w, y, sEditLabels[f]);
			NumberAt(w, 80, y, v, 4);
			if (name != NULL)
				LineAt(w, 120, y, name);
			if (f == (u32) tField)
				AddTextPrinterParameterized(w, 2, gText_HackDebug_Cursor, 0, y, 0xFF, NULL);
		}
		Line(w, 128, gText_HackDebug_EditHelp);
	}
	else if (tMode == MODE_GETMON)
	{
		Line(w, 16, sTopLabels[ITEM_TOP_GETMON]);
		AddTextPrinterParameterized(w, 2, gText_HackDebug_Cursor, 0, tField ? 62 : 34, 0xFF, NULL);
		Line(w, 34, gText_HackDebug_ESpecies);
		NumberAt(w, 80, 34, tValue, 4);
		LineAt(w, 120, 34, gSpeciesNames[tValue]);
		Line(w, 62, gText_HackDebug_ELevel);
		NumberAt(w, 80, 62, tCount, 3);
		Line(w, 90, gText_HackDebug_HelpBig);
		Line(w, 128, tMsg ? sMessages[tMsg] : gText_HackDebug_GetMonHelp);
	}
	else if (tMode == MODE_SHOP)
	{
		u32 first = 1 + tValue * SHOP_PAGE;
		Line(w, 16, sTopLabels[ITEM_TOP_SHOP]);
		Line(w, 40, gText_HackDebug_ShopRange);
		NumberAt(w, 16, 56, first, 4);
		NumberAt(w, 80, 56, first + SHOP_PAGE - 1 < ITEMS_COUNT - 1 ? first + SHOP_PAGE - 1 : ITEMS_COUNT - 1, 4);
		Line(w, 128, tMsg ? sMessages[tMsg] : gText_HackDebug_ShopHelp);
	}
	else if (tMode == MODE_ITEM)
	{
		u8 name[64];
		CopyItemName(tValue, name);
		Line(w, 16, tField ? gText_HackDebug_ItemNumber : gText_HackDebug_ItemNumberSel);
		Number(w, 30, tValue);
		Line(w, 44, name[0] == 0xAC ? gText_HackDebug_ItemUnused : name);      //「？」で始まる名前＝使っていない番号
		Line(w, 60, tField ? gText_HackDebug_ItemCountSel : gText_HackDebug_ItemCount);
		Number(w, 74, tCount);
		Line(w, 90, gText_HackDebug_HelpUpDown);
		Line(w, 102, gText_HackDebug_HelpStep);
		Line(w, 114, gText_HackDebug_HelpBig);
		Line(w, 130, tResult == 1 ? gText_HackDebug_ItemGot : tResult == 2 ? gText_HackDebug_ItemFail : gText_HackDebug_ItemHelp);
	}
	else if (tMode == MODE_WARP)
	{
		const u8* name = MapRegion_GetName(DebugMapHeader(tValue, tCount)->regionMapSectionId, GetMapRegionOf(tValue, tCount));
		Line(w, 16, gText_HackDebug_WarpTitle);
		Line(w, 30, tField ? gText_HackDebug_WarpGroup : gText_HackDebug_WarpGroupSel);
		Number(w, 42, tValue);
		Line(w, 56, tField ? gText_HackDebug_WarpMapSel : gText_HackDebug_WarpMap);
		Number(w, 68, tCount);
		Line(w, 82, IsRomText(name) ? name : gText_HackDebug_WarpNoName);
		Line(w, 98, gText_HackDebug_WarpHelpUD);
		Line(w, 110, gText_HackDebug_HelpBig);
		Line(w, 128, gText_HackDebug_WarpHelp);
	}
	else
	{
		Line(w, 22, gText_HackDebug_FlagTitle);
		Number(w, 44, tValue);
		Line(w, 68, FlagGet(tValue) ? gText_HackDebug_On : gText_HackDebug_Off);
		Line(w, 92, gText_HackDebug_HelpStep);
		Line(w, 108, gText_HackDebug_HelpBig);
		Line(w, 128, gText_HackDebug_FlagHelp);
	}

	PutWindowTilemap(w);
	CopyWindowToVram(w, 3);
	ShowBg(0);
	ScheduleBgCopyTilemapToVram(0);
}

static void Close(u8 taskId)
{
	s16* data = gTasks[taskId].data;
	u8 w = tWindow;

	gHelpSystemEnabled = tHelp;
	ClearStdWindowAndFrameToTransparent(w, TRUE);
	RemoveWindow(w);
	DestroyTask(taskId);
	ScriptContext2_Disable();
}

static s32 Adjust(s32 n, s32 delta, s32 lo, s32 hi)
{
	n += delta;
	if (n < lo)
		n = lo;
	if (n > hi)
		n = hi;
	return n;
}

static void Task_HackDebugMenu(u8 taskId)
{
	s16* data = gTasks[taskId].data;
	u16 keys = gMain.newKeys | (gMain.newAndRepeatedKeys & (DPAD_RIGHT | DPAD_LEFT));
	u16 held = gMain.heldKeysRaw;

	//メニューの間は L/R のヘルプを開かせない（元と同じ・閉じるときに tHelp へ戻す）
	gMain.heldKeysRaw &= ~(L_BUTTON | R_BUTTON);
	gMain.newKeysRaw &= ~(L_BUTTON | R_BUTTON);
	gMain.heldKeys &= ~(L_BUTTON | R_BUTTON);
	gMain.newKeys &= ~(L_BUTTON | R_BUTTON);
	gMain.newAndRepeatedKeys &= ~(L_BUTTON | R_BUTTON);

	if (tWaitA)
	{
		if (held & (A_BUTTON | B_BUTTON))
			return;
		tWaitA = FALSE;
	}
	if (keys == 0)
		return;

	if (keys & B_BUTTON)
	{
		tMsg = MSG_NONE;
		if (tMode == MODE_EDIT)          //手持ち編集からは手持ちを選ぶ画面へ
		{
			tMode = MODE_PICK;
			Draw(taskId);
		}
		else if (tMode != MODE_TOP)
		{
			tMode = MODE_TOP;
			Draw(taskId);
		}
		else
			Close(taskId);
		return;
	}
	tMsg = MSG_NONE;

	if (tMode == MODE_TOP)
	{
		if (keys & DPAD_UP)
			tCursor = tCursor ? tCursor - 1 : ITEM_TOP_COUNT - 1;
		if (keys & DPAD_DOWN)
			tCursor = tCursor == ITEM_TOP_COUNT - 1 ? 0 : tCursor + 1;
		if (tCursor < tScroll)
			tScroll = tCursor;
		if (tCursor >= tScroll + TOP_ROWS)
			tScroll = tCursor - TOP_ROWS + 1;
		if ((keys & A_BUTTON) && sTopFlags[tCursor])        //切り替え（捕獲 100%・やせいなし・むてき・いちげき・トレーナーむし）
		{
			if (FlagGet(sTopFlags[tCursor]))
				FlagClear(sTopFlags[tCursor]);
			else
				FlagSet(sTopFlags[tCursor]);
		}
		else if (keys & A_BUTTON)
		{
			switch (tCursor)
			{
				case ITEM_TOP_MOEMON:
				case ITEM_TOP_SHINY:
				case ITEM_TOP_EDIT:
					tMode = MODE_PICK;
					tPurpose = tCursor;
					tSlot = 0;
					break;
				case ITEM_TOP_GETMON:
					tMode = MODE_GETMON;
					tValue = 1;
					tCount = 50;
					tField = 0;
					break;
				case ITEM_TOP_SHOP:
					tMode = MODE_SHOP;
					tValue = 0;
					break;
				case ITEM_TOP_LV200:
				{
					u32 i;
					for (i = 0; i < PARTY_SIZE; ++i)
					{
						if (IsUsableMon(i))
							EditSet(&gPlayerParty[i], 1, MAX_LEVEL);
					}
					Message(tWindow, gText_HackDebug_Lv200Done);
					return;
				}
				case ITEM_TOP_DEX:
				{
					u16 n;
					for (n = 1; n < NATIONAL_DEX_COUNT; ++n)
					{
						GetSetPokedexFlag(n, DEX_FLAG_SET_SEEN);
						GetSetPokedexFlag(n, DEX_FLAG_SET_CAUGHT);
					}
					Message(tWindow, gText_HackDebug_DexDone);
					return;
				}
				case ITEM_TOP_ITEM:
					tMode = MODE_ITEM;
					tValue = 1;
					tCount = 1;
					tField = 0;
					tResult = 0;
					break;
				case ITEM_TOP_WARP:            //初めは今いる地図
					tMode = MODE_WARP;
					tValue = (u8) gSaveBlock1->location.mapGroup;
					tCount = (u8) gSaveBlock1->location.mapNum;
					tField = 0;
					if (tValue >= DEBUG_MAP_GROUP_COUNT || tCount >= MapCountInGroup(tValue))
						tValue = tCount = 0;
					break;
				case ITEM_TOP_HEAL:
					HealPlayerParty();
					Message(tWindow, gText_HackDebug_Healed);
					return;
				case ITEM_TOP_MONEY:
					AddMoney(&gSaveBlock1->money, DEBUG_MONEY_GIVE);
					Message(tWindow, gText_HackDebug_MoneyDone);
					return;
				case ITEM_TOP_BADGES:
				{
					u16 f;
					for (f = FLAG_BADGE01_GET; f <= FLAG_BADGE08_GET; ++f)
						FlagSet(f);
					Message(tWindow, gText_HackDebug_BadgesDone);
					return;
				}
				case ITEM_TOP_FLAG:
					tMode = MODE_FLAG;
					tValue = 1;
					break;
				case ITEM_TOP_CLOSE:
					Close(taskId);
					return;
			}
		}
	}
	else if (tMode == MODE_PICK)
	{
		u32 k;
		for (k = 0; k < PARTY_SIZE && (keys & (DPAD_UP | DPAD_DOWN)); ++k)     //次の手持ち（タマゴ・空きは飛ばす）
		{
			tSlot = (keys & DPAD_UP) ? (tSlot + PARTY_SIZE - 1) % PARTY_SIZE : (tSlot + 1) % PARTY_SIZE;
			if (IsUsableMon(tSlot))
				break;
		}
		if (keys & A_BUTTON)
		{
			struct BoxPokemon* box = (struct BoxPokemon*) &gPlayerParty[tSlot];
			if (!IsUsableMon(tSlot))
				tMsg = MSG_NO_MON;
			else if (tPurpose == ITEM_TOP_MOEMON)
			{
				HackMonId_SetMoemon(box, !HackMonId_IsMoemonHit(box));
				tMsg = MSG_CHANGED;
			}
			else if (tPurpose == ITEM_TOP_SHINY)
			{
				HackMonId_SetShiny(box, !HackMonId_IsShinyBox(box));
				tMsg = MSG_CHANGED;
			}
			else
			{
				tMode = MODE_EDIT;
				tField = 0;
				tEScroll = 0;
			}
		}
	}
	else if (tMode == MODE_EDIT)
	{
		s32 step = (held & L_BUTTON) ? 100 : (held & R_BUTTON) ? 10 : 1;
		s32 delta = (keys & DPAD_RIGHT) ? step : (keys & DPAD_LEFT) ? -step : 0;
		struct Pokemon* mon = &gPlayerParty[tSlot];

		if (keys & DPAD_UP)
			tField = tField ? tField - 1 : EDIT_FIELDS - 1;
		if (keys & DPAD_DOWN)
			tField = tField == EDIT_FIELDS - 1 ? 0 : tField + 1;
		if (tField < tEScroll)
			tEScroll = tField;
		if (tField >= tEScroll + EDIT_ROWS)
			tEScroll = tField - EDIT_ROWS + 1;
		if (delta)
			EditSet(mon, tField, Adjust(EditGet(mon, tField), delta, EditMin(tField), EditMax(tField)));
	}
	else if (tMode == MODE_GETMON)
	{
		s32 step = (held & L_BUTTON) ? 100 : (held & R_BUTTON) ? 10 : 1;
		s32 delta = (keys & DPAD_RIGHT) ? step : (keys & DPAD_LEFT) ? -step : 0;

		if (keys & (DPAD_UP | DPAD_DOWN))
			tField ^= 1;
		if (tField)
			tCount = Adjust(tCount, delta, 1, MAX_LEVEL);
		else
			tValue = Adjust(tValue, delta, 1, NUM_SPECIES - 1);
		if (keys & A_BUTTON)
		{
			u8 r = ScriptGiveMon(tValue, tCount, ITEM_NONE, 0, 0, 0);    //0 手持ち・1 ボックス・2 入らない（素の流れ）
			tMsg = r == 0 ? MSG_GOT_PARTY : r == 1 ? MSG_GOT_BOX : MSG_GOT_FAIL;
		}
	}
	else if (tMode == MODE_SHOP)
	{
		s32 pages = (ITEMS_COUNT - 1 + SHOP_PAGE - 1) / SHOP_PAGE;
		s32 step = (held & R_BUTTON) ? 10 : 1;
		s32 delta = (keys & DPAD_RIGHT) ? step : (keys & DPAD_LEFT) ? -step : 0;

		tValue = Adjust(tValue, delta, 0, pages - 1);
		if (keys & A_BUTTON)
		{
			u8* script = BuildShop(tValue);
			if (script == NULL)
				tMsg = MSG_SHOP_EMPTY;
			else
			{
				Close(taskId);                       //ScriptContext2 を下ろしてから台本を流す（台本の release で主人公を放す）
				ScriptContext1_SetupScript(script);
				return;
			}
		}
	}
	else if (tMode == MODE_WARP)
	{
		s32 step = (held & L_BUTTON) ? 100 : (held & R_BUTTON) ? 10 : 1;
		s32 delta = (keys & DPAD_RIGHT) ? step : (keys & DPAD_LEFT) ? -step : 0;

		if (keys & (DPAD_UP | DPAD_DOWN))
			tField ^= 1;
		if (tField == 0)
		{
			s32 g = tValue + delta;
			while (g >= 0 && g < DEBUG_MAP_GROUP_COUNT && MapCountInGroup(g) == 0)   //地図の無い群は飛ばす
				g += delta > 0 ? 1 : -1;
			if (delta && g >= 0 && g < DEBUG_MAP_GROUP_COUNT)
			{
				tValue = g;
				tCount = 0;
			}
		}
		else
			tCount = Adjust(tCount, delta, 0, MapCountInGroup(tValue) - 1);
		if (keys & A_BUTTON)
		{
			u8 group = tValue, num = tCount;
			Close(taskId);
			DebugWarpTo(group, num);
			return;
		}
	}
	else
	{
		s32 step = (held & L_BUTTON) ? 100 : (held & R_BUTTON) ? 10 : 1;    //元と同じ: L＋左右 ±100・R＋左右 ±10
		s32 delta = (keys & DPAD_RIGHT) ? step : (keys & DPAD_LEFT) ? -step : 0;

		if (tMode == MODE_ITEM)
		{
			if (keys & (DPAD_UP | DPAD_DOWN))
			{
				tField ^= 1;
				tResult = 0;
			}
			if (delta)
				tResult = 0;
			if (tField)
				tCount = Adjust(tCount, delta, 1, DEBUG_ITEM_COUNT_MAX);
			else
				tValue = Adjust(tValue, delta, 1, ITEMS_COUNT - 1);
			if (keys & A_BUTTON)
			{
				u8 name[64];
				CopyItemName(tValue, name);
				tResult = (name[0] != 0xAC && AddBagItem(tValue, tCount)) ? 1 : 2;
			}
		}
		else
		{
			tValue = Adjust(tValue, delta, 1, DEBUG_FLAG_MAX);
			if (keys & A_BUTTON)
			{
				if (FlagGet(tValue))
					FlagClear(tValue);
				else
					FlagSet(tValue);
			}
		}
	}

	Draw(taskId);
}

//B を押しながら Select でメニューを開く（入力の処理＝素の CB1_Overworld の直前に呼ぶ・ネット交換の NetTrade_CB1 から）
void HackDebugMenu_TryOpen(void)
{
	u8 taskId;
	u16 window;
	s16* data;

	if (!(gMain.heldKeysRaw & B_BUTTON) || !(gMain.newKeysRaw & SELECT_BUTTON))
		return;
	if (ScriptContext2_IsEnabled() || gPaletteFade->active || FindTaskIdByFunc(Task_HackDebugMenu) != 0xFF)
		return;

	taskId = CreateTask(Task_HackDebugMenu, 80);
	if (taskId == 0xFF)
		return;
	window = AddWindow(&sDebugWindow);
	if (window == 0xFF)
	{
		DestroyTask(taskId);
		return;
	}

	data = gTasks[taskId].data;
	tWindow = window;
	tCursor = 0;
	tMode = MODE_TOP;
	tWaitA = TRUE;
	tHelp = gHelpSystemEnabled;
	gHelpSystemEnabled = FALSE;

	//このフレームの Select は入力の処理に渡さない（登録した道具を使わない）
	gMain.newKeys &= ~SELECT_BUTTON;
	gMain.newKeysRaw &= ~SELECT_BUTTON;
	gMain.newAndRepeatedKeys &= ~SELECT_BUTTON;

	ScriptContext2_Enable();
	Draw(taskId);
}

//★ 2026-10-12 gMain.callback1 の差し替え（HackDebugMenu_CB1・HackDebugMenu_FieldTick）はネット交換の NetTrade_CB1（src/hack_network_trade.c）に
//  まとめた（2 つが別々に差し替えると先に差し替えたほうだけが動く）。NetTrade_CB1 がこの HackDebugMenu_TryOpen を呼んでから素の CB1_Overworld を呼ぶ

bool8 HackDebugMenu_CatchAll(void)
{
	return FlagGet(HACK_FLAG_DEBUG_CATCH);
}

#endif
