#include "defines.h"
#include "../include/event_data.h"
#include "../include/task.h"
#include "../include/global.fieldmap.h"
#include "../include/field_player_avatar.h"   //PlayerGetDestCoords（long_call＝素の 0805BDF4 は BL で届かない）

/*
	ダウジングマシン（itemfinder）をホウエンなどの地方でも（2026-10-12 ユーザー「ホウエン地方でダウジングとバトルサーチャーが使えないみたい
	他地方でも使えるように」・資料 docs/research/2026-10-12_itemfinder-vsseeker-regions.md）。

	素の itemfinder は今の地図の看板（bg event）のうち種類 7（隠し道具）だけを探す。ホウエンの隠し道具は tools/rom/build_hoenn_scripts.py が
	「種類 PLAYER_FACING_ANY の看板＋台本」で出している（素の隠し道具の欄は旗が 8 ビットで、ホウエンの旗 0x1000〜 が入らない）。
	→ 生成器が隠し道具の台本の頭に印 HIDDEN_MARK（nop・nop1・nop・nop1）と setvar 0x8004 旗 を置き、ここではその印だけを見て旗を読む。
	素の 2 関数を functionrewrites で置き換える（中身は pokefirered の itemfinder.c と同じ・印の看板を足しただけ）:
	  HiddenItemIsWithinRangeOfPlayer 0813F538（今の地図）・HiddenItemAtPos 0813F728（つながった地図のマス）
	静的な補助の関数は素の番地のまま呼ぶ（ROM の BL の飛び先で確かめた）。
*/

#define TASK_HIDDEN_ITEM_FOUND 2   //tHiddenItemFound＝data[2]（pokefirered の itemfinder.c）

#define GetHiddenItemAttr                ((u16 (*)(u32, u8)) (0x080CD5C4 | 1))
#define SetUnderfootHiddenItem           ((void (*)(u8, u32)) (0x0813F664 | 1))
#define RegisterHiddenItemCoordsIfCloser ((void (*)(u8, s16, s16)) (0x0813F944 | 1))
#define FindHiddenItemsInConnectedMaps   ((void (*)(u8)) (0x0813F82C | 1))
#define SetNormalHiddenItem              ((void (*)(u8)) (0x0813F6C0 | 1))

enum { ATTR_ITEM, ATTR_FLAG, ATTR_QUANTITY, ATTR_UNDERFOOT };

//印の付いた隠し道具の看板なら旗を返す（そうでなければ 0）。台本の頭: 00 01 00 01（nop・nop1 × 2）16 04 80 <旗 2 バイト>（setvar 0x8004 旗）
u16 HackItemfinder_MarkedFlag(const struct BgEvent* bg)
{
	const u8* p = bg->bgUnion.script;

	if (bg->kind == 7 || (u32) p < 0x08000000 || (u32) p >= 0x0A000000)
		return 0;
	if (p[0] != 0x00 || p[1] != 0x01 || p[2] != 0x00 || p[3] != 0x01 || p[4] != 0x16 || p[5] != 0x04 || p[6] != 0x80)
		return 0;
	return p[7] | (p[8] << 8);
}

//functionrewrites: HiddenItemIsWithinRangeOfPlayer 0813F538
bool8 HackItemfinder_InRange(const struct MapEvents* events, u8 taskId)
{
	s16 x, y, i, dx, dy;
	u16 flag;

	PlayerGetDestCoords(&x, &y);
	gTasks[taskId].data[TASK_HIDDEN_ITEM_FOUND] = FALSE;
	for (i = 0; i < events->bgEventCount; i++)
	{
		const struct BgEvent* bg = &events->bgEvents[i];

		dx = bg->x + 7 - x;
		dy = bg->y + 7 - y;
		if (bg->kind == 7)
		{
			if (FlagGet(GetHiddenItemAttr(bg->bgUnion.hiddenItem, ATTR_FLAG)))
				continue;
			if (GetHiddenItemAttr(bg->bgUnion.hiddenItem, ATTR_UNDERFOOT) == TRUE)
			{
				if (dx == 0 && dy == 0)
				{
					SetUnderfootHiddenItem(taskId, bg->bgUnion.hiddenItem);
					return TRUE;
				}
				continue;
			}
		}
		else if ((flag = HackItemfinder_MarkedFlag(bg)) == 0 || FlagGet(flag))   //印の付いた隠し道具の看板（拾っていない）
			continue;
		if (dx >= -7 && dx <= 7 && dy >= -5 && dy <= 5)
			RegisterHiddenItemCoordsIfCloser(taskId, dx, dy);
	}
	FindHiddenItemsInConnectedMaps(taskId);
	if (gTasks[taskId].data[TASK_HIDDEN_ITEM_FOUND] == TRUE)
	{
		SetNormalHiddenItem(taskId);
		return TRUE;
	}
	return FALSE;
}

//functionrewrites: HiddenItemAtPos 0813F728（つながった地図のマス）
bool8 HackItemfinder_AtPos(const struct MapEvents* events, s16 x, s16 y)
{
	u8 count = events->bgEventCount;
	const struct BgEvent* bgEvents = events->bgEvents;
	u16 flag;
	int i;

	for (i = 0; i < count; i++)
	{
		if (x != bgEvents[i].x || y != bgEvents[i].y)
			continue;
		if (bgEvents[i].kind == 7)
		{
			flag = GetHiddenItemAttr(bgEvents[i].bgUnion.hiddenItem, ATTR_FLAG);
			return GetHiddenItemAttr(bgEvents[i].bgUnion.hiddenItem, ATTR_UNDERFOOT) != TRUE && !FlagGet(flag);
		}
		if ((flag = HackItemfinder_MarkedFlag(&bgEvents[i])) != 0)
			return !FlagGet(flag);
	}
	return FALSE;
}
