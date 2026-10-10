#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/fieldmap.h"
#include "../include/overworld.h"
#include "../include/script.h"
#include "../include/string_util.h"
#include "../include/pokemon.h"
#include "../include/item.h"
#include "../include/list_menu.h"
#include "../include/new/frontier.h"
#include "../include/new/multi.h"
#include "../include/new/overworld.h"
#include "../include/new/util.h"
#include "../include/constants/items.h"
#include "hoenn_trainer_hill_data.h"

/*
 * トレーナーヒル（#40・2026-10-03 ユーザー「カードe+ の機能を NPC に搭載して選べるように」→「B（トレーナーヒルそのものを作る）」）。
 * docs/research/2026-10-03_trainer-hill.md。pokeemerald src/trainer_hill.c を FRLG（CFRU）の仕組みで書き直したもの。
 *
 *   組（10 個・受付で選ぶ）・階・地形・手持ちは src/hoenn_trainer_hill_data.h（tools/frontier/build_trainer_hill.py が生成）。
 *   トレーナーの名前・セリフ・見た目は特別なトレーナーの表 gSpecialTowerTrainers の 300 番から（build_frontier_trainers.py）。
 *
 *   ・地図: 群 69 の 60 入口・61〜64 が 1〜4 階・65 屋上・66 エレベーター（エメラルドの群 26 の 60〜66）
 *   ・階の地形（エメラルドの GenerateTrainerHillFloorLayout）: 階のマップスクリプトの ON_LOAD から HoennHill_FloorOnLoad。
 *     上 5 行（入口と出口の階段）は地図のまま、その下 16 行を組の地形で上書き（素 FRLG の VMap・ピラミッドと同じ）
 *   ・2 人（LoadTrainerHillObjectEventTemplates）: ワープで入ったときだけ置く（ON_TRANSITION が一時変数に印）。
 *     台本は trainerbattle 1 の番号 0x7F8 + 何人目（src/hoenn_trainer_flags.c が倒した印の旗 0x12F0 + 階 × 2 + 何人目 に読み替え・
 *     src/overworld.c の差し込み口（patches/0130）が戦う前に特別なトレーナーの番号へ差し替える）。2 人同時に見つかると 2 人とのダブル
 *   ・手持ち: 1 人目は 1〜3 匹目・2 人目は 4〜6 匹目（エメラルドの sTrainerPartySlots・生成器が 3 匹ずつにしてある）。
 *     レベルは手持ちのいちばん高いレベル（エメラルドの GetHighestLevelInPlayerParty）。src/build_pokemon.c の差し込み口（patches/0143）
 *   ・戦いのあと回復しない（src/hoenn_pike.c の HoennPike_KeepDamageAfterBattle）・負けたら受付へ（回復してもらえる）
 *   ・時間: 始めたときのプレイ時間（フレーム）を覚え、屋上の主人に話すまでの差。最速の記録は組ごとに秒で
 *   ・賞品（エメラルドの GetPrizeItemId）: 4 階の組だけ。12 分未満は組ごとの賞品・それ以上はエーテル〜グレートボール
 * ★ 書き換え可能な static は使えない（6.13）。状態は予約変数・旗・一時変数。
 */

#define HILL_GROUP 69
#define HILL_MAP_ENTRANCE 60
#define HILL_MAP_1F 61
#define HILL_MAP_4F 64
#define HILL_MAP_ROOF 65
#define HILL_FLOORS 4
#define HILL_TRAINERS_PER_FLOOR 2
#define HILL_MARGIN 5                       //上 5 行は地図のまま（エメラルドの HILL_FLOOR_HEIGHT_MARGIN）
#define HILL_TRAINER_TID_BASE 0x7F8         //台本の trainerbattle の番号（src/hoenn_trainer_flags.c・patches/0130 と同じ）
#define HILL_FLAG_BASE 0x12F0               //倒した印 0x12F0〜0x12F7（build_hoenn_scripts.RESERVED_FLAGS）
#define VAR_HILL_GROUP 0x51EF               //選んだ組（build_hoenn_scripts.RESERVED_VARS）
#define VAR_HILL_STATE 0x51F0
#define VAR_HILL_START 0x51F1               //始めたときのプレイ時間（フレーム・下位 / 上位 16 ビット）
#define VAR_HILL_TIME 0x51F3                //止めた時間（フレーム）
#define VAR_HILL_BEST 0x51F6                //組ごとの最速（秒・0 は記録なし）× 10
#define ST_ACTIVE 0x01
#define ST_LOST 0x02
#define ST_SPOKE_OWNER 0x04
#define ST_PRIZE 0x08
#define ST_CHECKED_TIME 0x10
#define VAR_TEMP_OUTCOME 0x4002             //1 勝った / 2 負けた（スクリプトと同じ）
#define VAR_TEMP_SLOT_A 0x4003
#define VAR_TEMP_SLOT_B 0x4004
#define VAR_TEMP_FRESH 0x4005
#define MAP_OFFSET 7
#define FRAMES_PER_SECOND 60

