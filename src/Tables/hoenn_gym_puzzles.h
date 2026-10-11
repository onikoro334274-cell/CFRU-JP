// tools/rom/build_hoenn_gym_puzzles.py が生成。手で編集しないこと。
//
// ホウエンのジムの仕掛けのメタタイル番号（**詰め直し済み**）。
// 元は pokeemerald の field_specials.c / field_tasks.c。

// ★ ジムの引き戸だけの挙動値。素の 0x69 と違い**歩いては入れない**
//   （`TryDoorWarp` は 0x69 しか見ない）。アニメと「着いたら一歩出る」
//   のほう（`MetatileBehavior_IsWarpDoor_2` = 08059498）だけ広げる。
#define HOENN_MB_GYM_DOOR   0xEB
#define HOENN_MB_WARP_DOOR  0x69

#define HOENN_PUZZLE_IMPASSABLE 0x0C00
#define HOENN_MAP_OFFSET 7

// トウカジムの引き戸（素 PetalburgGymSetDoorMetatiles）
//   ★ 縦 2 マス。上下で**別のメタタイル**（詰め直し後は
//     「+8」の関係が成り立たないので両方持つ）
#define HOENN_PETALBURG_ROOMS 8
#define HOENN_MT_PETALBURG_DOOR_UP  982
#define HOENN_MT_PETALBURG_DOOR_LOW 986
static const u8 sHoennPetalburgDoorY[HOENN_PETALBURG_ROOMS] =
{
	104,
	78,
	91,
	39,
	52,
	65,
	13,
	26,
};

// x は 1 部屋あたり最大 2 つ。0xFF は「無い」
static const u8 sHoennPetalburgDoorX[HOENN_PETALBURG_ROOMS][2] =
{
	{1, 7},
	{1, 7},
	{1, 7},
	{7, 0xFF},
	{1, 7},
	{1, 0xFF},
	{7, 0xFF},
	{1, 0xFF},
};

// キンセツジム（ニューキンセツ）のバリア
#define HOENN_MAUVILLE_SWITCHES 4
static const u8 sHoennMauvilleSwitchXY[][2] =
{
	{0, 15},
	{4, 12},
	{3, 9},
	{8, 9},
};

#define HOENN_MT_MAUVILLE_PRESSED 646
#define HOENN_MT_MAUVILLE_RAISED  645
#define HOENN_MAUVILLE_X0 0
#define HOENN_MAUVILLE_Y0 5
#define HOENN_MAUVILLE_X1 9
#define HOENN_MAUVILLE_Y1 17

// 床タイルだけは「1 つ上が何か」で行き先が変わる
#define HOENN_MT_MAUVILLE_FLOOR       980
#define HOENN_MT_MAUVILLE_FLOOR_ABOVE 701
#define HOENN_MT_MAUVILLE_FLOOR_YES   991
#define HOENN_MT_MAUVILLE_FLOOR_NO    992

// スイッチを踏むたびの切り替え（24 件）
static const u16 sHoennMauvilleToggle[][2] =
{
	{661, 682},	// GreenBeamH1_On -> GreenBeamH1_Off
	{668, 686},	// GreenBeamH2_On -> GreenBeamH2_Off
	{676, 693},	// GreenBeamH3_On -> GreenBeamH3_Off
	{677, 694},	// GreenBeamH4_On -> GreenBeamH4_Off
	{682, 661},	// GreenBeamH1_Off -> GreenBeamH1_On
	{686, 668},	// GreenBeamH2_Off -> GreenBeamH2_On
	{693, 676 | HOENN_PUZZLE_IMPASSABLE},	// GreenBeamH3_Off -> GreenBeamH3_On
	{694, 677 | HOENN_PUZZLE_IMPASSABLE},	// GreenBeamH4_Off -> GreenBeamH4_On
	{669, 688},	// RedBeamH1_On -> RedBeamH1_Off
	{671, 689},	// RedBeamH2_On -> RedBeamH2_Off
	{678, 695},	// RedBeamH3_On -> RedBeamH3_Off
	{988, 696},	// RedBeamH4_On -> RedBeamH4_Off
	{688, 669},	// RedBeamH1_Off -> RedBeamH1_On
	{689, 671},	// RedBeamH2_Off -> RedBeamH2_On
	{695, 678 | HOENN_PUZZLE_IMPASSABLE},	// RedBeamH3_Off -> RedBeamH3_On
	{696, 988 | HOENN_PUZZLE_IMPASSABLE},	// RedBeamH4_Off -> RedBeamH4_On
	{701, 704 | HOENN_PUZZLE_IMPASSABLE},	// GreenBeamV1_On -> PoleBottom_On
	{991, 980},	// GreenBeamV2_On -> FloorTile
	{702, 705 | HOENN_PUZZLE_IMPASSABLE},	// RedBeamV1_On -> PoleBottom_Off
	{992, 980},	// RedBeamV2_On -> FloorTile
	{704, 701 | HOENN_PUZZLE_IMPASSABLE},	// PoleBottom_On -> GreenBeamV1_On
	{705, 702 | HOENN_PUZZLE_IMPASSABLE},	// PoleBottom_Off -> RedBeamV1_On
	{714, 713 | HOENN_PUZZLE_IMPASSABLE},	// PoleTop_Off -> PoleTop_On
	{713, 714},	// PoleTop_On -> PoleTop_Off
};

