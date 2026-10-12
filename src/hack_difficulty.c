#include "defines.h"
#include "defines_battle.h"
#include "hack_difficulty.h"
#include "hack_debug_menu.h"   //[hack] patches/0331 デバッグメニューの捕獲 100%（-DHACK_DEBUG_MENU のときだけ）
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/new/item.h"
#include "../include/random.h"
#include "../include/constants/items.h"   //[hack] 2026-10-11 ITEM_MOMO_SHERBET・liru さんの回復のきのみ（patches/0320）
#include "../include/new/util.h"
#include "../include/new/frontier.h"
#include "../include/new/learn_move.h"   //GiveBoxMonInitialMoveset（ももアイスの覚える技 +10・2026-10-11）
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
/* ★ 2026-10-11 ユーザー「難易度ももアイス LV＋3 で覚える技＋10」（patches/0314）:
   ももアイス（4）のとき、トレーナー戦の相手（施設も）に レベル +3（上限 MAX_LEVEL）。技が「レベルで覚える技」のまま（型の技ではない）なら、
   上げたあとのレベル +10 で覚える技に入れ替える（型＝Smogon の技を持つ相手はそのまま）。施設にも効く（ユーザーの答え 2026-10-11） */
#define MOMO_LEVEL_UP 5                  //★ 2026-10-11 ユーザーの判断（5e 経由）: +5（liru さんの 3 回目と同じ・前は +3）。マージでは liru さんの +5 と 1 つにする
#define MOMO_MOVES_LEVEL_UP 10
/* ★ 2026-10-11 ユーザー「難易度りるアイス やさしいより やさしい 購入金額とか個体値の高いポケモンや対戦相手のやさしい 塩梅は決めて」（patches/0321）:
   りるアイス（5）= やさしい の中身（AI 最弱・努力値 0・持ち物なし）に加えて レベル −5（やさしいは −3）・野生は 6V（CreateWildMon のあと）・
   お店の買い値は半額（1 円より下にはしない・売値はそのまま）。施設は今までどおり。AI の段階は config.h の表の 6 つ目（−24・0・0 ＝ いつも 0） */
#define RIRU_LEVEL_DOWN 5
#define HARD_LEVEL_UP 1                  //liru さん（patches/0320）: むずかしい のふつうのトレーナーの相手は レベル +1（ももアイスの +5 は MOMO_LEVEL_UP）

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

static bool8 RiruIceTrainerBattle(void)
{
	return VarGet(HOENN_VAR_DIFFICULTY) == DIFFICULTY_RIRU_ICE && OrdinaryTrainerBattle();
}

static bool8 MediumTrainerBattle(void)
{
	return VarGet(HOENN_VAR_DIFFICULTY) == DIFFICULTY_MEDIUM && OrdinaryTrainerBattle();
}

//★ 施設（バトルフロンティアなど）も含む（ユーザーの答え 2026-10-11・dd → 5e 経由「ももアイスは施設にも効かせる」＝CLAUDE.md #33 のとおり）
static bool8 MomoIceTrainerBattleAny(void)
{
	return VarGet(HOENN_VAR_DIFFICULTY) == DIFFICULTY_MOMO_ICE && (gBattleTypeFlags & BATTLE_TYPE_TRAINER);
}

//★ 2026-10-11 なんいど ももアイス のふつうのトレーナー戦: 相手のポケモンに ももシャーベット（ももアイスの下位版・src/hack_momo_ice.c）。
//  施設の相手は build_pokemon.c の CreateNPCTrainerParty で IsFrontierTrainerId → BuildFrontierParty へ先に抜けるので、ここには来ない（5e の指摘）
static bool8 MomoSherbetBattle(void)
{
	return VarGet(HOENN_VAR_DIFFICULTY) == DIFFICULTY_MOMO_ICE && OrdinaryTrainerBattle();
}

static void SetTrainerMonLevel(struct Pokemon* mon, u32 level)
{
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);

	SetMonData(mon, MON_DATA_EXP, &gExperienceTables[gBaseStats[species].growthRate][level]);
	SetMonData(mon, MON_DATA_LEVEL, &level);
}

