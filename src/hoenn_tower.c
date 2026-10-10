#include "defines.h"
#include "../include/event_data.h"
#include "../include/fieldmap.h"
#include "../include/load_save.h"
#include "../include/overworld.h"
#include "../include/save.h"
#include "../include/sound.h"
#include "../include/task.h"
#include "../include/random.h"
#include "../include/script.h"
#include "../include/string_util.h"
#include "../include/new/build_pokemon.h"
#include "../include/new/character_customization.h"
#include "../include/new/frontier.h"
#include "../include/new/util.h"
#include "Tables/hoenn_script_flags.h"

/*
 * ホウエンのバトルタワー（#5・2026-09-28 ユーザー「実装して」「エメラルドから移植だよね」）
 * docs/research/2026-09-28_battle-frontier-npcs.md の「バトルタワーの進め方」
 *
 * 流れとセリフはエメラルドどおり（assembly/frontier/tower_scripts.s・tools/frontier/build_frontier_tower.py が生成）。
 * 戦いそのものは CFRU の施設の仕組み（FLAG_BATTLE_FACILITY・相手はエメラルドの 300 人 = gSpecialTowerTrainers）。
 * ここはスクリプトから callasm で呼ぶ小さな処理だけ。状態はすべて予約した変数（HOENN_VAR_TOWER_*）に置く（6.13）。
 *
 * ★ 連勝の記録（CFRU の gBattleTowerStreaks）はレベルを「100 か、それ以外か」の 2 つでしか分けない。
 *   オープンレベルの相手のレベル（60〜100）をそのまま VAR_BATTLE_FACILITY_POKE_LEVEL に入れると 50 の記録と混ざるので、
 *   記録を触る前は 50 / 100（HoennTower_SetRecordLevel）、戦う直前だけ相手のレベル（HoennTower_SetBattleLevel）にする
 */

#define TOWER_FACILITY_VAR 0x403A          //CFRU の BATTLE_FACILITY_NUM（0 = バトルタワー）
#define HOENN_FACILITY_TOWER 0              //HOENN_VAR_FRONTIER_FACILITY
#define HOENN_FACILITY_PALACE 1
#define HOENN_FACILITY_ARENA 2
#define HOENN_FACILITY_PIKE 3
#define HOENN_FACILITY_FACTORY 4          //★ 2026-10-03 バトルファクトリー（src/hoenn_factory.c）
#define HOENN_FACILITY_DOME 5             //★ 2026-10-03 バトルドーム（src/hoenn_dome.c）
#define HOENN_FACILITY_PYRAMID 6          //★ 2026-10-03 バトルピラミッド（src/hoenn_pyramid.c）
#define PALACE_MAX_STREAK 9999              //エメラルドの MAX_STREAK
#define HOENN_ST_WON_BRAIN 5                //挑戦の状態: ブレーンに勝って 7 人抜き（スクリプトの ST_WON_BRAIN と同じ）
#define TOWER_MAX_LEVEL_50 50
#define TOWER_OPEN_MIN_LEVEL 60             //エメラルドのオープンレベルの相手の最低レベル

extern u16 HoennFrontier_ChooseNewTrainer(u16 challengeNum, u16 battleNum); //src/hoenn_frontier.c（この挑戦でまだ出ていない人）
void HoennTower_SetRecordLevel(void);

//参加の子の数。タワーのダブルは 4 匹・ほかは 3 匹（パレス・アリーナは 3 匹・エメラルドの FRONTIER_PARTY_SIZE）
static u8 TowerPartySize(void)
{
	if (VarGet(HOENN_VAR_FRONTIER_FACILITY) != HOENN_FACILITY_TOWER)
		return 3;
	return (VarGet(HOENN_VAR_TOWER_MODE) == 1) ? 4 : 3;
}

/*
 * ★ バトルパレス（2026-09-29・②受付・通路・対戦部屋）もここの処理を使う。挑戦の状態（HOENN_VAR_TOWER_* の 6 つ）は共用
 *   （挑戦は一度に 1 つ・状態はその施設のロビーでしか片付かない）。違うのは連勝の置き場と BP の表だけ:
 *   - タワーの連勝 … CFRU の gBattleTowerStreaks
 *   - パレスの連勝 … 予約変数 HOENN_VAR_PALACE_STREAK（+ モード * 2 + レベル・最高は HOENN_VAR_PALACE_MAX_STREAK）
 *   パレスの戦いそのもの（性格で技を選ぶ）は src/hoenn_palace.c
 */
//★ バトルアリーナ（2026-09-29・②③）も同じ。連勝は予約変数 HOENN_VAR_ARENA_STREAK（+ レベル・最高は + 2・シングルだけ）
static bool8 InPalace(void)
{
	return VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_PALACE;
}

static bool8 InArena(void)
{
	return VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_ARENA;
}

