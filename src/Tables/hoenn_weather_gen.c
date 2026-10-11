// このファイルは tools/rom/build_hoenn_weather.py が生成しました。手で編集しないこと。
//
// 天気の表を 16 件に広げ、15 番に灰を足す（素 FRLG の 7 番は CFRU が白いすなあらしに
// 差し替えているため）。0〜14 は素 ROM の sWeatherFuncs（08389944）の写し。

#include "../defines.h"

struct HoennWeatherCallbacks
{
	void (*initVars)(void);
	void (*main)(void);
	void (*initAll)(void);
	bool8 (*finish)(void);
};

#define W(a, b, c, d) { (void (*)(void))(a), (void (*)(void))(b), (void (*)(void))(c), (bool8 (*)(void))(d) }

const struct HoennWeatherCallbacks gHoennWeatherFuncs[16] = {
	W(0x08079621, 0x08079641, 0x08079621, 0x08079645),	/* 0 */
	W(0x0807AA3D, 0x0807AAB9, 0x0807AA89, 0x0807AB19),	/* 1 */
	W(0x0807AB61, 0x0807AB91, 0x0807AB85, 0x0807AB95),	/* 2 */
	W(0x0807AF29, 0x0807AFC5, 0x0807AF95, 0x0807B025),	/* 3 */
	W(0x0807B58D, 0x0807B649, 0x0807B5DD, 0x0807B685),	/* 4 */
	W(0x0807BA45, 0x0807BB85, 0x0807BAB9, 0x0807BF11),	/* 5 */
	W(0x0807C061, 0x0807C0F1, 0x0807C0C1, 0x0807C1B1),	/* 6 */
#ifdef REPLACE_ASH_WEATHER_WITH_WHITE_SANDSTORM
	W(0x0807CA5D, 0x0807CB0D, 0x0807CADD, 0x0807CB89),	/* 7 白いすなあらし（CFRU の雪・bytereplacement 083899B4 と同じ） */
#else
	W(0x0807C3BD, 0x0807C449, 0x0807C419, 0x0807C4F5),	/* 7 灰 */
#endif
	W(0x0807CA5D, 0x0807CB0D, 0x0807CADD, 0x0807CB89),	/* 8 */
	W(0x0807C6E5, 0x0807C799, 0x0807C769, 0x0807C7FD),	/* 9 */
	W(0x0807C061, 0x0807C0F1, 0x0807C0C1, 0x0807C1B1),	/* 10 */
	W(0x0807CFF1, 0x0807D02D, 0x0807D021, 0x0807D031),	/* 11 */
	W(0x0807ACD5, 0x0807AD39, 0x0807AD09, 0x0807AE1D),	/* 12 */
	W(0x0807BAE9, 0x0807BB85, 0x0807BB55, 0x0807BF11),	/* 13 */
	W(0x0807D035, 0x0807D0B9, 0x0807D089, 0x0807D12D),	/* 14 */
	W(0x0807C3BD, 0x0807C449, 0x0807C419, 0x0807C4F5),	/* 15 ★ 灰（素の 7 番） */
};

/* 天気を切り替えるマス（エメラルドの map.json の coord_events・type = weather）
   { 群, 番号, x, y, 天気 }。群はエメラルド + 43、天気は FRLG の番号 */
const u8 gHoennWeatherCoords[][5] = {
	{ 43, 28, 19, 11, 15 },	/* Route113 */
	{ 43, 28, 19, 10, 15 },	/* Route113 */
	{ 43, 28, 19, 12, 15 },	/* Route113 */
	{ 43, 28, 19, 13, 15 },	/* Route113 */
	{ 43, 28, 86, 9, 15 },	/* Route113 */
	{ 43, 28, 85, 10, 15 },	/* Route113 */
	{ 43, 28, 85, 11, 15 },	/* Route113 */
	{ 43, 28, 14, 10, 2 },	/* Route113 */
	{ 43, 28, 14, 11, 2 },	/* Route113 */
	{ 43, 28, 14, 12, 2 },	/* Route113 */
	{ 43, 28, 14, 13, 2 },	/* Route113 */
	{ 43, 28, 94, 8, 2 },	/* Route113 */
	{ 43, 28, 94, 9, 2 },	/* Route113 */
	{ 43, 28, 94, 10, 2 },	/* Route113 */
	{ 43, 28, 94, 11, 2 },	/* Route113 */
	{ 43, 28, 19, 14, 15 },	/* Route113 */
	{ 43, 28, 87, 8, 15 },	/* Route113 */
	{ 43, 28, 87, 6, 15 },	/* Route113 */
	{ 43, 28, 87, 7, 15 },	/* Route113 */
	{ 67, 13, 13, 15, 2 },	/* JaggedPass */
	{ 67, 13, 21, 12, 15 },	/* JaggedPass */
	{ 67, 13, 14, 15, 2 },	/* JaggedPass */
	{ 67, 13, 18, 17, 2 },	/* JaggedPass */
	{ 67, 13, 22, 19, 2 },	/* JaggedPass */
};
const u16 gHoennWeatherCoordCount = 24;
