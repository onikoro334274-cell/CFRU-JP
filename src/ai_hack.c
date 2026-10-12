#include "defines.h"
#include "defines_battle.h"
#include "../include/random.h"
#include "../include/constants/abilities.h"
#include "../include/new/ai_advanced.h"
#include "../include/new/ai_master.h"
#include "../include/new/ai_util.h"
#include "../include/new/battle_util.h"
#include "../include/new/damage_calc.h"
#include "../include/new/terastal.h"

/*
    戦闘 AI の手直し（2026-10-02）

    ユーザー「AI最適化できる？」→（AskUserQuestion）「AI そのものを作り直す」→ 調べた弱いところ
    （docs/research/2026-10-02_cfru-ai-audit.md）から「回復の道具を賢く」「ダイマとテラの乱数をやめる」
    「テラスタルを守りにも使う」「交代を賢く」の 4 つ。

    ★ 上流の AI（src/Battle_AI/ の .c）は差し込み口だけ（patches/0121）。判断はこのファイル。
    ★ 書き換え可能な static は使えない（CLAUDE.md 6.13）。どれも状態を持たない。
*/

#define HACK_SECOND_ABILITY_LEVEL 50     //src/Tables/second_abilities.h の SECOND_ABILITY_UNLOCK_LEVEL と同じ（検算が見る）

u16 SecondAbility_OfSpecies(u16 species);
u16 SecondAbility_OfMon(struct Pokemon* mon);

//そのタイプの技が、この 3 つのタイプの相手にどれだけ効くか（TYPE_MUL_NORMAL = 1000）
static u32 HackAI_TypeMul(u8 moveType, u8 t1, u8 t2, u8 t3)
{
	u32 mul = TYPE_MUL_NORMAL;

	if (moveType >= NUMBER_OF_MON_TYPES)
		return mul;
	if (t1 < NUMBER_OF_MON_TYPES)
		mul = (mul * gTypeEffectiveness[moveType][t1]) / TYPE_MUL_NORMAL;
	if (t2 < NUMBER_OF_MON_TYPES && t2 != t1)
		mul = (mul * gTypeEffectiveness[moveType][t2]) / TYPE_MUL_NORMAL;
	if (t3 < NUMBER_OF_MON_TYPES && t3 != t1 && t3 != t2)
		mul = (mul * gTypeEffectiveness[moveType][t3]) / TYPE_MUL_NORMAL;
	return mul;
}

//テラスタルすると、相手の技（読み）の弱点が半減以下になるか（＝守りのテラスタル）
bool8 HackAI_TeraBlocksKO(u8 bankAtk, u8 bankDef, u16 foeMove)
{
	u8 tera, moveType;
	u32 now, after;

	if (foeMove == MOVE_NONE || SPLIT(foeMove) == SPLIT_STATUS || IsTerastal(bankAtk))
		return FALSE;

	tera = GetTeraType(bankAtk);
	if (tera >= NUMBER_OF_MON_TYPES || tera == TYPE_STELLAR || tera == TYPE_MYSTERY)
		return FALSE;

	moveType = GetMoveTypeSpecial(bankDef, foeMove);
	now = HackAI_TypeMul(moveType, gBattleMons[bankAtk].type1, gBattleMons[bankAtk].type2, gBattleMons[bankAtk].type3);
	after = HackAI_TypeMul(moveType, tera, tera, tera);
	return now > TYPE_MUL_NORMAL && after < TYPE_MUL_NORMAL;
}

//ダイマックスもテラスタルもできるとき、テラスタルを選ぶか（素は 50% の乱数・battle_controller_opponent.c）
//→ 守りのテラスタルが効く場面だけテラスタル、ほかはダイマックス
bool8 HackAI_PreferTerastal(u8 bankAtk, u8 bankDef)
{
	if (bankAtk == bankDef || !BATTLER_ALIVE(bankDef))
		return FALSE;
	return HackAI_TeraBlocksKO(bankAtk, bankDef, IsValidMovePrediction(bankDef, bankAtk));
}

