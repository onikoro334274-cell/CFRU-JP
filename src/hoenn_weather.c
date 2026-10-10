#include "defines.h"

/*
 * 天気 15 番（灰）と、ホウエンの「天気を切り替えるマス」
 * （#2・2026-09-24 ユーザー「天気定義できない？」→「進めて」）
 *
 * CFRU の REPLACE_ASH_WEATHER_WITH_WHITE_SANDSTORM が素の天気の表の 7 番（灰）を白いすなあらし
 * （CFRU の雪）で上書きしているので、灰を新しい番号 15 として足した:
 *   - 天気の表: src/Tables/hoenn_weather_gen.c の gHoennWeatherFuncs[16]（repoints で 4 か所を向ける）
 *   - 地図の天気番号を検める関数 TranslateWeatherNum（0807A92C）の飛び先の表の 15 番の行 →
 *     assembly/hoenn/hoenn_weather_hook.s の HoennWeatherTranslateAsh（repoints 0807A980）
 *
 * ここは「天気を切り替えるマス」を見る。素 FRLG の切り替えの処理（coord_event_weather.c）は
 * 全部空の関数なので、エメラルドの map.json から拾ったマス（gHoennWeatherCoords）に
 * 主人公が乗ったら、素の SetWeather を呼ぶ（エメラルドの DoCoordEventWeather と同じ効き目）。
 * 詳細は docs/research/2026-09-24_weather-ash.md
 */

#define HOENN_MAP_OFFSET 7

/* 素 FRLG（BPRJ）の番地（tools/rom/verify_hoenn_weather.py が先頭の命令を見る）*/
#define BPRJ_SET_WEATHER        0x0807A8D4  /* SetWeather: SetSav1Weather → SetNextWeather(GetSav1Weather()) */
#define BPRJ_GET_SAV1_WEATHER   0x0807A890  /* GetSav1Weather（BPRJ.ld に無い）*/
#define HoennSetWeather     ((void (*)(u32))(BPRJ_SET_WEATHER + 1))
#define HoennGetSav1Weather ((u8 (*)(void))(BPRJ_GET_SAV1_WEATHER + 1))

extern const u8 gHoennWeatherCoords[][5];
extern const u16 gHoennWeatherCoordCount;

void HoennWeatherCoordTick(bool8 moved, s16 px, s16 py);

/* 1 歩ごと（src/hoenn_steps.c の HoennStepTick から。px, py は PlayerGetDestCoords の値）*/
void HoennWeatherCoordTick(bool8 moved, s16 px, s16 py)
{
	u16 i;
	const u8 *c;

	if (!moved)
		return;

	for (i = 0; i < gHoennWeatherCoordCount; i++)
	{
		c = gHoennWeatherCoords[i];
		if (gSaveBlock1->location.mapGroup == c[0]
		 && gSaveBlock1->location.mapNum == c[1]
		 && px == c[2] + HOENN_MAP_OFFSET
		 && py == c[3] + HOENN_MAP_OFFSET)
		{
			if (HoennGetSav1Weather() != c[4])
				HoennSetWeather(c[4]);
			return;
		}
	}
}
