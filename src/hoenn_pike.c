#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/fieldmap.h"
#include "../include/overworld.h"
#include "../include/sprite.h"
#include "../include/wild_encounter.h"
#include "../include/new/wild_encounter.h"
#include "../include/palette.h"
#include "../include/random.h"
#include "../include/script.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/new/frontier.h"
#include "../include/new/util.h"
#include "Tables/hoenn_script_flags.h"

/*
 * ホウエンのバトルチューブ（#5・2026-09-29 ユーザー「つづけて」）
 * docs/research/2026-09-28_battle-frontier-npcs.md の「バトルチューブ」
 *
 * 流れとセリフはエメラルドどおり（assembly/frontier/pike_scripts.s・tools/frontier/build_frontier_pike.py が生成）。
 * 部屋の選び方・ヒント・状態異常・回復はエメラルドの src/battle_pike.c を移した（名前もなるべく同じ）。
 * ★ 状態はすべて予約変数に置く（6.13・static は使えない）:
 *   HOENN_VAR_PIKE_STATE … 今の部屋の種類・ヒントの部屋の種類と位置・回復の部屋を出さないか・状態異常の子と種類
 *   HOENN_VAR_PIKE_NPC   … 人の部屋の人の番号
 *   HOENN_VAR_PIKE_PARTY … やすむときの挑戦のチームの様子（HP・状態・PP・持ち物 × 3 匹）
 *   この挑戦で出たトレーナーは src/hoenn_frontier.c（全施設で共用・HOENN_VAR_FRONTIER_TRAINERS）
 * ★ 2026-10-03 ユーザー「チューブを仕上げて」: 野生の部屋（草むら・src/wild_encounter.c の差し込み口・patches/0126）・
 *   ダブルの部屋（相手 2 人・src/overworld.c の差し込み口・patches/0126）・チューブクイーン アザミ（エメラルドのルーシー）の部屋
 */

extern u16 HoennFrontier_ChooseNewTrainer(u16 challengeNum, u16 battleNum); //src/hoenn_frontier.c
extern void HoennFrontier_ClearFoughtTrainers(void);
extern u8 HoennFrontier_BrainStatusAhead(u8 ahead);                     //src/hoenn_tower.c
extern void HoennFrontier_SetBrainOpponent(void);

#define HOENN_FACILITY_PIKE 3               //HOENN_VAR_FRONTIER_FACILITY
//★ 2026-10-03 バトルピラミッド（src/hoenn_pyramid.c）は野生・回復しないところをここの差し込み口で共用
extern bool8 HoennPyramid_OnFloor(void);
extern const struct WildPokemonInfo* HoennPyramid_WildMonsInfo(u8 type);
extern void HoennPyramid_GenerateWildMon(void);
extern void HoennPyramid_RecordWildOutcome(void);
#define NUM_PIKE_ROOMS 14
#define FRONTIER_PARTY_SIZE 3

enum
{
	PIKE_ROOM_SINGLE_BATTLE,
	PIKE_ROOM_HEAL_FULL,
	PIKE_ROOM_NPC,
	PIKE_ROOM_STATUS,
	PIKE_ROOM_HEAL_PART,
	PIKE_ROOM_WILD_MONS,
	PIKE_ROOM_HARD_BATTLE,
	PIKE_ROOM_DOUBLE_BATTLE,
	PIKE_ROOM_BRAIN,
	NUM_PIKE_ROOM_TYPES
};

enum {PIKE_HINT_NOSTALGIA, PIKE_HINT_WHISPERING, PIKE_HINT_POKEMON, PIKE_HINT_PEOPLE, PIKE_HINT_BRAIN};
enum {PIKE_STATUSMON_KIRLIA, PIKE_STATUSMON_DUSCLOPS};
enum {PIKE_STATUS_FREEZE, PIKE_STATUS_BURN, PIKE_STATUS_TOXIC, PIKE_STATUS_PARALYSIS, PIKE_STATUS_SLEEP};

//出してよい部屋（ビット = 部屋の種類）。★ 2026-10-03 野生・ダブルも（アザミの部屋はヒントの部屋でだけ出る）
#define HOENN_PIKE_ROOMS_READY ((1 << PIKE_ROOM_SINGLE_BATTLE) | (1 << PIKE_ROOM_HEAL_FULL) | (1 << PIKE_ROOM_NPC) \
                              | (1 << PIKE_ROOM_STATUS) | (1 << PIKE_ROOM_HEAL_PART) | (1 << PIKE_ROOM_WILD_MONS) \
                              | (1 << PIKE_ROOM_HARD_BATTLE) | (1 << PIKE_ROOM_DOUBLE_BATTLE))

//アザミの部屋の前の回復（エメラルドの sNumMonsToHealBeforePikeQueen[ヒントの位置 0〜5][選んだ扉 0〜2]）
static const u8 sNumMonsToHealBeforePikeQueen[6][3] =
{
	{2, 1, 0},
	{2, 0, 1},
	{1, 2, 0},
	{1, 0, 2},
	{0, 2, 1},
	{0, 1, 2},
};

