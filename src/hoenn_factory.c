#include "defines.h"
#include "../include/event_data.h"
#include "../include/random.h"
#include "../include/string_util.h"
#include "../include/new/build_pokemon.h"
#include "../include/new/frontier.h"
#include "../include/new/util.h"
#include "hoenn_factory_data.h"
#include "Tables/hoenn_script_flags.h"

/*
 * ホウエンのバトルファクトリー（#5・2026-10-03 ユーザー「全部一気に」→「エメラルドどおりで一気に」）
 * docs/research/2026-09-28_battle-frontier-npcs.md の「バトルファクトリーの進め方」
 *
 * 流れはエメラルドどおり（assembly/frontier/factory_scripts.s・tools/frontier/build_frontier_factory.py が生成）。
 * ここはエメラルドの src/battle_factory.c の書き直し（スクリプトから callasm で呼ぶ）。レンタル・交換の画面は src/hoenn_factory_screen.c。
 *
 * ★ 状態はすべて予約変数（6.13）。エメラルドのセーブの欄との対応:
 *   rentalMons[6]          → HOENN_VAR_FACTORY_RENTAL + 0〜5（選ぶ画面では候補 6 匹・挑戦中は 0〜2 が自分・3〜5 が直前の相手）
 *   gFrontierTempParty[3]  → HOENN_VAR_FACTORY_OPP + 0〜2（次の相手）
 *   factoryWinStreaks / factoryRecordWinStreaks → HOENN_VAR_FACTORY_STREAK / _MAX_STREAK（src/hoenn_tower.c が足し引きする）
 *   factoryRentsCount      → HOENN_VAR_FACTORY_RENTS（+ モード * 2 + レベル）
 *   sPerformedRentalSwap   → HOENN_VAR_FACTORY_STATE のビット 0
 *   ポケモンは 1 語 = 表の番号（下 10 ビット・src/hoenn_frontier.c の sHoennFrontierMons・エメラルドの gBattleFrontierMons と同じ並び）
 *   + 個体値 << 10（エメラルドは個体値を rentalMons[].ivs に持つ）
 * ★ エメラルドは性格値と特性の番号も持つが、こちらは Smogon の型（性格・特性つき）から毎回作るので持たない
 *   （性別だけは作るたびに変わりうる）
 * ★ 相手の個体値はエメラルドのまちがい（タワーの連勝で決める）を直した BUGFIX の形（ファクトリーの連勝）
 */

#define FACTORY_FACILITY 4                  //HOENN_VAR_FRONTIER_FACILITY（src/hoenn_tower.c の HOENN_FACILITY_FACTORY）
#define DOME_FACILITY 5                     //同じく HOENN_FACILITY_DOME
#define FACTORY_MON_ID(v) ((v) & 0x3FF)
#define FACTORY_MON_IV(v) ((v) >> 10)
#define FACTORY_PACK(id, iv) ((id) | ((iv) << 10))
#define FACTORY_NONE 0xFFFF
#define FACTORY_PARTY_SIZE 3
#define FACTORY_SELECTABLE 6
#define FACTORY_MAX_STREAK 9999
#define FACTORY_STATE_SWAPPED 1
#define FACTORY_NO_TYPE 18                  //エメラルドの NUMBER_OF_MON_TYPES（いちばん多いタイプが決まらない）
#define FACTORY_NUM_STYLES 8                //エメラルドの FACTORY_NUM_STYLES（決まらないとき）
#define FRONTIER_MONS_HIGH_TIER 849         //これより後ろはオープンレベルだけ（pokeemerald include/constants/battle_frontier_mons.h）

extern const struct BattleTowerSpread* HoennFactory_MakeSpread(struct BattleTowerSpread* out, u16 packed); //src/hoenn_frontier.c
extern const struct BattleTowerSpread* HoennFrontier_MakeBrainSpread(struct BattleTowerSpread* out, u16 tableId, u8 slot);
extern u16 HoennFactory_MonSpecies(u16 monId);
extern u16 HoennFactory_MonItem(u16 monId);
extern u16 HoennFactory_MonMove(u16 monId, u8 slot);
extern u16 HoennFrontier_ChooseNewTrainer(u16 challengeNum, u16 battleNum); //src/hoenn_frontier.c
extern void HoennFactory_CreateMon(struct Pokemon* mon, u8 level, const struct BattleTowerSpread* spread, bool8 forPlayer); //src/build_pokemon.c（patches/0128）

