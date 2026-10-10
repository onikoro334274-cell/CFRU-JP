#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/random.h"

#include "../include/new/ai_master.h"
#include "../include/new/battle_controller_opponent.h"
#include "../include/new/battle_util.h"
#include "../include/new/move_menu.h"
#include "Tables/hoenn_script_flags.h"

/*
 * ホウエンのバトルパレスの戦い（#5・2026-09-28 ユーザー「バトルフロンティアの続き進めて」→ ブレーンも入れる）
 * docs/research/2026-09-28_battle-frontier-npcs.md の「バトルパレスの進め方」
 *
 * エメラルドのパレスは「主人公が技を選ばず、ポケモンが性格で技を決める」。FRLG にも CFRU にも無いので
 * pokeemerald の次の所を CFRU の上に書き直した:
 *   - 技と狙いの選び方      … src/battle_gfx_sfx_util.c ChooseMoveAndTargetInBattlePalace / GetBattlePalaceMoveGroup / GetBattlePalaceTarget
 *   - 性格の表 2 つ          … src/battle_script_commands.c gBattlePalaceNatureToMoveGroupLikelihood / src/battle_anim_smokescreen.c gBattlePalaceNatureToMoveTarget
 *   - うまく技が出せない     … src/battle_util.c:263（palaceUnableToUseMove → BattleScript_MoveUsedLoafingAround の INCAPABLE_OF_POWER）
 *   - HP 半分のセリフ        … src/battle_main.c:4015（BattleTurnPassed）・src/battle_script_commands.c:6387（VARIOUS_PALACE_FLAVOR_TEXT）
 *   - 入れ替えたときの印     … src/battle_script_commands.c:4659（Cmd_switchindataupdate）
 *
 * 差し込み口（上流は数行だけ・patches/0085）:
 *   - 主人公・相手の技選び … 素の命令の表（0x0820D2F4 / 0x0820D48C）を repoints でこちらの関数へ。パレスでなければ元の関数へ渡す
 *   - 技を出す所           … CFRU src/battle_start_turn_start.c HandleAction_UseMove → HoennPalace_TryIncapable
 *   - ターンの頭           … CFRU src/trainer_sliding.c CheckLastMonLowHPSlide（素の BattleTurnPassed の最後から毎ターン呼ばれる）→ HoennPalace_TryFlavorText
 *   - 入れ替え             … CFRU src/switching.c atk4D_switchindataupdate → HoennPalace_OnSwitchIn
 *
 * ★ 「パレスの戦いか」は FLAG_BATTLE_FACILITY（施設の戦い）＋ HOENN_VAR_FRONTIER_FACILITY == パレス ＋ トレーナー戦。
 *   ふつうの戦いでは 1 つも効かない（どの関数も最初にこれを見る）
 * ★ 状態の置き場（6.13・static は使えない）:
 *   - HP 半分の印（エメラルドの palaceFlags の下 4 ビット）… gDisableStructs[b].unkD（素でも CFRU でも使っていない。
 *     入れ替え・ひんしで素のコードが 0 に戻すので、エメラルドの「入れ替えで消す」と同じになる）
 *   - うまく出せない印 … gProtectStructs[b].palaceUnableToUseMove（CFRU が名前だけ用意して使っていない。ターンの終わりに素が 0 に戻す）
 *
 * エメラルドとの違い（わざと）:
 *   - 引いた組の技が全部使えない（かなしばり・いちゃもん・ちょうはつ・ふういん・こだわり）ときは「うまく出せない」にする。
 *     エメラルドはその技を選んでから「かなしばりで だせない」などを出す（CFRU の選ぶ画面の確かめは、選び直しを求めるので使えない）
 *   - ねむり・こおりの子が「うまく出せない」を引いたときは、ふつうの技の流れに任せる（ねむり・こおりの判定は CFRU の attackcanceler が行う。
 *     エメラルドは palacetryescapestatus で同じ判定をしてから「うまく出せない」を出す）
 *   - メガシンカ・Z 技・ダイマックス・テラスタルはしない（エメラルドに無い・技を選ぶ画面が無い）
 */