//★ バトルチューブ（2026-09-29）も同じ。連勝は予約変数 HOENN_VAR_PIKE_STREAK（部屋の数・+ レベル・最高は + 2）
static bool8 InPike(void)
{
	return VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_PIKE;
}

//★ バトルファクトリー（2026-10-03）も同じ。連勝は予約変数 HOENN_VAR_FACTORY_STREAK（+ モード * 2 + レベル・最高は HOENN_VAR_FACTORY_MAX_STREAK）
static bool8 InFactory(void)
{
	return VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_FACTORY;
}

//★ バトルドーム（2026-10-03）も同じ。連勝は優勝の回数（エメラルドの domeWinStreaks・予約変数 HOENN_VAR_DOME_STREAK + モード * 2 + レベル）
static bool8 InDome(void)
{
	return VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_DOME;
}

//★ バトルピラミッド（2026-10-03）も同じ。連勝は抜けた階の数（エメラルドの pyramidWinStreaks・予約変数 HOENN_VAR_PYRAMID_STREAK + レベル）
static bool8 InPyramid(void)
{
	return VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_PYRAMID;
}

//連勝を予約変数に持つ施設（パレス・アリーナ・チューブ・ファクトリー・ドーム・ピラミッド）。タワーは CFRU の gBattleTowerStreaks
static bool8 HasOwnStreak(void)
{
	return InPalace() || InArena() || InPike() || InFactory() || InDome() || InPyramid();
}

static u16 PalaceStreakVar(bool8 max)
{
	if (InPyramid())
		return (max ? HOENN_VAR_PYRAMID_MAX_STREAK : HOENN_VAR_PYRAMID_STREAK) + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0);
	if (InDome())
		return (max ? HOENN_VAR_DOME_MAX_STREAK : HOENN_VAR_DOME_STREAK)
		     + (VarGet(HOENN_VAR_TOWER_MODE) == 1) * 2 + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0);
	if (InFactory())
		return (max ? HOENN_VAR_FACTORY_MAX_STREAK : HOENN_VAR_FACTORY_STREAK)
		     + (VarGet(HOENN_VAR_TOWER_MODE) == 1) * 2 + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0);
	if (InPike())
		return (max ? HOENN_VAR_PIKE_MAX_STREAK : HOENN_VAR_PIKE_STREAK) + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0);
	if (InArena())
		return (max ? HOENN_VAR_ARENA_MAX_STREAK : HOENN_VAR_ARENA_STREAK) + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0);
	return (max ? HOENN_VAR_PALACE_MAX_STREAK : HOENN_VAR_PALACE_STREAK)
	     + (VarGet(HOENN_VAR_TOWER_MODE) == 1) * 2 + (VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0);
}

static void SetUpFacility(u16 facility)
{
	VarSet(HOENN_VAR_FRONTIER_FACILITY, facility); //★ 今いる施設（パレスなら戦いの仕組みが効く・src/hoenn_palace.c）
	VarSet(TOWER_FACILITY_VAR, IN_BATTLE_TOWER);   //CFRU の施設の戦い（相手はエメラルドの 300 人）はどちらもタワーの仕組み
	VarSet(VAR_BATTLE_FACILITY_BATTLE_TYPE, (VarGet(HOENN_VAR_TOWER_MODE) == 1) ? BATTLE_FACILITY_DOUBLE : BATTLE_FACILITY_SINGLE);
	VarSet(VAR_BATTLE_FACILITY_POKE_NUM, TowerPartySize());
	VarSet(VAR_BATTLE_FACILITY_TIER, BATTLE_FACILITY_STANDARD);
	VarSet(VAR_BATTLE_FACILITY_POKE_LEVEL, (VarGet(HOENN_VAR_TOWER_LVL_MODE) == 0) ? 50 : MAX_LEVEL);
}

//施設の設定（ロビー・対戦室に入るたび／挑戦の頭で呼ぶ）。レベルは記録の側（50 / 100）
void HoennTower_SetUpFacility(void)
{
	SetUpFacility(HOENN_FACILITY_TOWER);
}

void HoennPalace_SetUpFacility(void)
{
	SetUpFacility(HOENN_FACILITY_PALACE);
}

//アリーナはシングルだけ（エメラルドの受付は 1 人・FRONTIER_MODE_SINGLES）
void HoennArena_SetUpFacility(void)
{
	VarSet(HOENN_VAR_TOWER_MODE, 0);
	SetUpFacility(HOENN_FACILITY_ARENA);
}

//チューブもシングルだけ（ダブルの部屋はそこだけダブル）
void HoennPike_SetUpFacility(void)
{
	VarSet(HOENN_VAR_TOWER_MODE, 0);
	SetUpFacility(HOENN_FACILITY_PIKE);
}

