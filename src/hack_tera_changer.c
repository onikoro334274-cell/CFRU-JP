#include "defines.h"
#include "../include/event_data.h"
#include "../include/list_menu.h"
#include "../include/menu.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/string_util.h"
#include "../include/constants/pokemon.h"
#include "../include/constants/songs.h"

/*
 * テラスタイプを変えてくれる人（2026-10-06・バトルフロンティアのフレンドリィショップ (5,4)）。
 * ユーザー「バトルフロンティアにテラスタルタイプを変えてくれる人導入してほしい」。
 * 書き換えは CFRU の special 0x131（sp131_ChangeTeraType・0x8004 手持ちの番号・0x8005 タイプ）。
 * ここはタイプを選ぶスクロールの一覧だけ（src/music_player.c と同じ手順・状態はタスクの data[]＝6.13）。
 * 結果は LASTRESULT（タイプの番号・B でやめたら 0x7F）。スクリプトは assembly/hack_tera_changer/。
 */

extern const u8 gText_Tera_Normal[];
extern const u8 gText_Tera_Fire[];
extern const u8 gText_Tera_Water[];
extern const u8 gText_Tera_Electric[];
extern const u8 gText_Tera_Grass[];
extern const u8 gText_Tera_Ice[];
extern const u8 gText_Tera_Fighting[];
extern const u8 gText_Tera_Poison[];
extern const u8 gText_Tera_Ground[];
extern const u8 gText_Tera_Flying[];
extern const u8 gText_Tera_Psychic[];
extern const u8 gText_Tera_Bug[];
extern const u8 gText_Tera_Rock[];
extern const u8 gText_Tera_Ghost[];
extern const u8 gText_Tera_Dragon[];
extern const u8 gText_Tera_Dark[];
extern const u8 gText_Tera_Steel[];
extern const u8 gText_Tera_Fairy[];
extern const u8 gText_Tera_Stellar[];

static const struct ListMenuItem sTeraTypeItems[] =
{
	{gText_Tera_Normal, TYPE_NORMAL},
	{gText_Tera_Fire, TYPE_FIRE},
	{gText_Tera_Water, TYPE_WATER},
	{gText_Tera_Electric, TYPE_ELECTRIC},
	{gText_Tera_Grass, TYPE_GRASS},
	{gText_Tera_Ice, TYPE_ICE},
	{gText_Tera_Fighting, TYPE_FIGHTING},
	{gText_Tera_Poison, TYPE_POISON},
	{gText_Tera_Ground, TYPE_GROUND},
	{gText_Tera_Flying, TYPE_FLYING},
	{gText_Tera_Psychic, TYPE_PSYCHIC},
	{gText_Tera_Bug, TYPE_BUG},
	{gText_Tera_Rock, TYPE_ROCK},
	{gText_Tera_Ghost, TYPE_GHOST},
	{gText_Tera_Dragon, TYPE_DRAGON},
	{gText_Tera_Dark, TYPE_DARK},
	{gText_Tera_Steel, TYPE_STEEL},
	{gText_Tera_Fairy, TYPE_FAIRY},
	{gText_Tera_Stellar, TYPE_STELLAR},
};

#define MAX_SHOWN 6

#define tListTaskId data[0]
#define tWindowId   data[1]

static void TeraChanger_MoveCursor(unusedArg s32 itemIndex, bool8 onInit, unusedArg struct ListMenu* list)
{
	if (!onInit)
		PlaySE(SE_SELECT);
}

static void Task_TeraChangerList(u8 taskId)
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

//callasm HackTeraChanger_ShowList → waitstate
void HackTeraChanger_ShowList(void)
{
	struct ListMenuTemplate list;
	struct WindowTemplate win;
	u8 windowId, taskId;

	win.bg = 0;
	win.tilemapLeft = 21;                       //画面の右上（幅 8）
	win.tilemapTop = 1;
	win.width = 8;
	win.height = MAX_SHOWN * 2;
	win.paletteNum = 15;
	win.baseBlock = 0x038;
	windowId = AddWindow(&win);
	DrawStdWindowFrame(windowId, FALSE);

	list.items = sTeraTypeItems;
	list.moveCursorFunc = TeraChanger_MoveCursor;
	list.itemPrintFunc = NULL;
	list.totalItems = ARRAY_COUNT(sTeraTypeItems);
	list.maxShowed = MAX_SHOWN;
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

	taskId = CreateTask(Task_TeraChangerList, 80);
	gTasks[taskId].tWindowId = windowId;
	gTasks[taskId].tListTaskId = ListMenuInit(&list, 0, 0);
	PutWindowTilemap(windowId);
	CopyWindowToVram(windowId, COPYWIN_BOTH);
}

bool8 TeraTypeChangeBanned(u16 species);   //src/scripting.c（CFRU）
bool8 sp131_ChangeTeraType(void);          //src/scripting.c（CFRU）

//callasm（0x8004 手持ちの番号・0x8005 タイプ）: LASTRESULT 0 変えた・1 もうそのタイプ・2 変えられない。gStringVar2 にタイプの名前
void HackTeraChanger_Apply(void)
{
	u16 slot = Var8004, type = Var8005;
	u32 i;

	for (i = 0; i < ARRAY_COUNT(sTeraTypeItems); ++i)
	{
		if (sTeraTypeItems[i].id == type)
			StringCopy(gStringVar2, sTeraTypeItems[i].name);
	}

	if (slot >= PARTY_SIZE || TeraTypeChangeBanned(gPlayerParty[slot].species))
		gSpecialVar_LastResult = 2;
	else if (gPlayerParty[slot].teratype == type)
		gSpecialVar_LastResult = 1;
	else
		gSpecialVar_LastResult = sp131_ChangeTeraType() ? 0 : 2;
}