// リーダーを倒したあとの全消し（12 件）
static const u16 sHoennMauvilleClear[][2] =
{
	{661, 682},	// GreenBeamH1_On -> GreenBeamH1_Off
	{668, 686},	// GreenBeamH2_On -> GreenBeamH2_Off
	{676, 693},	// GreenBeamH3_On -> GreenBeamH3_Off
	{677, 694},	// GreenBeamH4_On -> GreenBeamH4_Off
	{669, 688},	// RedBeamH1_On -> RedBeamH1_Off
	{671, 689},	// RedBeamH2_On -> RedBeamH2_Off
	{678, 695},	// RedBeamH3_On -> RedBeamH3_Off
	{988, 696},	// RedBeamH4_On -> RedBeamH4_Off
	{701, 704 | HOENN_PUZZLE_IMPASSABLE},	// GreenBeamV1_On -> PoleBottom_On
	{702, 705 | HOENN_PUZZLE_IMPASSABLE},	// RedBeamV1_On -> PoleBottom_Off
	{992, 980},	// RedBeamV2_On -> FloorTile
	{713, 714},	// PoleTop_On -> PoleTop_Off
};

// トクサネジムの回る床。**番号の引き算では出せない**ので表にする。
//   エメラルドは (番号 - 黄の右) / 8 = 色、% 8 = 向き で出しているが、
//   こちらはメタタイルを詰め直しているので成り立たない（6.19 節）。
//   { 詰め直したメタタイル, 色, 向き(0 右 / 1 下 / 2 左 / 3 上) }
#define HOENN_MOSSDEEP_ARROWS 20
static const u16 sHoennMossdeepArrows[][3] =
{
	{678, 0, 0},
	{679, 0, 1},
	{680, 0, 2},
	{681, 0, 3},
	{686, 1, 0},
	{688, 1, 1},
	{689, 1, 2},
	{690, 1, 3},
	{692, 2, 0},
	{693, 2, 1},
	{694, 2, 2},
	{995, 2, 3},
	{695, 3, 0},
	{696, 3, 1},
	{698, 3, 2},
	{997, 3, 3},
	{702, 4, 0},
	{704, 4, 1},
	{705, 4, 2},
	{706, 4, 3},
};
#define HOENN_MOSSDEEP_GYM_GROUP 57
#define HOENN_MOSSDEEP_GYM_NUM 0

#define HOENN_VAR_ICE_STEP_COUNT 0x5078

// ルネジムの氷（1 歩ごとに 薄氷 -> ひび -> 割れる）
#define HOENN_MT_SOOTOPOLIS_CRACKED 978
#define HOENN_MT_SOOTOPOLIS_BROKEN  646
#define HOENN_SOOTOPOLIS_GYM_GROUP 58
#define HOENN_SOOTOPOLIS_GYM_NUM 0

// そらのはしら 2F・4F のひびの床 -> 穴（マッハ自転車の速さでないと落ちる）
#define HOENN_MT_SKY_PILLAR_CRACKED 708
#define HOENN_MT_SKY_PILLAR_HOLE 709
#define HOENN_SKY_PILLAR_GROUP 67
#define HOENN_SKY_PILLAR_2F_NUM 80
#define HOENN_SKY_PILLAR_4F_NUM 82

