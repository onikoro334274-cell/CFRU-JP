#include "defines.h"
#include "defines_battle.h"
#include "hack_difficulty.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/new/item.h"
#include "../include/new/util.h"
#include "../include/new/frontier.h"
#include "../include/random.h"
#include "../include/constants/items.h"
#include "../include/constants/trainer_classes.h"
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

#define EASY_LEVEL_DOWN 3
#define MEDIUM_LEVEL_DOWN 2
#define HARD_LEVEL_UP 1
#define MOMO_ICE_LEVEL_UP 5

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

// Level boundaries and condition weights live in config.h for balancing.
static void RaiseTrainerMonLevel(struct Pokemon* mon, u32 up)
{
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	u32 level = GetMonData(mon, MON_DATA_LEVEL, NULL);
	if (level >= MAX_LEVEL)
		return;
	level = MathMin(level + up, MAX_LEVEL);
	SetMonData(mon, MON_DATA_EXP, &gExperienceTables[gBaseStats[species].growthRate][level]);
	SetMonData(mon, MON_DATA_LEVEL, &level);
}

u8 HackAI_GetRank(const struct AIRankContext* context)
{
    static const s8 offsets[DIFFICULTY_COUNT] = AI_RANK_DIFFICULTY_OFFSETS;
    static const u8 floors[DIFFICULTY_COUNT] = AI_RANK_DIFFICULTY_FLOORS;
    static const u8 caps[DIFFICULTY_COUNT] = AI_RANK_DIFFICULTY_CAPS;
    static const s8 wildOffsets[DIFFICULTY_COUNT] = AI_RANK_WILD_DIFFICULTY_OFFSETS;
    static const u8 wildFloors[DIFFICULTY_COUNT] = AI_RANK_WILD_DIFFICULTY_FLOORS;
    static const u8 wildCaps[DIFFICULTY_COUNT] = AI_RANK_WILD_DIFFICULTY_CAPS;
    u16 difficulty = context->difficulty < DIFFICULTY_COUNT ? context->difficulty : DIFFICULTY_NORMAL;
    s16 rank = MathMin(context->level, AI_RANK_LEVEL_CAP) / (context->wild ? AI_RANK_WILD_LEVEL_STEP : AI_RANK_LEVEL_STEP)
             + (context->wild ? wildOffsets[difficulty] : offsets[difficulty]);

    u16 points = (context->importantTrainer ? AI_RANK_IMPORTANT_TRAINER_POINTS : 0)
               + (context->lowHp ? AI_RANK_LOW_HP_POINTS : 0)
               + (context->statused ? AI_RANK_STATUS_POINTS : 0)
               + (context->prolongedBattle ? AI_RANK_LONG_BATTLE_POINTS : 0)
               + (context->boostedFoe ? AI_RANK_BOOSTED_FOE_POINTS : 0);
    rank += MathMin(points / AI_RANK_POINTS_PER_STEP, AI_RANK_MAX_CONTEXT_BOOST);
    u8 floor = context->wild ? wildFloors[difficulty] : floors[difficulty];
    u8 cap = context->wild ? wildCaps[difficulty] : caps[difficulty];
    return MathMin(MathMax(rank, floor), cap);
}

static u32 RankAIFlags(u8 rank)
{
    u32 flags = AI_SCRIPT_CHECK_BAD_MOVE;
    if (rank >= AI_RANK_SEMI_SMART) flags |= AI_SCRIPT_SEMI_SMART;
    if (rank >= AI_RANK_STANDARD) flags |= AI_SCRIPT_CHECK_GOOD_MOVE;
    if (rank >= AI_RANK_ADVANCED) flags |= AI_SCRIPT_ADVANCED;
    if (rank >= AI_RANK_EXPERT) flags |= AI_SCRIPT_EXPERT;
    return flags;
}

static u8 CommonBattlerRank(u8 bank, bool8 wild)
{
    struct AIRankContext context = {0};
    context.difficulty = VarGet(HOENN_VAR_DIFFICULTY);
    context.wild = wild;
    struct BattlePokemon* mon = &gBattleMons[bank];
    context.level = mon->level;
    context.lowHp = mon->hp != 0 && (u32)mon->hp * 100 <= (u32)mon->maxHP * AI_RANK_LOW_HP_PERCENT;
    context.statused = (mon->status1 & STATUS1_ANY) != 0;
    context.prolongedBattle = gBattleResults.battleTurnCounter >= AI_RANK_ADAPT_TURN;
    if (!wild && OrdinaryTrainerBattle() && !IsFrontierTrainerId(gTrainerBattleOpponent_A))
        switch (gTrainers[gTrainerBattleOpponent_A].trainerClass)
        {
        case CLASS_LEADER: case CLASS_ELITE_FOUR: case CLASS_CHAMPION:
        case CLASS_LEADER_RS: case CLASS_ELITE_FOUR_RS: case CLASS_CHAMPION_RS:
        case CLASS_AQUA_LEADER: case CLASS_MAGMA_LEADER:
            context.importantTrainer = TRUE;
            break;
        }
    for (u8 foe = 0; foe < gBattlersCount; ++foe)
        if (SIDE(foe) != SIDE(bank) && gBattleMons[foe].hp != 0
        && ((wild && gBattleMons[foe].level >= (u16)mon->level + AI_RANK_WILD_THREAT_LEVEL_GAP)
         || gBattleMons[foe].statStages[STAT_ATK] >= 6 + AI_RANK_FOE_STAT_BOOST
         || gBattleMons[foe].statStages[STAT_SPATK] >= 6 + AI_RANK_FOE_STAT_BOOST
         || gBattleMons[foe].statStages[STAT_SPEED] >= 6 + AI_RANK_FOE_STAT_BOOST))
            context.boostedFoe = TRUE;
    return HackAI_GetRank(&context);
}