static void LowerTrainerMonLevel(struct Pokemon* mon, u32 down)
{
	u32 level = GetMonData(mon, MON_DATA_LEVEL, NULL);

	//★ 2026-10-11 5e の指摘: 「level > down + 1」だと Lv5・6 の相手が りるアイス（−5）で下がらず やさしい（−3）より強かった → 下限 2 まで下げる
	if (level > 2)
		SetTrainerMonLevel(mon, MathMax((s32) level - (s32) down, 2));
}

static void RaiseTrainerMonLevel(struct Pokemon* mon, u32 up)
{
	u32 level = GetMonData(mon, MON_DATA_LEVEL, NULL) + up;

	SetTrainerMonLevel(mon, MathMin(level, MAX_LEVEL));
}

//技が「今のレベルで覚える技」のままか（CreateMon が入れたもの＝型の技ではない）。同じ手順で作って 4 つとも同じなら
//  ★ CFRU の struct Pokemon は平ら（暗号化なし）なので moves[] を直に読める。写しは約 100 バイト（6.35 の 1,024 B より下）
static bool8 HasNaturalMoveset(struct Pokemon* mon)
{
	struct Pokemon tmp = *mon;
	u32 i;

	for (i = 0; i < MAX_MON_MOVES; ++i)
	{
		tmp.moves[i] = MOVE_NONE;
		tmp.pp[i] = 0;
	}
	GiveBoxMonInitialMoveset((struct BoxPokemon*) &tmp);
	for (i = 0; i < MAX_MON_MOVES; ++i)
	{
		if (tmp.moves[i] != mon->moves[i])
			return FALSE;
	}
	return TRUE;
}

//レベル level で覚える技（最後の 4 つ）に入れ替える。レベル・経験値は変えない（GiveBoxMonInitialMoveset は経験値からレベルを見るので一時的に差し替える）
static void GiveNaturalMovesetAtLevel(struct Pokemon* mon, u32 level)
{
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	u32 exp = GetMonData(mon, MON_DATA_EXP, NULL);
	u32 i;

	level = MathMin(level, MAX_LEVEL);
	for (i = 0; i < MAX_MON_MOVES; ++i)
	{
		mon->moves[i] = MOVE_NONE;
		mon->pp[i] = 0;
	}
	mon->ppBonuses = 0;
	SetMonData(mon, MON_DATA_EXP, &gExperienceTables[gBaseStats[species].growthRate][level]);
	GiveBoxMonInitialMoveset((struct BoxPokemon*) mon);
	SetMonData(mon, MON_DATA_EXP, &exp);
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
    //★ 2026-10-11 MathMin／MathMax は u32（include/new/util.h）。rank が負（やさしい Lv39 以下・ほどほど Lv19 以下）だと
    //  巨大な値になり cap（いちばん賢い段）が返っていた → s16 のまま挟む（patches/0311）
    if (rank < floor)
        rank = floor;
    if (rank > cap)
        rank = cap;
    return rank;
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

//ももアイス: レベル +3・レベルで覚える技のままなら上げたレベル +10 の技（ふつうのトレーナーも施設も同じ）
static void MomoIceAdjust(struct Pokemon* mon)
{
	//★ 比べるのはレベルを上げる前（上げたあとは「覚える技」が変わる）
	bool8 natural = HasNaturalMoveset(mon);

	RaiseTrainerMonLevel(mon, MOMO_LEVEL_UP);
	if (natural)
		GiveNaturalMovesetAtLevel(mon, GetMonData(mon, MON_DATA_LEVEL, NULL) + MOMO_MOVES_LEVEL_UP);
}

//施設（バトルフロンティア・トレーナーヒル）の相手: src/build_pokemon.c の CreateFrontierMon（能力を計算する直前・BuildFrontierParty は CreateNPCTrainerParty より先に抜けるので
//  HackDifficulty_AdjustTrainerMon は通らない＝5e の指摘 2026-10-11）。レンタル（forPlayer）は触らない。施設の型の技は変えない（HasNaturalMoveset が偽）
void HackDifficulty_AdjustFrontierMon(struct Pokemon* mon, bool8 forPlayer)
{
	//★ 5e の指摘 2026-10-11: マルチの相方（0x396）とレイドの助っ人（0x395）は forPlayer = FALSE で gPlayerParty に作られる → 味方側の入れ物なら触らない
	if (forPlayer || (mon >= gPlayerParty && mon < gPlayerParty + PARTY_SIZE) || VarGet(HOENN_VAR_DIFFICULTY) != DIFFICULTY_MOMO_ICE)
		return;
	MomoIceAdjust(mon);
}

//B・C. src/build_pokemon.c の CreateNPCTrainerParty（相手側のポケモン・能力を計算する直前）
static void AdjustTrainerMonCore(struct Pokemon* mon, u8 side)
{
	u16 none = ITEM_NONE;
	u8 zero = 0;
	u32 i;

	if (side != B_SIDE_OPPONENT)
		return;

	if (MomoIceTrainerBattleAny())
	{
		MomoIceAdjust(mon);   //ももアイス: レベル +5（MOMO_LEVEL_UP＝liru さんの +5 と同じ・二重に上げない）・技 +10・施設も
		//★ return しない（5e の指摘 2026-10-11）: f8 のももシャーベット（持ち物）のブロックが同じ所に入るので、先の return で飛ばさない。
		//  下の ほどほど／やさしい は なんいど の値が違うので通らない
	}
	if (MomoSherbetBattle())
	{
		u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);
		u16 sherbet = ITEM_MOMO_SHERBET;

		if (!IsMegaStone(item) && !IsPrimalOrb(item) && !IsZCrystal(item))
			SetMonData(mon, MON_DATA_HELD_ITEM, &sherbet);   //能力の計算の前なので 1.5 倍が効く
	}
	if (OrdinaryTrainerBattle() && VarGet(HOENN_VAR_DIFFICULTY) == DIFFICULTY_HARD)   //liru さん（patches/0320）: むずかしい +1 レベル（ももアイスの +5 は上の MomoIceAdjust）
	{
		RaiseTrainerMonLevel(mon, HARD_LEVEL_UP);
		return;
	}

	if (MediumTrainerBattle())
	{
		u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);

		if (!IsMegaStone(item) && !IsPrimalOrb(item))
			SetMonData(mon, MON_DATA_HELD_ITEM, &none);
		LowerTrainerMonLevel(mon, MEDIUM_LEVEL_DOWN);
		return;
	}
	if (!EasyTrainerBattle() && !RiruIceTrainerBattle())
		return;

	for (i = 0; i < NUM_STATS; ++i)
		SetMonData(mon, MON_DATA_HP_EV + i, &zero);
	SetMonData(mon, MON_DATA_HELD_ITEM, &none);

	LowerTrainerMonLevel(mon, RiruIceTrainerBattle() ? RIRU_LEVEL_DOWN : EASY_LEVEL_DOWN);
}