struct BackupMapLayoutFR { s32 width; s32 height; u16 *map; };
#define VMap (*(struct BackupMapLayoutFR *)0x03005080)   //素 FRLG の VMap（src/hoenn_pyramid.c と同じ）

extern const u8 *const gHoennHillTrainerScripts[HILL_TRAINERS_PER_FLOOR];   //assembly/frontier/hill_scripts.s

/* ---- 状態 ------------------------------------------------------------------------------------------- */
static u16 State(void)
{
	return VarGet(VAR_HILL_STATE);
}

static void SetState(u16 bits, bool8 on)
{
	VarSet(VAR_HILL_STATE, on ? (State() | bits) : (State() & ~bits));
}

static u8 Group(void)
{
	return VarGet(VAR_HILL_GROUP) % HOENN_HILL_GROUP_COUNT;
}

static const struct HoennHillGroup* CurGroup(void)
{
	return &sHoennHillGroups[Group()];
}

//いる階（0〜3）・階でなければ 0xFF
static u8 FloorIdx(void)
{
	if (gSaveBlock1->location.mapGroup != HILL_GROUP)
		return 0xFF;
	if ((u8) gSaveBlock1->location.mapNum < HILL_MAP_1F || (u8) gSaveBlock1->location.mapNum > HILL_MAP_4F)
		return 0xFF;
	return (u8) gSaveBlock1->location.mapNum - HILL_MAP_1F;
}

//組の階（無い階＝組の階の数をこえた階は NULL）
static const struct HoennHillFloor* CurFloor(void)
{
	u8 f = FloorIdx();

	if (f == 0xFF || f >= CurGroup()->numFloors)
		return NULL;
	return &sHoennHillFloors[CurGroup()->firstFloor + f];
}

bool8 HoennHill_OnFloor(void)
{
	return (State() & ST_ACTIVE) && CurFloor() != NULL;
}

static u32 PlayFrames(void)
{
	return ((u32) gSaveBlock2->playTimeHours * 3600 + gSaveBlock2->playTimeMinutes * 60 + gSaveBlock2->playTimeSeconds)
	       * FRAMES_PER_SECOND + gSaveBlock2->playTimeVBlanks;
}

static u32 GetVar32(u16 var)
{
	return VarGet(var) | ((u32) VarGet(var + 1) << 16);
}

static void SetVar32(u16 var, u32 v)
{
	VarSet(var, v & 0xFFFF);
	VarSet(var + 1, v >> 16);
}

//今の時間（止めたあとは止めた時間）
static u32 ElapsedFrames(void)
{
	if (State() & ST_SPOKE_OWNER)
		return GetVar32(VAR_HILL_TIME);
	return PlayFrames() - GetVar32(VAR_HILL_START);
}

/* ---- 階の地形と 2 人 ------------------------------------------------------------------------------------ */
static void GenerateFloorLayout(const struct HoennHillFloor* floor)
{
	const struct HoennHillMap* map = &sHoennHillMaps[floor->map];
	s32 x, y;

	for (y = 0; y < 16; ++y)
	{
		u16* dst = VMap.map + VMap.width * (MAP_OFFSET + HILL_MARGIN + y) + MAP_OFFSET;
		for (x = 0; x < 16; ++x)
			dst[x] = map->blocks[y * 16 + x];
	}
}

static void SetTemplateGfx(struct EventObjectTemplate *t, u16 gfx)
{
	t->graphicsIdLowerByte = gfx & 0xFF;
	t->graphicsIdUpperByte = gfx >> 8;
}

