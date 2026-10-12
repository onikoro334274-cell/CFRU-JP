#include "defines.h"
#include "defines_battle.h"
#include "../include/random.h"
#include "../include/new/build_pokemon.h"

/*
    トレーナーのクラスごとの手持ちの候補（2026-10-11 ユーザー「list_trainer_pokemon.md これ前のきんこんしきみたいにやってほしい」）。
    資料: docs/research/2026-10-11_class-pools.md

    選んだこと: もらった表のクラスのトレーナー・手持ちの数とレベルは元のまま・Smogon の型・戦うたびに選び直す。
    CFRU の CreateNPCTrainerParty（src/build_pokemon.c・patches/0305）が 1 匹ずつ作ったあと、難易度の調整（HackDifficulty_AdjustTrainerMon）の前に
    これを呼ぶ。そのトレーナーのクラスに候補があれば、その子を候補から選んだ種族で作り直す（レベル・親の名前と ID・性格値・個体値・ボール・なつき度は
    元の子のまま）。型（技・持ち物・性格・特性・努力値・テラ）は tools/rom/build_class_pools.py の表。型の無い種族はレベルで覚える技・持ち物なし。
    同じ手持ちの中では同じ種族を選ばない（候補が足りなければ重なってよい）。
    やさしい・ほどほどの難易度は、このあとの HackDifficulty_AdjustTrainerMon が努力値・持ち物を外す（今までどおり）。
*/

struct HackClassPoolSet
{
	u16 species;
	u16 item;
	u16 moves[MAX_MON_MOVES];
	u8 nature;
	u8 ability;          //0 / 1 / 0xFF（隠れ）
	u8 evs[NUM_STATS];   //HP 攻撃 防御 素早さ 特攻 特防
	u8 tera;             //0 = 元のタイプ・それ以外はタイプ + 1
	bool8 noSet;         //型なし（レベルで覚える技・持ち物なし）
};

struct HackClassPool
{
	const u16* sets;      //1 匹ずつの候補（型の表の番号）
	u16 count;
	const u16 (*pairs)[2]; //2 匹の組（ふたごちゃん・カップル・きんこんしき）
	u16 pairCount;
};

//ジムの地図ごとの候補（2026-10-11 #97・tools/rom/gym_pools.md）
struct HackGymPool
{
	u8 mapGroup;
	u8 mapNum;
	const u16* sets;
	u16 count;
};

#define HACK_CLASS_POOL_CLASSES 107   //ROM のクラス名の表の数（tools/data_editor/trainerlib.py の CLASS_NAME_COUNT）
#include "Tables/hack_class_pools.h"

//候補を使わないトレーナー（自分の作りがある人）
static const u16 sExcludedTrainers[] =
{
	935, //モモカ博士（tools/kanto/build_kanto_trainers.py の momoka_target）
	936, //きんこんしき ゲンゾウ（src/hack_golden_wedding.c）
	937, //きんこんしき ハナ
	938, //ヒガン（作者 ふたーら 氏・tools/oritore/higan）
	939, //ヒガン（再戦）
	940, 941, 942, 943, 944, 945, 946, 947, 948, 949, //隠しトレーナー わたしたち（#99・src/hack_watashitachi.c・レントラーの♀は並びで決めている）
};

static bool8 IsExcluded(u16 trainerId)
{
	u32 i;

	for (i = 0; i < ARRAY_COUNT(sExcludedTrainers); ++i)
	{
		if (sExcludedTrainers[i] == trainerId)
			return TRUE;
	}
	return FALSE;
}

static bool8 SpeciesUsedBefore(struct Pokemon* party, u8 index, u16 species)
{
	u32 i;

	for (i = 0; i < index; ++i)
	{
		if (GetMonData(&party[i], MON_DATA_SPECIES, NULL) == species)
			return TRUE;
	}
	return FALSE;
}

//今いる地図がジムで、リーダーでなければ、その町のジムの候補
static const struct HackGymPool* GymPoolFor(u8 trainerClass)
{
	u32 i;
	u8 group = gSaveBlock1->location.mapGroup;
	u8 num = (u8) gSaveBlock1->location.mapNum;   //6.60: mapNum は s8

	for (i = 0; i < ARRAY_COUNT(sHackGymLeaderClasses); ++i)
	{
		if (sHackGymLeaderClasses[i] == trainerClass)
			return NULL;
	}
	for (i = 0; i < HACK_GYM_POOL_MAPS; ++i)
	{
		if (gHackGymPools[i].mapGroup == group && gHackGymPools[i].mapNum == num)
			return &gHackGymPools[i];
	}
	return NULL;
}