//エメラルドの sFixedIVTable（周回ごと・[ふつう, 7 人目]）
static const u8 sFixedIVTable[][2] = {{3, 6}, {6, 9}, {9, 12}, {12, 15}, {15, 18}, {21, 31}, {31, 31}, {31, 31}};

//エメラルドの sInitialRentalMonRanges（[レベル50 の 8 行][オープンの 8 行]・表の番号の範囲）
static const u16 sInitialRentalMonRanges[][2] =
{
	{110, 199}, {162, 266}, {267, 371}, {372, 467}, {468, 563}, {564, 659}, {660, 755}, {372, 849},
	{372, 467}, {468, 563}, {564, 659}, {660, 755}, {372, 881}, {372, 881}, {372, 881}, {372, 881},
};

static u8 ModeIndex(void)
{
	return (VarGet(HOENN_VAR_TOWER_MODE) == 1) * 2 + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0);
}

static bool8 IsOpenLevel(void)
{
	return VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0;
}

static u16 Streak(void)
{
	return VarGet(HOENN_VAR_FACTORY_STREAK + ModeIndex());
}

static u8 ChallengeNum(void)
{
	return MathMin(Streak() / 7, 255);
}

static u8 Level(void)
{
	return IsOpenLevel() ? 100 : 50;   //★ 2026-10-09 上限を 200 にしたが、レンタルは今までどおり Lv100（決まったレベルのものは 100 のまま）
}

//エメラルドの GetFactoryMonFixedIV（BUGFIX の範囲の確かめ）
static u8 FixedIV(u8 challengeNum, bool8 isLastBattle)
{
	if (challengeNum >= NELEMS(sFixedIVTable))
		challengeNum = NELEMS(sFixedIVTable) - 1;
	return sFixedIVTable[challengeNum][isLastBattle ? 1 : 0];
}

//エメラルドの GetFactoryMonId
static u16 FactoryMonId(u8 challengeNum, bool8 useBetterRange)
{
	u8 row = MathMin(challengeNum + (useBetterRange && challengeNum < 7), 7) + (IsOpenLevel() ? 8 : 0);

	return sInitialRentalMonRanges[row][0] + Random() % (sInitialRentalMonRanges[row][1] - sInitialRentalMonRanges[row][0] + 1);
}

//エメラルドの GetNumPastRentalsRank
static u8 RentalRank(void)
{
	u16 rents = VarGet(HOENN_VAR_FACTORY_RENTS + ModeIndex());

	if (rents < 15) return 0;
	if (rents < 22) return 1;
	if (rents < 29) return 2;
	if (rents < 36) return 3;
	if (rents < 43) return 4;
	return 5;
}

/* ---- スクリプトから ------------------------------------------------------------------------------ */

//ファクトリーの戦い（相手の手持ちを先に決めた 3 匹にする・src/build_pokemon.c・patches/0128）。
//★ 2026-10-03 バトルドーム（施設 5・src/hoenn_dome.c）も同じ置き場を使う（1 戦ごとに選んだ 2 匹）
bool8 HoennFactory_InFactoryBattle(void)
{
	u16 facility = VarGet(HOENN_VAR_FRONTIER_FACILITY);

	return FlagGet(FLAG_BATTLE_FACILITY) && (facility == FACTORY_FACILITY || facility == DOME_FACILITY);
}

//★ 0x8000 + 何匹目か ＝ ブレーンの決まった 3 匹（ドームのヒース・src/hoenn_frontier.c HoennFrontier_MakeBrainSpread）
const struct BattleTowerSpread* HoennFactory_OpponentSpread(struct BattleTowerSpread* out, u8 slot)
{
	u16 packed = VarGet(HOENN_VAR_FACTORY_OPP + slot % FACTORY_PARTY_SIZE);

	if (packed != FACTORY_NONE && (packed & 0x8000))
		return HoennFrontier_MakeBrainSpread(out, VarGet(VAR_FACILITY_TRAINER_ID), packed & 3);
	return HoennFactory_MakeSpread(out, packed);
}