//ファクトリーはシングル・ダブル（モードは受付で HOENN_VAR_TOWER_MODE に入る）
void HoennFactory_SetUpFacility(void)
{
	SetUpFacility(HOENN_FACILITY_FACTORY);
}

//ドームもシングル・ダブル（参加は 3 匹・1 戦ごとに 2 匹を選ぶ・src/hoenn_dome.c）
void HoennDome_SetUpFacility(void)
{
	SetUpFacility(HOENN_FACILITY_DOME);
}

//ピラミッドはシングルだけ（参加は 3 匹・エメラルドの FRONTIER_MODE_SINGLES）
void HoennPyramid_SetUpFacility(void)
{
	VarSet(HOENN_VAR_TOWER_MODE, 0);
	SetUpFacility(HOENN_FACILITY_PYRAMID);
}

//今の連勝（施設ごとの置き場から）
static u16 CurrentStreak(void)
{
	if (HasOwnStreak())
		return VarGet(PalaceStreakVar(FALSE));
	HoennTower_SetRecordLevel();
	return GetCurrentBattleTowerStreak();
}

void HoennTower_SetRecordLevel(void)
{
	VarSet(VAR_BATTLE_FACILITY_POKE_LEVEL, (VarGet(HOENN_VAR_TOWER_LVL_MODE) == 0) ? 50 : MAX_LEVEL);
}

void HoennTower_SetBattleLevel(void)
{
	u16 level = VarGet(HOENN_VAR_TOWER_OPP_LEVEL);

	VarSet(VAR_BATTLE_FACILITY_POKE_LEVEL, (level == 0) ? 50 : level);
}

//参加できる子が足りるか（エメラルドの frontier_checkineligible）。→ Var8004 = TRUE なら足りない。
//★ レベルは見ない（2026-09-28 ユーザー「50を超える場合は50にさげてほしい　オープンはそのまま」。エメラルドは 50 をこえる子は出られない）
//同じ種類・同じ持ち物の子は 1 匹ずつしか数えない（エメラルドと同じく、頭の子をずらしながら一番多く数えられる並びを探す）
void HoennTower_CheckIneligible(void)
{
	u8 need = TowerPartySize();
	u8 best = 0;
	u32 start, k;

	for (start = 0; start < PARTY_SIZE && best < need; ++start)
	{
		u16 species[PARTY_SIZE], items[PARTY_SIZE];
		u8 count = 0;

		for (k = 0; k < PARTY_SIZE; ++k)
		{
			struct Pokemon* mon = &gPlayerParty[(start + k) % PARTY_SIZE];
			u16 sp = GetMonData(mon, MON_DATA_SPECIES, NULL);
			u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);
			u32 i;

			if (sp == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG, NULL))
				continue;
			if (IsMonBannedInTier(mon, BATTLE_FACILITY_STANDARD))
				continue;
			for (i = 0; i < count && species[i] != sp; ++i)
				;
			if (i != count)
				continue;
			if (item != ITEM_NONE)
			{
				for (i = 0; i < count && items[i] != item; ++i)
					;
				if (i != count)
					continue;
			}
			species[count] = sp;
			items[count] = item;
			++count;
		}
		if (count > best)
			best = count;
	}

	Var8004 = (best < need);
}

//選んだ子（gSelectedOrderFromParty・1〜6）を変数へ（3 ビットずつ・4 匹まで）。選んでいなければ Var8004 = FALSE
void HoennTower_PackSelection(void)
{
	u16 packed = 0;
	u32 i;

	for (i = 0; i < 4 && i < TowerPartySize(); ++i)
		packed |= (gSelectedOrderFromParty[i] & 7) << (3 * i);
	VarSet(HOENN_VAR_TOWER_SELECTED, packed);
	Var8004 = (gSelectedOrderFromParty[0] != 0);
}