void HackClassPool_ReplaceMon(struct Pokemon* mon, struct Pokemon* party, u8 index, u16 trainerId, u8 side)
{
	const struct HackClassPool* pool;
	const struct HackGymPool* gym;
	const struct HackClassPoolSet* set;
	u8 trainerClass, level, otGender, ball, friendship, fixedIv, otName[PLAYER_NAME_LENGTH + 1];
	u32 otId, personality, tries, i;
	u16 move;

	if (side != B_SIDE_OPPONENT || IsExcluded(trainerId))
		return;
	trainerClass = gTrainers[trainerId].trainerClass;
	set = NULL;
	gym = GymPoolFor(trainerClass);
	if (gym != NULL)
	{
		//ジムトレーナー: クラスの候補より先にジムの候補（組のクラスでも 1 匹ずつ）
		set = &gHackClassPoolSets[gym->sets[Random() % gym->count]];
		for (tries = 0; tries < 20 && SpeciesUsedBefore(party, index, set->species); ++tries)
			set = &gHackClassPoolSets[gym->sets[Random() % gym->count]];
		goto REPLACE;
	}
	if (trainerClass >= HACK_CLASS_POOL_CLASSES)
		return;
	pool = &gHackClassPools[trainerClass];
	if (pool->pairCount != 0)
	{
		/* 組のクラス: 偶数番目（1・3・5 匹目）は組を選んでその 1 匹目、奇数番目は 1 つ前の子が 1 匹目の組の 2 匹目
		 * （状態を持てない＝6.13 ので、前の子の種族から組を探す・同じ 1 匹目の組が複数あれば乱数で） */
		if (index & 1)
		{
			u16 prev = GetMonData(&party[index - 1], MON_DATA_SPECIES, NULL);
			u32 n = 0, k;

			for (i = 0; i < pool->pairCount; ++i)
				if (gHackClassPoolSets[pool->pairs[i][0]].species == prev)
					++n;
			if (n != 0)
			{
				k = Random() % n;
				for (i = 0; i < pool->pairCount; ++i)
				{
					if (gHackClassPoolSets[pool->pairs[i][0]].species == prev && k-- == 0)
					{
						set = &gHackClassPoolSets[pool->pairs[i][1]];
						break;
					}
				}
			}
		}
		if (set == NULL)
		{
			set = &gHackClassPoolSets[pool->pairs[Random() % pool->pairCount][0]];
			for (tries = 0; tries < 20 && SpeciesUsedBefore(party, index, set->species); ++tries)
				set = &gHackClassPoolSets[pool->pairs[Random() % pool->pairCount][0]];
		}
	}
	else
	{
		if (pool->count == 0 || pool->sets == NULL)
			return;
		//同じ手持ちで重ならない種族を選ぶ（20 回で見つからなければ重なってよい）
		set = &gHackClassPoolSets[pool->sets[Random() % pool->count]];
		for (tries = 0; tries < 20 && SpeciesUsedBefore(party, index, set->species); ++tries)
			set = &gHackClassPoolSets[pool->sets[Random() % pool->count]];
	}

REPLACE:
	level = GetMonData(mon, MON_DATA_LEVEL, NULL);
	otId = GetMonData(mon, MON_DATA_OT_ID, NULL);
	personality = GetMonData(mon, MON_DATA_PERSONALITY, NULL);
	otGender = GetMonData(mon, MON_DATA_OT_GENDER, NULL);
	ball = GetMonData(mon, MON_DATA_POKEBALL, NULL);
	friendship = GetMonData(mon, MON_DATA_FRIENDSHIP, NULL);
	fixedIv = GetMonData(mon, MON_DATA_HP_IV, NULL);   //トレーナーの個体値は 6 つ同じ値
	GetMonData(mon, MON_DATA_OT_NAME, otName);

	CreateMon(mon, set->species, level, fixedIv, TRUE, personality, OT_ID_PRESET, otId);
	SetMonData(mon, MON_DATA_OT_NAME, otName);
	SetMonData(mon, MON_DATA_OT_GENDER, &otGender);
	SetMonData(mon, MON_DATA_POKEBALL, &ball);
	SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship);

	if (set->noSet)
		return;   //型なし: CreateMon がレベルで覚える技を入れている・持ち物なし

	for (i = 0; i < NUM_STATS; ++i)
	{
		u8 ev = set->evs[i];
		SetMonData(mon, MON_DATA_HP_EV + i, &ev);
	}
	GiveMonNatureAndAbility(mon, set->nature, set->ability, FALSE, TRUE, FALSE);
	for (i = 0; i < MAX_MON_MOVES; ++i)
	{
		u8 pp;

		move = set->moves[i];
		pp = (move != MOVE_NONE) ? gBattleMoves[move].pp : 0;
		SetMonData(mon, MON_DATA_MOVE1 + i, &move);
		SetMonData(mon, MON_DATA_PP1 + i, &pp);
	}
	SetMonData(mon, MON_DATA_HELD_ITEM, &set->item);
	if (set->tera != 0)
		mon->teratype = set->tera - 1;
}
