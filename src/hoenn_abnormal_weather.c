#include "defines.h"
#include "../include/random.h"

#include "Tables/hoenn_script_flags.h"
#include "Tables/hoenn_abnormal_weather.h"

/*
hoenn_abnormal_weather.c
    うみのどうくつ（カイオーガ）／りくのどうくつ（グラードン）が出る「異常気象」（#2・2026-09-23）

    ユーザー「うみとりくの洞窟移植して」。

    ## エメラルドの仕組み（pokeemerald）

    ・殿堂入り後、気象研究所 2F の研究員に話すと `CreateAbnormalWeatherEvent`（field_specials.c）が
      16 か所（陸 8＝114/115/116/118 番道路・海 8＝105/125/127/129 番水道）から 1 か所を選び
      `VAR_ABNORMAL_WEATHER_LOCATION` に入れる（カイオーガを倒したあとは陸だけ、グラードンなら海だけ）
    ・その道路のマップスクリプトが、入ったとき天気を変え（ひざしがつよい／おおあめ）、
      洞窟の入口（荒れた深い海）のマスを置く（data/scripts/abnormal_weather.inc）
    ・1 歩ごとに `AbnormalWeatherHasExpired`（field_control_avatar.c）が数え、1000 歩で終わる
      （洞窟・海の底にいる間は「終わらせる合図」だけ立て、道路に戻ったときの `ON_FRAME` で片付ける）

    ## こちら

    ・**研究員は置かず、自動で起こす**（ユーザー判断 2026-09-23）。条件は**ホウエン殿堂入り後**
      （`FLAG_IS_CHAMPION`）。場所はタウンマップに出す（別の段）
    ・道路のスクリプトは `tools/rom/build_hoenn_scripts.py` が異常気象の行だけ写している
    ・洞窟の奥（カイオーガ・グラードン戦）は地図ごと移植（CAVE_MAPS）
    ・ここは **毎フレーム**（`src/hoenn_steps.c` の `HoennStepTick`）から呼ばれ、
      `moved` はその回に主人公のマスが変わったか（＝1 歩）

    ★ 状態はすべて変数（セーブされる）に置く。書き換え可能な static は使えない（CLAUDE.md 6.13）
*/

#define ABN_LOC   HOENN_VAR_ABNORMAL_WEATHER_LOCATION
#define ABN_END   HOENN_VAR_SHOULD_END_ABNORMAL_WEATHER
#define ABN_STEPS HOENN_VAR_ABNORMAL_WEATHER_STEPS
#define ABN_EXPIRE_STEPS 999        /* エメラルドと同じ（++steps > 999 で終わる） */

static bool8 HoennAbnIsHere(const u8 *map)
{
	return gSaveBlock1->location.mapGroup == map[0]
	    && gSaveBlock1->location.mapNum == map[1];
}

/* 洞窟・海の底（期限が来ても中にいる間は終わらせない） */
static bool8 HoennAbnInCave(void)
{
	u32 i;

	for (i = 0; i < NELEMS(sHoennAbnCaveMaps); ++i)
	{
		if (HoennAbnIsHere(sHoennAbnCaveMaps[i]))
			return TRUE;
	}
	return FALSE;
}

/* 16 か所のどれかの道路にいる（そこで新しく起こすと、入り直すまで絵も天気も出ない） */
static bool8 HoennAbnOnAnyRoute(void)
{
	u32 i;

	for (i = 0; i < NELEMS(sHoennAbnRoutes); ++i)
	{
		if (HoennAbnIsHere(sHoennAbnRoutes[i]))
			return TRUE;
	}
	return FALSE;
}

static bool8 HoennAbnOnEventRoute(u16 loc)
{
	return loc >= 1 && loc <= HOENN_ABN_COUNT && HoennAbnIsHere(sHoennAbnRoutes[loc - 1]);
}

/* pokeemerald の CreateAbnormalWeatherEvent と同じ選び方 */
static void HoennAbnCreate(void)
{
	u16 r = Random();
	u16 loc;

	if (FlagGet(HOENN_FLAG_DEFEATED_KYOGRE))
		loc = (r % HOENN_ABN_TERRA_COUNT) + HOENN_ABN_TERRA_START;
	else if (FlagGet(HOENN_FLAG_DEFEATED_GROUDON))
		loc = (r % HOENN_ABN_MARINE_COUNT) + HOENN_ABN_MARINE_START;
	else if ((r & 1) == 0)
		loc = (Random() % HOENN_ABN_TERRA_COUNT) + HOENN_ABN_TERRA_START;
	else
		loc = (Random() % HOENN_ABN_MARINE_COUNT) + HOENN_ABN_MARINE_START;

	VarSet(ABN_STEPS, 0);
	VarSet(ABN_LOC, loc);
}

void HoennAbnormalWeatherTick(bool8 moved)
{
	u16 loc, steps;

	if (!FlagGet(HOENN_FLAG_IS_CHAMPION))
		return;

	loc = VarGet(ABN_LOC);
	if (loc > HOENN_ABN_COUNT)
	{
		VarSet(ABN_LOC, 0);
		VarSet(ABN_END, 0);
		VarSet(ABN_STEPS, 0);
		return;
	}

	/* 終わらせる合図（倒した／捕まえた・洞窟の中で期限が来た）。
	   その道路にいれば道路の ON_FRAME（AbnormalWeather_EventScript_EndEventAndCleanup_1）が
	   文を出して片付けるので待つ。洞窟の中でも待つ。それ以外の所なら黙って片付ける */
	if (VarGet(ABN_END) == 1)
	{
		if (HoennAbnOnEventRoute(loc) || HoennAbnInCave())
			return;
		VarSet(ABN_END, 0);
		VarSet(ABN_LOC, 0);
		return;
	}

	if (loc == 0)
	{
		/* 両方とも倒した（捕まえた）ら、もう起こさない（エメラルドの研究員と同じ） */
		if (FlagGet(HOENN_FLAG_DEFEATED_KYOGRE) && FlagGet(HOENN_FLAG_DEFEATED_GROUDON))
			return;
		/* 道路や洞窟の中では起こさない（その場で起きると入り直すまで何も出ない） */
		if (HoennAbnOnAnyRoute() || HoennAbnInCave())
			return;
		HoennAbnCreate();
		return;
	}

	if (!moved)
		return;

	steps = VarGet(ABN_STEPS);
	if (steps >= ABN_EXPIRE_STEPS)
	{
		VarSet(ABN_STEPS, 0);
		/* その道路にいる → ON_FRAME が文を出して片付ける／洞窟の中 → 出てきたときに片付ける */
		if (HoennAbnOnEventRoute(loc) || HoennAbnInCave())
			VarSet(ABN_END, 1);
		else
			VarSet(ABN_LOC, 0);
	}
	else
	{
		VarSet(ABN_STEPS, steps + 1);
	}
}
