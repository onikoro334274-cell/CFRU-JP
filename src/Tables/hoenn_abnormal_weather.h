// tools/rom/build_hoenn_scripts.py が生成。手で編集しないこと。
//
// 異常気象（うみ／りくのどうくつ）の場所。地図の番号はこちらの群（EM + 43）。

#define HOENN_ABN_TERRA_START 1
#define HOENN_ABN_TERRA_COUNT 8
#define HOENN_ABN_MARINE_START 9
#define HOENN_ABN_MARINE_COUNT 8
#define HOENN_ABN_COUNT 16

// 場所 1〜16 の道路（添字は 場所 - 1）
static const u8 sHoennAbnRoutes[16][2] = {
	{43, 29},	// Route114
	{43, 29},	// Route114
	{43, 30},	// Route115
	{43, 30},	// Route115
	{43, 31},	// Route116
	{43, 31},	// Route116
	{43, 33},	// Route118
	{43, 33},	// Route118
	{43, 20},	// Route105
	{43, 20},	// Route105
	{43, 40},	// Route125
	{43, 40},	// Route125
	{43, 42},	// Route127
	{43, 42},	// Route127
	{43, 44},	// Route129
	{43, 44},	// Route129
};

// 中にいる間は期限が来ても終わらせない地図（洞窟・海の底）
static const u8 sHoennAbnCaveMaps[9][2] = {
	{67, 101},	// Underwater_MarineCave
	{67, 102},	// MarineCave_Entrance
	{67, 103},	// MarineCave_End
	{67, 104},	// TerraCave_Entrance
	{67, 105},	// TerraCave_End
	{43, 52},	// Underwater_Route127
	{43, 54},	// Underwater_Route129
	{43, 55},	// Underwater_Route105
	{43, 56},	// Underwater_Route125
};
