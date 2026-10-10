#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/random.h"
#include "../include/new/battle_util.h"
#include "hack_affection.h"

/*
    なつき度で戦いに効く 10 の効果（2026-10-09 ユーザー「原作である親愛度システムでかなしませないためにふんばったみたいな
    システム導入してほしい」→ ふんばる・状態異常を治す・よける・きゅうしょ ＋ 独自の案 6 つを「全部やな」）。
    資料: docs/research/2026-10-09_affection.md

    効くのは自分のポケモンだけ（相手・マルチバトルの味方には効かない）。フロンティア（施設の戦い・FLAG_BATTLE_FACILITY）・
    通信対戦・トレーナータワー・e+ では効かない。確率は なつき度 255 / 220 以上（仮の値・資料の表）。
      ふんばる（ひんしになるダメージを HP 1 で）        10% / 5%    src/damage_calc.c の AdjustDamage
      状態異常を治す（ターンの終わり）                  20% / 10%   src/end_turn.c（だっぴと同じ所）
      よける                                            10% / 5%    src/accuracy_calc.c
      きゅうしょ                                        +1 段 / 0   src/damage_calc.c の atk04_critcalc
      1 きずなのいちげき（HP 1/4 以下で威力 1.5 倍）    10% / 5%    atk05_damagecalc の前（1 回の技で 1 回だけ決める）
      2 さいごの いっぴき（戦える子がこの子だけ）       上の確率が全部 2 倍・最初に技を出すとき文（1 戦闘 1 回）
      3 かばう（ダブルで味方がひんしになる技を受ける）  10% / 5%    atk05_damagecalc の後（相手の単体の技だけ）
      4 ふりかえる（ねむり・こおり・まひで動けない）    10% / 5%    src/attackcanceler.c
      5 よろこびの けいけん（勝ったとき技を出した子）   30% で なつき度 +2（なつき度に関係なく）・src/end_battle.c
      6 なかよしの まもり（相手に能力を下げられる）     10% / 5%    src/stat_buffs.c
    1 回の技の間の印は gNewBS->hackAffectionMove（技の始め＝ CANCELLER_FLAGS で 0）・2 の文は hackAffectionLastMon（場のビット）・
    5 の「技を出した子」は hackAffectionUsed（手持ちの番号のビット）。gNewBS は戦いごとに 0 から（Calloc）。
*/

#define AFFECTION_HIGH 220
#define AFFECTION_MAX 255
#define JOY_CHANCE 30
#define JOY_FRIENDSHIP 2

#define MOVE_BOND_CHECKED  0x01
#define MOVE_BOND_ACTIVE   0x02
#define MOVE_COVER_CHECKED 0x04

static bool8 AllowedBattle(void)
{
	if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_TOWER_LINK_MULTI | BATTLE_TYPE_FRONTIER | BATTLE_TYPE_TRAINER_TOWER
	                        | BATTLE_TYPE_EREADER_TRAINER | BATTLE_TYPE_OLD_MAN | BATTLE_TYPE_POKE_DUDE | BATTLE_TYPE_OAK_TUTORIAL))
		return FALSE;
	if (FlagGet(FLAG_BATTLE_FACILITY))
		return FALSE;
	return TRUE;
}

static bool8 IsOwnBank(u8 bank)
{
	if (SIDE(bank) != B_SIDE_PLAYER)
		return FALSE;
	if ((gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER) && GetBattlerPosition(bank) == B_POSITION_PLAYER_RIGHT)
		return FALSE;   //マルチバトルの味方
	return AllowedBattle();
}

//0 = なし・1 = 220 以上・2 = 255
static u8 AffectionLevel(u8 bank)
{
	struct Pokemon* mon;

	if (bank >= gBattlersCount || !IsOwnBank(bank) || !BATTLER_ALIVE(bank))
		return 0;
	mon = GetBankPartyData(bank);
	if (mon->friendship >= AFFECTION_MAX)
		return 2;
	if (mon->friendship >= AFFECTION_HIGH)
		return 1;
	return 0;
}

//戦える子が 1 匹だけ（マルチバトルの味方の手持ちは数えない）
static bool8 IsLastMon(void)
{
	u32 i, count = 0;
	u32 limit = (gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER) ? 3 : PARTY_SIZE;

	for (i = 0; i < limit; ++i)
	{
		struct Pokemon* mon = &gPlayerParty[i];
		if (mon->species != SPECIES_NONE && !mon->isEgg && mon->hp > 0)
			++count;
	}
	return count == 1;
}

static bool8 Roll(u8 bank, u8 pctMax, u8 pctHigh)
{
	u8 level = AffectionLevel(bank);
	u32 pct;

	if (level == 0)
		return FALSE;
	pct = (level == 2) ? pctMax : pctHigh;
	if (IsLastMon())
		pct *= 2;
	return Random() % 100 < pct;
}

void HackAffection_MoveStart(u8 bankAtk)
{
	gNewBS->hackAffectionMove = 0;
	if (bankAtk < gBattlersCount && IsOwnBank(bankAtk))
		gNewBS->hackAffectionUsed |= gBitTable[gBattlerPartyIndexes[bankAtk]];
}