#define HOENN_FACILITY_PALACE 1             //HOENN_VAR_FRONTIER_FACILITY（0 タワー / 1 パレス）

#define PALACE_MOVE_GROUP_ATTACK  0
#define PALACE_MOVE_GROUP_DEFENSE 1
#define PALACE_MOVE_GROUP_SUPPORT 2

#define PALACE_TARGET_STRONGER 0
#define PALACE_TARGET_WEAKER   1
#define PALACE_TARGET_RANDOM   2

#define PALACE_DANGER 1                     //gDisableStructs[b].unkD: HP が半分以下になった（後ろの確率を使う）

#define PlayerHandleChooseMove_Vanilla ((void (*)(void)) (0x08032404 | 1))

extern const u8 HoennPalace_BattleScript_FlavorText[];
extern const u8 HoennPalace_BattleScript_FlavorEnd[];
extern const u8 HoennPalace_BattleScript_Incapable[];
extern const u8 HoennPalaceText_GlintInEye[];
extern const u8 HoennPalaceText_GettingInPos[];
extern const u8 HoennPalaceText_GrowlDeeply[];
extern const u8 HoennPalaceText_EagerForMore[];

//エメラルドの PALACE_STYLE(atk, def, atkLow, defLow) = {atk, atk + def, atkLow, atkLow + defLow}
#define PALACE_STYLE(atk, def, atkLow, defLow) {atk, atk + def, atkLow, atkLow + defLow}
static const u8 sNatureToMoveGroupLikelihood[NUM_NATURES][4] =
{
	[NATURE_HARDY]   = PALACE_STYLE(61,  7, 61,  7),
	[NATURE_LONELY]  = PALACE_STYLE(20, 25, 84,  8),
	[NATURE_BRAVE]   = PALACE_STYLE(70, 15, 32, 60),
	[NATURE_ADAMANT] = PALACE_STYLE(38, 31, 70, 15),
	[NATURE_NAUGHTY] = PALACE_STYLE(20, 70, 70, 22),
	[NATURE_BOLD]    = PALACE_STYLE(30, 20, 32, 58),
	[NATURE_DOCILE]  = PALACE_STYLE(56, 22, 56, 22),
	[NATURE_RELAXED] = PALACE_STYLE(25, 15, 75, 15),
	[NATURE_IMPISH]  = PALACE_STYLE(69,  6, 28, 55),
	[NATURE_LAX]     = PALACE_STYLE(35, 10, 29,  6),
	[NATURE_TIMID]   = PALACE_STYLE(62, 10, 30, 20),
	[NATURE_HASTY]   = PALACE_STYLE(58, 37, 88,  6),
	[NATURE_SERIOUS] = PALACE_STYLE(34, 11, 29, 11),
	[NATURE_JOLLY]   = PALACE_STYLE(35,  5, 35, 60),
	[NATURE_NAIVE]   = PALACE_STYLE(56, 22, 56, 22),
	[NATURE_MODEST]  = PALACE_STYLE(35, 45, 34, 60),
	[NATURE_MILD]    = PALACE_STYLE(44, 50, 34,  6),
	[NATURE_QUIET]   = PALACE_STYLE(56, 22, 56, 22),
	[NATURE_BASHFUL] = PALACE_STYLE(30, 58, 30, 58),
	[NATURE_RASH]    = PALACE_STYLE(30, 13, 27,  6),
	[NATURE_CALM]    = PALACE_STYLE(40, 50, 25, 62),
	[NATURE_GENTLE]  = PALACE_STYLE(18, 70, 90,  5),
	[NATURE_SASSY]   = PALACE_STYLE(88,  6, 22, 20),
	[NATURE_CAREFUL] = PALACE_STYLE(42, 50, 42,  5),
	[NATURE_QUIRKY]  = PALACE_STYLE(56, 22, 56, 22),
};

