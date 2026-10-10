#include "defines.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/constants/abilities.h"
#include "../include/constants/moves.h"
#include "../include/constants/species.h"
#include "../include/battle.h"
#include "../include/new/build_pokemon.h"
#include "../include/new/catching.h"
#include "../include/new/learn_move.h"
#include "../include/new/util.h"

/*
 * ライバルのお姉さん（ナナミ）の特別なジグザグマ（2026-10-03）。
 * ユーザー「ライバルのお姉さんにタウンマップ渡した後に色違い個体値最大のジグザグマもらえるようにして　とくせいはものひろい固定
 *          しんそく覚えている　しんそくのPP上限が15に設定されている特別なポケモンにして」
 *
 *   - タウンマップをもらったあと（素の変数 VAR_MAP_SCENE_PALLET_TOWN_RIVALS_HOUSE 0x4058 が 2 以上）にナナミに話しかけると 1 度だけ
 *     （assembly/kanto/kanto_daisy_scripts.s・印 0x12FD）
 *   - Lv5・色違い（CFRU の ForceMonShiny＝性格・性別はそのまま）・個体値 6V・とくせい ものひろい（特性 1・隠れ特性ではない）
 *   - 技: レベルで覚える技に「しんそく」を足す（空きが無ければ 4 番目を置き換え）
 *
 * ★ しんそくの PP 15: PP の上限は技ごとに決まり、計算（素の CalculatePPWithBonus 0804070C）はポケモンを知らない。
 *   そこで同じ名前・同じ効果で PP だけ 15 の技 MOVE_EXTREMESPEED_15（0x3E4・patches/0137）を作り、その技だけ
 *   ポイントアップを無視して 15 にする（functionrewrites で 0804070C を置き換え・ほかの技は素と同じ式）。
 *   忘れると思い出せない（覚える技の表にない）。ほかのポケモンのしんそくは素のまま（PP 5〜8）。
 */

#define DAISY_MON_SPECIES SPECIES_ZIGZAGOON
#define DAISY_MON_LEVEL   5

//素の CalculatePPWithBonus の置き換え: 基本 PP + 基本 PP × 20% × ポイントアップの数（素と同じ式）
u8 HackDaisy_CalculatePPWithBonus(u16 move, u8 ppBonuses, u8 moveIndex)
{
	u8 basePP = gBattleMoves[move].pp;
	u8 bonus;

	if (move == MOVE_EXTREMESPEED_15)
		return basePP; //ポイントアップでも増えない（上限 15）

	bonus = (ppBonuses >> (moveIndex * 2)) & 3;
	return basePP + ((basePP * 20 * bonus) / 100);
}

void HackDaisy_GiveZigzagoon(void)
{
	struct Pokemon mon;
	u8 zero = 0, i;

	CreateMon(&mon, DAISY_MON_SPECIES, DAISY_MON_LEVEL, 31, FALSE, 0, OT_ID_PLAYER_ID, 0); //個体値はぜんぶ 31
	ForceMonShiny(&mon);
	SetMonData(&mon, MON_DATA_ALT_ABILITY, &zero); //特性 1 = ものひろい
	mon.hiddenAbility = FALSE;

	for (i = 0; i < MAX_MON_MOVES; ++i)
	{
		if (GetMonData(&mon, MON_DATA_MOVE1 + i, NULL) == MOVE_NONE)
			break;
	}
	if (i >= MAX_MON_MOVES)
		i = MAX_MON_MOVES - 1;
	SetMonMoveSlot(&mon, MOVE_EXTREMESPEED_15, i);

	CalculateMonStats(&mon);
	SetMonPokedexFlags(&mon);
	gSpecialVar_LastResult = GiveMonToPlayer(&mon);
}
