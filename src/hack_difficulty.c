#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/new/item.h"
#include "../include/new/util.h"
#include "Tables/hoenn_script_flags.h"

/*
 * なんいど「やさしい」（2026-10-03 ユーザー「難易度さげたバージョン」→「ROM は 1 本・せっていで選ぶ・A＋B＋C」）。
 * 変数 HOENN_VAR_DIFFICULTY（0 ふつう / 1 やさしい・BW 風せっていの 6 行目「なんいど」・src/bw_option_menu.c）。
 *
 * やさしいのとき、**ふつうのトレーナー戦だけ**（バトル施設 FLAG_BATTLE_FACILITY の戦いは今までどおり）:
 *   A. 相手の AI をいちばん弱い段（AI_SCRIPT_CHECK_BAD_MOVE＝役に立たない技を避けるだけ）
 *      … src/Battle_AI/ai_master.c の GetAIFlags の最後（patches/0132）
 *   B. 相手のポケモンの努力値を 0・持ち物を外す（技はそのまま。メガストーン・Z クリスタル・オーブも外れる）
 *   C. 相手のポケモンのレベルを 3 下げる（手持ちに合わせて上がったあと・経験値で下げて能力を計算し直す）
 *      … B と C は src/build_pokemon.c の CreateNPCTrainerParty の、能力を計算する直前（patches/0132）
 *   ★ 一緒に戦う味方（タッグ）のポケモンは触らない
 *
 * ★ 2026-10-04 なんいど「ほどほど」（2・やさしい と ふつう の間・ユーザー「メガ進化系はもたせる　AI と努力値はふつう。レベルを 2 下げる」）:
 *   努力値は ふつう のまま（AI は 2026-10-05 から かしこい段を外す）。持ち物は メガストーン と ゲンシカイキの玉 だけ残して外す。レベルを 2 下げる（C と同じ下げ方）
 */

#define DIFFICULTY_EASY 1
#define DIFFICULTY_MEDIUM 2   //ほどほど（2026-10-04）
#define EASY_LEVEL_DOWN 3
#define MEDIUM_LEVEL_DOWN 2

static bool8 OrdinaryTrainerBattle(void)
{
	return (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
	    && !FlagGet(FLAG_BATTLE_FACILITY)
	    && !(gBattleTypeFlags & BATTLE_TYPE_FRONTIER);
}

static bool8 EasyTrainerBattle(void)
{
	return VarGet(HOENN_VAR_DIFFICULTY) == DIFFICULTY_EASY && OrdinaryTrainerBattle();
}

static bool8 MediumTrainerBattle(void)
{
	return VarGet(HOENN_VAR_DIFFICULTY) == DIFFICULTY_MEDIUM && OrdinaryTrainerBattle();
}

static void LowerTrainerMonLevel(struct Pokemon* mon, u32 down)
{
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	u32 level = GetMonData(mon, MON_DATA_LEVEL, NULL);

	if (level > down + 1)
	{
		level -= down;
		SetMonData(mon, MON_DATA_EXP, &gExperienceTables[gBaseStats[species].growthRate][level]);
		SetMonData(mon, MON_DATA_LEVEL, &level);
	}
}

//A. src/Battle_AI/ai_master.c の GetAIFlags（ふつうのトレーナーの枝の最後）
u32 HackDifficulty_AIFlags(u32 flags)
{
	if (EasyTrainerBattle())
		return AI_SCRIPT_CHECK_BAD_MOVE;
	//★ 2026-10-05 ユーザー「AIすこし下げられる？」: ほどほどは かしこい段（CHECK_GOOD_MOVE）を外して「そこそこ」（SEMI_SMART）に。
	//  もともと CHECK_GOOD_MOVE の無いトレーナーはそのまま（SEMI_SMART は かしこい段が無いときだけ効く・ai_positives.c）
	if (MediumTrainerBattle() && (flags & AI_SCRIPT_CHECK_GOOD_MOVE))
		return (flags & ~AI_SCRIPT_CHECK_GOOD_MOVE) | AI_SCRIPT_SEMI_SMART;
	return flags;
}

//B・C. src/build_pokemon.c の CreateNPCTrainerParty（相手側のポケモン・能力を計算する直前）
void HackDifficulty_AdjustTrainerMon(struct Pokemon* mon, u8 side)
{
	u16 none = ITEM_NONE;
	u8 zero = 0;
	u32 i;

	if (side != B_SIDE_OPPONENT)
		return;

	if (MediumTrainerBattle())
	{
		u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);

		if (!IsMegaStone(item) && !IsPrimalOrb(item))
			SetMonData(mon, MON_DATA_HELD_ITEM, &none);
		LowerTrainerMonLevel(mon, MEDIUM_LEVEL_DOWN);
		return;
	}
	if (!EasyTrainerBattle())
		return;

	for (i = 0; i < NUM_STATS; ++i)
		SetMonData(mon, MON_DATA_HP_EV + i, &zero);
	SetMonData(mon, MON_DATA_HELD_ITEM, &none);

	LowerTrainerMonLevel(mon, EASY_LEVEL_DOWN);
}

/* ★ 2026-10-03 ユーザー「設定の難易度デフォルトやさしいで」: はじめからのとき、なんいどを やさしい にしておく。
   src/save.c の NewGameWipeNewSaveData（拡張のセーブ 0x0203B0E8〜を 0 で消す・拡張変数 0x5000〜も中）の直後に呼ぶ（patches/0146）。
   ★ 値の意味（0 ふつう / 1 やさしい）は変えない（前のセーブで選んだ値はそのまま） */
void HackDifficulty_SetNewGameDefault(void)
{
	VarSet(HOENN_VAR_DIFFICULTY, DIFFICULTY_EASY);
}
