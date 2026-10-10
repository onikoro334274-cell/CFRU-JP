#include "defines.h"
#include "../include/item.h"
#include "../include/item_use.h"
#include "../include/party_menu.h"
#include "../include/pokemon.h"
#include "../include/sound.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/constants/songs.h"

/*
 * あぶないこなざとう（ITEM_DANGER_SUGAR 0x349・2026-10-09）: 手持ちに使うと なつき度が最大（255）になる消費アイテム。
 * ユーザー「あぶないこなざとうを追加してほしい　使うとなつき度が最大になる　タマムシデパートの警備員に販売させて」。
 * 売り場は 6F の警備員（あぶないはっぱと同じ・tools/megas/build_mega_shop.py の DANGER_LEAF_SHOP）。
 * タマゴ・もう最大の子には「つかっても こうかが ないよ」で減らさない。作りは src/hack_gender_tool.c と同じ。
 */

u8 __attribute__((long_call)) DisplayPartyMenuMessage(const u8* str, u8 b);
#define gText_WontHaveEffect ((const u8*) 0x83DDE0B)
#define FRIENDSHIP_MAX 255
extern const u8 gText_DangerSugar_Used[];

static void ItemUseCB_DangerSugar(u8 taskId, TaskFunc func)
{
	struct Pokemon* mon = &gPlayerParty[gPartyMenu.slotId];
	u16 item = Var800E;
	u8 friendship = FRIENDSHIP_MAX;

	PlaySE(SE_SELECT);
	if (GetMonData(mon, MON_DATA_IS_EGG, NULL)
	|| GetMonData(mon, MON_DATA_FRIENDSHIP, NULL) >= FRIENDSHIP_MAX
	|| !RemoveBagItem(item, 1))
	{
		gPartyMenuUseExitCallback = FALSE;
		DisplayPartyMenuMessage(gText_WontHaveEffect, TRUE);
		ScheduleBgCopyTilemapToVram(2);
		gTasks[taskId].func = func;
		return;
	}

	SetMonData(mon, MON_DATA_FRIENDSHIP, &friendship);
	GetMonNickname(mon, gStringVar1);
	StringExpandPlaceholders(gStringVar4, gText_DangerSugar_Used);
	DisplayPartyMenuMessage(gStringVar4, TRUE);
	ScheduleBgCopyTilemapToVram(2);
	gTasks[taskId].func = func;
}

void FieldUseFunc_DangerSugar(u8 taskId)
{
	gItemUseCB = ItemUseCB_DangerSugar;
	SetUpItemUseCallback(taskId);
}
