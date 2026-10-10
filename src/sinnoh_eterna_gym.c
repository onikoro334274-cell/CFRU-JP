#include "defines.h"
#include "../include/fieldmap.h"
#include "../include/field_camera.h"
#include "../include/event_data.h"

#include "Tables/sinnoh_eterna_clock.h"
#include "Tables/sinnoh_flags.h"

/*
 * ハクタイジムの花時計（やりたいこと #2・2026-10-03 ユーザー「本家どおり回る」）。
 *
 * 本家（pokeplatinum src/overlay008/gym_features.c）は 5 つの状態（はじめ・トレーナー 1〜3 人目のあと・ナタネのあと）で
 * 時計の針と噴水の「通れるマス」を切り替える。ここでは状態ごとの部品（メタタイル＋通れない）を
 * tools/sinnoh/garden_repaint.py が書いた表（Tables/sinnoh_eterna_clock.h）から置く。
 *
 * 状態はセーブに持たない: 勝ったトレーナーの数（本家の順にしか会えない形の部屋）・バッジがあれば 4。
 *
 *   SinnohEternaGym_ApplyClock    … 地図に入ったとき（ON_LOAD・callasm）。描くのは地図の読み込みが済んでから
 *   SinnohEternaGym_AdvanceClock  … トレーナー・ナタネに勝ったあとのスクリプトから（callasm）。置いて描き直す
 *
 * ★ 戦いから戻るときは地図を読み直さない（素の ResumeMap は ON_RESUME だけ・pokefirered src/overworld.c）ので、
 *   戦いの前に置いた部品はそのまま残る。勝ったあとは AdvanceClock で新しい状態にする
 */

#define ETERNA_MAP_OFFSET 7

bool8 HoennHasTrainerBeenFought(u16 trainerId);

static u8 EternaClockState(void)
{
	u32 i;
	u8 n = 0;

	if (FlagGet(SINNOH_FLAG_BADGE2))
		return ETERNA_CLOCK_STATES - 1;
	for (i = 0; i < ARRAY_COUNT(sEternaClockTrainers); ++i)
	{
		if (HoennHasTrainerBeenFought(sEternaClockTrainers[i]))
			++n;
	}
	return n;
}

static void PlaceClock(bool8 draw)
{
	u32 i;
	u8 state = EternaClockState();

	for (i = 0; i < ETERNA_CLOCK_CELLS; ++i)
	{
		int x = sEternaClockCellXY[i][0] + ETERNA_MAP_OFFSET;
		int y = sEternaClockCellXY[i][1] + ETERNA_MAP_OFFSET;

		MapGridSetMetatileIdAt(x, y, sEternaClockTiles[state][i]);
		if (draw)
			CurrentMapDrawMetatileAt(x, y);
	}
}

void SinnohEternaGym_ApplyClock(void)
{
	PlaceClock(FALSE);
}

void SinnohEternaGym_AdvanceClock(void)
{
	PlaceClock(TRUE);
}

/* 噴水が開いたか（2 人目・3 人目のあと → 本家は「ふんすいの すいいが さがった！」）→ LASTRESULT */
void SinnohEternaGym_FountainOpened(void)
{
	u8 state = EternaClockState();

	gSpecialVar_LastResult = (state == 2 || state == 3);
}