//人の絵・セリフ・カーテンのメタタイル（tools/frontier/build_frontier_pike.py の生成物）
#include "hoenn_pike_data.h"
bool8 HoennHill_OnFloor(void); //src/hoenn_trainer_hill.c

//エメラルドの sRoomTypeHints
static const u8 sRoomTypeHints[NUM_PIKE_ROOM_TYPES] =
{
	[PIKE_ROOM_SINGLE_BATTLE] = PIKE_HINT_PEOPLE,
	[PIKE_ROOM_HEAL_FULL]     = PIKE_HINT_PEOPLE,
	[PIKE_ROOM_NPC]           = PIKE_HINT_WHISPERING,
	[PIKE_ROOM_STATUS]        = PIKE_HINT_NOSTALGIA,
	[PIKE_ROOM_HEAL_PART]     = PIKE_HINT_NOSTALGIA,
	[PIKE_ROOM_WILD_MONS]     = PIKE_HINT_POKEMON,
	[PIKE_ROOM_HARD_BATTLE]   = PIKE_HINT_POKEMON,
	[PIKE_ROOM_DOUBLE_BATTLE] = PIKE_HINT_WHISPERING,
	[PIKE_ROOM_BRAIN]         = PIKE_HINT_BRAIN,
};

/* ---- 状態の置き場（HOENN_VAR_PIKE_STATE のビット） ------------------------------------ */
#define ST_ROOM_SHIFT      0     //4 ビット: 今の部屋の種類
#define ST_HINT_TYPE_SHIFT 4     //4 ビット: ヒントの部屋の種類
#define ST_HINT_IDX_SHIFT  8     //3 ビット: ヒントの部屋の位置（0 左・1 まん中・2 右／ルーシーは 0〜5）
#define ST_NO_HEAL_BIT     11    //回復の部屋を出さない
#define ST_STATUS_MON_BIT  12    //状態異常の子（0 キルリア・1 サマヨール）
#define ST_STATUS_SHIFT    13    //3 ビット: 状態異常の種類（PIKE_STATUS_*）

static u16 GetField(u8 shift, u16 mask)
{
	return (VarGet(HOENN_VAR_PIKE_STATE) >> shift) & mask;
}

static void SetField(u8 shift, u16 mask, u16 value)
{
	u16 v = VarGet(HOENN_VAR_PIKE_STATE);

	VarSet(HOENN_VAR_PIKE_STATE, (v & ~(mask << shift)) | ((value & mask) << shift));
}

#define RoomType()          GetField(ST_ROOM_SHIFT, 0xF)
#define HintedRoomType()    GetField(ST_HINT_TYPE_SHIFT, 0xF)
#define HintedRoomIndex()   GetField(ST_HINT_IDX_SHIFT, 0x7)
#define HealingDisabled()   GetField(ST_NO_HEAL_BIT, 1)

static u16 BattleNum(void)
{
	return VarGet(HOENN_VAR_TOWER_BATTLE_NUM);
}

static bool8 InPikeFacility(void)
{
	return VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_PIKE;
}

//戦いの終わりの回復・持ち物の戻しを止めるか（src/end_battle.c・patches/0099）。
//エメラルドのチューブは戦いのあとも回復しない（HP・状態・使った持ち物はそのまま次の部屋へ）
//★ 2026-10-03 バトルピラミッドの階も回復しない（エメラルドもピラミッドとチューブだけ）
bool8 HoennPike_KeepDamageAfterBattle(void)
{
	return FlagGet(FLAG_BATTLE_FACILITY) && (InPikeFacility() || HoennPyramid_OnFloor() || HoennHill_OnFloor()); //★ トレーナーヒルも回復しない（エメラルドと同じ）
}

/* ---- 手持ちの様子 ------------------------------------------------------------------------ */
static bool8 IsHealthy(struct Pokemon* mon) //状態異常なし・ひんしでない
{
	return (GetMonData(mon, MON_DATA_STATUS, NULL) & STATUS1_ANY) == 0 && GetMonData(mon, MON_DATA_HP, NULL) != 0;
}

static bool8 NeedsHealing(struct Pokemon* mon)
{
	u8 ppBonuses = GetMonData(mon, MON_DATA_PP_BONUSES, NULL);
	u32 j;

	if (GetMonData(mon, MON_DATA_HP, NULL) < GetMonData(mon, MON_DATA_MAX_HP, NULL))
		return TRUE;
	if (GetMonData(mon, MON_DATA_STATUS, NULL) & STATUS1_ANY)
		return TRUE;
	for (j = 0; j < MAX_MON_MOVES; ++j)
	{
		u16 move = GetMonData(mon, MON_DATA_MOVE1 + j, NULL);

		if (move != MOVE_NONE && GetMonData(mon, MON_DATA_PP1 + j, NULL) < CalculatePPWithBonus(move, ppBonuses, j))
			return TRUE;
	}
	return FALSE;
}

//エメラルドの AtLeastTwoAliveMons（倒れた子が 2 匹以上なら FALSE）
static bool8 AtLeastTwoAliveMons(void)
{
	u8 dead = 0;
	u32 i;

	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
		if (GetMonData(&gPlayerParty[i], MON_DATA_HP, NULL) == 0)
			++dead;
	return dead < 2;
}

