#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/constants/hold_effects.h"
#include "../include/new/battle_util.h"
#include "../include/new/util.h"

/*
    くりーぱーアイス（もたせる道具・ITEM_CREEPER_ICE 0x359・ITEM_EFFECT_CREEPER_ICE 160・2026-10-11・patches/0333）。
    ユーザー「毎ターン HPを少しずつ回復し、自爆技全般のダメージ量を20%に軽減する。
              5ターン後に完全に自爆してトレーナーごとポケモンセンター送りにする。」資料: docs/research/2026-10-11_creeper-ice.md

    - 回復: ターンの終わりに最大 HP の 1/16（たべのこし と同じ流れ・item_battle_effects.c）。
    - じばく・だいばくはつ・ミストバースト（技の効き目 EFFECT_EXPLOSION）で持ち主が受けるダメージは 20%（damage_calc.c・威力を 1/5）。
    - 持ち主が場に出たまま 5 回目のターンの終わり（end_turn.c の ET_Orbz）で爆発し、持ち主の側の手持ちが全員ひんし＝持ち主の側の負け
      （自分なら素の全滅の流れでポケモンセンターへ・相手なら こちらの勝ち）。それまでは毎ターン残りを文で出す。
      数は場ごと（gNewBS->hackCreeperIceTurns）で、別の子が場に出たら 0 から（gNewBS->hackCreeperIceMon で見分ける）。
    - 通信・施設（バトルフロンティアなど）では爆発しない（回復と 20% だけ・ユーザーの答え 2026-10-11）。
*/

#define CREEPER_ICE_TURNS 5

extern const u8 BattleScript_HackCreeperIceFuse[];
extern const u8 BattleScript_HackCreeperIceExplode[];
extern const u8 BattleScript_HackCreeperIceExplodeLoop[];
extern const u8 gText_HackCreeperIce_Fuse4[];
extern const u8 gText_HackCreeperIce_Fuse3[];
extern const u8 gText_HackCreeperIce_Fuse2[];
extern const u8 gText_HackCreeperIce_Fuse1[];

static const u8* const sFuseTexts[CREEPER_ICE_TURNS - 1] =
{
	gText_HackCreeperIce_Fuse4, gText_HackCreeperIce_Fuse3, gText_HackCreeperIce_Fuse2, gText_HackCreeperIce_Fuse1,
};

//爆発してよい戦いか（通信・施設・チュートリアルなどでは爆発しない）
static bool8 CanExplode(void)
{
	if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_TOWER_LINK_MULTI | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_TOWER
	                        | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_OLD_MAN | BATTLE_TYPE_POKE_DUDE | BATTLE_TYPE_OAK_TUTORIAL))
		return FALSE;
	if (FlagGet(FLAG_BATTLE_FACILITY))
		return FALSE;
	return TRUE;
}

//場に出ていない手持ちを全員ひんしに（持ち主の側だけ）
static void FaintBenchedMons(u8 bank)
{
	struct Pokemon* party = (SIDE(bank) == B_SIDE_PLAYER) ? gPlayerParty : gEnemyParty;
	u32 i;
	u16 zero = 0;

	for (i = 0; i < PARTY_SIZE; ++i)
	{
		if (i == gBattlerPartyIndexes[bank])
			continue;
		if (IS_DOUBLE_BATTLE && i == gBattlerPartyIndexes[PARTNER(bank)] && BATTLER_ALIVE(PARTNER(bank)))
			continue;   //場に出ている味方は台本の HP の書き換えで（ひんしの流れに乗せる）
		if (GetMonData(&party[i], MON_DATA_SPECIES, NULL) != SPECIES_NONE && !GetMonData(&party[i], MON_DATA_IS_EGG, NULL)   //タマゴは触らない（5e）
		 && GetMonData(&party[i], MON_DATA_HP, NULL) != 0)
			SetMonData(&party[i], MON_DATA_HP, &zero);
	}
}

//ターンの終わり（end_turn.c の ET_Orbz・持ち主が場に出ていて生きているとき）。出す台本（なければ NULL）
const u8* HackCreeperIce_EndTurn(u8 bank)
{
	u8 mon = gBattlerPartyIndexes[bank] + 1;
	u8 turns;

	if (gNewBS->hackCreeperIceMon[bank] != mon)
	{
		gNewBS->hackCreeperIceMon[bank] = mon;
		gNewBS->hackCreeperIceTurns[bank] = 0;
	}
	if (!CanExplode())
		return NULL;
	turns = ++gNewBS->hackCreeperIceTurns[bank];
	gBattleScripting.bank = bank;
	gStringBank = bank;
	gLastUsedItem = ITEM(bank);
	if (turns < CREEPER_ICE_TURNS)
	{
		gBattleStringLoader = sFuseTexts[turns - 1];
		return BattleScript_HackCreeperIceFuse;
	}
	gNewBS->hackCreeperIceTurns[bank] = 0;
	FaintBenchedMons(bank);
	gBattleMoveDamage = gBattleMons[bank].hp;
	return BattleScript_HackCreeperIceExplode;
}

//爆発の台本から（callasm）: ダブルで場に出ている味方がまだ生きていれば、その子も同じ流れでひんしに（台本の頭へ戻る）
void HackCreeperIce_NextVictim(void)
{
	u8 partner = PARTNER(gBattleScripting.bank);

	if (IS_DOUBLE_BATTLE && BATTLER_ALIVE(partner) && !(gAbsentBattlerFlags & gBitTable[partner]))
	{
		gBattleScripting.bank = partner;
		gBattleMoveDamage = gBattleMons[partner].hp;
		gBattlescriptCurrInstr = BattleScript_HackCreeperIceExplodeLoop - 5;
	}
}

//damage_calc.c から: じばく・だいばくはつ・ミストバースト の威力を、受ける側が持っていれば 1/5
u16 HackCreeperIce_AdjustPower(u16 power, u16 move, u8 defItemEffect)
{
	if (defItemEffect == ITEM_EFFECT_CREEPER_ICE && gBattleMoves[move].effect == EFFECT_EXPLOSION)
		return MathMax(1, power / 5);
	return power;
}