//エメラルドの InitFactoryChallenge（受付でレポートを書く前）。連勝が切れていれば交換の回数も 0 に（エメラルドは連勝の旗が無いとき）
void HoennFactory_InitChallenge(void)
{
	u32 i;

	if (Streak() == 0)
		VarSet(HOENN_VAR_FACTORY_RENTS + ModeIndex(), 0);
	for (i = 0; i < FACTORY_SELECTABLE; ++i)
		VarSet(HOENN_VAR_FACTORY_RENTAL + i, FACTORY_NONE);
	for (i = 0; i < FACTORY_PARTY_SIZE; ++i)
		VarSet(HOENN_VAR_FACTORY_OPP + i, FACTORY_NONE);
	VarSet(HOENN_VAR_FACTORY_STATE, 0);
	VarSet(HOENN_VAR_TOWER_OPP_LEVEL, Level());
}

//エメラルドの GenerateInitialRentalMons（候補 6 匹・同じ種類は 1 組だけ許す・持ち物は重ねない・アンノーンは出さない）。
//個体値も決めておく（エメラルドの CreateFrontierFactorySelectableMons: 交換の回数の段の数だけ 1 周先の個体値）
void HoennFactory_GenerateRentals(void)
{
	u16 species[FACTORY_SELECTABLE], items[FACTORY_SELECTABLE], ids[FACTORY_SELECTABLE];
	u8 challengeNum = ChallengeNum();
	u8 rank = RentalRank();
	u16 currSpecies = SPECIES_NONE;
	u32 i = 0, j, tries = 0;

	while (i < FACTORY_SELECTABLE)
	{
		u16 monId = FactoryMonId(challengeNum, i < rank);
		u16 sp = HoennFactory_MonSpecies(monId);
		u16 item = HoennFactory_MonItem(monId);

		if (++tries > 5000) //念のため（範囲は 90 匹以上あるので届かない）
			currSpecies = SPECIES_NONE;
		if (sp == SPECIES_UNOWN && tries <= 5000)
			continue;
		for (j = 0; j < i; ++j)
		{
			if (ids[j] == monId)
				break;
			if (species[j] == sp)
			{
				if (currSpecies == SPECIES_NONE)
					currSpecies = sp;
				else
					break;
			}
		}
		if (j != i && tries <= 5000)
			continue;
		for (j = 0; j < i; ++j)
		{
			if (items[j] != ITEM_NONE && items[j] == item)
			{
				if (sp == currSpecies)
					currSpecies = SPECIES_NONE;
				break;
			}
		}
		if (j != i && tries <= 5000)
			continue;

		ids[i] = monId;
		species[i] = sp;
		items[i] = item;
		VarSet(HOENN_VAR_FACTORY_RENTAL + i, FACTORY_PACK(monId, FixedIV(challengeNum + (i < rank), FALSE)));
		++i;
	}
}

static bool8 IsRentalSpecies(u16 species)
{
	u32 i;

	for (i = 0; i < FACTORY_SELECTABLE; ++i)
	{
		u16 v = VarGet(HOENN_VAR_FACTORY_RENTAL + i);

		if (v != FACTORY_NONE && HoennFactory_MonSpecies(FACTORY_MON_ID(v)) == species)
			return TRUE;
	}
	return FALSE;
}

static bool8 IsRentalMonId(u16 monId)
{
	u32 i;

	for (i = 0; i < FACTORY_SELECTABLE; ++i)
	{
		u16 v = VarGet(HOENN_VAR_FACTORY_RENTAL + i);

		if (v != FACTORY_NONE && FACTORY_MON_ID(v) == monId)
			return TRUE;
	}
	return FALSE;
}

//相手の 3 匹を選ぶ（brain: エメラルドの FillFactoryBrainParty は番号で、ふつうの相手は種類でレンタルと重ねない）
static void GenerateOpponentMons(u8 iv, bool8 brain)
{
	u16 species[FACTORY_PARTY_SIZE], items[FACTORY_PARTY_SIZE];
	u8 challengeNum = ChallengeNum();
	u32 i = 0, k, tries = 0;

	while (i < FACTORY_PARTY_SIZE)
	{
		u16 monId = FactoryMonId(challengeNum, FALSE);
		u16 sp = HoennFactory_MonSpecies(monId);
		u16 item = HoennFactory_MonItem(monId);
		bool8 relaxed = (++tries > 5000);

		if (!relaxed)
		{
			if (sp == SPECIES_UNOWN)
				continue;
			if (brain ? IsRentalMonId(monId) : IsRentalSpecies(sp))
				continue;
			if (!IsOpenLevel() && monId > FRONTIER_MONS_HIGH_TIER)
				continue;
		}
		for (k = 0; k < i && species[k] != sp; ++k)
			;
		if (k != i)
			continue;
		for (k = 0; k < i && (items[k] == ITEM_NONE || items[k] != item); ++k)
			;
		if (k != i && !relaxed)
			continue;

		species[i] = sp;
		items[i] = item;
		VarSet(HOENN_VAR_FACTORY_OPP + i, FACTORY_PACK(monId, iv));
		++i;
	}
}