//状態異常にする子の数（部屋の番号で増える）
static u8 StatusCount(void)
{
	if (BattleNum() <= 4)
		return 1;
	else if (BattleNum() <= 9)
		return 2;
	return 3;
}

//エメラルドの AtLeastOneHealthyMon（頭から StatusCount 匹のうちに元気な子がいるか）
static bool8 AtLeastOneHealthyMon(void)
{
	u8 count = StatusCount(), healthy = 0;
	u32 i;

	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
	{
		if (IsHealthy(&gPlayerParty[i]))
			++healthy;
		if (healthy == count)
			break;
	}
	return healthy != 0;
}

static void ShuffledIndices(u8* indices)
{
	u32 i, k;

	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
		indices[i] = i;
	for (k = 0; k < 10; ++k)
	{
		u8 a = Random() % FRONTIER_PARTY_SIZE, b = Random() % FRONTIER_PARTY_SIZE, t = indices[a];

		indices[a] = indices[b];
		indices[b] = t;
	}
}

/* ---- 状態異常の部屋（エメラルドの TryInflictRandomStatus） -------------------------------- */
static bool8 DoesAbilityPreventStatus(struct Pokemon* mon, u32 status)
{
	u16 ability = GetMonAbility(mon);

	switch (status) {
		case STATUS1_FREEZE:
			return ability == ABILITY_MAGMAARMOR;
		case STATUS1_BURN:
			return ability == ABILITY_WATERVEIL;
		case STATUS1_PARALYSIS:
			return ability == ABILITY_LIMBER;
		case STATUS1_SLEEP:
			return ability == ABILITY_INSOMNIA || ability == ABILITY_VITALSPIRIT;
		case STATUS1_TOXIC_POISON:
			return ability == ABILITY_IMMUNITY;
	}
	return FALSE;
}

static bool8 HasType(struct Pokemon* mon, u8 type)
{
	return GetMonType(mon, 0) == type || GetMonType(mon, 1) == type;
}

static bool8 DoesTypePreventStatus(struct Pokemon* mon, u32 status)
{
	switch (status) {
		case STATUS1_TOXIC_POISON:
			return HasType(mon, TYPE_STEEL) || HasType(mon, TYPE_POISON);
		case STATUS1_FREEZE:
			return HasType(mon, TYPE_ICE);
		case STATUS1_PARALYSIS:
			return HasType(mon, TYPE_GROUND) || HasType(mon, TYPE_ELECTRIC);
		case STATUS1_BURN:
			return HasType(mon, TYPE_FIRE);
	}
	return FALSE;
}

static u8 StatusToId(u32 status)
{
	switch (status) {
		case STATUS1_FREEZE:
			return PIKE_STATUS_FREEZE;
		case STATUS1_BURN:
			return PIKE_STATUS_BURN;
		case STATUS1_PARALYSIS:
			return PIKE_STATUS_PARALYSIS;
		case STATUS1_SLEEP:
			return PIKE_STATUS_SLEEP;
		default:
			return PIKE_STATUS_TOXIC;
	}
}

static bool8 TryInflictRandomStatus(void)
{
	u8 indices[FRONTIER_PARTY_SIZE];
	u8 count = StatusCount();
	u32 status = 0, chosen = 0, i, j;
	bool8 statusChosen;

	ShuffledIndices(indices);
	do
	{
		u8 rand = Random() % 100;

		statusChosen = FALSE;
		if (rand < 35)
			chosen = STATUS1_TOXIC_POISON;
		else if (rand < 60)
			chosen = STATUS1_FREEZE;
		else if (rand < 80)
			chosen = STATUS1_PARALYSIS;
		else if (rand < 90)
			chosen = STATUS1_SLEEP;
		else
			chosen = STATUS1_BURN;

		if (status != chosen)
		{
			status = chosen;
			j = 0;
			for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
			{
				struct Pokemon* mon = &gPlayerParty[indices[i]];

				if (IsHealthy(mon))
				{
					++j;
					if (!DoesTypePreventStatus(mon, chosen))
					{
						statusChosen = TRUE;
						break;
					}
				}
				if (j == count)
					break;
			}
			if (j == 0)
				return FALSE;
		}
	} while (!statusChosen);

	SetField(ST_STATUS_SHIFT, 0x7, StatusToId(chosen));
	if (chosen == STATUS1_FREEZE || (chosen == STATUS1_BURN && Random() % 2 != 0))
		SetField(ST_STATUS_MON_BIT, 1, PIKE_STATUSMON_DUSCLOPS);
	else
		SetField(ST_STATUS_MON_BIT, 1, PIKE_STATUSMON_KIRLIA);

	j = 0;
	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
	{
		struct Pokemon* mon = &gPlayerParty[indices[i]];

		if (IsHealthy(mon))
		{
			++j;
			if (!DoesAbilityPreventStatus(mon, chosen) && !DoesTypePreventStatus(mon, chosen))
				SetMonData(mon, MON_DATA_STATUS, &chosen);
		}
		if (j == count)
			break;
	}
	return TRUE;
}

