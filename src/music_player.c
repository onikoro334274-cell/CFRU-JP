#include "defines.h"
#include "../include/event_data.h"
#include "../include/item_use.h"
#include "../include/list_menu.h"
#include "../include/menu.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/constants/songs.h"

/*
 * たいせつなもの「おんがくプレイヤー」（#28・2026-09-27 ユーザー要望
 * 「たいせつなものミュージックプレイヤーみたいなの作成してそこから音楽ながせるように」）
 *
 * バッグで「つかう」→ バッグを閉じてフィールドに戻る → スクリプトを動かす。
 * 形は CFRU の がくしゅうそうち（src/party_menu.c の FieldUseFunc_ExpShare）と同じ。
 * 流す・止めるはスクリプト側（assembly/music_player/music_player_scripts.s・tools/bgm/build_music_player.py が生成）。
 * 流した曲は playsong（一時）なので、マップを移るとその場所の曲に戻る。
 *
 * 一覧から選ぶ画面（2026-09-28 ユーザー「一覧から選ぶ画面作って」）:
 *   素の FRLG のスクロール選択肢（pokefirered src/field_specials.c の Task_CreateScriptListMenu /
 *   Task_ListMenuHandleInput / Task_DestroyListMenu）と同じ手順。項目（曲の名前ぜんぶ＋「おんがくを とめる」）は
 *   スクリプトの gMusicPlayerListItems（ListMenuItem の表・生成）。
 *   ★ 書き換え可能な static は使えない（6.13）ので、状態はタスクの data[] に置く。
 *   ★ スクロールの矢印（AddScrollIndicatorArrowPair）は BPRJ.ld に番地が無いので出さない。一度に見せるのは最大 MAX_SHOWN 行
 *   結果は LASTRESULT（選んだ番号・B でやめたら 0x7F）に入れてスクリプトを再開する。
 */

extern const u8 EventScript_MusicPlayer[];
extern const struct ListMenuItem gMusicPlayerListItems[];
extern const u16 gMusicPlayerListCount;

#define MAX_SHOWN 8

#define tListTaskId data[0]
#define tWindowId   data[1]

static void Task_MusicPlayerField(u8 taskId)
{
	ScriptContext1_SetupScript(EventScript_MusicPlayer);
	DestroyTask(taskId);
}

void FieldUseFunc_MusicPlayer(u8 taskId)
{
	sItemUseOnFieldCB = Task_MusicPlayerField;
	SetUpItemUseOnFieldCallback(taskId);
}

static void MusicPlayer_MoveCursor(unusedArg s32 itemIndex, bool8 onInit, unusedArg struct ListMenu* list)
{
	if (!onInit)
		PlaySE(SE_SELECT);
}

static void Task_MusicPlayerList(u8 taskId)
{
	struct Task* task = &gTasks[taskId];
	s32 input = ListMenu_ProcessInput(task->tListTaskId);

	if (input == LIST_NOTHING_CHOSEN)
		return;

	gSpecialVar_LastResult = (input == LIST_CANCEL) ? 0x7F : (u16) input;
	PlaySE(SE_SELECT);
	DestroyListMenuTask(task->tListTaskId, NULL, NULL);
	ClearStdWindowAndFrameToTransparent(task->tWindowId, TRUE);
	RemoveWindow(task->tWindowId);
	DestroyTask(taskId);
	EnableBothScriptContexts();
}

//callasm MusicPlayer_ShowList → waitstate
void MusicPlayer_ShowList(void)
{
	struct ListMenuTemplate list;                //★ = {0} にすると memset の呼び出しが出る。欄は下で全部代入する
	struct WindowTemplate win;
	u32 i, count = gMusicPlayerListCount;
	s32 maxWidth = 0;
	u8 width, shown, windowId, taskId;

	for (i = 0; i < count; ++i)
	{
		s32 w = GetStringWidth(2, gMusicPlayerListItems[i].name, 0);
		if (w > maxWidth)
			maxWidth = w;
	}

	width = (maxWidth + 9) / 8 + 1;             //素の Task_CreateScriptListMenu と同じ（カーソルの分と余白）
	if (width > 28)
		width = 28;
	shown = (count < MAX_SHOWN) ? count : MAX_SHOWN;

	win.bg = 0;
	win.tilemapLeft = 29 - width;               //画面の右上
	win.tilemapTop = 1;
	win.width = width;
	win.height = shown * 2;
	win.paletteNum = 15;
	win.baseBlock = 0x038;                      //素のスクロール選択肢と同じ
	windowId = AddWindow(&win);
	DrawStdWindowFrame(windowId, FALSE);

	list.items = gMusicPlayerListItems;
	list.moveCursorFunc = MusicPlayer_MoveCursor;
	list.itemPrintFunc = NULL;
	list.totalItems = count;
	list.maxShowed = shown;
	list.windowId = windowId;
	list.header_X = 0;
	list.item_X = 8;
	list.cursor_X = 0;
	list.upText_Y = 0;
	list.cursorPal = 2;
	list.fillValue = 1;
	list.cursorShadowPal = 3;
	list.lettersSpacing = 0;
	list.itemVerticalPadding = 0;
	list.scrollMultiple = LIST_NO_MULTIPLE_SCROLL;
	list.fontId = 2;
	list.cursorKind = 0;

	taskId = CreateTask(Task_MusicPlayerList, 80);
	gTasks[taskId].tWindowId = windowId;
	gTasks[taskId].tListTaskId = ListMenuInit(&list, 0, 0);
	PutWindowTilemap(windowId);
	CopyWindowToVram(windowId, COPYWIN_BOTH);
}
