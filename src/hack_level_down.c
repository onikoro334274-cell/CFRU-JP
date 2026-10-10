#include "defines.h"
#include "../include/item.h"
#include "../include/item_use.h"
#include "../include/party_menu.h"
#include "../include/pokemon.h"
#include "../include/sound.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/constants/songs.h"
#include "../include/new/rom_locs.h"
#include "../include/new/Vanilla_functions.h"

/*
 * レベルダウン（ITEM_LEVEL_DOWN 0x344・2026-10-06）: ポケモンのレベルを 1 下げる消費アイテム（ふしぎなアメの逆）。
 * ユーザー「レベルダウンアイテム作れる？」→ 1 つで 1 下がる・使うとなくなる・タマムシデパート 5F の薬の店員。
 * 流れは どりょくリセット（src/hack_ev_reset.c）と同じ（手持ちを選ぶ → 文 → 1 つ減る）。
 * 経験値はそのレベルの最初の値にする（成長の型の表）。覚えた技・進化はそのまま。レベル 1 なら「つかっても こうかが ないよ」で減らさない。
 */

u8 __attribute__((long_call)) DisplayPartyMenuMessage(const u8* str, u8 b);
void __attribute__((long_call)) UpdateMonDisplayInfoAfterRareCandy(u8 slot, struct Pokemon *mon);
#define gText_WontHaveEffect ((const u8*) 0x83DDE0B)
extern const u8 gText_LevelDown_Done[];

static void ItemUseCB_LevelDown(u8 taskId, TaskFunc func)
{
	struct Pokemon* mon = &gPlayerParty[gPartyMenu.slotId];
	u16 item = Var800E;
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	u8 level = GetMonData(mon, MON_DATA_LEVEL, NULL);
	u32 exp;
	u16 oldHP, maxHP;

	PlaySE(SE_SELECT);
	if (level <= 1 || GetMonData(mon, MON_DATA_IS_EGG, NULL)
	|| !RemoveBagItem(item, 1))
	{
		gPartyMenuUseExitCallback = FALSE;
		DisplayPartyMenuMessage(gText_WontHaveEffect, TRUE);
		ScheduleBgCopyTilemapToVram(2);
		gTasks[taskId].func = func;
		return;
	}

	--level;
	exp = gExperienceTables[gBaseStats[species].growthRate][level];
	oldHP = GetMonData(mon, MON_DATA_HP, NULL);
	SetMonData(mon, MON_DATA_EXP, &exp);
	SetMonData(mon, MON_DATA_LEVEL, &level);
	CalculateMonStats(mon);

	maxHP = GetMonData(mon, MON_DATA_MAX_HP, NULL);
	if (oldHP == 0)
		SetMonData(mon, MON_DATA_HP, &oldHP); //ひんしのまま
	else
	{
		oldHP = (oldHP > maxHP) ? maxHP : oldHP;
		SetMonData(mon, MON_DATA_HP, &oldHP); //ひんしにしない・最大をこえない
	}
	UpdateMonDisplayInfoAfterRareCandy(gPartyMenu.slotId, mon);

	GetMonNickname(mon, gStringVar1);
	ConvertIntToDecimalStringN(gStringVar2, level, STR_CONV_MODE_LEFT_ALIGN, 3);
	StringExpandPlaceholders(gStringVar4, gText_LevelDown_Done);
	DisplayPartyMenuMessage(gStringVar4, TRUE);
	ScheduleBgCopyTilemapToVram(2);
	gTasks[taskId].func = func;
}

void FieldUseFunc_LevelDown(u8 taskId)
{
	gItemUseCB = ItemUseCB_LevelDown;
	SetUpItemUseCallback(taskId);
}
