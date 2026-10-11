#include "defines.h"
#include "defines_battle.h"
#include "../include/constants/hold_effects.h"
#include "../include/new/battle_util.h"

/*
    にのや（二の矢・もたせる道具・ITEM_SECOND_ARROW 0x34F・2026-10-10 ユーザー「二の矢 技が失敗した時に A C S 1段階上昇！」
    →「1 回で消える」「外れたときと うまくきまらなかったとき」「タマムシ 6F の 2 人目の店員」）。資料: docs/research/2026-10-10_second-arrow.md

    - 持ち主が技を出したあと（cmd49.c の ATK49_SHELL_BELL_LIFE_ORB_RECOIL → ItemEffects_ContactAttacker・からぶりほけん と同じ所）に、
      攻撃技なら 1 度もダメージを与えられなかった（外れた・まもる・こうかがない など）、変化技なら効かなかった（うまく きまらなかった）とき、
      こうげき・とくこう・すばやさ を 1 段階ずつ上げて、道具は なくなる（gConsumableItemEffects＝戦いのあとも戻らない）。
    - 技を出せなかったとき（まひ・ねむり・なまける など＝HITMARKER_UNABLE_TO_USE_MOVE）は呼び元が除く。3 つとも上がりきっていれば使わない。
*/

static const u8 sStats[] = {STAT_STAGE_ATK, STAT_STAGE_SPATK, STAT_STAGE_SPEED};

bool8 HackSecondArrow_TryActivate(u8 bank)
{
	u32 i;
	bool8 failed, any = FALSE;

	if (!BATTLER_ALIVE(bank))
		return FALSE;
	if (SPLIT(gCurrentMove) == SPLIT_STATUS)
		failed = !MOVE_HAD_EFFECT;
	else
		failed = !gNewBS->AttackerDidDamageAtLeastOnce;
	if (!failed)
		return FALSE;

	for (i = 0; i < ARRAY_COUNT(sStats); ++i)
	{
		if (gBattleMons[bank].statStages[sStats[i] - 1] < STAT_STAGE_MAX)
		{
			gBattleMons[bank].statStages[sStats[i] - 1]++;
			any = TRUE;
		}
	}
	if (!any)
		return FALSE;

	gBattleScripting.bank = bank;
	gStringBank = bank;
	return TRUE;
}