/* ---- 部屋の種類（エメラルドの GetNextRoomType） ----------------------------------------------- */
static u8 GetNextRoomType(u8 door)
{
	bool8 disabled[NUM_PIKE_ROOM_TYPES - 1];
	u8 candidates[NUM_PIKE_ROOM_TYPES - 1];
	u8 hint, num = 0, type;
	u32 i;

	if (HintedRoomType() == PIKE_ROOM_BRAIN)
		return PIKE_ROOM_BRAIN;

	//ヒントの人が言った扉なら、その部屋
	if (door == HintedRoomIndex())
	{
		if (HintedRoomType() == PIKE_ROOM_STATUS)
			TryInflictRandomStatus();
		return HintedRoomType();
	}

	//ほかの 2 つはヒントと同じ種類の手がかりの部屋にならない
	hint = sRoomTypeHints[HintedRoomType()];
	for (i = 0; i < NUM_PIKE_ROOM_TYPES - 1; ++i)
		disabled[i] = (sRoomTypeHints[i] == hint) || !(HOENN_PIKE_ROOMS_READY & (1 << i));

	//手持ちに効かない部屋は出さない
	if (!AtLeastTwoAliveMons())
		disabled[PIKE_ROOM_DOUBLE_BATTLE] = TRUE;
	if (!AtLeastOneHealthyMon())
		disabled[PIKE_ROOM_STATUS] = TRUE;
	if (HealingDisabled())
	{
		disabled[PIKE_ROOM_HEAL_FULL] = TRUE;
		disabled[PIKE_ROOM_HEAL_PART] = TRUE;
	}

	for (i = 0; i < NUM_PIKE_ROOM_TYPES - 1; ++i)
		if (!disabled[i])
			candidates[num++] = i;
	if (num == 0) //★ エメラルドには無い守り（部屋を作り途中で候補が 0 になりうる）: 人の部屋
		return PIKE_ROOM_NPC;

	type = candidates[Random() % num];
	if (type == PIKE_ROOM_STATUS)
		TryInflictRandomStatus();
	return type;
}

/* ---- スクリプトから呼ぶもの（callasm） ------------------------------------------------------ */

//通路で挑戦を始める（エメラルドの pike_cleartrainerids / pike_nohealing TRUE）
void HoennPike_StartChallenge(void)
{
	VarSet(HOENN_VAR_PIKE_STATE, 0);
	SetField(ST_NO_HEAL_BIT, 1, TRUE);
	HoennFrontier_ClearFoughtTrainers();
}

//三つ又の部屋に入ったとき（エメラルドの SetHintedRoom）→ LASTRESULT = TRUE ならルーシーのヒント
void HoennPike_SetHintRoom(void)
{
	u8 candidates[NUM_PIKE_ROOM_TYPES - 1];
	u8 num = 0, type;
	u32 i;

	gSpecialVar_LastResult = FALSE;
	//次の部屋がアザミの部屋なら、扉は 3 つともアザミの部屋（ヒントの位置 0〜5 は回復の数を決める・エメラルドの GetPikeQueenFightType(1)）
	if (HoennFrontier_BrainStatusAhead(1) != 0)
	{
		gSpecialVar_LastResult = TRUE;
		SetField(ST_HINT_IDX_SHIFT, 0x7, Random() % 6);
		SetField(ST_HINT_TYPE_SHIFT, 0xF, PIKE_ROOM_BRAIN);
		return;
	}
	SetField(ST_HINT_IDX_SHIFT, 0x7, Random() % 3);
	for (i = 0; i < NUM_PIKE_ROOM_TYPES - 1; ++i)
	{
		if (!(HOENN_PIKE_ROOMS_READY & (1 << i)))
			continue;
		if (HealingDisabled() && (i == PIKE_ROOM_HEAL_FULL || i == PIKE_ROOM_HEAL_PART))
			continue;
		candidates[num++] = i;
	}
	type = candidates[Random() % num];
	if (type == PIKE_ROOM_STATUS && !AtLeastOneHealthyMon())
		type = PIKE_ROOM_NPC;
	if (type == PIKE_ROOM_DOUBLE_BATTLE && !AtLeastTwoAliveMons())
		type = PIKE_ROOM_NPC;
	SetField(ST_HINT_TYPE_SHIFT, 0xF, type);
}

//ヒントの手がかり（PIKE_HINT_*）→ LASTRESULT
void HoennPike_GetHint(void)
{
	gSpecialVar_LastResult = sRoomTypeHints[HintedRoomType()];
}

//ヒントの扉（0 左・1 まん中・2 右）→ LASTRESULT
void HoennPike_GetHintRoomId(void)
{
	gSpecialVar_LastResult = HintedRoomIndex();
}

//扉を選んだ（Var8007 = 0 左・1 まん中・2 右）→ 次の部屋の種類を決める（状態異常の部屋ならここで状態異常にする）
void HoennPike_SetNextRoom(void)
{
	SetField(ST_ROOM_SHIFT, 0xF, GetNextRoomType(Var8007));
}

void HoennPike_GetRoomType(void)
{
	gSpecialVar_LastResult = RoomType();
}