/* ★ 2026-10-11 ユーザー「難易度によって捕獲率を上げる 難易度が下がるほど補正がかかるように」（patches/0326）:
   src/catching.c の捕獲の式（ボール・状態異常・レイドの倍率のあと・0xFF で頭打ちの手前）で odds に掛ける。10 分の 1 の単位。
   りるアイス 3.0・やさしい 2.0・ほどほど 1.5・ふつう 1.0・むずかしい 0.7・ももアイス 0.5（下げるのは 2026-10-11 ユーザー「難易度むずかしいとももアイス捕獲率下げるようにしてほしい」）。サファリにも効く。
   マスターボールは式のあとで必ず捕まえる（catching.c の BALL_TYPE_MASTER_BALL の分岐）ので下げても捕まる。伝説レイド（0181）はこのあと 0xFF にするので関係ない。
   odds は GetBaseBallCatchOdds の値（255 × ボールの倍率 ÷ 10 ほど・数千まで）なので 3 倍でも u32 はあふれない。施設では捕まえられないので影響なし */
static const u8 sCatchOddsTenths[DIFFICULTY_COUNT] = {10, 20, 15, 7, 5, 30};   //並びは enum HackDifficulty（0 ふつう・1 やさしい・2 ほどほど・3 むずかしい・4 ももアイス・5 りるアイス）

