#include "defines.h"
#include "../include/event_data.h"
#include "../include/random.h"
#include "../include/new/catching.h"
#include "../include/new/frontier.h"
#include "../include/new/util.h"
#include "../include/bg.h"
#include "../include/window.h"
#include "hoenn_frontier_data.h"
#include "hoenn_frontier_brain_mons.h"
#include "Tables/hoenn_script_flags.h"

/*
 * ホウエンのバトルフロンティア（#5・2026-09-28 ユーザー「バトルフロンティアの移植　各施設のNPCだけ」）
 * docs/research/2026-09-28_battle-frontier-npcs.md
 *
 * 相手はエメラルドのトレーナー 300 人（CFRU の「特別なトレーナー」gSpecialTowerTrainers・名前・セリフ・見た目は
 * src/Tables/hoenn_frontier_trainers.h）。手持ちはエメラルドと同じく、そのトレーナーの候補の一覧から 1 匹ずつ選ぶ。
 * ★ CFRU の形（1 匹 28 バイト）で 300 人ぶんの候補を持つと約 180KB で CFRU の枠をほぼ使い切るので、
 *   エメラルドの小さい形（hoenn_frontier_data.h・tools/frontier/build_frontier_trainers.py が生成）を持ち、
 *   CFRU が 1 匹選ぶ所（src/build_pokemon.c の SPECIAL_TRAINER_REGULAR_SPREADS・patches/0078）でここを呼ぶ。
 *   同じポケモン・ルール違反の選び直しは CFRU の今の処理がそのまま効く。
 * ★ 書き換え可能な static は使えない（CLAUDE.md 6.13）。できあがった 1 匹は呼び元の関数の中の変数（out）に書く。
 */

static const struct BattleTowerSpread* MakeMonSpread(struct BattleTowerSpread* out, const struct HoennFrontierMon* mon, u8 iv);

//エメラルドの 1 匹 → CFRU の形。個体値はトレーナーの番号で決まる（GetFrontierTrainerFixedIvs）・努力値は振る能力の数で 510 を割る
const struct BattleTowerSpread* HoennFrontier_MakeSpread(struct BattleTowerSpread* out, u16 tableId)
{
	const struct HoennFrontierTrainerMons* trainer = &sHoennFrontierTrainerMons[tableId % HOENN_FRONTIER_TRAINERS_COUNT];

	return MakeMonSpread(out, &sHoennFrontierMons[trainer->mons[Random() % trainer->count]], trainer->iv);
}

/* ---- バトルファクトリー（2026-10-03・src/hoenn_factory.c）------------------------------------------
 * レンタル・相手の 1 匹は「表の番号（下 10 ビット）+ 個体値 << 10」の 1 語（予約変数 HOENN_VAR_FACTORY_*）。
 * 表はエメラルドの gBattleFrontierMons と同じ並び（中身は Smogon の型）なので、エメラルドの番号の範囲がそのまま使える */
const struct BattleTowerSpread* HoennFactory_MakeSpread(struct BattleTowerSpread* out, u16 packed)
{
	return MakeMonSpread(out, &sHoennFrontierMons[(packed & 0x3FF) % NELEMS(sHoennFrontierMons)], packed >> 10);
}

u16 HoennFactory_MonSpecies(u16 monId)
{
	return sHoennFrontierMons[monId % NELEMS(sHoennFrontierMons)].species;
}

u16 HoennFactory_MonItem(u16 monId)
{
	return sHoennFrontierMons[monId % NELEMS(sHoennFrontierMons)].item;
}

u16 HoennFactory_MonMove(u16 monId, u8 slot)
{
	return sHoennFrontierMons[monId % NELEMS(sHoennFrontierMons)].moves[slot & 3];
}

u16 HoennFactory_NumMons(void)
{
	return NELEMS(sHoennFrontierMons);
}