//次は最後の部屋か（エメラルドの IsNextRoomFinal）→ LASTRESULT
void HoennPike_IsFinalRoom(void)
{
	gSpecialVar_LastResult = BattleNum() > NUM_PIKE_ROOMS;
}

//部屋の人の絵（エメラルドの SetupRoomObjectEvents）。絵 0xFF00 / 0xFF01（CFRU の VAR_RUNTIME_CHANGEABLE + 0 / + 1）。
//戦いの部屋は相手を決める（タワーと同じ表・並びの番号 1 ＝ ふつう／6 ＝ 強い）
void HoennPike_SetRoomObjects(void)
{
	u16 gfx0 = HOENN_PIKE_GFX_RECEPTIONIST, gfx1 = HOENN_PIKE_GFX_DUSCLOPS;
	u16 streak = VarGet(HOENN_VAR_PIKE_STREAK + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0));
	u16 id;

	switch (RoomType()) {
		case PIKE_ROOM_SINGLE_BATTLE:
		case PIKE_ROOM_HARD_BATTLE:
			id = HoennFrontier_ChooseNewTrainer(streak / NUM_PIKE_ROOMS, (RoomType() == PIKE_ROOM_HARD_BATTLE) ? 6 : 1);
			VarSet(VAR_FACILITY_TRAINER_ID, id);
			gfx0 = gSpecialTowerTrainers[id].owNum;
			gfx1 = HOENN_PIKE_GFX_RECEPTIONIST;
			break;
		case PIKE_ROOM_HEAL_FULL:
			gfx0 = HOENN_PIKE_GFX_RECEPTIONIST;
			break;
		case PIKE_ROOM_NPC:
			id = Random() % HOENN_PIKE_NPC_COUNT;
			VarSet(HOENN_VAR_PIKE_NPC, id);
			gfx0 = sHoennPikeNpcGfx[id];
			break;
		case PIKE_ROOM_STATUS:
			gfx0 = HOENN_PIKE_GFX_GENTLEMAN;
			gfx1 = GetField(ST_STATUS_MON_BIT, 1) == PIKE_STATUSMON_DUSCLOPS ? HOENN_PIKE_GFX_DUSCLOPS : HOENN_PIKE_GFX_KIRLIA;
			break;
		case PIKE_ROOM_HEAL_PART:
			gfx0 = HOENN_PIKE_GFX_GENTLEMAN;
			break;
		case PIKE_ROOM_DOUBLE_BATTLE: //エメラルドの PrepareTwoTrainers（2 人とも並びの番号 1・この挑戦で出ていない人）
			id = HoennFrontier_ChooseNewTrainer(streak / NUM_PIKE_ROOMS, 1);
			VarSet(VAR_FACILITY_TRAINER_ID, id);
			gfx0 = gSpecialTowerTrainers[id].owNum;
			id = HoennFrontier_ChooseNewTrainer(streak / NUM_PIKE_ROOMS, 1);
			VarSet(VAR_FACILITY_TRAINER_ID + 1, id);
			gfx1 = gSpecialTowerTrainers[id].owNum;
			break;
		case PIKE_ROOM_BRAIN: //アザミ（歩き姿は HoennFrontier_SetBrainOpponent が VAR_RUNTIME_CHANGEABLE に書く）・回復の係
			HoennFrontier_SetBrainOpponent();
			gfx0 = VarGet(VAR_RUNTIME_CHANGEABLE);
			gfx1 = HOENN_PIKE_GFX_RECEPTIONIST;
			break;
	}
	VarSet(VAR_RUNTIME_CHANGEABLE + 0, gfx0);
	VarSet(VAR_RUNTIME_CHANGEABLE + 1, gfx1);
}

//状態異常の種類（PIKE_STATUS_*）・した子（0 キルリア・1 サマヨール）→ LASTRESULT
void HoennPike_GetStatus(void)
{
	gSpecialVar_LastResult = GetField(ST_STATUS_SHIFT, 0x7);
}

void HoennPike_GetStatusMon(void)
{
	gSpecialVar_LastResult = GetField(ST_STATUS_MON_BIT, 1);
}

//エメラルドの TryHealMons（回復が要る子を でたらめな順に count 匹）
static void TryHealMons(u8 count)
{
	u8 indices[FRONTIER_PARTY_SIZE];
	u32 i;

	if (count == 0)
		return;
	ShuffledIndices(indices);
	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
	{
		struct Pokemon* mon = &gPlayerParty[indices[i]];

		if (NeedsHealing(mon))
		{
			HealMon(mon);
			if (--count == 0)
				break;
		}
	}
}

//一部回復の部屋（エメラルドの HealOneOrTwoMons）→ LASTRESULT = 回復する数（1 か 2）
void HoennPike_HealOneOrTwoMons(void)
{
	u8 n = (Random() % 2) + 1;

	TryHealMons(n);
	gSpecialVar_LastResult = n;
}

