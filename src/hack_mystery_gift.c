#include "defines.h"
#include "../include/event_data.h"
#include "../include/item.h"
#include "../include/item_use.h"
#include "../include/party_menu.h"
#include "../include/save.h"
#include "../include/script.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/constants/items.h"

/*
 * ふしぎなおくりもの（がくしゅうそうち＋おもいだしノート）と おもいだしノート（2026-10-03）。
 * ユーザー「おもいだしアイテムを作成してほしい（キノコ消費なし）」「ふしぎなおくりものでがくしゅうそうち配布したい
 *          （もらえるイベントはがくしゅうそうちもっていたら消えるように）」→ タイトル画面のメニュー・カントーのポケセン全部・
 *          思い出しの道具も配達員が一緒にくれる・「ショップのアンケートでの表示フラグはなくして」。
 *
 * ## タイトル画面
 *   - 「ふしぎなおくりもの」は素では旗 0x839（ショップのアンケートの合言葉）で出る → 0806DAB0（IsMysteryGiftEnabled）を
 *     いつも TRUE に（bytereplacement）
 *   - 選ぶと素はワイヤレスアダプタの有無で通信の画面へ行く。こちらはいつも「お知らせ」の道（0800C4D2 の BL を 1 に・
 *     bytereplacement）→ お知らせのタスク（0800C51C の番地を HackMG_Task に・routinepointers）
 *   - HackMG_Task: 受け取った印（旗 0x12FE）を付けてレポートを書き、結果の文を gStringVar4 に入れて、素のお知らせのタスク
 *     （0800C5BD）に引き継ぐ。お知らせの文（0800C614 / 0800C630 の番地の語）は gStringVar4 そのものに（bytereplacement）。
 *     ★ 「{STR_VAR_3}」の文に向けると何も出なかった（文字を出す 0800C824 は差し込みを展開しない・2026-10-03 ユーザーの動画）
 *
 * ## 配達員（素のカントー・ななしまのポケセン 2 階 19 か所・localId 4・隠す旗 0x070）
 *   - 出るか: 素は CableClub_OnTransition の special 0x180（ValidateSavedWonderCard）。表の 0x180 番を
 *     HackMG_ShouldShowDeliveryman に（routinepointers 08163668）。受け取った印があって、がくしゅうそうちか
 *     おもいだしノートのどちらかを持っていないときだけ出る
 *   - 話しかけたとき: 素は trywondercardscript → GetSavedRamScriptIfValid（08069708）。functionrewrites で
 *     HackMG_GetDeliverymanScript に（ふしぎなカードの仕組みは使わない・通信が無いので本物のカードは来ない）
 *
 * ## おもいだしノート（ITEM_MOVE_NOTE 0x33E・たいせつなもの）
 *   - つかう → バッグを閉じて EventScript_MoveNote（2 のしまの技思い出しの人と同じ流れで、キノコを払う所だけ無い）
 */

#define HACK_FLAG_MYSTERY_GIFT 0x12FE       //受け取った印（build_hoenn_scripts.RESERVED_FLAGS）
#define MAIN_MENU_ERROR_TASK ((TaskFunc)(0x0800C5BC | 1))   //素のお知らせのタスク（文を出して A でメニューへ）
#define SAVE_NORMAL 0

extern const u8 HackMG_EventScript_Deliveryman[];
extern const u8 EventScript_MoveNote[];
extern const u8 gText_HackMG_Received[];
extern const u8 gText_HackMG_Nothing[];
extern const u8 gText_HackMG_SaveFailed[];

#define KANTO_FLAG_MG_LUCKY_EGG 0x12EF //tools/rom/build_hoenn_scripts.RESERVED_FLAGS と同じ
#define KANTO_FLAG_MG_MOMO_ICE 0x12ED  //同上（2026-10-05 ももアイス・1 回だけ）

static bool8 GiftLeft(void)
{
	return !CheckBagHasItem(ITEM_EXP_SHARE, 1) || !CheckBagHasItem(ITEM_MOVE_NOTE, 1)
	    || !FlagGet(KANTO_FLAG_MG_LUCKY_EGG)    //★ 2026-10-04 しあわせタマゴ（持たせられるので「持っているか」ではなく 1 回だけの印で）
	    || !FlagGet(KANTO_FLAG_MG_MOMO_ICE);    //★ 2026-10-05 ももアイス（同じく 1 回だけ）
}

//★ 2026-10-04 ユーザーの動画「カンナの部屋で思い出しノート使用したら手持ちトップのコノヨザル固定になって…キャンセルすら出来ない」:
//手持ちの画面（special 0x9F／0xDB）は 0x8004 に選んだ番号（やめたら 7）を入れるが、フィールドへ戻るとき地図の ON_RESUME が走り、
//四天王の部屋は setvar VAR_0x8004, 0（pokefirered PokemonLeague_*Room_OnResume）で上書きしていた → 先頭を選んだことになり抜けられなかった。
//waitstate のあと callasm で、手持ちの画面が持っている番号（gPartyMenu.slotId・やめたら 7）から戻す。
void HackPartyChoice_RestoreSlot(void)
{
	s8 slot = gPartyMenu.slotId;

	Var8004 = (slot >= 0 && slot < PARTY_SIZE) ? slot : PARTY_SIZE + 1;
}

//タイトル画面: 「ふしぎなおくりもの」を選んだとき（素のお知らせのタスクの前に 1 度だけ）
//★ 2026-10-10 受け取る中身（BW のタイトルのメニュー src/bw_main_menu.c からも呼ぶ）: 印を付けてレポート → 結果の文を gStringVar4 に
void HackMG_Receive(void)
{
	if (!GiftLeft())
		StringCopy(gStringVar4, gText_HackMG_Nothing);
	else
	{
		FlagSet(HACK_FLAG_MYSTERY_GIFT);
		if (TrySavingData(SAVE_NORMAL) == 1) //SAVE_STATUS_OK
			StringCopy(gStringVar4, gText_HackMG_Received);
		else
			StringCopy(gStringVar4, gText_HackMG_SaveFailed);
	}
}

void HackMG_Task(u8 taskId)
{
	HackMG_Receive();
	gTasks[taskId].func = MAIN_MENU_ERROR_TASK;
}

//ポケセン 2 階の配達員を出すか（special 0x180 の代わり）
u16 HackMG_ShouldShowDeliveryman(void)
{
	return FlagGet(HACK_FLAG_MYSTERY_GIFT) && GiftLeft();
}

//配達員に話しかけたときのスクリプト（素の GetSavedRamScriptIfValid の代わり・無ければ NULL ＝ 何もしない）
const u8* HackMG_GetDeliverymanScript(void)
{
	if (HackMG_ShouldShowDeliveryman())
		return HackMG_EventScript_Deliveryman;
	return NULL;
}

/* ---- おもいだしノート ---- */
static void Task_MoveNoteField(u8 taskId)
{
	ScriptContext1_SetupScript(EventScript_MoveNote);
	DestroyTask(taskId);
}

void FieldUseFunc_MoveNote(u8 taskId)
{
	sItemUseOnFieldCB = Task_MoveNoteField;
	SetUpItemUseOnFieldCallback(taskId);
}