static const u8 sNatureToMoveTarget[NUM_NATURES] =
{
	[NATURE_HARDY]   = PALACE_TARGET_STRONGER,
	[NATURE_LONELY]  = PALACE_TARGET_STRONGER,
	[NATURE_BRAVE]   = PALACE_TARGET_WEAKER,
	[NATURE_ADAMANT] = PALACE_TARGET_STRONGER,
	[NATURE_NAUGHTY] = PALACE_TARGET_WEAKER,
	[NATURE_BOLD]    = PALACE_TARGET_WEAKER,
	[NATURE_DOCILE]  = PALACE_TARGET_RANDOM,
	[NATURE_RELAXED] = PALACE_TARGET_STRONGER,
	[NATURE_IMPISH]  = PALACE_TARGET_STRONGER,
	[NATURE_LAX]     = PALACE_TARGET_STRONGER,
	[NATURE_TIMID]   = PALACE_TARGET_WEAKER,
	[NATURE_HASTY]   = PALACE_TARGET_WEAKER,
	[NATURE_SERIOUS] = PALACE_TARGET_WEAKER,
	[NATURE_JOLLY]   = PALACE_TARGET_STRONGER,
	[NATURE_NAIVE]   = PALACE_TARGET_RANDOM,
	[NATURE_MODEST]  = PALACE_TARGET_WEAKER,
	[NATURE_MILD]    = PALACE_TARGET_STRONGER,
	[NATURE_QUIET]   = PALACE_TARGET_WEAKER,
	[NATURE_BASHFUL] = PALACE_TARGET_WEAKER,
	[NATURE_RASH]    = PALACE_TARGET_STRONGER,
	[NATURE_CALM]    = PALACE_TARGET_STRONGER,
	[NATURE_GENTLE]  = PALACE_TARGET_STRONGER,
	[NATURE_SASSY]   = PALACE_TARGET_WEAKER,
	[NATURE_CAREFUL] = PALACE_TARGET_WEAKER,
	[NATURE_QUIRKY]  = PALACE_TARGET_STRONGER,
};

//エメラルドの sBattlePalaceNatureToFlavorTextId（0 めの いろ / 1 たいせい / 2 うなり / 3 やるき）
static const u8 sNatureToFlavorText[NUM_NATURES] =
{
	[NATURE_HARDY]   = 3, [NATURE_LONELY]  = 0, [NATURE_BRAVE]   = 1, [NATURE_ADAMANT] = 0, [NATURE_NAUGHTY] = 0,
	[NATURE_BOLD]    = 1, [NATURE_DOCILE]  = 3, [NATURE_RELAXED] = 0, [NATURE_IMPISH]  = 1, [NATURE_LAX]     = 2,
	[NATURE_TIMID]   = 2, [NATURE_HASTY]   = 0, [NATURE_SERIOUS] = 3, [NATURE_JOLLY]   = 1, [NATURE_NAIVE]   = 3,
	[NATURE_MODEST]  = 1, [NATURE_MILD]    = 2, [NATURE_QUIET]   = 3, [NATURE_BASHFUL] = 3, [NATURE_RASH]    = 2,
	[NATURE_CALM]    = 1, [NATURE_GENTLE]  = 0, [NATURE_SASSY]   = 2, [NATURE_CAREFUL] = 2, [NATURE_QUIRKY]  = 3,
};

static const u8* const sFlavorTexts[4] =
{
	HoennPalaceText_GlintInEye, HoennPalaceText_GettingInPos, HoennPalaceText_GrowlDeeply, HoennPalaceText_EagerForMore,
};

