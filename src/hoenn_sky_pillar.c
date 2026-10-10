#include "defines.h"
#include "../include/fieldmap.h"
#include "../include/field_camera.h"
#include "../include/event_data.h"

#include "Tables/hoenn_gym_puzzles.h"

/*
hoenn_sky_pillar.c
    そらのはしら 2F・4F のひびの床（2026-10-04・ユーザー「そらのはしらの壊れる床お願い」）。

    エメラルドの CrackedFloorPerStepCallback（pokeemerald src/field_tasks.c）を写したもの。
    - ひびの床（HOENN_MT_SKY_PILLAR_CRACKED）に乗ると、3 コマあとにその床が穴（HOENN_MT_SKY_PILLAR_HOLE）になる。
      2 枚まで同時に数える（自転車で続けて渡るとき）。
    - いちばん速い速さ（GetPlayerSpeed() == 4・マッハ自転車／B を押した自転車）でないと VAR_ICE_STEP_COUNT を 0 にする
      → マップスクリプト（CaveHole_CheckFallDownHole・ON_FRAME）が下の階へ落とす（setholewarp の行き先）。
    - 穴に立っていても 0 にする（エメラルドと同じ）。

    ★ 素 FRLG にも同じ名前の 1 歩ごとの処理が残っているが、メタタイルの番号が素のもので、ホウエンの詰め直した番号と合わない。
      setstepcallback は変換で落としてあり、ここで地図を見て動かす（src/hoenn_steps.c の毎フレームの処理から呼ぶ）。
    ★ ひびの床の挙動値は CFRU で 0xD3 がでこぼこ坂と重なるのでただの床にしてある（build_emerald_tileset.py）→ メタタイル番号で見る。
    ★ 状態はタスクの data[7]〜data[11]・data[14]。このタスクは素の 1 歩ごとの処理と同じもので、いてだきのどうくつの氷が data[1]〜data[6]、
      src/hoenn_steps.c が data[12]・[13]・[15] を使う。書き換え可能な static は使えない（6.13）。
*/

extern s16 GetPlayerSpeed(void);   /* src/overworld.c（CFRU） */

#define tFloor1Delay data[7]
#define tFloor1X     data[8]
#define tFloor1Y     data[9]
#define tFloor2Delay data[10]
#define tFloor2X     data[11]
#define tFloor2Y     data[14]

static void SetHole(s16 x, s16 y)
{
	if (MapGridGetMetatileIdAt(x, y) == HOENN_MT_SKY_PILLAR_CRACKED)
	{
		MapGridSetMetatileIdAt(x, y, HOENN_MT_SKY_PILLAR_HOLE);
		CurrentMapDrawMetatileAt(x, y);
	}
}

void HoennSkyPillarCrackedFloorTick(s16 *data, s16 x, s16 y, bool8 moved)
{
	u16 metatile;

	if (gSaveBlock1->location.mapGroup != HOENN_SKY_PILLAR_GROUP
	 || (gSaveBlock1->location.mapNum != HOENN_SKY_PILLAR_2F_NUM && gSaveBlock1->location.mapNum != HOENN_SKY_PILLAR_4F_NUM))
	{
		tFloor1Delay = 0;
		tFloor2Delay = 0;
		return;
	}

	if (tFloor1Delay != 0 && (--tFloor1Delay) == 0)
		SetHole(tFloor1X, tFloor1Y);
	if (tFloor2Delay != 0 && (--tFloor2Delay) == 0)
		SetHole(tFloor2X, tFloor2Y);

	metatile = MapGridGetMetatileIdAt(x, y);
	if (metatile == HOENN_MT_SKY_PILLAR_HOLE)
		VarSet(HOENN_VAR_ICE_STEP_COUNT, 0);

	if (!moved)
		return;

	if (metatile == HOENN_MT_SKY_PILLAR_CRACKED)
	{
		if (GetPlayerSpeed() != 4)
			VarSet(HOENN_VAR_ICE_STEP_COUNT, 0);

		if (tFloor1Delay == 0)
		{
			tFloor1Delay = 3;
			tFloor1X = x;
			tFloor1Y = y;
		}
		else if (tFloor2Delay == 0)
		{
			tFloor2Delay = 3;
			tFloor2X = x;
			tFloor2Y = y;
		}
	}
}