//エメラルドの LoadTrainerHillObjectEventTemplates（倒した人も置く・話しかけると試合のあとのセリフ）
static void LoadObjectTemplates(const struct HoennHillFloor* floor)
{
	const struct HoennHillMap* map = &sHoennHillMaps[floor->map];
	struct EventObjectTemplate *t = gSaveBlock1->eventObjectTemplates;
	u32 i;

	for (i = 0; i < 64; ++i)
		Memset(&t[i], 0, sizeof(t[i]));
	for (i = 0; i < HILL_TRAINERS_PER_FLOOR; ++i)
	{
		u16 id = HOENN_HILL_FIRST_TRAINER + floor->trainers[i];

		t[i].localId = i + 1;
		SetTemplateGfx(&t[i], gSpecialTowerTrainers[id].owNum);
		t[i].x = map->coords[i] & 0xF;
		t[i].y = (map->coords[i] >> 4) + HILL_MARGIN;
		t[i].elevation = 3;
		t[i].movementType = (map->dirs[i] - 1) + 7;   //エメラルドと同じ（DIR - 1 + MOVEMENT_TYPE_FACE_UP）
		t[i].trainerType = 1;                            //TRAINER_TYPE_NORMAL
		t[i].trainerRange_berryTreeId = map->ranges[i];
		t[i].script = gHoennHillTrainerScripts[i];
	}
}

//ON_TRANSITION（ワープで入ったときだけ）
void HoennHill_FloorOnTransition(void)
{
	VarSet(VAR_TEMP_FRESH, 1);
}

//ON_LOAD（ワープでも「つづきから」でも）
void HoennHill_FloorOnLoad(void)
{
	const struct HoennHillFloor* floor = CurFloor();

	if (floor == NULL || !(State() & ST_ACTIVE))
		return;
	if (VarGet(VAR_TEMP_FRESH))
	{
		LoadObjectTemplates(floor);
		VarSet(VAR_TEMP_FRESH, 0);
	}
	GenerateFloorLayout(floor);
}

//ON_RESUME: 相手のレベル（手持ちのいちばん高いレベル）
void HoennHill_FloorOnResume(void)
{
	u8 i, level = 1;

	for (i = 0; i < PARTY_SIZE; ++i)
	{
		struct Pokemon* mon = &gPlayerParty[i];
		if (GetMonData(mon, MON_DATA_SPECIES, NULL) != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG, NULL))
			level = MathMax(level, GetMonData(mon, MON_DATA_LEVEL, NULL));
	}
	VarSet(VAR_BATTLE_FACILITY_POKE_LEVEL, level);
	VarSet(VAR_BATTLE_FACILITY_POKE_NUM, 3);
	VarSet(VAR_BATTLE_FACILITY_TIER, 0);                  //BATTLE_FACILITY_STANDARD（レベルを変えない）
}

/* ---- 戦い（src/overworld.c の差し込み口 patches/0130・src/build_pokemon.c patches/0143） ------------------ */
static u16 ResolveOne(u16 tid, u16 tempVar, u8 battler)
{
	const struct HoennHillFloor* floor = CurFloor();
	u16 slot;

	if (floor == NULL || tid < HILL_TRAINER_TID_BASE || tid >= HILL_TRAINER_TID_BASE + HILL_TRAINERS_PER_FLOOR)
		return tid;
	slot = tid - HILL_TRAINER_TID_BASE;
	VarSet(tempVar, slot + 1);
	VarSet(VAR_FACILITY_TRAINER_ID + battler, HOENN_HILL_FIRST_TRAINER + floor->trainers[slot]);
	HoennHill_FloorOnResume();
	return BATTLE_TOWER_SPECIAL_TID;
}

//1 人目（trainerbattle の番号 0x7F8 + n → 特別なトレーナー）
void HoennHill_ResolveOpponent(void)
{
	if (!HoennHill_OnFloor())
		return;
	VarSet(VAR_TEMP_SLOT_B, 0);
	gTrainerBattleOpponent_A = ResolveOne(gTrainerBattleOpponent_A, VAR_TEMP_SLOT_A, 0);
}

//2 人目（2 人同時に見つかったとき）
void HoennHill_ResolveOpponentB(void)
{
	if (!HoennHill_OnFloor())
		return;
	gTrainerBattleOpponent_B = ResolveOne(gTrainerBattleOpponent_B, VAR_TEMP_SLOT_B, 1);
	VarSet(VAR_SECOND_OPPONENT, gTrainerBattleOpponent_B);
}