bool8 HackAffection_TryLastMonMessage(u8 bankAtk)
{
	if (gNewBS->hackAffectionLastMon & gBitTable[bankAtk])
		return FALSE;
	if (AffectionLevel(bankAtk) == 0 || !IsLastMon())
		return FALSE;
	gNewBS->hackAffectionLastMon |= gBitTable[bankAtk];
	gBattleScripting.bank = bankAtk;
	return TRUE;
}

bool8 HackAffection_TryEndure(u8 bankDef, s32 damage)
{
	if (damage < gBattleMons[bankDef].hp || gBattleMons[bankDef].hp <= 1)
		return FALSE;
	return Roll(bankDef, 10, 5);
}

bool8 HackAffection_TryCureStatus(u8 bank)
{
	if (!(gBattleMons[bank].status1 & STATUS1_ANY) || !Roll(bank, 20, 10))
		return FALSE;
	ClearBankStatus(bank);   //gBattleTextBuff1 に状態異常の名前
	gBattleScripting.bank = bank;
	BattleScriptExecute(BattleScript_AffectionCured);
	return TRUE;
}

bool8 HackAffection_TryEvade(u8 bankDef)
{
	if (SIDE(gBankAttacker) == SIDE(bankDef) || !Roll(bankDef, 10, 5))
		return FALSE;
	gBattleStringLoader = gText_Affection_Evaded;
	return TRUE;
}

u8 HackAffection_CritBonus(u8 bankAtk)
{
	u8 level = AffectionLevel(bankAtk);

	if (level == 0)
		return 0;
	return (level == 2) + IsLastMon();
}

//1 きずなのいちげき: 技の最初の damagecalc で 1 回だけ決める。出たら文を出してから damagecalc をもう一度
bool8 HackAffection_BeforeDamageCalc(void)
{
	u8 bank = gBankAttacker;

	if (gNewBS->hackAffectionMove & MOVE_BOND_CHECKED)
		return FALSE;
	gNewBS->hackAffectionMove |= MOVE_BOND_CHECKED;
	if (gBattleMons[bank].hp == 0 || gBattleMons[bank].hp * 4 > gBattleMons[bank].maxHP)
		return FALSE;
	if (!Roll(bank, 10, 5))
		return FALSE;
	gNewBS->hackAffectionMove |= MOVE_BOND_ACTIVE;
	gBattleScripting.bank = bank;
	BattleScriptPushCursor();
	gBattlescriptCurrInstr = BattleScript_AffectionBond;
	return TRUE;
}

s32 HackAffection_ModifyDamage(s32 damage)
{
	if (gNewBS->hackAffectionMove & MOVE_BOND_ACTIVE)
		return (damage * 3) / 2;
	return damage;
}

//3 かばう: ダブルで相手の単体の技が自分の子をひんしにするとき、となりの子が代わりに受ける（damagecalc を相手を変えてもう一度）
bool8 HackAffection_AfterDamageCalc(void)
{
	u8 target = gBankTarget, partner;

	if (gNewBS->hackAffectionMove & MOVE_COVER_CHECKED)
		return FALSE;
	gNewBS->hackAffectionMove |= MOVE_COVER_CHECKED;
	if (!IS_DOUBLE_BATTLE || SIDE(gBankAttacker) == B_SIDE_PLAYER || SIDE(target) != B_SIDE_PLAYER)
		return FALSE;
	if (gCurrentMove == MOVE_DRAGONDARTS || gBattleMoveDamage < gBattleMons[target].hp)
		return FALSE;
	partner = PARTNER(target);
	if (!BATTLER_ALIVE(partner) || partner == gBankAttacker || BATTLER_SEMI_INVULNERABLE(partner)
	|| gProtectStructs[partner].protected)
		return FALSE;
	if (!Roll(partner, 10, 5))
		return FALSE;

	gNewBS->criticalMultiplier[partner] = gNewBS->criticalMultiplier[target];
	gNewBS->ResultFlags[partner] = gNewBS->ResultFlags[target];
	gBankTarget = partner;
	gBattleScripting.bank = partner;
	BattleScriptPushCursor();
	gBattlescriptCurrInstr = BattleScript_AffectionCover;
	return TRUE;
}

bool8 HackAffection_TryShakeOff(u8 bankAtk)
{
	return Roll(bankAtk, 10, 5);
}

bool8 HackAffection_TryBlockStatDrop(u8 bank)
{
	if (SIDE(gBankAttacker) == SIDE(bank))
		return FALSE;   //自分や味方の技では はじかない
	return Roll(bank, 10, 5);
}

//5 よろこびの けいけん: 勝ったら、この戦いで技を出した子（手持ち 0〜5）を 30% で なつき度 +2
void HackAffection_BattleEnd(void)
{
	u32 i;

	if (!AllowedBattle() || gBattleOutcome != B_OUTCOME_WON)
		return;
	for (i = 0; i < PARTY_SIZE; ++i)
	{
		struct Pokemon* mon = &gPlayerParty[i];
		if (!(gNewBS->hackAffectionUsed & gBitTable[i]) || mon->species == SPECIES_NONE || mon->isEgg)
			continue;
		if (Random() % 100 < JOY_CHANCE)
			mon->friendship = (mon->friendship + JOY_FRIENDSHIP > AFFECTION_MAX) ? AFFECTION_MAX : mon->friendship + JOY_FRIENDSHIP;
	}
}
