#include "defines.h"
#include "../include/event_data.h"
#include "../include/item.h"
#include "../include/item_use.h"
#include "../include/random.h"
#include "../include/script.h"
#include "../include/task.h"
#include "../include/constants/items.h"

/*
 * かがやくおまもり（ITEM_SHINING_CHARM 0x341・たいせつなもの・2026-10-04）。
 * ユーザー「ひかるおまもりの上位版を作成してほしい 確率1/500で ON/OFFできるように」→ 受け取りはひかるおまもりと同じお母さん・
 * 名前は「かがやくおまもり」（ユーザー判断）。
 *
 * - 持っていて ON（旗 0x12F8）のとき、ポケモンを作るたびに 1/500 で色違い（src/build_pokemon.c の CheckShinyMon から
 *   HackShiningCharm_Roll を聞く・ひかるおまもりと同じ所＝野生・もらう・タマゴ。トレーナーの手持ちは色違いにならない作りのまま）
 * - バッグで つかう → バッグを閉じて EventScript_ShiningCharm（assembly/hack_charm/charm_scripts.s）が ON/OFF を切り替えて知らせる
 * - 色違いにする仕組みは CFRU の ForceMonShiny（性格・性別などを変えずに PID を選び直す＝新しく作るポケモンだけ・6.5）
 */
#define KANTO_FLAG_SHINING_CHARM_ON 0x12F8   //build_hoenn_scripts.RESERVED_FLAGS（ON の印）
#define SHINING_CHARM_ODDS 500

extern const u8 EventScript_ShiningCharm[];

bool8 HackShiningCharm_Roll(void)
{
	return FlagGet(KANTO_FLAG_SHINING_CHARM_ON)
	    && CheckBagHasItem(ITEM_SHINING_CHARM, 1)
	    && Random() % SHINING_CHARM_ODDS == 0;
}

//callasm: ON/OFF を切り替え、LASTRESULT = 切り替えたあと ON か
void HackShiningCharm_Toggle(void)
{
	if (FlagGet(KANTO_FLAG_SHINING_CHARM_ON))
		FlagClear(KANTO_FLAG_SHINING_CHARM_ON);
	else
		FlagSet(KANTO_FLAG_SHINING_CHARM_ON);
	gSpecialVar_LastResult = FlagGet(KANTO_FLAG_SHINING_CHARM_ON) != 0;
}

static void Task_ShiningCharmField(u8 taskId)
{
	ScriptContext1_SetupScript(EventScript_ShiningCharm);
	DestroyTask(taskId);
}

void FieldUseFunc_ShiningCharm(u8 taskId)
{
	sItemUseOnFieldCB = Task_ShiningCharmField;
	SetUpItemUseOnFieldCallback(taskId);
}