//挑戦のチームを作る（CFRU の special 0x73 の代わり）。手持ちは SavePlayerParty で退避してから呼ぶこと。
//選んだ子を頭から並べ、ほかは空に。相手のレベルを決める。
//★ レベル（2026-09-28 ユーザー「50を超える場合は50にさげてほしい　オープンはそのまま」）:
//  レベル50 は 50 をこえる子だけ 50 に下げる（経験値を 50 の頭にして能力を計算し直す・CFRU の 0x73 と同じやり方）。
//  50 以下の子とオープンレベルは変えない。★ 挑戦のチームは写しで、終わったら退避した元の手持ちに戻る（元の子は下がらない）
void HoennTower_PrepareTeam(void)
{
	struct Pokemon* team = Calloc(sizeof(struct Pokemon) * PARTY_SIZE);
	u16 packed = VarGet(HOENN_VAR_TOWER_SELECTED);
	u8 size = TowerPartySize();
	u8 highest = 0;
	u32 i;

	if (team == NULL)
		return;
	for (i = 0; i < size; ++i)
	{
		u8 slot = (packed >> (3 * i)) & 7;
		u8 level;

		if (slot == 0 || slot > PARTY_SIZE)
			break;
		Memcpy(&team[i], &gPlayerParty[slot - 1], sizeof(struct Pokemon));
		level = GetMonData(&team[i], MON_DATA_LEVEL, NULL);
		if (VarGet(HOENN_VAR_TOWER_LVL_MODE) == 0 && level > TOWER_MAX_LEVEL_50)
		{
			u16 species = GetMonData(&team[i], MON_DATA_SPECIES, NULL);

			SetMonData(&team[i], MON_DATA_EXP, &gExperienceTables[gBaseStats[species].growthRate][TOWER_MAX_LEVEL_50]);
			CalculateMonStats(&team[i]);
			level = TOWER_MAX_LEVEL_50;
		}
		HealMon(&team[i]);
		if (level > highest)
			highest = level;
	}
	Memcpy(gPlayerParty, team, sizeof(struct Pokemon) * PARTY_SIZE);
	Free(team);
	CalculatePlayerPartyCount();

	//戦いの間の「選んだ順番」は並べ直したあとの番号にしておく（古い番号のままだと空の枠を指す）
	for (i = 0; i < PARTY_SIZE; ++i)
		gSelectedOrderFromParty[i] = (i < size) ? i + 1 : 0;

	if (VarGet(HOENN_VAR_TOWER_LVL_MODE) == 0)
		VarSet(HOENN_VAR_TOWER_OPP_LEVEL, 50);
	else
		VarSet(HOENN_VAR_TOWER_OPP_LEVEL, MathMin(MathMax(highest, TOWER_OPEN_MIN_LEVEL), MAX_LEVEL));
}

//次の相手を決める（エメラルドの tower_setopponent）。連勝の周回と何戦目かで範囲を選ぶ（src/hoenn_frontier.c）。
//CFRU の変数（VAR_FACILITY_TRAINER_ID）と、歩き姿（VAR_RUNTIME_CHANGEABLE + 0 → 絵 0xFF00 の NPC）を決め、名前を gStringVar1 へ
void HoennTower_SetOpponent(void)
{
	u16 streak, id;

	streak = CurrentStreak();
	id = HoennFrontier_ChooseNewTrainer(streak / 7, VarGet(HOENN_VAR_TOWER_BATTLE_NUM)); //★ 同じ挑戦で二度出さない（2026-09-29）
	VarSet(VAR_FACILITY_TRAINER_ID, id);
	VarSet(VAR_RUNTIME_CHANGEABLE, gSpecialTowerTrainers[id].owNum);
	StringCopy(gStringVar1, gSpecialTowerTrainers[id].name);
}