u32 HackDifficulty_CatchOdds(u32 odds)
{
	u16 difficulty = VarGet(HOENN_VAR_DIFFICULTY);

#ifdef HACK_DEBUG_MENU
	if (HackDebugMenu_CatchAll())   //[hack] patches/0331 デバッグメニューの「ほかく100」（印 HACK_FLAG_DEBUG_CATCH・ふつうの版では見ない）
		return 0xFF;
#endif

	u32 before = odds;

	if (difficulty >= DIFFICULTY_COUNT)
		difficulty = DIFFICULTY_NORMAL;
	odds = odds * sCatchOddsTenths[difficulty] / 10;
	if (odds == 0 && before != 0)
		odds = 1;   //5e の指摘: 元が 1〜2 のとき 0 になると揺れの判定（catching.c は 0xFF0000 / odds）が壊れる＝下げても最低 1
	return odds > 0xFF ? 0xFF : odds;
}

//お店の買い値（src/mega_shop.c の MegaShop_ItemIdGetPrice（買う画面のとき）と MegaShop_Price から・引数の形は f8 の liru さん 3 回目と同じ）
//  りるアイス: 半額（0 になったら 1）。ほかの なんいど はそのまま（liru さんの むずかしい／ももアイス の割引はユーザー判断でやめた・2026-10-11 5e 経由＝せっていで切り替えると 1/5 で買って 1/2 で売れるため）
u32 HackDifficulty_BuyPrice(u32 price, u16 difficulty)
{
	if (price == 0)
		return price;
	if (difficulty == DIFFICULTY_RIRU_ICE)
		return price >= 2 ? price / 2 : 1;
	return price;
}

//りるアイス: 野生（ずかんナビ・レイドも CreateWildMon を通る）は 6V。性格値は触らない（6.5）
void HackDifficulty_WildMon(struct Pokemon* mon)
{
	u8 iv = 31;
	u32 i;

	if (VarGet(HOENN_VAR_DIFFICULTY) != DIFFICULTY_RIRU_ICE)
		return;
	for (i = 0; i < NUM_STATS; ++i)
		SetMonData(mon, MON_DATA_HP_IV + i, &iv);
	CalculateMonStats(mon);   //個体値を変えたので能力を計算し直す（5e の指摘 2026-10-11）
}

bool8 HackDifficulty_BagLimitReached(u16 difficulty, u8 usedCount)
{
	if (difficulty == DIFFICULTY_HARD)
		return usedCount >= 3;
	if (difficulty == DIFFICULTY_MOMO_ICE)
		return usedCount >= 1;
	return FALSE;
}

bool8 HackDifficulty_ForceSetStyle(u16 difficulty)
{
	return difficulty == DIFFICULTY_HARD || difficulty == DIFFICULTY_MOMO_ICE;
}

u16 HackDifficulty_RecoveryItem(u16 currentItem, u8 level, u8 roll, u16 difficulty)
{
	//★ なんいどの並び（0 ふつう・1 やさしい・2 ほどほど・3 むずかしい・4 ももアイス）。なんいどを足したら必ずここにも 1 つ足す（6.79・5e）
	static const u8 chances[DIFFICULTY_COUNT] = {30, 10, 20, 50, 70, 0};   //6 つ目 りるアイス（配らない）
	if (currentItem != ITEM_NONE)
		return currentItem;
	if (difficulty >= DIFFICULTY_COUNT)
		difficulty = DIFFICULTY_NORMAL;
	//★ やさしい は「持ち物なし」・ほどほど は「メガストーンとゲンシの玉だけ」（CLAUDE.md #33・ユーザー判断）なので、外した持ち物が
	//  きのみとして戻らないように配らない（liru さんの表の 10・20% は使わない・5e の指摘 2026-10-11）
	if (difficulty != DIFFICULTY_HARD && difficulty != DIFFICULTY_MOMO_ICE)
		return ITEM_NONE;   //ふつう も今までどおり（持ち物を足さない・5e）
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

/* ★ 2026-10-03 ユーザー「設定の難易度デフォルトやさしいで」: はじめからのとき、なんいどを やさしい にしておく。
   src/save.c の NewGameWipeNewSaveData（拡張のセーブ 0x0203B0E8〜を 0 で消す・拡張変数 0x5000〜も中）の直後に呼ぶ（patches/0146）。
   ★ 値の意味（0 ふつう / 1 やさしい）は変えない（前のセーブで選んだ値はそのまま） */
void HackDifficulty_SetNewGameDefault(void)
{
	VarSet(HOENN_VAR_DIFFICULTY, DIFFICULTY_EASY);
}