//アザミの部屋の前の回復（エメラルドの HealSomeMonsBeforePikeQueen）: ヒントの位置と選んだ扉（Var8007）で 0〜2 匹 → LASTRESULT
void HoennPike_HealBeforeQueen(void)
{
	u8 n = sNumMonsToHealBeforePikeQueen[MathMin(HintedRoomIndex(), 5)][MathMin(Var8007, 2)];

	TryHealMons(n);
	gSpecialVar_LastResult = n;
}

//人の部屋のセリフ → gStringVar4（エメラルドの BufferNPCMessage・部屋の番号で 3 通り）
void HoennPike_BufferNpcMessage(void)
{
	u16 npc = VarGet(HOENN_VAR_PIKE_NPC) % HOENN_PIKE_NPC_COUNT;
	u8 k = (BattleNum() <= 4) ? 0 : (BattleNum() <= 10) ? 1 : 2;

	StringCopy(gStringVar4, sHoennPikeNpcSpeech[npc][k]);
}

//部屋を出るとき（エメラルドの pike_ispartyfullhealth → pike_nohealing）: 全員元気なら次は回復の部屋を出さない
void HoennPike_UpdateHealingRooms(void)
{
	bool8 full = TRUE;
	u32 i;

	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
		if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES, NULL) != SPECIES_NONE && NeedsHealing(&gPlayerParty[i]))
			full = FALSE;
	SetField(ST_NO_HEAL_BIT, 1, full);
}

/* ---- 状態異常のときの画面の点滅（エメラルドの StatusInflictionScreenFlash） -------------------- */
#define tState     data[0]
#define tOutDelay  data[1]
#define tInDelay   data[2]
#define tFades     data[3]
#define tOutSpeed  data[4]
#define tInSpeed   data[5]
#define tTimer     data[6]
#define tBlend     data[7]

static void Task_HoennPikeStatusFlash(u8 taskId)
{
	s16* data = gTasks[taskId].data;

	if (tState == 0) //暗くする
	{
		if (tTimer == 0 || --tTimer == 0)
		{
			tTimer = tOutDelay;
			tBlend += tOutSpeed;
			if (tBlend > 16)
				tBlend = 16;
			BlendPalettes(0xFFFFFFFF, tBlend, RGB(11, 11, 11));
		}
		if (tBlend >= 16)
		{
			tState = 1;
			tTimer = tInDelay;
		}
	}
	else //戻す
	{
		if (tTimer == 0 || --tTimer == 0)
		{
			tTimer = tInDelay;
			tBlend -= tInSpeed;
			if (tBlend < 0)
				tBlend = 0;
			BlendPalettes(0xFFFFFFFF, tBlend, RGB(11, 11, 11));
		}
		if (tBlend == 0)
		{
			if (--tFades == 0)
			{
				DestroyTask(taskId);
				EnableBothScriptContexts();
			}
			else
			{
				tTimer = tOutDelay;
				tState = 0;
			}
		}
	}
}

//スクリプトは callasm のあと waitstate（終わったら続きを動かす）。エメラルドと同じ 3 回・速さ 2
void HoennPike_StatusFlash(void)
{
	u8 taskId = CreateTask(Task_HoennPikeStatusFlash, 3);
	s16* data = gTasks[taskId].data;

	tOutDelay = 0;
	tInDelay = 0;
	tFades = 3;
	tOutSpeed = 2;
	tInSpeed = 2;
	tTimer = 0;
}

#undef tState
#undef tOutDelay
#undef tInDelay
#undef tFades
#undef tOutSpeed
#undef tInSpeed
#undef tTimer
#undef tBlend

/* ---- カーテン（エメラルドの CloseBattlePikeCurtain・pokeemerald src/field_specials.c） -------------- */
//主人公の位置の左 1・上 3 から 3 × 4 マスを、閉じていく 3 コマの絵で 4 フレームずつ書き換える。
//メタタイルの番号は詰め直しのあとの番号（生成物 sHoennPikeCurtain・3 コマ × 4 行 × 3 列）
#define DrawWholeMapView ((void (*)(void)) (0x08059F40 | 1))  //素の DrawWholeMapView（BPRJ.ld に無い・src/hoenn_tower.c と同じ）
#define tFrame data[3]

static void Task_HoennPikeCloseCurtain(u8 taskId)
{
	s16* data = gTasks[taskId].data;
	u32 x, y;

	if (--data[tFrame] != 0)
		return;
	for (y = 0; y < 4; ++y)
		for (x = 0; x < 3; ++x)
			MapGridSetMetatileIdAt(gSaveBlock1->pos.x + x + 7 - 1, gSaveBlock1->pos.y + y + 7 - 3,
			                       sHoennPikeCurtain[tFrame * 12 + y * 3 + x]);
	DrawWholeMapView();
	if (++tFrame == 3)
	{
		DestroyTask(taskId);
		EnableBothScriptContexts();
	}
}

//スクリプトは callasm のあと waitstate
void HoennPike_CloseCurtain(void)
{
	u8 taskId = CreateTask(Task_HoennPikeCloseCurtain, 8);

	gTasks[taskId].data[0] = 4;
	gTasks[taskId].data[1] = 4;
	gTasks[taskId].data[2] = 4;
	gTasks[taskId].tFrame = 0;
}

#undef tFrame