u8 HackAI_GetTrainerRank(u8 bank)
{
    return CommonBattlerRank(bank, FALSE);
}

u8 HackAI_GetWildRank(u8 bank)
{
    return CommonBattlerRank(bank, TRUE);
}

u8 HackAI_GetBattlerRank(u8 bank)
{
    return (gBattleTypeFlags & BATTLE_TYPE_TRAINER) ? HackAI_GetTrainerRank(bank) : HackAI_GetWildRank(bank);
}

// Easy/medium/normal scale ordinary trainers. Hard/Momo also upgrade facilities.
u32 HackAI_TrainerFlags(u32 flags, u8 bank)
{
    u16 difficulty = VarGet(HOENN_VAR_DIFFICULTY);
    if (difficulty != DIFFICULTY_HARD && difficulty != DIFFICULTY_MOMO_ICE
    && (!OrdinaryTrainerBattle() || IsFrontierTrainerId(gTrainerBattleOpponent_A)))
        return flags;
    u32 rankMask = RankAIFlags(AI_RANK_EXPERT);
    return (flags & ~rankMask) | RankAIFlags(HackAI_GetTrainerRank(bank));
}

// Wild temperament/survival has its own script, never the trainer's prediction layer.
u32 HackAI_WildFlags(u32 flags, u8 bank)
{
    if (flags == 0)
        return flags;
    u32 rankMask = RankAIFlags(AI_RANK_EXPERT) | AI_SCRIPT_WILD_INSTINCT;
    u32 rankFlags = RankAIFlags(HackAI_GetWildRank(bank)) & ~(AI_SCRIPT_ADVANCED | AI_SCRIPT_EXPERT);
    return (flags & ~rankMask) | rankFlags | AI_SCRIPT_WILD_INSTINCT;
}

u32 HackDifficulty_AIFlags(u32 flags)
{
    if (gBattleTypeFlags & (BATTLE_TYPE_SAFARI | BATTLE_TYPE_ROAMER | BATTLE_TYPE_OAK_TUTORIAL))
        return flags;
    if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
        return HackAI_TrainerFlags(flags, gActiveBattler);
    if ((gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER) && SIDE(gActiveBattler) == B_SIDE_PLAYER)
        return flags;
    return HackAI_WildFlags(flags, gActiveBattler);
}

//B・C. src/build_pokemon.c の CreateNPCTrainerParty（相手側のポケモン・能力を計算する直前）
static void AdjustTrainerMonCore(struct Pokemon* mon, u8 side)
{
	u16 none = ITEM_NONE;
	u8 zero = 0;
	u32 i;

	if (side != B_SIDE_OPPONENT)
		return;
	if (OrdinaryTrainerBattle())
	{
		u16 difficulty = VarGet(HOENN_VAR_DIFFICULTY);
		if (difficulty == DIFFICULTY_HARD || difficulty == DIFFICULTY_MOMO_ICE)
		{
			RaiseTrainerMonLevel(mon, difficulty == DIFFICULTY_HARD ? HARD_LEVEL_UP : MOMO_ICE_LEVEL_UP);
			return;
		}
	}

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
u32 HackDifficulty_BuyPrice(u32 price, u16 difficulty)
{
	u32 discounted;
	if (price == 0 || (difficulty != DIFFICULTY_HARD && difficulty != DIFFICULTY_MOMO_ICE))
		return price;
	discounted = price / 5;
	if (difficulty == DIFFICULTY_MOMO_ICE)
		discounted = discounted * 2 + (price % 5) * 2 / 5;
	return discounted == 0 ? 1 : discounted;
}

u16 HackDifficulty_RecoveryItem(u16 currentItem, u8 level, u8 roll, u16 difficulty)
{
	static const u8 chances[DIFFICULTY_COUNT] = {30, 10, 20, 50, 70};
	if (currentItem != ITEM_NONE)
		return currentItem;
	if (difficulty >= DIFFICULTY_COUNT)
		difficulty = DIFFICULTY_NORMAL;
	if (roll >= chances[difficulty])
		return ITEM_NONE;
	return level <= 20 ? ITEM_ORAN_BERRY : ITEM_SITRUS_BERRY;
}

void HackDifficulty_AdjustTrainerMon(struct Pokemon* mon, u8 side)
{
	AdjustTrainerMonCore(mon, side);
	if (side == B_SIDE_OPPONENT && OrdinaryTrainerBattle())
	{
		u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);
		if (item == ITEM_NONE)
		{
			u8 level = GetMonData(mon, MON_DATA_LEVEL, NULL);
			item = HackDifficulty_RecoveryItem(item, level, Random() % 100, VarGet(HOENN_VAR_DIFFICULTY));
			if (item != ITEM_NONE)
				SetMonData(mon, MON_DATA_HELD_ITEM, &item);
		}
	}
}

void HackDifficulty_SetNewGameDefault(void)
{
	VarSet(HOENN_VAR_DIFFICULTY, DIFFICULTY_EASY);
}