//倒した印の旗（src/hoenn_trainer_flags.c）→ 0 = ヒルの人ではない
u16 HoennHill_TrainerFlag(u16 trainerId)
{
	u8 f = FloorIdx();

	if (f == 0xFF)
		return 0;
	if (trainerId >= HILL_TRAINER_TID_BASE && trainerId < HILL_TRAINER_TID_BASE + HILL_TRAINERS_PER_FLOOR)
		return HILL_FLAG_BASE + f * HILL_TRAINERS_PER_FLOOR + (trainerId - HILL_TRAINER_TID_BASE);
	if (trainerId == BATTLE_TOWER_SPECIAL_TID && HoennHill_OnFloor() && VarGet(VAR_TEMP_SLOT_A))
		return HILL_FLAG_BASE + f * HILL_TRAINERS_PER_FLOOR + (VarGet(VAR_TEMP_SLOT_A) - 1);
	return 0;
}

//手持ち（src/build_pokemon.c の SPECIAL_TRAINER_REGULAR_SPREADS の前・patches/0143）
bool8 HoennHill_InHillBattle(void)
{
	return HoennHill_OnFloor() && VarGet(VAR_TEMP_SLOT_A) != 0;
}

const struct BattleTowerSpread* HoennHill_MakeSpread(struct BattleTowerSpread* out, u16 tableId, u8 slot)
{
	const struct HoennHillMon* mon = &sHoennHillMons[(tableId - HOENN_HILL_FIRST_TRAINER) % HOENN_HILL_TRAINER_COUNT][slot % 3];
	u32 i, numStats = 0;
	u8 ev, iv = 31;

	Memset(out, 0, sizeof(struct BattleTowerSpread));
	out->species = mon->species;
	out->nature = mon->nature;
	out->ball = 3;   //BALL_TYPE_POKE_BALL（エメラルドの施設のポケモンと同じ）
	out->hpIv = out->atkIv = out->defIv = out->spdIv = out->spAtkIv = out->spDefIv = iv;
	for (i = 0; i < NUM_STATS; ++i)
	{
		if (mon->evSpread & (1 << i))
			++numStats;
	}
	ev = (numStats == 0) ? 0 : MathMin(255, 510 / numStats);
	if (mon->evSpread & (1 << 0)) out->hpEv = ev;
	if (mon->evSpread & (1 << 1)) out->atkEv = ev;
	if (mon->evSpread & (1 << 2)) out->defEv = ev;
	if (mon->evSpread & (1 << 3)) out->spdEv = ev;
	if (mon->evSpread & (1 << 4)) out->spAtkEv = ev;
	if (mon->evSpread & (1 << 5)) out->spDefEv = ev;
	out->item = mon->item;
	for (i = 0; i < MAX_MON_MOVES; ++i)
		out->moves[i] = mon->moves[i];
	out->ability = mon->ability;
	out->teraType = mon->teraType;
	out->gigantamax = mon->gigantamax;
	if (mon->hpIvMask & 0x80)
	{
		u8 m = mon->hpIvMask;
		out->hpIv    = (m & 0x01) ? (iv | 1) : (iv & ~1);
		out->atkIv   = (m & 0x02) ? (iv | 1) : (iv & ~1);
		out->defIv   = (m & 0x04) ? (iv | 1) : (iv & ~1);
		out->spdIv   = (m & 0x08) ? (iv | 1) : (iv & ~1);
		out->spAtkIv = (m & 0x10) ? (iv | 1) : (iv & ~1);
		out->spDefIv = (m & 0x20) ? (iv | 1) : (iv & ~1);
	}
	out->forSingles = TRUE;
	out->forDoubles = TRUE;
	return out;
}

//トレーナー戦の終わり（負けても全滅にせず地図へ戻る・勝ったら 2 人とも倒した印）
void HoennHill_CB2_EndTrainerBattle(void)
{
	u8 f = FloorIdx();

	if (gBattleOutcome == B_OUTCOME_WON)
	{
		VarSet(VAR_TEMP_OUTCOME, 1);
		if (f != 0xFF)
		{
			if (VarGet(VAR_TEMP_SLOT_A))
				FlagSet(HILL_FLAG_BASE + f * HILL_TRAINERS_PER_FLOOR + VarGet(VAR_TEMP_SLOT_A) - 1);
			if (VarGet(VAR_TEMP_SLOT_B))
				FlagSet(HILL_FLAG_BASE + f * HILL_TRAINERS_PER_FLOOR + VarGet(VAR_TEMP_SLOT_B) - 1);
		}
	}
	else
	{
		VarSet(VAR_TEMP_OUTCOME, 2);
		SetState(ST_LOST, TRUE);
	}
	VarSet(VAR_TEMP_SLOT_A, 0);
	VarSet(VAR_TEMP_SLOT_B, 0);
	SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
}

