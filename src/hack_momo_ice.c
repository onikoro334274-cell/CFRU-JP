#include "defines.h"
#include "../include/pokemon.h"
#include "../include/constants/items.h"
#include "../include/constants/species.h"

/*
hack_momo_ice.c
    ももアイス（ITEM_MOMO_ICE 0x343・ITEM_EFFECT_MOMO_ICE・2026-10-05・patches/0175）

    ユーザー「アイスはとりあえずもたせると全能力2倍体力も状態異常無効、能力が下がらない相手からの技や自分の技でも
    毎ターン1/3回復、泥棒はたき落とす系を無効、トリックも」→ 体力も能力の値そのものを 2 倍（画面にも出る）・ふしぎなおくりもの。

    ・能力 2 倍: CalculateMonStatsNew（src/build_pokemon.c）が持っている間だけ HP と 5 つの能力を 2 倍にする（ヌケニンの HP は 1 のまま）
    ・持たせた／外したときに能力を計算し直す: 素の SetBoxMonData の中（持ち物の欄）では計算し直せない（中身を戻している途中）。
      → フィールドの毎フレーム（src/overworld_speedup.c）とステータス画面を開くとき、手持ちの最大 HP が
        「持っているかどうか」に合っていなければ計算し直す（状態は持たない・6.13）
    ・状態異常・能力ダウン・毎ターン回復・うばわれない は戦いの処理（src/battle_util.c・stat_buffs.c・item_battle_effects.c）
*/

bool8 HackMomoIce_Held(struct Pokemon* mon)
{
	return mon->item == ITEM_MOMO_ICE; //CFRU の手持ちは暗号化されていない（include/pokemon.h）。GetMonData を通すと重い
}

//CalculateMonStatsNew の最大 HP を出した直後（持っていれば 2 倍）
u16 HackMomoIce_MaxHP(struct Pokemon* mon, u16 species, u16 maxHP)
{
	#ifdef SPECIES_SHEDINJA
	if (species == SPECIES_SHEDINJA)
		return maxHP;
	#endif
	if (!HackMomoIce_Held(mon))
		return maxHP;
	return (maxHP > 0x7FFF) ? 0xFFFF : maxHP * 2;
}

//CalculateMonStatsNew の 5 つの能力を出した直後
void HackMomoIce_Stats(struct Pokemon* mon)
{
	u32 i;

	if (!HackMomoIce_Held(mon))
		return;
	for (i = MON_DATA_ATK; i <= MON_DATA_SPDEF; ++i)
	{
		u32 stat = GetMonData(mon, i, NULL) * 2;
		if (stat > 0xFFFF)
			stat = 0xFFFF;
		SetMonData(mon, i, &stat);
	}
}

//計算し直さずに、持ち物と最大 HP が合っているか（素の式・ハイパートレーニングの個体値）
//★ 2026-10-07 欄を直接読む（CFRU の手持ちは暗号化されていない）。前は GetMonData を 1 匹 8 回ほど呼んでいて、
//  フィールドの毎コマで約 44,500 サイクル（1 コマの 16%）を使い、ホウエンの重い曲と重なって処理落ちした（デモ版の報告「処理落ち」）
static bool8 MaxHPMatchesItem(struct Pokemon* mon)
{
	u16 species = mon->species;
	u32 iv, level, maxHP;

	if (species == SPECIES_NONE || mon->isEgg)
		return TRUE;
	#ifdef SPECIES_SHEDINJA
	if (species == SPECIES_SHEDINJA)
		return TRUE;
	#endif
	iv = (mon->hyperTraining & gBitTable[STAT_HP]) ? 31 : mon->hpIV;
	level = mon->level;
	maxHP = (((2 * gBaseStats[species].baseHP + iv + mon->hpEv / 4) * level) / 100) + level + 10;
	if (maxHP > 0xFFFF)
		maxHP = 0xFFFF;
	return mon->maxHP == HackMomoIce_MaxHP(mon, species, maxHP);
}

//フィールドの毎フレーム・ステータス画面を開くとき
void HackMomoIce_RefreshParty(void)
{
	u32 i;

	for (i = 0; i < PARTY_SIZE; ++i)
	{
		struct Pokemon* mon = &gPlayerParty[i];

		if (mon->species == SPECIES_NONE)
			continue;
		if (!MaxHPMatchesItem(mon))
			CalculateMonStats(mon);
	}
}

/* ---- 交換に出せない（2026-10-09 ユーザー「ももアイスをトレード不可アイテムに」→「持たせた子を交換に出せない」・
 * 通信交換・ミラクル交換・ゲームの中の人との交換）---- */

//通信交換: 素の CanTradeSelectedMon（0804F1E4）の入口（assembly/hack_momo_ice/momo_ice_hooks.s）から。TRUE なら交換に出せない
bool8 HackMomoIce_TradeBlocked(struct Pokemon* party, u32 idx)
{
	return idx < PARTY_SIZE && HackMomoIce_Held(&party[idx]);
}

//ゲームの中の人との交換: 素の GetTradeSpecies（special 0xFF・080535F0）の置き換え（routinepointers 08163464）。
//ももアイスを持っていれば種族なし＝交換の人は「たのんだ ポケモンじゃない」の返事になる（人ごとの台本は書き換えない）
u16 HackMomoIce_GetTradeSpecies(void)
{
	struct Pokemon* mon = &gPlayerParty[*(u16*) 0x02036FF6];   //gSpecialVar_0x8005

	if (GetMonData(mon, MON_DATA_IS_EGG, NULL) || HackMomoIce_Held(mon))
		return SPECIES_NONE;
	return GetMonData(mon, MON_DATA_SPECIES, NULL);
}