//エメラルドの GenerateOpponentMons（相手のトレーナーもここで決める・歩き姿は 0xFF00・名前は gStringVar1）
void HoennFactory_GenerateOpponent(void)
{
	u16 battleNum = VarGet(HOENN_VAR_TOWER_BATTLE_NUM);
	u16 id = HoennFrontier_ChooseNewTrainer(ChallengeNum(), battleNum);

	VarSet(VAR_FACILITY_TRAINER_ID, id);
	VarSet(VAR_RUNTIME_CHANGEABLE, gSpecialTowerTrainers[id].owNum);
	StringCopy(gStringVar1, gSpecialTowerTrainers[id].name);
	GenerateOpponentMons(FixedIV(ChallengeNum(), battleNum >= 6), FALSE);
}

//エメラルドの FillFactoryBrainParty（ダツラ: 2 周先の個体値・レンタルの番号と重ねない）
void HoennFactory_GenerateBrainMons(void)
{
	GenerateOpponentMons(FixedIV(ChallengeNum() + 2, FALSE), TRUE);
}

//エメラルドの GetOpponentMostCommonMonType → LASTRESULT（タイプ・決まらなければ 18）。
//★ エメラルドに無いタイプ（フェアリーなど）がいちばん多いときも「決まらない」にする（文が無い）
void HoennFactory_GetOpponentMonType(void)
{
	u8 counts[NUMBER_OF_MON_TYPES];
	u8 best = 0, second = 0;
	u32 i;

	Memset(counts, 0, sizeof(counts));
	for (i = 0; i < FACTORY_PARTY_SIZE; ++i)
	{
		u16 sp = HoennFactory_MonSpecies(FACTORY_MON_ID(VarGet(HOENN_VAR_FACTORY_OPP + i)));
		u8 t1 = gBaseStats[sp].type1, t2 = gBaseStats[sp].type2;

		if (t1 < NUMBER_OF_MON_TYPES)
			counts[t1]++;
		if (t2 != t1 && t2 < NUMBER_OF_MON_TYPES)
			counts[t2]++;
	}
	for (i = 1; i < NUMBER_OF_MON_TYPES; ++i)
	{
		if (counts[best] < counts[i])
			best = i;
		else if (counts[best] == counts[i])
			second = i;
	}

	if (counts[best] == 0 || (counts[best] <= counts[second] && best != second) || best >= FACTORY_NO_TYPE)
		gSpecialVar_LastResult = FACTORY_NO_TYPE;
	else
		gSpecialVar_LastResult = best;
}

static u8 MoveBattleStyle(u16 move)
{
	u32 i, j;

	for (i = 0; i < NELEMS(sMoveStyles); ++i)
		for (j = 0; sMoveStyles[i][j] != 0; ++j)
			if (sMoveStyles[i][j] == move)
				return i + 1;
	return 0;
}

//エメラルドの GetOpponentBattleStyle → LASTRESULT（0 とくになし / 1〜7 / 8 = いろいろ）
void HoennFactory_GetOpponentStyle(void)
{
	u8 points[FACTORY_NUM_STYLES];
	u32 i, j, count = 0;

	Memset(points, 0, sizeof(points));
	for (i = 0; i < FACTORY_PARTY_SIZE; ++i)
	{
		u16 monId = FACTORY_MON_ID(VarGet(HOENN_VAR_FACTORY_OPP + i));

		for (j = 0; j < MAX_MON_MOVES; ++j)
			points[MoveBattleStyle(HoennFactory_MonMove(monId, j))]++;
	}

	gSpecialVar_LastResult = 0;
	for (i = 1; i < FACTORY_NUM_STYLES; ++i)
	{
		if (points[i] >= sRequiredMoveCounts[i - 1])
		{
			gSpecialVar_LastResult = i;
			++count;
		}
	}
	if (count > 2)
		gSpecialVar_LastResult = FACTORY_NUM_STYLES;
}