//話しかけたときの試合のあとのセリフ（エメラルドの ShowTrainerHillPostBattleText）→ gStringVar4
void HoennNative_HillBufferAfterText(void)
{
	const struct HoennHillFloor* floor = CurFloor();
	u8 slot = gSpecialVar_LastTalked - 1;

	if (floor == NULL || slot >= HILL_TRAINERS_PER_FLOOR)
	{
		gStringVar4[0] = 0xFF;
		return;
	}
	StringCopy(gStringVar4, sHoennHillAfterText[floor->trainers[slot]]);
}

/* ---- 受付・屋上（assembly/frontier/hill_scripts.s の callasm から） --------------------------------------- */
//組の選択肢（0x8005 = ページ 0 / 1）: CFRU の自由な選択肢（gMultiChoice・7 つまで）に 5 組 ＋ つぎへ／もどる ＋ やめる
//→ スクリプトは multichoice の 0x25（7 つ）で出す。選んだ番号 n（0〜4）の組は ページ × 5 + n
extern const u8 gText_HackHill_Next[];
extern const u8 gText_HackHill_Back[];
extern const u8 gText_HackHill_Cancel[];
void HoennNative_HillAddGroupMenu(void)
{
	u8 page = Var8005 & 1, i;

	for (i = 0; i < 5; ++i)
		gMultiChoice[i].name = sHoennHillGroupNames[(page * 5 + i) % HOENN_HILL_GROUP_COUNT];
	gMultiChoice[5].name = page ? gText_HackHill_Back : gText_HackHill_Next;
	gMultiChoice[6].name = gText_HackHill_Cancel;
}

//組を決めて始める（0x8005 = 組の番号）。倒した印を消し、時間を計り始める
void HoennNative_HillStart(void)
{
	u32 i;

	VarSet(VAR_HILL_GROUP, Var8005 % HOENN_HILL_GROUP_COUNT);
	VarSet(VAR_HILL_STATE, ST_ACTIVE);
	for (i = 0; i < HILL_FLOORS * HILL_TRAINERS_PER_FLOOR; ++i)
		FlagClear(HILL_FLAG_BASE + i);
	SetVar32(VAR_HILL_START, PlayFrames());
	SetVar32(VAR_HILL_TIME, 0);
}

//挑戦をやめる（受付へ戻ったとき・負けたとき）
void HoennNative_HillEnd(void)
{
	VarSet(VAR_HILL_STATE, 0);
}

//→ LASTRESULT: 0 ふつう / 1 負けて戻った
void HoennNative_HillGetStatus(void)
{
	gSpecialVar_LastResult = (State() & ST_LOST) ? 1 : 0;
}

//→ LASTRESULT: 挑戦中か
void HoennNative_HillInChallenge(void)
{
	gSpecialVar_LastResult = (State() & ST_ACTIVE) && !(State() & ST_SPOKE_OWNER);
}

//→ LASTRESULT: 組の階の数が 4 か・STR_VAR_1 = 階の数（エメラルドの GetAllFloorsUsed）
void HoennNative_HillAllFloorsUsed(void)
{
	u8 n = sHoennHillGroups[Var8005 % HOENN_HILL_GROUP_COUNT].numFloors;

	ConvertIntToDecimalStringN(gStringVar1, n, STR_CONV_MODE_LEFT_ALIGN, 1);
	gSpecialVar_LastResult = (n == HILL_FLOORS);
}

//屋上の主人（エメラルドの GetOwnerState）: 時間を止める → LASTRESULT 0 はじめて / 1 2 回目 / 2 賞品と記録が済んだ
void HoennNative_HillGetOwnerState(void)
{
	u16 r = 0;

	if (!(State() & ST_SPOKE_OWNER))
	{
		SetVar32(VAR_HILL_TIME, ElapsedFrames());
		SetState(ST_SPOKE_OWNER, TRUE);
	}
	else
		++r;
	if ((State() & ST_PRIZE) && (State() & ST_CHECKED_TIME))
		++r;
	gSpecialVar_LastResult = r;
}