/* ---- やすむ（三つ又の部屋の係）: 挑戦のチームの様子を変数に取っておき、続きから戻す ------------------ */
//1 匹 5 語: HP・状態（下 16 ビット）・PP（2 語）・持ち物。元の手持ち（SavePlayerParty で退避）とは別にしておかないと、
//レポートで元の手持ちが上書きされる（素 FRLG のレポートは gPlayerParty を書く）
void HoennPike_SavePartyState(void)
{
	u32 i, j;

	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
	{
		struct Pokemon* mon = &gPlayerParty[i];
		u16 base = HOENN_VAR_PIKE_PARTY + i * 5;
		u8 pp[MAX_MON_MOVES];

		for (j = 0; j < MAX_MON_MOVES; ++j)
			pp[j] = GetMonData(mon, MON_DATA_PP1 + j, NULL);
		VarSet(base + 0, GetMonData(mon, MON_DATA_HP, NULL));
		VarSet(base + 1, GetMonData(mon, MON_DATA_STATUS, NULL) & 0xFFFF);
		VarSet(base + 2, pp[0] | (pp[1] << 8));
		VarSet(base + 3, pp[2] | (pp[3] << 8));
		VarSet(base + 4, GetMonData(mon, MON_DATA_HELD_ITEM, NULL));
	}
}

//続きから: HoennTower_PrepareTeam で作り直した挑戦のチームに、取っておいた様子を戻す
void HoennPike_LoadPartyState(void)
{
	u32 i, j;

	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
	{
		struct Pokemon* mon = &gPlayerParty[i];
		u16 base = HOENN_VAR_PIKE_PARTY + i * 5;
		u16 hp = MathMin(VarGet(base + 0), GetMonData(mon, MON_DATA_MAX_HP, NULL));
		u32 status = VarGet(base + 1);
		u16 item = VarGet(base + 4);

		if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE)
			continue;
		SetMonData(mon, MON_DATA_HP, &hp);
		SetMonData(mon, MON_DATA_STATUS, &status);
		for (j = 0; j < MAX_MON_MOVES; ++j)
		{
			u8 pp = VarGet(base + 2 + j / 2) >> ((j % 2) * 8);

			SetMonData(mon, MON_DATA_PP1 + j, &pp);
		}
		SetMonData(mon, MON_DATA_HELD_ITEM, &item);
	}
}

/* ---- ダブルの部屋（相手 2 人・エメラルドの SPECIAL_BATTLE_PIKE_DOUBLE） ---------------------------------- */
//エメラルドは 1 人 1 匹（FillFrontierTrainersParties(1)）。施設の戦いの型は CFRU に「相手 2 人・味方なし」が無いので、
//FLAG_TWO_OPPONENTS を立てて src/overworld.c の BattleSetup_StartTrainerBattle（patches/0126）で ダブル＋相手 2 人 にする。
//手持ちの数は VAR_BATTLE_FACILITY_POKE_NUM（BuildFrontierParty が読む）を戦いの間だけ 1 に。
//★ HoennTower_SetBattleLevel のあと・trainerbattle の前に呼ぶ。終わりの方は LASTRESULT（勝ち負け）を変えない
void HoennPike_SetupDoubleBattle(void)
{
	FlagSet(FLAG_TWO_OPPONENTS);
	VarSet(VAR_SECOND_OPPONENT, BATTLE_TOWER_SPECIAL_TID);
	VarSet(VAR_BATTLE_FACILITY_POKE_NUM, 1);
}

void HoennPike_EndDoubleBattle(void)
{
	FlagClear(FLAG_TWO_OPPONENTS);
	VarSet(VAR_BATTLE_FACILITY_POKE_NUM, FRONTIER_PARTY_SIZE);
}

//施設の戦いを ダブル＋相手 2 人 にするか（src/overworld.c・patches/0126）
bool8 HoennPike_IsTwoOpponentBattle(void)
{
	return InPikeFacility() && FlagGet(FLAG_TWO_OPPONENTS);
}

/* ---- 野生の部屋（エメラルドの TryGenerateBattlePikeWildMon・草むら） ----------------------------------- */
//その地図にいるか（表は src/hoenn_pike_data.h・生成器が地図の番号を書く）
bool8 HoennPike_InWildRoom(void)
{
	if (FlagGet(FLAG_BATTLE_FACILITY) && HoennPyramid_OnFloor()) //★ 2026-10-03 バトルピラミッドの階もどのマスでも出る（src/hoenn_pyramid.c）
		return TRUE;
	return FlagGet(FLAG_BATTLE_FACILITY) && InPikeFacility()
	    && gSaveBlock1->location.mapGroup == HOENN_PIKE_WILD_GROUP && gSaveBlock1->location.mapNum == HOENN_PIKE_WILD_NUM;
}

//連勝で表を選ぶ（エメラルドの GetBattlePikeWildMonHeaderId: 20・40・60 周まで）
static u8 WildHeaderId(void)
{
	u16 streak = VarGet(HOENN_VAR_PIKE_STREAK + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0));

	if (streak <= 20 * NUM_PIKE_ROOMS)
		return 0;
	else if (streak <= 40 * NUM_PIKE_ROOMS)
		return 1;
	else if (streak <= 60 * NUM_PIKE_ROOMS)
		return 2;
	return 3;
}

