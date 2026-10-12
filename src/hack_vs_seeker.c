#include "defines.h"
#include "../include/event_data.h"

/*
	バトルサーチャー（VS シーカー）をホウエン・シンオウでも（2026-10-12 ユーザー「ホウエン地方でダウジングとバトルサーチャーが使えないみたい
	他地方でも使えるように」・資料 docs/research/2026-10-12_itemfinder-vsseeker-regions.md）。

	素の vs_seeker.c は再戦の表 sRematches（素の ROM 0x08414034・221 行）を番号で引き、表の長さは 5 つの関数に即値（cmp #220）で入っている。
	  - 表: tools/rom/build_vs_seeker.py が素の 221 行＋ホウエン・シンオウの道路の人を足した gHackRematches（src/Tables/hack_rematches.h）。
	    表の番地を持つ素の語 7 つは repoints で gHackRematches へ（呼び元が渡す表の番地も、GetRematchTrainerId が直に引く番地も）。
	  - 表を回す 5 関数を functionrewrites で置き換え、長さを HACK_REMATCH_COUNT に（中身は pokefirered の vs_seeker.c と同じ）:
	      ShouldTryRematchBattleInternal 0810D858・LookupVsSeekerOpponentInArray 0810D8E8・IsTrainerReadyForRematchInternal 0810D9A8・
	      GetRematchIdx 0810DB5C・GetNextAvailableRematchTrainer 0810DC3C（素の ROM の cmp #220 と呼び元で確かめた）
	再戦の印（gSaveBlock1 の trainerRematches[100]）は今いる地図の人の localId で引く＝トレーナーの数では増えない。
	倒したかは素の HasTrainerBeenFought（0807FB44・CFRU の HoennHasTrainerBeenFought が 768 番以降の旗も読む）。
*/

#define MAX_REMATCH_PARTIES 6
#define SKIP 0xFFFF

struct HackRematchData
{
	u16 trainerIdxs[MAX_REMATCH_PARTIES];
	u16 mapGroup;   //使われない
	u16 mapNum;     //使われない
};

#include "Tables/hack_rematches.h"

#define TRAINER_REMATCHES ((u8*) ((u8*) gSaveBlock1 + 0x63A))     //gSaveBlock1->trainerRematches[100]（include/global.h の 0x063A）
#define HasTrainerBeenFought ((bool8 (*)(u16)) (0x0807FB44 | 1))

static bool8 IsThisTrainerRematchable(u32 localId)
{
	return TRAINER_REMATCHES[localId] != 0;
}

//functionrewrites: GetRematchIdx 0810DB5C
int HackVs_GetRematchIdx(const struct HackRematchData* vsSeekerData, u16 trainerFlagIdx)
{
	int i;

	for (i = 0; i < HACK_REMATCH_COUNT; i++)
	{
		if (vsSeekerData[i].trainerIdxs[0] == trainerFlagIdx)
			return i;
	}
	return -1;
}

//functionrewrites: ShouldTryRematchBattleInternal 0810D858
bool8 HackVs_ShouldTryRematchBattleInternal(const struct HackRematchData* vsSeekerData, u16 trainerBattleOpponent)
{
	s32 rematchIdx = HackVs_GetRematchIdx(vsSeekerData, trainerBattleOpponent);

	if (rematchIdx == -1)
		return FALSE;
	if (rematchIdx >= 0 && rematchIdx < HACK_REMATCH_COUNT)
	{
		if (IsThisTrainerRematchable(gSpecialVar_LastTalked))
			return TRUE;
	}
	return FALSE;
}

//functionrewrites: LookupVsSeekerOpponentInArray 0810D8E8
int HackVs_LookupOpponentInArray(const struct HackRematchData* array, u16 trainerId)
{
	int i, j;

	for (i = 0; i < HACK_REMATCH_COUNT; i++)
	{
		for (j = 0; j < MAX_REMATCH_PARTIES; j++)
		{
			u16 testTrainerId;
			if (array[i].trainerIdxs[j] == 0)
				break;
			testTrainerId = array[i].trainerIdxs[j];
			if (testTrainerId == SKIP)
				continue;
			if (testTrainerId == trainerId)
				return i;
		}
	}
	return -1;
}

//functionrewrites: IsTrainerReadyForRematchInternal 0810D9A8
bool8 HackVs_IsTrainerReadyForRematchInternal(const struct HackRematchData* array, u16 trainerId)
{
	int rematchTrainerIdx = HackVs_LookupOpponentInArray(array, trainerId);

	if (rematchTrainerIdx == -1)
		return FALSE;
	if (rematchTrainerIdx >= HACK_REMATCH_COUNT)
		return FALSE;
	if (!IsThisTrainerRematchable(gSpecialVar_LastTalked))
		return FALSE;
	return TRUE;
}

//functionrewrites: GetNextAvailableRematchTrainer 0810DC3C
u8 HackVs_GetNextAvailableRematchTrainer(const struct HackRematchData* vsSeekerData, u16 trainerFlagNo, u8* idxPtr)
{
	int i, j;

	for (i = 0; i < HACK_REMATCH_COUNT; i++)
	{
		if (vsSeekerData[i].trainerIdxs[0] == trainerFlagNo)
		{
			*idxPtr = i;
			for (j = 1; j < MAX_REMATCH_PARTIES; j++)
			{
				if (vsSeekerData[i].trainerIdxs[j] == 0)
					return j - 1;
				if (vsSeekerData[i].trainerIdxs[j] == SKIP)
					continue;
				if (HasTrainerBeenFought(vsSeekerData[i].trainerIdxs[j]))
					continue;
				return j;
			}
			return j - 1;
		}
	}
	*idxPtr = 0;
	return 0;
}

//functionrewrites: GetRematchTrainerId 0810D93C（素は表の何行目かを u8 で受け取る＝256 行目から先を指せない → 行の番号を int で持つ）
#define TryGetRematchTrainerIdGivenGameState ((void (*)(const u16*, u8*)) (0x0810D778 | 1))
int HackVs_GetRematchTrainerId(u16 trainerId)
{
	int i;
	u8 j = 0;

	for (i = 0; i < HACK_REMATCH_COUNT; i++)
	{
		if (gHackRematches[i].trainerIdxs[0] == trainerId)
		{
			u8 dummy;
			j = HackVs_GetNextAvailableRematchTrainer(gHackRematches, trainerId, &dummy);
			break;
		}
	}
	if (i >= HACK_REMATCH_COUNT || j == 0)
		return 0;
	TryGetRematchTrainerIdGivenGameState(gHackRematches[i].trainerIdxs, &j);
	return gHackRematches[i].trainerIdxs[j];
}