//エメラルドの GetPrizeItemId（12 分未満は組ごとの賞品・それ以上は時間で）
static const u16 sGrandPrizes[HOENN_HILL_GROUP_COUNT] =
{
	ITEM_TM26, ITEM_TM36, ITEM_TM11, ITEM_TM05, ITEM_TM48, ITEM_ELIXIR, ITEM_TM19, ITEM_TM31, ITEM_NONE, ITEM_NONE,
};

static u16 PrizeItem(void)
{
	u32 minutes = ElapsedFrames() / (FRAMES_PER_SECOND * 60);

	if (minutes < 12)
		return sGrandPrizes[Group()];
	if (minutes < 13)
		return ITEM_ETHER;
	if (minutes < 14)
		return ITEM_MAX_POTION;
	if (minutes < 16)
		return ITEM_REVIVE;
	if (minutes < 18)
		return ITEM_FLUFFY_TAIL;
	return ITEM_GREAT_BALL;
}

//→ LASTRESULT 0 もらった（STR_VAR_2 = 道具の名前）/ 1 バッグがいっぱい / 2 賞品なし（もらった・4 階でない組）
void HoennNative_HillGivePrize(void)
{
	u16 item = PrizeItem();

	if (CurGroup()->numFloors != HILL_FLOORS || item == ITEM_NONE || (State() & ST_PRIZE))
		gSpecialVar_LastResult = 2;
	else if (AddBagItem(item, 1))
	{
		CopyItemName(item, gStringVar2);
		Var8004 = item;
		SetState(ST_PRIZE, TRUE);
		gSpecialVar_LastResult = 0;
	}
	else
		gSpecialVar_LastResult = 1;
}

//→ LASTRESULT 0 新記録 / 1 新記録でない / 2 もう見た（エメラルドの CheckFinalTime）
void HoennNative_HillCheckFinalTime(void)
{
	u32 secs = ElapsedFrames() / FRAMES_PER_SECOND;
	u16 best = VarGet(VAR_HILL_BEST + Group());

	if (State() & ST_CHECKED_TIME)
		gSpecialVar_LastResult = 2;
	else if (best == 0 || secs < best)
	{
		VarSet(VAR_HILL_BEST + Group(), MathMin(secs, 0xFFFF));
		gSpecialVar_LastResult = 0;
	}
	else
		gSpecialVar_LastResult = 1;
	SetState(ST_CHECKED_TIME, TRUE);
}

//STR_VAR_1 分・STR_VAR_2 秒・STR_VAR_3 百分の一秒（エメラルドの BufferChallengeTime）
void HoennNative_HillBufferTime(void)
{
	u32 total = ElapsedFrames();
	u32 minutes = total / (60 * 60);

	if (minutes > 99)
		total = 99 * 3600 + 59 * 60 + 59;
	ConvertIntToDecimalStringN(gStringVar1, total / 3600, STR_CONV_MODE_RIGHT_ALIGN, 2);
	total %= 3600;
	ConvertIntToDecimalStringN(gStringVar2, total / 60, STR_CONV_MODE_RIGHT_ALIGN, 2);
	ConvertIntToDecimalStringN(gStringVar3, ((total % 60) * 168) / 100, STR_CONV_MODE_LEADING_ZEROS, 2);
}

//掲示板（エメラルドの ShowTrainerHillRecords の代わりに文で）: 0x8005 = 組 → STR_VAR_1 組の名前・STR_VAR_2 分・STR_VAR_3 秒
//→ LASTRESULT 記録があるか
void HoennNative_HillBufferRecord(void)
{
	u8 g = Var8005 % HOENN_HILL_GROUP_COUNT;
	u16 best = VarGet(VAR_HILL_BEST + g);

	StringCopy(gStringVar1, sHoennHillGroupNames[g]);
	ConvertIntToDecimalStringN(gStringVar2, best / 60, STR_CONV_MODE_RIGHT_ALIGN, 2);
	ConvertIntToDecimalStringN(gStringVar3, best % 60, STR_CONV_MODE_LEADING_ZEROS, 2);
	gSpecialVar_LastResult = (best != 0);
}

//組の名前の選択肢の文（スクリプトの選択肢に使う）: 0x8005 = 組 → gStringVar1
const u8* HoennHill_GroupName(u8 g)
{
	return sHoennHillGroupNames[g % HOENN_HILL_GROUP_COUNT];
}
