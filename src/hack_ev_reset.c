#include "defines.h"
#include "../include/item.h"
#include "../include/item_use.h"
#include "../include/party_menu.h"
#include "../include/pokemon.h"
#include "../include/sound.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/constants/songs.h"
#include "../include/new/Vanilla_functions.h"

/*
 * どりょくリセット（ITEM_EV_RESET 0x340・2026-10-03）: ポケモンの努力値を 6 つとも 0 にする消費アイテム。
 * ユーザー「努力値リセットアイテムを導入してほしい　消費アイテムでタマムシデパートで販売」。
 * 使い方は CFRU の EV を下げるきのみ（src/party_menu.c の ItemUseCB_EVReducingBerry）と同じ流れ（手持ちを選ぶ → 文 → 1 つ減る）。
 * 努力値がもう全部 0 なら「つかっても こうかが ないよ」で減らさない。
 * 売り場はタマムシデパート 5F の薬の店員（assembly/kanto/kanto_mart.s の KantoCeladonDept5F_Vitamins）。
 */

u8 __attribute__((long_call)) DisplayPartyMenuMessage(const u8* str, u8 b);
void __attribute__((long_call)) UpdateMonDisplayInfoAfterRareCandy(u8 slot, struct Pokemon *mon);
#define gText_WontHaveEffect ((const u8*) 0x83DDE0B)
extern const u8 gText_EVReset_Done[];

static void ItemUseCB_EVReset(u8 taskId, TaskFunc func)
{
	struct Pokemon* mon = &gPlayerParty[gPartyMenu.slotId];
	u16 item = Var800E;
	u8 i, zero = 0, total = 0;
	u16 oldHP, maxHP;

	PlaySE(SE_SELECT);
	for (i = 0; i < NUM_STATS; ++i)
		total |= GetMonData(mon, MON_DATA_HP_EV + i, NULL);

	if (total == 0)
	{
		gPartyMenuUseExitCallback = FALSE;
		DisplayPartyMenuMessage(gText_WontHaveEffect, TRUE);
		ScheduleBgCopyTilemapToVram(2);
		gTasks[taskId].func = func;
		return;
	}

	oldHP = GetMonData(mon, MON_DATA_HP, NULL);
	for (i = 0; i < NUM_STATS; ++i)
		SetMonData(mon, MON_DATA_HP_EV + i, &zero);
	CalculateMonStats(mon);

	maxHP = GetMonData(mon, MON_DATA_MAX_HP, NULL);
	if (oldHP == 0)
		SetMonData(mon, MON_DATA_HP, &oldHP); //ひんしのまま
	else if (GetMonData(mon, MON_DATA_HP, NULL) > maxHP || GetMonData(mon, MON_DATA_HP, NULL) == 0)
	{
		oldHP = (oldHP > maxHP) ? maxHP : oldHP;
		SetMonData(mon, MON_DATA_HP, &oldHP); //ひんしにしない・最大をこえない
	}
	UpdateMonDisplayInfoAfterRareCandy(gPartyMenu.slotId, mon);

	GetMonNickname(mon, gStringVar1);
	StringExpandPlaceholders(gStringVar4, gText_EVReset_Done);
	RemoveBagItem(item, 1);
	DisplayPartyMenuMessage(gStringVar4, TRUE);
	ScheduleBgCopyTilemapToVram(2);
	gTasks[taskId].func = func;
}

void FieldUseFunc_EVReset(u8 taskId)
{
	gItemUseCB = ItemUseCB_EVReset;
	SetUpItemUseCallback(taskId);
}