//エメラルドの SetRentalsToOpponentParty（勝ったあと: 直前の相手を交換の候補 3〜5 へ）
void HoennFactory_SetOpponentMonsToRentals(void)
{
	u32 i;

	for (i = 0; i < FACTORY_PARTY_SIZE; ++i)
		VarSet(HOENN_VAR_FACTORY_RENTAL + FACTORY_PARTY_SIZE + i, VarGet(HOENN_VAR_FACTORY_OPP + i));
}

//自分の 3 匹をレンタルから作り直す（エメラルドの SetPlayerAndOpponentParties と RestorePlayerPartyHeldItems・回復も兼ねる）
void HoennFactory_BuildPlayerParty(void)
{
	struct BattleTowerSpread spread;
	u32 i;

	ZeroPlayerPartyMons();
	for (i = 0; i < FACTORY_PARTY_SIZE; ++i)
	{
		u16 v = VarGet(HOENN_VAR_FACTORY_RENTAL + i);

		if (v == FACTORY_NONE)
			continue;
		HoennFactory_CreateMon(&gPlayerParty[i], Level(), HoennFactory_MakeSpread(&spread, v), TRUE);
	}
	CalculatePlayerPartyCount();
	for (i = 0; i < PARTY_SIZE; ++i)
		gSelectedOrderFromParty[i] = (i < FACTORY_PARTY_SIZE) ? i + 1 : 0;
}

//交換の画面の前に、直前の相手（候補 3〜5）を gEnemyParty に作る（やすんだあとでも同じ中身になるように）
void HoennFactory_BuildEnemyFromRentals(void)
{
	struct BattleTowerSpread spread;
	u32 i;

	ZeroEnemyPartyMons();
	for (i = 0; i < FACTORY_PARTY_SIZE; ++i)
	{
		u16 v = VarGet(HOENN_VAR_FACTORY_RENTAL + FACTORY_PARTY_SIZE + i);

		if (v == FACTORY_NONE)
			continue;
		HoennFactory_CreateMon(&gEnemyParty[i], Level(), HoennFactory_MakeSpread(&spread, v), TRUE);
	}
}

//交換した（エメラルドの factory_setswapped）
void HoennFactory_SetSwapped(void)
{
	VarSet(HOENN_VAR_FACTORY_STATE, VarGet(HOENN_VAR_FACTORY_STATE) | FACTORY_STATE_SWAPPED);
}

//勝ったあと（エメラルドの DefeatedOpponent の WIN_STREAK_SWAPS）: 交換していれば交換の回数を 1 足す
void HoennFactory_CountRentAfterWin(void)
{
	u16 var = HOENN_VAR_FACTORY_RENTS + ModeIndex();
	u16 state = VarGet(HOENN_VAR_FACTORY_STATE);

	if (state & FACTORY_STATE_SWAPPED)
	{
		if (VarGet(var) < FACTORY_MAX_STREAK)
			VarSet(var, VarGet(var) + 1);
		VarSet(HOENN_VAR_FACTORY_STATE, state & ~FACTORY_STATE_SWAPPED);
	}
}

//連勝が切れた（負け・リタイア・書かずにやめた）: 交換の回数も 0 に（エメラルドは次の挑戦の頭）
void HoennFactory_ResetRents(void)
{
	VarSet(HOENN_VAR_FACTORY_RENTS + ModeIndex(), 0);
}

/* ---- 画面（src/hoenn_factory_screen.c）から ----------------------------------------------------------- */

u16 HoennFactory_RentalPacked(u8 i)
{
	return VarGet(HOENN_VAR_FACTORY_RENTAL + i % FACTORY_SELECTABLE);
}

void HoennFactory_SetRentalPacked(u8 i, u16 v)
{
	VarSet(HOENN_VAR_FACTORY_RENTAL + i % FACTORY_SELECTABLE, v);
}

//候補の 1 匹を作る（選ぶ画面の表示用）
void HoennFactory_CreateRentalMon(struct Pokemon* mon, u16 packed)
{
	struct BattleTowerSpread spread;

	HoennFactory_CreateMon(mon, Level(), HoennFactory_MakeSpread(&spread, packed), TRUE);
}

u16 HoennFactory_PackedSpecies(u16 packed)
{
	return HoennFactory_MonSpecies(FACTORY_MON_ID(packed));
}