//HP を回復する道具（キズぐすりなど）を使うか（素は「減った HP が回復量より多い」だけで使う・ai_master.c）
bool8 HackAI_ShouldUseHealItem(u8 bank, u16 healAmount)
{
	u8 foes[2], i;
	bool8 recover = FALSE;

	if (gBattleMons[bank].hp * 2 > gBattleMons[bank].maxHP) //まだ半分より多い
		return FALSE;

	foes[0] = FOE(bank);
	foes[1] = IS_DOUBLE_BATTLE ? PARTNER(FOE(bank)) : FOE(bank);
	for (i = 0; i < 2; ++i)
	{
		if (!BATTLER_ALIVE(foes[i]))
			continue;
		if (CanKnockOutAfterHealing(foes[i], bank, healAmount, 1)) //回復しても次で倒される → 使わずに攻める
			return FALSE;
		if (ShouldRecover(bank, foes[i], 0xFFFF))
			recover = TRUE;
	}

	if (ViableMonCountFromBank(bank) <= 1) //最後の 1 匹
		return TRUE;
	return recover || gBattleMons[bank].hp * 4 < gBattleMons[bank].maxHP;
}

//主に使う攻撃（物理か特殊か）の能力が 2 段以上下がったら交代する（素には無い）
//★ 交代を繰り返さないよう: 出てきたばかりなら交代しない・HP が半分以上・相手を倒せるなら交代しない・半分の確率
bool8 HackAI_ShouldSwitchStatsDropped(void)
{
	u8 bank = gActiveBattler;
	u8 phys = 0, spec = 0, stage, i;
	u8 foe1 = FOE(bank), foe2 = IS_DOUBLE_BATTLE ? PARTNER(FOE(bank)) : FOE(bank);

	if (gNewBS->ai.switchingCooldown[bank] || !AIFlagsHaveStrategy(AI_THINKING_STRUCT->aiFlags))
		return FALSE;
	if (gBattleMons[bank].hp * 2 < gBattleMons[bank].maxHP)
		return FALSE;

	for (i = 0; i < MAX_MON_MOVES; ++i)
	{
		u16 move = GetBattleMonMove(bank, i);
		if (move == MOVE_NONE || gBattleMoves[move].power == 0)
			continue;
		if (CalcMoveSplit(bank, move, foe1) == SPLIT_PHYSICAL)
			++phys;
		else if (CalcMoveSplit(bank, move, foe1) == SPLIT_SPECIAL)
			++spec;
	}
	if (phys == 0 && spec == 0)
		return FALSE;

	stage = gBattleMons[bank].statStages[(phys >= spec) ? STAT_STAGE_ATK : STAT_STAGE_SPATK];
	if (stage > 6 - 2) //6 = 変化なし
		return FALSE;
	if ((BATTLER_ALIVE(foe1) && CanKnockOut(bank, foe1)) || (BATTLER_ALIVE(foe2) && CanKnockOut(bank, foe2)))
		return FALSE;
	if (Random() & 1)
		return FALSE;
	if (GetMostSuitableMonToSwitchInto() >= PARTY_SIZE)
		return FALSE;

	gBattleStruct->switchoutIndex[SIDE(bank)] = PARTY_SIZE;
	EmitTwoReturnValues(1, ACTION_SWITCH, 0);
	return TRUE;
}

//交代先の点数: 場に出たときの 2 つ目の特性（いかく系・天気・フィールド）があれば +1（src/second_ability.c）
s8 HackAI_SecondAbilitySwitchBonus(struct Pokemon* mon)
{
	u16 ability;

	if (GetMonData(mon, MON_DATA_LEVEL, NULL) < HACK_SECOND_ABILITY_LEVEL)
		return 0;
	ability = SecondAbility_OfMon(mon);
	if (ability == ABILITY_NONE || ability == GetMonAbility(mon))
		return 0;

	switch (ability)
	{
		case ABILITY_INTIMIDATE:
		case ABILITY_SCARE:
		case ABILITY_MIWAKUNOUROKO:   //★ 2026-10-11 ミワクノウロコ
		case ABILITY_DRIZZLE:
		case ABILITY_SUN_AVATAR:   //★ 2026-10-11 たいようのけしん＝ひでり
		case ABILITY_DROUGHT:
		case ABILITY_SANDSTREAM:
		case ABILITY_SNOWWARNING:
		case ABILITY_ELECTRICSURGE:
		case ABILITY_PSYCHICSURGE:
		case ABILITY_MISTYSURGE:
		case ABILITY_GRASSYSURGE:
			return 1;
	}
	return 0;
}