//src/wild_encounter.c の LoadProperMonsData（patches/0126）: 草むらの表（それ以外の種類は無し）
const struct WildPokemonInfo* HoennPike_WildMonsInfo(u8 type)
{
	if (HoennPyramid_OnFloor())
		return HoennPyramid_WildMonsInfo(type);
	if (type != LAND_MONS_HEADER)
		return NULL;
	return &sHoennPikeWildInfo[WildHeaderId()];
}

static u8 HighestPartyLevel(void)
{
	u8 best = 0;
	u32 i;

	for (i = 0; i < FRONTIER_PARTY_SIZE; ++i)
		if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES, NULL) != SPECIES_NONE)
			best = MathMax(best, GetMonData(&gPlayerParty[i], MON_DATA_LEVEL, NULL));
	return best;
}

//出た子のレベル・技・特性を決め直す（src/wild_encounter.c・patches/0126。エメラルドの TryGenerateBattlePikeWildMon(FALSE)）。
//レベル 50 は 50 - 差・オープンは手持ちのいちばん高いレベル - 差（下は 60・エメラルドの FRONTIER_MIN_LEVEL_OPEN）
void HoennPike_AdjustWildMon(void)
{
	struct Pokemon* mon = &gEnemyParty[0];
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	bool8 open = VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0;
	const struct HoennPikeWildMon* set = NULL;
	s32 level;
	u8 altAbility = 0;
	u32 i;

	if (HoennPyramid_OnFloor()) //★ ピラミッドはエメラルドの GenerateBattlePyramidWildMon
	{
		HoennPyramid_GenerateWildMon();
		return;
	}
	for (i = 0; i < 3; ++i)
		if (sHoennPikeWildMons[open][WildHeaderId()][i].species == species)
			set = &sHoennPikeWildMons[open][WildHeaderId()][i];
	if (set == NULL)
		return;

	if (open)
		level = MathMax(HighestPartyLevel() - set->levelDelta, 60);
	else
		level = 50 - set->levelDelta;
	SetMonData(mon, MON_DATA_EXP, &gExperienceTables[gBaseStats[species].growthRate][level]);
	if (gBaseStats[species].ability2 != ABILITY_NONE)
		altAbility = Random() % 2;
	SetMonData(mon, MON_DATA_ALT_ABILITY, &altAbility);
	for (i = 0; i < MAX_MON_MOVES; ++i)
	{
		u16 move = set->moves[i];
		u8 pp = gBattleMoves[move].pp;

		SetMonData(mon, MON_DATA_MOVE1 + i, &move);
		SetMonData(mon, MON_DATA_PP1 + i, &pp);
	}
	CalculateMonStats(mon);
}

//野生の戦いの終わり（src/wild_encounter.c の DoStandardWildBattle が gMain.savedCallback に置く・patches/0126）。
//素の CB2_EndWildBattle は負けると全滅（ポケセンへ）にする。チューブは地図に戻し、負け（引き分けも）なら一時変数 VAR_TEMP_1 を 1 に →
//野生の部屋の ON_FRAME がロビーへ（エメラルドの BattleFrontier_BattlePikeRoomWildMons_OnResume）。
//★ gBattleOutcome を地図に戻ってから見ると、前の挑戦の最後の戦いの結果が残っていることがある（入っただけで負けになる）ので、ここで付ける。
//  一時変数は戦いから戻っただけでは消えない（消えるのはワープのとき）
void HoennPike_CB2_EndWildBattle(void)
{
	if (HoennPyramid_OnFloor()) //★ ピラミッドは結果を一時変数 VAR_TEMP_2 に（階の ON_FRAME が見る）
		HoennPyramid_RecordWildOutcome();
	else if (gBattleOutcome == B_OUTCOME_LOST || gBattleOutcome == B_OUTCOME_DREW)
		VarSet(0x4001, 1); //VAR_TEMP_1
	CpuFill16(0, (void*) BG_PLTT, BG_PLTT_SIZE);
	ResetOamRange(0, 128);
	SetMainCallback2(CB2_ReturnToField);
}

/* ---- アザミの部屋のカーテン（エメラルドの SetCurtainTiles*・部屋の 7 マス） ----------------------------------- */
//Var8004 = 0 閉じた / 1 ほとんど閉じた / 2 少し閉じた / 3 開いた。(4, 3) だけ通れる（エメラルドの setmetatile の FALSE）
void HoennPike_SetQueenCurtain(void)
{
	static const u8 sXY[7][2] = {{4, 1}, {3, 2}, {4, 2}, {5, 2}, {3, 3}, {4, 3}, {5, 3}};
	u8 stage = MathMin(Var8004, 3);
	u32 i;

	for (i = 0; i < 7; ++i)
		MapGridSetMetatileIdAt(sXY[i][0] + 7, sXY[i][1] + 7, sHoennPikeQueenCurtain[stage][i] | (i == 5 ? 0 : METATILE_COLLISION_MASK));
	DrawWholeMapView();
}
