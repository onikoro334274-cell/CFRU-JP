#include "defines.h"
#include "../include/palette.h"
#include "../include/task.h"
#include "hoenn_tileset_anim.h"

/*
 * バトルドームの床の明かり（エメラルドのタイルセット 083B80E4 のアニメ・2026-10-03・#13）。
 * エメラルドの TilesetAnim_BattleDome（pokeemerald/src/tileset_anims.c）はタイルではなく**パレット**を入れ替える:
 *   4 コマごとに BG のパレット 8（こちらでは +1 して 9）を dome_anim1〜4 の色に替え、画面の暗転（gPaletteFade）に合わせて混ぜる。
 *   戦闘に入る演出（Task_BattleTransition_Intro）が動いている間は混ぜない（演出の暗転を上書きしない）。
 *   エメラルドは演出が始まると 32 回だけ混ぜずに続けて止まるが、こちらは演出の間だけ混ぜない（戻ったら地図ごと作り直される）。
 * タイルセットの callback（tools/rom/build_hoenn.py の PALETTE_ANIMS）がここの HoennDomeAnim_Init を指す。
 * 色は pokeemerald graphics/battle_frontier/dome_anim1〜4.pal（日本語版エメラルドの ROM に同じバイト列: 0835EEB0・08D85644/64/84）。
 */

#define DOME_FLOOR_PAL 9                              /* EM のパレット 8 → FR 9（セカンダリは +1・palRemap なし） */
#define Task_BattleTransition_Intro ((TaskFunc) (0x080D4D84 | 1))   /* 素 FRLG（CreateTask(…, 3) と IsIntroTaskDone で確認） */

static const u16 sDomeFloorLightPals[4][16] =
{
	{0x0659, 0x354A, 0x41CE, 0x5273, 0x62F7, 0x7FFF, 0x320E, 0x294B, 0x6379, 0x4E0A, 0x628E, 0x6F34, 0x29B0, 0x11FF, 0x3633, 0x437F},
	{0x0659, 0x354A, 0x41CE, 0x5273, 0x62F7, 0x7FFF, 0x320E, 0x294B, 0x6379, 0x4E0A, 0x628E, 0x6F34, 0x29B0, 0x227F, 0x3633, 0x32FF},
	{0x0659, 0x354A, 0x41CE, 0x5273, 0x62F7, 0x7FFF, 0x320E, 0x294B, 0x6379, 0x4E0A, 0x628E, 0x6F34, 0x29B0, 0x32FF, 0x3633, 0x227F},
	{0x0659, 0x354A, 0x41CE, 0x5273, 0x62F7, 0x7FFF, 0x320E, 0x294B, 0x6379, 0x4E0A, 0x628E, 0x6F34, 0x29B0, 0x437F, 0x3633, 0x11FF},
};

static void HoennDomeAnim_Frame(u16 timer)
{
	if (timer % 4 != 0)
		return;

	CpuCopy16(sDomeFloorLightPals[(timer / 4) % 4], &gPlttBufferUnfaded[DOME_FLOOR_PAL * 16], 32);
	if (FindTaskIdByFunc(Task_BattleTransition_Intro) == 0xFF)
		BlendPalette(DOME_FLOOR_PAL * 16, 16, gPaletteFade->y, gPaletteFade->blendColor & 0x7FFF);
}

void HoennDomeAnim_Init(void)
{
	sHoennSecondaryCounter = 0;
	sHoennSecondaryPeriod = 16;   /* 4 コマ × 4 フレーム（EM はプライマリの周回を借りるが、こちらのプライマリにアニメが無いと 0 になり止まる） */
	sHoennSecondaryCallback = HoennDomeAnim_Frame;
}