bool8 HoennPalace_IsActive(void)
{
	return (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
		&& FlagGet(FLAG_BATTLE_FACILITY)
		&& VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_PALACE;
}

static u8 BattlerNature(u8 bank)
{
	return GetNatureFromPersonality(gBattleMons[bank].personality);
}

//HP が半分以下で、ひんしでもねむりでもない（エメラルドの印を立てる条件）
static bool8 InDanger(u8 bank)
{
	return gBattleMons[bank].maxHP / 2 >= gBattleMons[bank].hp
		&& gBattleMons[bank].hp != 0
		&& !(gBattleMons[bank].status1 & STATUS1_SLEEP);
}

static u8 GetMoveGroup(u16 move)
{
	switch (gBattleMoves[move].target) {
		case MOVE_TARGET_SELECTED:
		case MOVE_TARGET_USER_OR_PARTNER: //エメラルドの MOVE_TARGET_USER_OR_SELECTED（同じ 0x2）
		case MOVE_TARGET_RANDOM:
		case MOVE_TARGET_BOTH:
		case MOVE_TARGET_FOES_AND_ALLY:
			return (gBattleMoves[move].power == 0) ? PALACE_MOVE_GROUP_SUPPORT : PALACE_MOVE_GROUP_ATTACK;
		case MOVE_TARGET_DEPENDS:
		case MOVE_TARGET_OPPONENTS_FIELD:
			return PALACE_MOVE_GROUP_SUPPORT;
		case MOVE_TARGET_USER:
			return PALACE_MOVE_GROUP_DEFENSE;
		default:
			return PALACE_MOVE_GROUP_ATTACK;
	}
}

//ダブルの狙い（エメラルドの GetBattlePalaceTarget）。シングルは向かいの子
static u8 GetPalaceTarget(u8 bank)
{
	if (IS_DOUBLE_BATTLE)
	{
		u8 opposing1, opposing2;

		if (SIDE(bank) == B_SIDE_PLAYER)
		{
			opposing1 = GetBattlerAtPosition(B_POSITION_OPPONENT_LEFT);
			opposing2 = GetBattlerAtPosition(B_POSITION_OPPONENT_RIGHT);
		}
		else
		{
			opposing1 = GetBattlerAtPosition(B_POSITION_PLAYER_LEFT);
			opposing2 = GetBattlerAtPosition(B_POSITION_PLAYER_RIGHT);
		}

		if (gBattleMons[opposing1].hp == gBattleMons[opposing2].hp)
			return ((bank & BIT_SIDE) ^ BIT_SIDE) + (Random() & 2);

		switch (sNatureToMoveTarget[BattlerNature(bank)]) {
			case PALACE_TARGET_STRONGER:
				return (gBattleMons[opposing1].hp > gBattleMons[opposing2].hp) ? opposing1 : opposing2;
			case PALACE_TARGET_WEAKER:
				return (gBattleMons[opposing1].hp < gBattleMons[opposing2].hp) ? opposing1 : opposing2;
			default:
				return ((bank & BIT_SIDE) ^ BIT_SIDE) + (Random() & 2);
		}
	}

	return bank ^ BIT_SIDE;
}

//使える技のうち最初のもの（「うまく出せない」のときに CFRU の選ぶ画面の確かめを通すための番号）
static u8 FirstUsableMove(u8 bank, u8 unusable)
{
	u32 i;

	for (i = 0; i < MAX_MON_MOVES; ++i)
	{
		if (gBattleMons[bank].moves[i] != MOVE_NONE && !(gBitTable[i] & unusable))
			return i;
	}
	return 0;
}

//エメラルドの ChooseMoveAndTargetInBattlePalace。→ 下位 8 ビット = 技の番号・上位 8 ビット = 狙い
static u16 ChooseMoveAndTarget(void)
{
	u8 bank = gActiveBattler;
	u8 nature = BattlerNature(bank);
	u8 unusable = CheckMoveLimitations(bank, 0, 0xFF);
	s32 percent = Random() % 100;
	s32 chosen = -1;
	u32 i, base;
	u8 group, selected, usableSelected, moveTarget;
	u16 move;

	//HP が半分以下（印）なら後ろの 2 つの確率
	base = (gDisableStructs[bank].unkD & PALACE_DANGER) ? 2 : 0;
	for (i = base; i < base + 2; ++i)
	{
		if (sNatureToMoveGroupLikelihood[nature][i] > percent)
			break;
	}
	group = (i == base + 2) ? PALACE_MOVE_GROUP_SUPPORT : i - base;

	//引いた組の、PP の残っている技
	for (selected = 0, i = 0; i < MAX_MON_MOVES; ++i)
	{
		if (gBattleMons[bank].moves[i] == MOVE_NONE)
			break;
		if (group == GetMoveGroup(gBattleMons[bank].moves[i]) && gBattleMons[bank].pp[i] != 0)
			selected |= gBitTable[i];
	}

	usableSelected = selected & ~unusable;
	if (selected != 0 && usableSelected == 0)
	{
		//★ 引いた組の技はあるが全部使えない（かなしばり等）→ うまく出せない（エメラルドとの違い・冒頭）
		gProtectStructs[bank].palaceUnableToUseMove = TRUE;
		return FirstUsableMove(bank, unusable) | ((bank ^ BIT_SIDE) << 8);
	}

	//その技だけを AI に渡して 1 つ選ばせる
	if (usableSelected != 0)
	{
		BattleAI_SetupAIData(usableSelected);
		chosen = BattleAI_ChooseMoveOrAction();
		if (chosen < 0 || chosen >= MAX_MON_MOVES || !(usableSelected & gBitTable[chosen]))
		{
			do
			{
				chosen = Random() % MAX_MON_MOVES;
			} while (!(usableSelected & gBitTable[chosen]));
		}
	}

	//引いた組の技が無い → ほかの組から選び直す（AI は見ない）。50% でうまく出せない
	if (chosen == -1)
	{
		if (unusable != 0xF)
		{
			u32 numMovesPerGroup = 0, numMultipleMoveGroups = 0;
			u8 randSelectGroup = PALACE_MOVE_GROUP_ATTACK;

			for (i = 0; i < MAX_MON_MOVES; ++i)
			{
				if (gBattleMons[bank].moves[i] == MOVE_NONE || (gBitTable[i] & unusable))
					continue;
				switch (GetMoveGroup(gBattleMons[bank].moves[i])) {
					case PALACE_MOVE_GROUP_ATTACK:
						numMovesPerGroup += (1 << 0);
						break;
					case PALACE_MOVE_GROUP_DEFENSE:
						numMovesPerGroup += (1 << 4);
						break;
					default:
						numMovesPerGroup += (1 << 8);
						break;
				}
			}

			//★ エメラルドの不具合もそのまま（へんかの組の数え方が ぼうぎょの数を見ていて、決して数えられない）
			if ((numMovesPerGroup & 0xF) >= 2)
				++numMultipleMoveGroups;
			if ((numMovesPerGroup & (0xF << 4)) >= (2 << 4))
				++numMultipleMoveGroups;
			if ((numMovesPerGroup & (0xF << 4)) >= (2 << 8))
				++numMultipleMoveGroups;

			if (numMultipleMoveGroups != 1)
			{
				do
				{
					i = Random() % MAX_MON_MOVES;
					if (gBattleMons[bank].moves[i] != MOVE_NONE && !(gBitTable[i] & unusable))
						chosen = i;
				} while (chosen == -1);
			}
			else
			{
				if ((numMovesPerGroup & 0xF) >= 2)
					randSelectGroup = PALACE_MOVE_GROUP_ATTACK;
				if ((numMovesPerGroup & (0xF << 4)) >= (2 << 4))
					randSelectGroup = PALACE_MOVE_GROUP_DEFENSE;
				if ((numMovesPerGroup & (0xF << 4)) >= (2 << 8))
					randSelectGroup = PALACE_MOVE_GROUP_SUPPORT;

				do
				{
					i = Random() % MAX_MON_MOVES;
					if (gBattleMons[bank].moves[i] != MOVE_NONE && !(gBitTable[i] & unusable)
					&& randSelectGroup == GetMoveGroup(gBattleMons[bank].moves[i]))
						chosen = i;
				} while (chosen == -1);
			}

			if (Random() % 100 >= 50)
			{
				gProtectStructs[bank].palaceUnableToUseMove = TRUE;
				return chosen | ((bank ^ BIT_SIDE) << 8);
			}
		}
		else
		{
			//使える技が 1 つも無い（ふつうは わるあがき の判定が先に来るのでここへは来ない）
			gProtectStructs[bank].palaceUnableToUseMove = TRUE;
			return 0 | ((bank ^ BIT_SIDE) << 8);
		}
	}

	move = gBattleMons[bank].moves[chosen];
	if (move == MOVE_CURSE)
		moveTarget = IsOfType(bank, TYPE_GHOST) ? MOVE_TARGET_SELECTED : MOVE_TARGET_USER;
	else
		moveTarget = gBattleMoves[move].target;

	if (moveTarget & MOVE_TARGET_USER)
		return chosen | (bank << 8);
	else if (moveTarget == MOVE_TARGET_SELECTED)
		return chosen | (GetPalaceTarget(bank) << 8);
	else
		return chosen | (GetBattlerAtPosition(SIDE(bank) ^ BIT_SIDE) << 8);
}

static void EmitPalaceChoice(void)
{
	u16 choice = ChooseMoveAndTarget();

	EmitMoveChosen(1, choice & 0xFF, choice >> 8, FALSE, FALSE, FALSE, FALSE, FALSE);
}

//主人公の技選び（素の命令の表 0x0820D2F4 の CONTROLLER_CHOOSEMOVE・repoints）
void HoennPalace_PlayerHandleChooseMove(void)
{
	if (!HoennPalace_IsActive())
	{
		PlayerHandleChooseMove_Vanilla();
		return;
	}

	EmitPalaceChoice();
	PlayerBufferExecCompleted();
}

//相手の技選び（素の命令の表 0x0820D48C・CFRU の OpponentHandleChooseMove の前に）
void HoennPalace_OpponentHandleChooseMove(void)
{
	if (!HoennPalace_IsActive())
	{
		OpponentHandleChooseMove();
		return;
	}

	EmitPalaceChoice();
	OpponentBufferExecCompleted();
}

//技を出す所（CFRU HandleAction_UseMove・patches/0085）。TRUE なら「うまく技が出せない」の戦闘スクリプトにした
bool8 HoennPalace_TryIncapable(void)
{
	u8 bank = gBankAttacker;

	if (!gProtectStructs[bank].palaceUnableToUseMove)
		return FALSE;
	gProtectStructs[bank].palaceUnableToUseMove = FALSE;
	if (!HoennPalace_IsActive())
		return FALSE;
	if (gBattleMons[bank].status1 & (STATUS1_SLEEP | STATUS1_FREEZE))
		return FALSE; //★ ねむり・こおりはふつうの流れに任せる（冒頭）

	gBattlescriptCurrInstr = HoennPalace_BattleScript_Incapable;
	return TRUE;
}

//入れ替えたとき（CFRU atk4D_switchindataupdate・patches/0085）。すでに HP 半分以下なら黙って印を立てる
void HoennPalace_OnSwitchIn(u8 bank)
{
	if (HoennPalace_IsActive() && InDanger(bank))
		gDisableStructs[bank].unkD |= PALACE_DANGER;
}

static bool8 NeedsFlavorText(u8 bank)
{
	return !(gDisableStructs[bank].unkD & PALACE_DANGER) && InDanger(bank);
}

//ターンの頭（CFRU CheckLastMonLowHPSlide・patches/0085）。HP が半分以下になったばかりの子がいればセリフの戦闘スクリプトへ
void HoennPalace_TryFlavorText(void)
{
	u32 i;

	if (!HoennPalace_IsActive())
		return;

	for (i = 0; i < gBattlersCount; ++i)
	{
		if (NeedsFlavorText(i))
		{
			BattleScriptExecute(HoennPalace_BattleScript_FlavorText);
			return;
		}
	}
}

//セリフの戦闘スクリプトから（callasm）。次の子の印を立ててセリフを用意する。いなければ終わりへ跳ぶ
void HoennPalace_FlavorNext(void)
{
	u32 i;

	for (i = 0; i < gBattlersCount; ++i)
	{
		if (NeedsFlavorText(i))
		{
			gDisableStructs[i].unkD |= PALACE_DANGER;
			gBattleScripting.bank = i;
			gBattleStringLoader = sFlavorTexts[sNatureToFlavorText[BattlerNature(i)]];
			return;
		}
	}

	gBattlescriptCurrInstr = HoennPalace_BattleScript_FlavorEnd - 5; //callasm が 5 足す
}