static const struct BattleTowerSpread* MakeMonSpread(struct BattleTowerSpread* out, const struct HoennFrontierMon* mon, u8 iv)
{
	u32 i, numStats = 0;
	u8 ev;

	Memset(out, 0, sizeof(struct BattleTowerSpread));
	out->species = mon->species;
	out->nature = mon->nature;
	out->ball = BALL_TYPE_POKE_BALL;
	out->hpIv = iv;
	out->atkIv = iv;
	out->defIv = iv;
	out->spdIv = iv;
	out->spAtkIv = iv;
	out->spDefIv = iv;

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
	out->ability = mon->ability; //★ 2026-10-01 FRONTIER_ABILITY_*（エメラルドのままの 1 匹は とくせい1・tools/frontier/build_frontier_trainers.py）
	out->teraType = mon->teraType; //★ 2026-10-01 0 = そのまま・タイプ + 1
	out->gigantamax = mon->gigantamax;
	if (mon->hpIvMask & 0x80) //★ 2026-10-01 めざめるパワーのタイプ: 個体値の偶数・奇数をそろえる（CalcMonHiddenPowerType）
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

//次の相手の番号（エメラルドの GetRandomScaledFrontierTrainerId・pokeemerald src/battle_tower.c:1104）。
//challengeNum = 連勝 / 7（何周目か）・battleNum = その周の何戦目か（0〜6。6 は 7 戦目＝強い範囲）
u16 HoennFrontier_ChooseTrainer(u16 challengeNum, u16 battleNum)
{
	const u16 (*ranges)[2] = (battleNum == 6) ? sHoennFrontierTrainerRangesHard : sHoennFrontierTrainerRanges;
	u16 row = MathMin(challengeNum, 7);

	return ranges[row][0] + Random() % (ranges[row][1] - ranges[row][0] + 1);
}

/* ---- この挑戦で出たトレーナー（エメラルドの frontier.trainerIds・PrepareOneTrainer の do-while） ------------ */
//★ 2026-09-29 ユーザー「同じ挑戦で同じトレーナーが二度出るのを防ぐ仕組みいれて」（チューブ）→「同じやり方でいれてちょうだい」
//  （タワー・パレス・アリーナ）。挑戦は一度に 1 つなので置き場は全施設で共用。
//  14 枠（チューブの NUM_PIKE_ROOMS・ダブルの部屋は 1 部屋 2 人。7 連戦の施設は 7 枠しか使わない）× 9 ビット
//  （トレーナーの並びの番号 + 1・0 は空き）を予約変数 8 語（HOENN_VAR_FRONTIER_TRAINERS〜+7）に詰める。
//  やすんでも変数なので残る（エメラルドもセーブに残す）。挑戦の頭（受付でレポートを書く前・チューブは通路も）で消す
#define FOUGHT_SLOTS 14
#define FOUGHT_BITS 9

static u16 GetTrainerSlot(u8 slot)
{
	u32 bit = slot * FOUGHT_BITS;
	u32 lo = VarGet(HOENN_VAR_FRONTIER_TRAINERS + bit / 16), hi = VarGet(HOENN_VAR_FRONTIER_TRAINERS + bit / 16 + 1);

	return ((lo | (hi << 16)) >> (bit % 16)) & ((1 << FOUGHT_BITS) - 1);
}

static void SetTrainerSlot(u8 slot, u16 value)
{
	u32 bit = slot * FOUGHT_BITS, i;

	for (i = 0; i < FOUGHT_BITS; ++i, ++bit)
	{
		u16 var = HOENN_VAR_FRONTIER_TRAINERS + bit / 16;
		u16 mask = 1 << (bit % 16);
		u16 v = VarGet(var);

		VarSet(var, (value & (1 << i)) ? (v | mask) : (v & ~mask));
	}
}

static bool8 TrainerAlreadyFought(u16 id)
{
	u32 i;

	for (i = 0; i < FOUGHT_SLOTS; ++i)
		if (GetTrainerSlot(i) == id + 1)
			return TRUE;
	return FALSE;
}

static void RememberTrainer(u16 id)
{
	u32 i;

	for (i = 0; i < FOUGHT_SLOTS; ++i)
	{
		if (GetTrainerSlot(i) == 0)
		{
			SetTrainerSlot(i, id + 1);
			return;
		}
	}
}

//相手を選ぶ（エメラルドの GetRandomScaledFrontierTrainerId を回す do-while・タワーの SetNextFacilityOpponent も同じ）:
//この挑戦でまだ出ていない人になるまで選び直す。
//★ エメラルドは出ていない人が出るまで回り続ける。範囲は 20 人以上・1 回の挑戦で出るのは 14 人までなので必ず見つかるが、
//  念のため 200 回で打ち切る（そのときは重なっても出す）
u16 HoennFrontier_ChooseNewTrainer(u16 challengeNum, u16 battleNum)
{
	u16 id = 0;
	u32 tries;

	for (tries = 0; tries < 200; ++tries)
	{
		id = HoennFrontier_ChooseTrainer(challengeNum, battleNum);
		if (id >= NUM_SPECIAL_TOWER_TRAINERS)
			id = Random() % NUM_SPECIAL_TOWER_TRAINERS;
		if (!TrainerAlreadyFought(id))
			break;
	}
	RememberTrainer(id);
	return id;
}

//この挑戦で出たトレーナーを消す（callasm でも呼べる）
void HoennFrontier_ClearFoughtTrainers(void)
{
	u32 i;

	for (i = 0; i < (FOUGHT_SLOTS * FOUGHT_BITS + 15) / 16; ++i)
		VarSet(HOENN_VAR_FRONTIER_TRAINERS + i, 0);
}

//フロンティアブレーンの 1 匹（2026-09-29 バトルパレス ③）。エメラルドの CreateFrontierBrainPokemon と同じく、決まった 3 匹を順に・
//個体値は全部 fixedIV・努力値と性格と持ち物と技はそのまま（tools/frontier/build_frontier_brains.py が生成した表）。
//tableId = VAR_FACILITY_TRAINER_ID（施設 × 2 + 銀 0 / 金 1）・slot = 何匹目か（src/build_pokemon.c の BuildFrontierParty の i・patches/0087）
const struct BattleTowerSpread* HoennFrontier_MakeBrainSpread(struct BattleTowerSpread* out, u16 tableId, u8 slot)
{
	const struct HoennFrontierBrainMon* mon = &sHoennFrontierBrainMons[tableId % HOENN_FRONTIER_BRAIN_COUNT][slot % 3];
	u32 i;

	Memset(out, 0, sizeof(struct BattleTowerSpread));
	out->species = mon->species;
	out->nature = mon->nature;
	out->ball = BALL_TYPE_POKE_BALL;
	out->hpIv = mon->fixedIV;
	out->atkIv = mon->fixedIV;
	out->defIv = mon->fixedIV;
	out->spdIv = mon->fixedIV;
	out->spAtkIv = mon->fixedIV;
	out->spDefIv = mon->fixedIV;
	out->hpEv = mon->evs[0];
	out->atkEv = mon->evs[1];
	out->defEv = mon->evs[2];
	out->spdEv = mon->evs[3];
	out->spAtkEv = mon->evs[4];
	out->spDefEv = mon->evs[5];
	out->item = mon->item;
	for (i = 0; i < MAX_MON_MOVES; ++i)
		out->moves[i] = mon->moves[i];
	out->ability = mon->ability; //★ 2026-10-05 ブレーンを Smogon の型に（tools/frontier/build_frontier_brains.py の STRONG_BRAINS・エメラルドのままの子は 1）
	out->teraType = mon->teraType;
	out->forSingles = TRUE;
	out->forDoubles = TRUE;
	return out;
}

/* ---- バトルドーム（2026-10-03・src/hoenn_dome.c）が使う入口 ---------------------------------------------
 * エメラルドの gFacilityTrainers[].monIds（候補の一覧）・gFacilityTrainerMons[].nature / evSpread・
 * GetFrontierBrainMonSpecies / Move / Nature / Evs（ブレーンの決まった 3 匹） */
u8 HoennFrontier_MonNature(u16 monId)
{
	return sHoennFrontierMons[monId % NELEMS(sHoennFrontierMons)].nature;
}

u8 HoennFrontier_MonEvSpread(u16 monId)
{
	return sHoennFrontierMons[monId % NELEMS(sHoennFrontierMons)].evSpread;
}

//エメラルドの GetRandomFrontierMonFromSet（その人の候補から 1 匹）
u16 HoennFrontier_RandomMonOfTrainer(u16 trainerId)
{
	const struct HoennFrontierTrainerMons* trainer = &sHoennFrontierTrainerMons[trainerId % HOENN_FRONTIER_TRAINERS_COUNT];

	return trainer->mons[Random() % trainer->count];
}

static const struct HoennFrontierBrainMon* BrainMon(u16 tableId, u8 slot)
{
	return &sHoennFrontierBrainMons[tableId % HOENN_FRONTIER_BRAIN_COUNT][slot % 3];
}

u16 HoennFrontier_BrainMonSpecies(u16 tableId, u8 slot)
{
	return BrainMon(tableId, slot)->species;
}

u16 HoennFrontier_BrainMonMove(u16 tableId, u8 slot, u8 moveSlot)
{
	return BrainMon(tableId, slot)->moves[moveSlot & 3];
}

u8 HoennFrontier_BrainMonNature(u16 tableId, u8 slot)
{
	return BrainMon(tableId, slot)->nature;
}

u8 HoennFrontier_BrainMonEv(u16 tableId, u8 slot, u8 stat)
{
	return BrainMon(tableId, slot)->evs[stat % NUM_STATS];
}

/* ---- エメラルドから写した画面の窓（2026-10-04） -----------------------------------------------------------------
 * エメラルドの InitWindows は窓の絵の入れ物と BG の配置の入れ物を AllocZeroed で取るが、素 FRLG は Alloc（中身は前の画面の
 * 残り・pokefirered src/window.c）。写した画面（ドームのトーナメント表・情報カード、ファクトリーの画面）はエメラルドの前提で
 * 塗らずに字を書くので、前の画面の残りが字のまわりに出た（ユーザー「バトルドームバグ」）。InitWindows の代わりに呼ぶ */
#define BG_TILEMAP_OF(bg) (*(void **)(0x030008E8 + (bg) * 16 + 4))    /* 素の sGpuBgConfigs2[bg].tilemap（BPRJ.ld に無い） */

bool16 HoennFrontier_InitWindowsZeroed(const struct WindowTemplate *templates)
{
	static const u16 sizes[4] = {0x800, 0x1000, 0x1000, 0x2000};    /* 画面の大きさ 0〜3 の配置の入れ物（素の GetBgMetricTextMode） */
	void *before[4];
	u32 i;
	bool16 ret;

	for (i = 0; i < 4; ++i)
		before[i] = BG_TILEMAP_OF(i);
	ret = InitWindows(templates);
	if (!ret)
		return ret;
	for (i = 0; templates[i].bg != 0xFF; ++i)
		FillWindowPixelBuffer(i, PIXEL_FILL(0));
	for (i = 0; i < 4; ++i)
	{
		void *buf = BG_TILEMAP_OF(i);

		if (before[i] == NULL && buf != NULL)   /* InitWindows が取った入れ物だけ（呼ぶ前に決めてあった入れ物は触らない） */
			CpuFill16(0, buf, sizes[GetBgAttribute(i, BG_ATTR_SCREENSIZE) & 3]);
	}
	return ret;
}
