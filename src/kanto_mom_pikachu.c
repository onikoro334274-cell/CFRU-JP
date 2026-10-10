#include "defines.h"
#include "../include/pokemon.h"
#include "../include/random.h"
#include "../include/constants/abilities.h"
#include "../include/constants/items.h"
#include "../include/constants/moves.h"
#include "../include/constants/pokemon.h"
#include "../include/constants/species.h"
#include "../include/new/build_pokemon.h"
#include "../include/new/catching.h"
#include "../include/new/learn_move.h"
#include "../include/new/util.h"

/*
 * カントーの自宅: 最初のポケモンをもらったあと、お母さんに話しかけるとピカチュウをもらえる
 * （2026-09-24 ユーザー要望）。スクリプトは assembly/kanto/kanto_home_scripts.s の
 * KantoHome1F_EventScript_MomTalk（callasm でこの関数を呼ぶ）。
 *
 *   Lv5 / でんきだま / ボルテッカー / めざめるパワー（こおり・ユーザー要望で威力 80・patches/0067）
 *   ★ 残りの 2 つは バチバチアクセル / ザブザブサーフ（2026-09-28 ユーザー要望・Let's Go! の値で技を足した・
 *     include/constants/moves.h の MOVE_ZIPPYZAP 0x3E0 / MOVE_SPLISHYSPLASH 0x3E1・patches/0084）。
 *     以前はレベルで覚える技が自動で入っていた（押し出し）。今は 4 つとも決めて置く
 *   個体値: めざめるパワー（こおり）で最大ダメージ。CFRU は OLD_HIDDEN_POWER_BP が無効なので威力は固定。
 *           こおりになる組み合わせのうち、こうげき・とくこう・すばやさが 31 のまま残るもの
 *           H30 / A31 / B30 / C31 / D31 / S31（CalcMonHiddenPowerType の式で 58 → こおり）
 *   キョダイマックス: できる（mon.gigantamax。DPE の進化の表に SPECIES_PIKACHU -> SPECIES_PIKACHU_GIGA がある）
 *   努力値: こうげき 252 / とくこう 252 / すばやさ 6（余り。合計 510）
 *   性格:   ランダム（CreateMon が作る性格値のまま。性格値は作り直さない＝色違いの判定も素のまま・6.5）
 *   特性:   ランダム（その種族が持つ特性 1 / 2 / 隠れ特性のうち、空でないものから等確率）
 *
 * 結果は gSpecialVar_LastResult に GiveMonToPlayer の返り値（0 手持ち / 1 パソコン / 2 どちらも満杯）。
 * 詳細は docs/research/2026-09-24_kanto-mom-pikachu.md
 */

// ★ 2026-10-09 ユーザー「1089のピカチュウをママからもらったにしてほしい　別のポケモンデータでバチバチアクセル　ザブザブサーフを入れた版」
//   →「ポップスターピカチュウ」（0x441 = 1089）。覚える技の表にその 2 つを足した（DPE src/Learnsets.c・patches/0250）
#define MOM_PIKACHU_SPECIES   SPECIES_PIKACHU_POP_STAR
#define MOM_PIKACHU_LEVEL     5
#define MOM_PIKACHU_ITEM      ITEM_LIGHT_BALL
#define MOM_PIKACHU_MOVE      MOVE_VOLTTACKLE
#define MOM_PIKACHU_MOVE2     MOVE_HIDDENPOWER
#define MOM_PIKACHU_MOVE3     MOVE_ZIPPYZAP
#define MOM_PIKACHU_MOVE4     MOVE_SPLISHYSPLASH

void KantoMomGivePikachu(void);
bool8 HackGiftShiny_Roll(void);

static const u8 sMomPikachuIVs[6] =   /* HP, こうげき, ぼうぎょ, すばやさ, とくこう, とくぼう（MON_DATA_*_IV の並び）*/
{
	30, 31, 30, 31, 31, 31,
};

static const u8 sMomPikachuEVs[6] =   /* HP, こうげき, ぼうぎょ, すばやさ, とくこう, とくぼう（MON_DATA_*_EV の並び）*/
{
	0, 252, 0, 6, 252, 0,
};

void KantoMomGivePikachu(void)
{
	struct Pokemon mon;
	u16 item = MOM_PIKACHU_ITEM;
	u16 abilities[3];
	u8 count = 0, pick, i;

	CreateMon(&mon, MOM_PIKACHU_SPECIES, MOM_PIKACHU_LEVEL, 0, FALSE, 0, OT_ID_PLAYER_ID, 0);
	if (HackGiftShiny_Roll())  /* 2026-10-03 1/16 で色違い（src/hack_gift_shiny.c・性格と性別はそのまま）*/
		ForceMonShiny(&mon);

	for (i = 0; i < 6; ++i)
	{
		u8 iv = sMomPikachuIVs[i];
		u8 ev = sMomPikachuEVs[i];
		SetMonData(&mon, MON_DATA_HP_IV + i, &iv);
		SetMonData(&mon, MON_DATA_HP_EV + i, &ev);
	}

	/* 特性: 空でないものから等確率。0 / 1 は通常の枠、2 は隠れ特性 */
	abilities[0] = GetAbility1(MOM_PIKACHU_SPECIES);
	abilities[1] = GetAbility2(MOM_PIKACHU_SPECIES);
	abilities[2] = GetHiddenAbility(MOM_PIKACHU_SPECIES);
	{
		u8 slots[3];
		for (i = 0; i < 3; ++i)
		{
			if (abilities[i] != ABILITY_NONE && (i == 0 || abilities[i] != abilities[0]))
				slots[count++] = i;
		}
		pick = (count == 0) ? 0 : slots[((u32) Random() * count) >> 16];   /* Random() は 16 ビット。割り算を使わない */
	}
	if (pick == 2)
		mon.hiddenAbility = TRUE;
	else
		SetMonData(&mon, MON_DATA_ALT_ABILITY, &pick);

	SetMonData(&mon, MON_DATA_HELD_ITEM, &item);
	mon.gigantamax = TRUE;     /* キョダイマックスできる（ユーザー要望 2026-09-24）*/

	/* 技は 4 つとも決めて置く（PP は技の最大値・SetMonMoveSlot が入れる） */
	SetMonMoveSlot(&mon, MOM_PIKACHU_MOVE, 0);
	SetMonMoveSlot(&mon, MOM_PIKACHU_MOVE2, 1);
	SetMonMoveSlot(&mon, MOM_PIKACHU_MOVE3, 2);
	SetMonMoveSlot(&mon, MOM_PIKACHU_MOVE4, 3);

	CalculateMonStats(&mon);
	SetMonPokedexFlags(&mon);
	gSpecialVar_LastResult = GiveMonToPlayer(&mon);
}
