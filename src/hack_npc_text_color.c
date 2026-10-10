#include "defines.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/new/ram_locs.h"
#include "Tables/npc_text_colors.h"

/*
    話す人の文の色（男 青・女 ピンク）を、足した人の見た目でも正しく（2026-10-09 ユーザー「メッセージの色分けがただしくないみたい」
    「かいじゅうマニアが女のメッセージになっていたり　アダンも　ナギが男のメッセージになっていたり」）。

    素の ContextNpcGetTextColor（080CCF90・functionrewrites で置き換え）は見た目の番号の下 8 ビット（graphicsIdLowerByte）しか読まない。
    CFRU の見た目の番号は上の 8 ビット（graphicsIdUpperByte）が表の番号で、表 7（ホウエン・ジョウト・シンオウ・モモカ）の人が
    関係ない素の人の色になっていた → 上の 8 ビットも見る:
      表 0          素と同じ（素の GetColorFromTextColorTable 0813D468・変数で決まる見た目 0xF0〜 は素の VarGetObjectEventGraphicsId 0806DD98）
      表 7          src/Tables/npc_text_colors.h（tools/rom/build_npc_text_colors.py）
      表 1〜5       連れ歩きのポケモン → 2（ポケモン）
      ほか          3（ほか）
    色の番号は素と同じ: 0 男・1 女・2 ポケモン・3 ほか。textcolor で決めた色（gSpecialVar_TextColor）がいちばん先（素と同じ）。
*/

#define NPC_TEXT_COLOR_DEFAULT 0xFF
#define NPC_TEXT_COLOR_MON 2
#define NPC_TEXT_COLOR_NEUTRAL 3
#define OBJ_EVENT_GFX_VAR_0 0xF0
#define GetColorFromTextColorTable_ ((u8 (*)(u16)) (0x0813D468 | 1))
#define VarGetObjectEventGraphicsId_ ((u8 (*)(u8)) (0x0806DD98 | 1))

u8 HackNpcTextColor_ContextNpc(void)
{
	struct EventObject *obj;
	u8 lo, hi;

	if (gSpecialVar_TextColor != NPC_TEXT_COLOR_DEFAULT)
		return gSpecialVar_TextColor;
	if (gSelectedEventObject == 0)
		return NPC_TEXT_COLOR_NEUTRAL;

	obj = &gEventObjects[gSelectedEventObject];
	lo = obj->graphicsIdLowerByte;
	hi = obj->graphicsIdUpperByte;
	if (hi == 0)
	{
		if (lo >= OBJ_EVENT_GFX_VAR_0)
			lo = VarGetObjectEventGraphicsId_(lo - OBJ_EVENT_GFX_VAR_0);
		return GetColorFromTextColorTable_(lo);
	}
	if (hi == NPC_TEXT_COLOR_TABLE)
		return (lo < NPC_TEXT_COLOR_COUNT) ? sHackNpcTextColors[lo] : NPC_TEXT_COLOR_NEUTRAL;
	if (hi >= 1 && hi <= 5)
		return NPC_TEXT_COLOR_MON;
	return NPC_TEXT_COLOR_NEUTRAL;
}
