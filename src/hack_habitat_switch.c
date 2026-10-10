#include "defines.h"
#include "../include/event_data.h"
#include "../include/item.h"
#include "../include/item_use.h"
#include "../include/main.h"
#include "../include/script.h"
#include "../include/task.h"
#include "../include/constants/items.h"

#include "Tables/hoenn_script_flags.h"

/*
 * せいそくスイッチ（ITEM_HABITAT_SWITCH 0x34A・たいせつなもの・2026-10-10）。
 * ユーザー「アイテムで切り替えでカントーホウエンのもとの生息地でなるの実装してほしい」
 * （判断: 名前「せいそくスイッチ」・はじめは入れ替えた野生のまま・最初から持っている）。
 *
 * - 印 HACK_FLAG_HABITAT_ORIGINAL（0x12CC）が立っていれば、カントー・ホウエンの野生は入れ替えない＝元の生息地
 *   （src/wild_shuffle.c の HackWild_Shuffle が見る）。最初は下りている＝入れ替えた野生（今までと同じ）。
 * - バッグで つかう → バッグを閉じて EventScript_HabitatSwitch（assembly/hack_habitat_switch/habitat_switch.s）が
 *   切り替えて知らせる（Moemonボタンと同じ形・src/hack_moemon_switch.c）。
 * - 持っていなければフィールドで黙って渡す（64 コマごと・src/overworld_speedup.c から・前のセーブにも入る）。
 */

extern const u8 EventScript_HabitatSwitch[];

//callasm: 切り替え、LASTRESULT = 切り替えたあと元の生息地か
void HackHabitatSwitch_Toggle(void)
{
	if (FlagGet(HACK_FLAG_HABITAT_ORIGINAL))
		FlagClear(HACK_FLAG_HABITAT_ORIGINAL);
	else
		FlagSet(HACK_FLAG_HABITAT_ORIGINAL);
	gSpecialVar_LastResult = FlagGet(HACK_FLAG_HABITAT_ORIGINAL) != 0;
}

static void Task_HabitatSwitchField(u8 taskId)
{
	ScriptContext1_SetupScript(EventScript_HabitatSwitch);
	DestroyTask(taskId);
}

void FieldUseFunc_HabitatSwitch(u8 taskId)
{
	sItemUseOnFieldCB = Task_HabitatSwitchField;
	SetUpItemUseOnFieldCallback(taskId);
}

void HackHabitatSwitch_FieldTick(void)
{
	//Moemonボタン（0x20）と同じコマにしない＝1 コマに 1 つだけ道具を足す
	//★ 2026-10-10 ユーザー「設定細分化 アイテムでON/OFF系」: 切り替えは「せってい」の行に（src/bw_option_menu.c）。道具はもう渡さず、持っていれば消す
	if ((gMain.vblankCounter2 & 0x3F) == 0x30 && CheckBagHasItem(ITEM_HABITAT_SWITCH, 1))
		RemoveBagItem(ITEM_HABITAT_SWITCH, 1);
}