//7 人抜きの BP（エメラルドの GiveBattlePoints・sBattlePointAwards の列 [タワー / パレス / アリーナ / チューブ / ファクトリー / ドーム][シングル / ダブル]）。
//★ ドームは周回 = 連勝（優勝の回数）そのまま（エメラルドの GiveBattlePoints の FRONTIER_FACILITY_DOME）
//★ チューブは周回 = 連勝（部屋の数）/ 14（エメラルドの NUM_PIKE_ROOMS）
//周回 = 連勝 / 7（勝った 7 人を含む）で、**表は 周回 - 1 の行を引く**（エメラルドの challengeNum--・最初の 7 人抜きが 0 行目）。
//★ 2026-09-29 までのタワーは - 1 が抜けていて、1 行ずつ多く渡していた（最初の 7 人抜きでシングル 2 BP → 1 BP が正しい）。
//表は 30 行（それより先は最後の行）。BP を足して、もらった数を gStringVar1 へ
void HoennTower_GiveBattlePoints(void)
{
	static const u8 sPoints[7][2][30] =
	{
		{   //バトルタワー
			{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
			{2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
		},
		{   //バトルパレス
			{4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15},
			{5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
		},
		{   //バトルアリーナ（シングルだけ・ダブルの列は使わない）
			{1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
			{1, 1, 1, 2, 2, 2, 3, 3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
		},
		{   //バトルチューブ（シングルだけ・ダブルの列は使わない）
			{1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 8, 10, 10, 10, 10, 12, 12, 12, 12, 12, 14, 14, 14, 14, 15, 15, 15, 15, 15},
			{1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 8, 10, 10, 10, 10, 12, 12, 12, 12, 12, 14, 14, 14, 14, 15, 15, 15, 15, 15},
		},
		{   //バトルファクトリー（2026-10-03）
			{3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 15, 15},
			{4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15},
		},
		{   //バトルドーム（2026-10-03）
			{1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15},
			{1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15},
		},
		{   //バトルピラミッド（2026-10-03・シングルだけ・ダブルの列は使わない）
			{5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
			{5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15},
		},
	};
	u16 challengeNum, points;
	u32 total;

	challengeNum = CurrentStreak() / (InPike() ? 14 : (InDome() ? 1 : 7));
	if (challengeNum != 0)
		--challengeNum;
	challengeNum = MathMin(challengeNum, NELEMS(sPoints[0][0]) - 1);
	points = sPoints[MathMin(VarGet(HOENN_VAR_FRONTIER_FACILITY), NELEMS(sPoints) - 1)][VarGet(HOENN_VAR_TOWER_MODE) == 1][challengeNum];
	if (VarGet(HOENN_VAR_TOWER_STATUS) == HOENN_ST_WON_BRAIN) //エメラルドと同じく、最後がブレーンなら + 10
		points += 10;
	total = VarGet(HOENN_VAR_FRONTIER_BP) + points;
	VarSet(HOENN_VAR_FRONTIER_BP, MathMin(total, 9999));
	ConvertIntToDecimalStringN(gStringVar1, points, STR_CONV_MODE_LEFT_ALIGN, 2);
}

//今の連勝（記録のレベルで）→ LASTRESULT。ロビーで「つぎは 50 にんめ」を出すかに使う
void HoennTower_GetStreak(void)
{
	gSpecialVar_LastResult = CurrentStreak();
}

//連勝を 1 足す／なしにする（タワーは CFRU の special 0x55。記録のレベルにしてから）
//パレス・アリーナはエメラルドの IncrementPalaceStreak / IncrementArenaStreak（9999 まで・最高も上げる）
void HoennTower_IncrementStreak(void)
{
	if (HasOwnStreak())
	{
		u16 streak = VarGet(PalaceStreakVar(FALSE));

		if (streak < PALACE_MAX_STREAK)
			VarSet(PalaceStreakVar(FALSE), ++streak);
		if (streak > VarGet(PalaceStreakVar(TRUE)))
			VarSet(PalaceStreakVar(TRUE), streak);
		return;
	}
	HoennTower_SetRecordLevel();
	Var8000 = 0;
	sp055_UpdateBattleFacilityStreak();
}

void HoennTower_ResetStreak(void)
{
	if (HasOwnStreak())
	{
		VarSet(PalaceStreakVar(FALSE), 0);
		return;
	}
	HoennTower_SetRecordLevel();
	Var8000 = 1;
	sp055_UpdateBattleFacilityStreak();
}

//通路の係のひとこと（エメラルドの GetPalaceCommentId）→ LASTRESULT: 連勝 50 未満は 0〜2 のでたらめ・99 未満は 3・ほかは 4
void HoennPalace_GetCommentId(void)
{
	u16 streak = CurrentStreak();

	if (streak < 50)
		gSpecialVar_LastResult = Random() % 3;
	else if (streak < 99)
		gSpecialVar_LastResult = 3;
	else
		gSpecialVar_LastResult = 4;
}

//対戦部屋の「主人公の代わり」の人の絵（エメラルドは VAR_OBJ_GFX_ID_1 にユウキ／ハルカ）→ VAR_RUNTIME_CHANGEABLE + 1（絵 0xFF01）。
//エメラルドの対戦部屋は本物の主人公を消して、カメラを部屋の真ん中に置いたまま、代わりの人を歩かせる。
//こちらは今の主人公の歩き姿（BW の主人公の塗り替えも含む）をそのまま写す
void HoennPalace_SetPlayerGfx(void)
{
	VarSet(VAR_RUNTIME_CHANGEABLE + 1, GetEventObjectGraphicsId(&gEventObjects[gPlayerAvatar->eventObjectId]));
}

//アリーナの審判が呼ぶ相手の名前（エメラルドの arena_gettrainername）→ gStringVar1。
//戦いのあとは gStringVar1 が書き換わっているので、そのたびに VAR_FACILITY_TRAINER_ID から引き直す。
//一時変数 0x4003 = 1 ならブレーン（スクリプトが戦いの前に入れる・assembly/frontier/arena_scripts.s）
void HoennArena_BufferOpponentName(void)
{
	u16 id = VarGet(VAR_FACILITY_TRAINER_ID);

	if (VarGet(0x4003) != 0)
		StringCopy(gStringVar1, gFrontierBrains[id].name);
	else
		StringCopy(gStringVar1, gSpecialTowerTrainers[id % NUM_SPECIAL_TOWER_TRAINERS].name);
}

//確認なしでレポートを書く（エメラルドの「レポートを かいています」）。→ LASTRESULT = TRUE なら書けた
//★ 素の保存の手順（start_menu.c の StartMenu_PrepareForSave → SaveDialogCB_DoSave）から、画面の状態の保存と書き込みだけ
//★ 挑戦の途中（状態が ST_NONE 以外）のレポートは「続きから」を今いる地図の今の位置へのワープにする（2026-09-29 ユーザー
//  「やすむをしたあと再開すると試合再開ではなく自由に動き回れ」）。ふつうの続きからは地図に入り直さないので、一時変数
//  （入ったかの印 0x4000 など）がレポートのときの 1 のまま戻り、マップスクリプトの ON_FRAME が二度と動かなかった。
//  ワープなら一時変数・一時フラグが 0 に戻り、ON_TRANSITION / ON_WARP / ON_FRAME が入ったときと同じに動く
//  （エメラルドはレポートの前に VAR_TEMP_CHALLENGE_STATUS を 0 にして同じことをしている）。
//  素の SetContinueGameWarp（0x08054FB8）/ SetContinueGameWarpStatus（0x0804B964）/ 印を消す（0x0804B994）は素 ROM で中身を確かめた。
//  印はレポートに書いたらすぐ消す（このあとふつうにレポートを書いたとき、続きからがここへ飛ばないように）
#define SaveMapView ((void (*)(void)) (0x08058994 | 1))
void HoennTower_SaveGame(void)
{
	bool8 midChallenge = (VarGet(HOENN_VAR_TOWER_STATUS) != 0);

	if (midChallenge)
	{
		SetContinueGameWarp(gSaveBlock1->location.mapGroup, gSaveBlock1->location.mapNum, -1, gSaveBlock1->pos.x, gSaveBlock1->pos.y);
		SetContinueGameWarpStatus();
	}
	SaveMapView();
	gSpecialVar_LastResult = (TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK);
	if (midChallenge)
		ClearContinueGameWarpStatus2();
}

/* ---- エレベーター（エメラルドの MoveElevator・pokeemerald src/field_specials.c） ----------------
 * ★ 素 FRLG の AnimateElevator（special 0x111）は同じ位置（x 1〜3・y 0〜2）に**シルフカンパニーの窓**の
 *   メタタイルを書くので、エメラルドの地図の窓が崩れる。揺れと光をエメラルドどおりにこちらで行う。
 *   光のメタタイルはこちらの詰め直しの番号（生成物 gHoennTowerElevatorLights・tools/frontier/build_frontier_tower.py）
 * ★ 何階から何階へ（エメラルドの BufferBattleTowerElevatorFloors）: 連勝が 7・14・21・28・35・49・63・77・91 を
 *   こえるまで 5〜13 階。今は 4 階（ロビー）から
 */
#define SetCameraPanningCallback ((void (*)(void*)) (0x0805A690 | 1))
#define SetCameraPanning ((void (*)(s16, s16)) (0x0805A69C | 1))
#define InstallCameraPanAheadCallback ((void (*)(void)) (0x0805A6B4 | 1))
#define DrawWholeMapView ((void (*)(void)) (0x08059F40 | 1))
#define MAP_OFFSET 7
#define SE_ELEVATOR 82
#define SE_DING_DONG 66

extern const u16 gHoennTowerElevatorLights[2][3][3];  //[上り/下り][行][段階]

static void Task_HoennTowerElevatorLights(u8 taskId)
{
	s16* data = gTasks[taskId].data;

	if (data[1] == 6)
	{
		u32 x, y;

		data[0]++;
		for (y = 0; y < 3; ++y)
		{
			for (x = 0; x < 3; ++x)
				MapGridSetMetatileIdAt(x + MAP_OFFSET + 1, y + MAP_OFFSET, gHoennTowerElevatorLights[data[2]][y][data[0] % 3] | METATILE_COLLISION_MASK);
		}
		DrawWholeMapView();
		data[1] = 0;
		if (data[0] == data[3])
			DestroyTask(taskId);
	}
	data[1]++;
}

static void Task_HoennTowerElevator(u8 taskId)
{
	s16* data = gTasks[taskId].data;

	data[1]++;
	if (data[1] % 3 == 0)
	{
		data[1] = 0;
		data[2]++;
		data[4] = -data[4];
		SetCameraPanning(0, data[4]);
		if (data[2] == data[5])
		{
			PlaySE(SE_DING_DONG);
			DestroyTask(taskId);
			EnableBothScriptContexts();
			InstallCameraPanAheadCallback();
		}
	}
}

//揺れ（sTripLength）と窓の光を始める。delta は 0〜8 階
static void HoennElevator_Start(u8 delta, bool8 down)
{
	static const u8 sTripLength[9] = {8, 16, 24, 32, 38, 46, 52, 56, 57};
	static const u8 sLightCycles[9] = {3, 6, 9, 12, 15, 18, 21, 24, 27};
	u8 taskId;
	s16* data = gTasks[CreateTask(Task_HoennTowerElevator, 9)].data;

	data[1] = 0;
	data[2] = 0;
	data[4] = 1;
	data[5] = sTripLength[delta];
	SetCameraPanningCallback(NULL);

	if (!FuncIsActiveTask(Task_HoennTowerElevatorLights))
	{
		taskId = CreateTask(Task_HoennTowerElevatorLights, 8);
		gTasks[taskId].data[0] = 0;
		gTasks[taskId].data[1] = 0;
		gTasks[taskId].data[2] = down; //0 上り・1 下り
		gTasks[taskId].data[3] = sLightCycles[delta];
	}
	PlaySE(SE_ELEVATOR);
}

//スクリプトは callasm のあと waitstate で止まって待つ
void HoennTower_MoveElevator(void)
{
	static const u16 sThresholds[] = {7, 14, 21, 28, 35, 49, 63, 77, 91};
	u16 streak;
	u8 dest = 12, delta;
	u32 i;

	HoennTower_SetRecordLevel();
	streak = GetCurrentBattleTowerStreak();
	for (i = 0; i < NELEMS(sThresholds); ++i)
	{
		if (sThresholds[i] > streak)
		{
			dest = i + 5;
			break;
		}
	}
	delta = MathMin(dest - 4, 8);
	HoennElevator_Start(delta, FALSE);
}

/* ---- ミナモデパートのエレベーター（2026-10-08 ユーザー「ミナモデパートのエレベーターのボタンがない」）----
 * エメラルドは係の人に話しかけて階を選ぶ（ShowDeptStoreElevatorFloorSelect・MoveElevator は素 FRLG に無い＝係が隠れていた）。
 * スクリプトは assembly/hack_dept_elevator/dept_elevator.s。今の階の窓は素 FRLG の DrawElevatorCurrentFloorWindow
 * （0x8005: 4 = 1F 〜 8 = 5F）。揺れと窓の光はバトルタワーと同じ（窓の光のメタタイルの番号は同じ・build_frontier_tower.py が確かめる）。
 * 今の階: このエレベーターで動いたあとは一時変数 0x4001（地図に入り直すと 0）・それまでは乗った階（dynamicWarp の地図）
 */
#define DEPT_FLOOR_TEMP_VAR 0x4001
#define DEPT_GROUP 56          //エメラルドの gMapGroup_IndoorLilycove（13）+ 43
#define DEPT_MAP_1F 16         //LilycoveCity_DepartmentStore_1F（5F は 20）

//0x8005 = 今の階（4〜8）・LASTRESULT = 選択肢の初めの位置（5F が 0）
void HoennDept_ElevatorSetup(void)
{
	u16 floor = VarGet(DEPT_FLOOR_TEMP_VAR);

	if (floor < 4 || floor > 8)
	{
		u8 map = (u8) gSaveBlock1->dynamicWarp.mapNum;
		floor = 4;
		if ((u8) gSaveBlock1->dynamicWarp.mapGroup == DEPT_GROUP && map >= DEPT_MAP_1F && map <= DEPT_MAP_1F + 4)
			floor = 4 + map - DEPT_MAP_1F;
	}
	Var8005 = floor;
	gSpecialVar_LastResult = 8 - floor;
}

//0x8005 → 0x8006 へ動く。スクリプトは callasm のあと waitstate で待つ
void HoennDept_MoveElevator(void)
{
	u16 from = Var8005, to = Var8006;
	bool8 down = from > to;
	u8 delta = down ? from - to : to - from;

	VarSet(DEPT_FLOOR_TEMP_VAR, to);
	HoennElevator_Start(MathMin(delta, 8), down);
}

/* ---- フロンティアブレーン（2026-09-29 バトルパレス ③ ウコン） ----------------------------------------
 * エメラルドの GetFrontierBrainStatus / frontier_givesymbol / frontier_getsymbols（pokeemerald src/frontier_util.c）。
 * ブレーンの表 gFrontierBrains は tools/frontier/build_frontier_brains.py の生成物（並び = 施設 × 2 + 銀 0 / 金 1）・
 * 手持ちは src/hoenn_frontier.c HoennFrontier_MakeBrainSpread
 * シンボルは HOENN_VAR_FRONTIER_SYMBOLS に 2 ビットずつ（0 無し / 1 銀 / 2 金）・並びはエメラルドの施設の番号（フロンティアパスと同じ）
 */
#define BRAIN_NOT_READY   0
#define BRAIN_SILVER      1
#define BRAIN_GOLD        2
#define BRAIN_STREAK      3
#define BRAIN_STREAK_LONG 4

//こちらの施設（HOENN_VAR_FRONTIER_FACILITY）→ エメラルドの施設の番号・出番（sFrontierBrainStreakAppearances: 銀・金・金のあとの間隔・足す数）
static const u8 sEmFacility[] = {0, 2, 3, 5, 4, 1, 6};         //タワー 0・パレス 2・アリーナ 3・チューブ 5・ファクトリー 4・ドーム 1・ピラミッド 6
static const u8 sBrainAppearances[][4] = {{35, 70, 35, 1}, {21, 42, 21, 1}, {28, 56, 28, 1}, {28, 140, 56, 1}, {21, 42, 21, 1}, {4, 9, 5, 0}, {21, 70, 35, 0}};

static u8 Facility(void)
{
	return MathMin(VarGet(HOENN_VAR_FRONTIER_FACILITY), NELEMS(sEmFacility) - 1);
}

static u8 SymbolLevel(void)
{
	return (VarGet(HOENN_VAR_FRONTIER_SYMBOLS) >> (sEmFacility[Facility()] * 2)) & 3;
}

//ahead = いまの連勝の何戦（チューブは何部屋）先を見るか（エメラルドのチューブの GetPikeQueenFightType(nextRoom)）
static u8 BrainStatusAhead(u8 ahead)
{
	const u8* a = sBrainAppearances[Facility()];
	u16 winStreak;

	if (VarGet(HOENN_VAR_TOWER_MODE) != 0) //シングルだけ
		return BRAIN_NOT_READY;
	winStreak = CurrentStreak() + ahead + a[3];
	switch (SymbolLevel()) {
		case 0:
		case 1:
			if (winStreak == a[SymbolLevel()])
				return SymbolLevel() + 1; //BRAIN_SILVER / BRAIN_GOLD
			return BRAIN_NOT_READY;
		default:
			if (winStreak == a[0])
				return BRAIN_STREAK;
			if (winStreak == a[1] || (winStreak > a[1] && (winStreak - a[1]) % a[2] == 0))
				return BRAIN_STREAK_LONG;
			return BRAIN_NOT_READY;
	}
}

static u8 BrainStatus(void)
{
	return BrainStatusAhead(0);
}

//★ 2026-10-03 チューブの三つ又の部屋（src/hoenn_pike.c）: 次の部屋がアザミの部屋か（1 部屋先を見る）
u8 HoennFrontier_BrainStatusAhead(u8 ahead)
{
	return BrainStatusAhead(ahead);
}

//次の相手がブレーンか → LASTRESULT（0 ちがう / 1 銀 / 2 金 / 3 銀の強さ（もう両方持っている）/ 4 金の強さ）
void HoennFrontier_GetBrainStatus(void)
{
	gSpecialVar_LastResult = BrainStatus();
}

//ブレーンを相手にする（エメラルドの SetFacilityBrainObjectEvent + TRAINER_FRONTIER_BRAIN）。歩き姿は絵 0xFF00 の人へ、名前は gStringVar1
void HoennFrontier_SetBrainOpponent(void)
{
	u8 status = BrainStatus();
	u16 id = Facility() * 2 + (status == BRAIN_GOLD || status == BRAIN_STREAK_LONG);

	VarSet(VAR_FACILITY_TRAINER_ID, id);
	VarSet(VAR_RUNTIME_CHANGEABLE, gFrontierBrains[id].owNum);
	StringCopy(gStringVar1, gFrontierBrains[id].name);
}

//その施設のシンボル（0 無し / 1 銀 / 2 金）→ LASTRESULT
void HoennFrontier_GetSymbols(void)
{
	gSpecialVar_LastResult = SymbolLevel();
}

//シンボルを 1 段上げる（銀 → 金。エメラルドの frontier_givesymbol）
void HoennFrontier_GiveSymbol(void)
{
	u8 shift = sEmFacility[Facility()] * 2;
	u16 v = VarGet(HOENN_VAR_FRONTIER_SYMBOLS);
	u8 level = MathMin(((v >> shift) & 3) + 1, 2);

	VarSet(HOENN_VAR_FRONTIER_SYMBOLS, (v & ~(3 << shift)) | (level << shift));
}

//前口上を聞いたか → LASTRESULT・聞いた印を立てる（エメラルドの FRONTIER_DATA_HEARD_BRAIN_SPEECH・予約フラグ）
void HoennFrontier_GetHeardBrainSpeech(void)
{
	gSpecialVar_LastResult = FlagGet(HOENN_FLAG_FRONTIER_HEARD_BRAIN + sEmFacility[Facility()]);
}

void HoennFrontier_SetHeardBrainSpeech(void)
{
	FlagSet(HOENN_FLAG_FRONTIER_HEARD_BRAIN + sEmFacility[Facility()]);
}

//ブレーンの表の番号（施設 × 2 + 金なら 1・HoennFrontier_SetBrainOpponent と同じ）。バトルドームの対戦表がヒースの 3 匹を引くのに使う
u16 HoennFrontier_BrainTableId(void)
{
	u8 status = BrainStatus();

	return Facility() * 2 + (status == BRAIN_GOLD || status == BRAIN_STREAK_LONG);
}
