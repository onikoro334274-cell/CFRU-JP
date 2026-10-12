#include "defines.h"
#include "../include/event_data.h"
#include "../include/field_screen_effect.h"
#include "../include/field_weather.h"
#include "../include/constants/field_weather.h"
#include "../include/item.h"
#include "../include/item_menu.h"
#include "../include/item_use.h"
#include "../include/main.h"
#include "../include/malloc.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/task.h"
#include "../include/constants/items.h"
#include "../include/menu_helpers.h"
#include "../include/sound.h"
#include "../include/window.h"
#include "../include/constants/hold_effects.h"
#include "../include/constants/songs.h"
#include "../include/new/item.h"
extern const struct Item gItemData[];   //src/Tables/item_tables.c

/*
 * いくせいポーチ（ITEM_TRAINING_POUCH 0x345・2026-10-06）。
 * ユーザー「努力値やミントってわざマシンみたいな袋作ってそのなかにいれられない？」→ 新しい袋 →「印とかで分けられない？」。
 *
 * 道具は今までどおり「どうぐ」ポケットの置き場にしまう（セーブの形は変えない）。育成の道具かどうかは道具の番号で決める
 * （HackTrainingPouch_IsTrainingItem）。バッグの個数を数える所（CFRU の StoreBagItemCount・patches/0187）で、
 *   ふだん: 育成の道具を「どうぐ」の後ろへ寄せ、数は育成でない道具だけ
 *   ポーチ: 育成の道具を前へ寄せ、数は育成の道具だけ
 * にする。素のバッグは一覧の番号をそのまま置き場の番号に使うので、並べ替えて前の何個かだけ見せれば番号はずれない。
 * ポーチの間は印 HACK_FLAG_TRAINING_POUCH。たいせつなもので「つかう」とバッグを「どうぐ」で開き直す。フィールドに
 * 戻ると印を下ろす（HackTrainingPouch_FieldTick・src/overworld_speedup.c）。
 * ポーチは育成の道具を初めて持ったとき自動で入る（ユーザー判断・文は出さない）→ 2026-10-11 やめた（HackTrainingPouch_FieldTick）。
 */

#define HACK_FLAG_TRAINING_POUCH 0x12EB   //build_hoenn_scripts.RESERVED_FLAGS（2026-10-10 からは使わない＝前のセーブで立っていれば下ろすだけ）

/*
 * ★ 2026-10-10 バッグの どうぐ を種類ごとのページに（#73・ユーザー「その分け方で作って」）:
 *   どうぐ（その他）／かいふく／もちもの／せんとう／いくせい の 5 ページ。置き場とセーブの形は変えない（どうぐ 700 枠を共有）。
 *   今のページは変数 HACK_VAR_BAG_PAGE（0x50DD・build_hoenn_scripts.RESERVED_VARS）＝閉じても次に開いたとき同じページ。
 *   いくせいポーチは「いくせい」のページになった（ポーチを つかう と いくせい のページで開く）。
 *   左右: どうぐ のポケットの中でページを回し、いちばん右（いくせい）の次が たいせつなもの・たいせつなもの の左は いくせい
 *   （素の ProcessPocketSwitchInput 08109C60 を functionrewrites）。見出しは素の PrintBagPocketName 081092E8 を functionrewrites して
 *   どうぐ のポケットのときだけページの名前。ページを替えるときは素の SwitchPockets（08109CC8）を向き 0 で呼んで一覧を作り直す。
 */
#define HACK_VAR_BAG_PAGE 0x50DD
enum { BAG_PAGE_OTHER, BAG_PAGE_MEDICINE, BAG_PAGE_HELD, BAG_PAGE_BATTLE, BAG_PAGE_TRAINING, BAG_PAGE_COUNT };

struct BagPocketsCFRU
{
	struct ItemSlot* itemRam;
	u32 itemAmount;
};
#define gBagPocketsCFRU ((struct BagPocketsCFRU*) 0x20397D8)   //src/item.c の gBagPockets（先頭の 2 つだけ使う）

bool8 HackTrainingPouch_IsTrainingItem(u16 item)
{
	if (item >= ITEM_HEALTH_WING && item <= ITEM_PP_MAX)          //努力値のはね 6・努力値の薬・ふしぎなアメ・ポイントアップ／マックス
		return TRUE;
	if (item >= ITEM_LONELY_MINT && item <= ITEM_SERIOUS_MINT)    //ミント 21
		return TRUE;
	if ((item >= ITEM_EXP_CANDY_XS && item <= ITEM_EXP_CANDY_XL) || item == ITEM_EXP_CANDY_XXL)   //けいけんアメ（XXL 0x357 は番号が続いていない・patches/0323）
		return TRUE;

	switch (item) {
		case ITEM_BOTTLE_CAP:
		case ITEM_GOLD_BOTTLE_CAP:
		case ITEM_ABILITY_CAPSULE:
		case ITEM_ABILITY_PATCH:
		case ITEM_EV_RESET:
		case ITEM_LEVEL_DOWN:
		case ITEM_GENDER_TOOL: //2026-10-06 きょせいどうぐ
		case ITEM_DANGER_SUGAR: //2026-10-09 あぶないこなざとう
			return TRUE;
	}
	return FALSE;
}

static u8 CurrentPage(void)
{
	u16 page = VarGet(HACK_VAR_BAG_PAGE);
	return (page < BAG_PAGE_COUNT) ? page : BAG_PAGE_OTHER;
}

//道具 → ページ（いくせい → かいふく（戦闘で使う薬）→ せんとう（プラスパワーなど）→ もちもの（持たせる効果）→ どうぐ）
u8 HackBagPage_Of(u16 item)
{
	u8 usage;

	if (HackTrainingPouch_IsTrainingItem(item))
		return BAG_PAGE_TRAINING;
	usage = gItemData[SanitizeItemId(item)].battleUsage;   //ItemId_GetBattleUsage は宣言だけで中身が無い
	if (usage == 1)
		return BAG_PAGE_MEDICINE;
	if (usage == 2)
		return BAG_PAGE_BATTLE;
	if (ItemId_GetHoldEffect(item) != 0)
		return BAG_PAGE_HELD;
	return BAG_PAGE_OTHER;
}

bool8 HackTrainingPouch_Active(void)
{
	return CurrentPage() == BAG_PAGE_TRAINING;
}

//CFRU の StoreBagItemCount（patches/0187）から: 見せる道具を前へ寄せ（それぞれの中の並びは保つ）、見せる数を返す
u16 HackTrainingPouch_Arrange(struct ItemSlot* slots, u16 capacity)
{
	struct ItemSlot* tmp = Malloc(capacity * sizeof(struct ItemSlot));
	u8 page = CurrentPage();
	u32 i, n = 0, shown;

	//★ 2026-10-10 守り（patches/0284）: 無い番号の道具（ITEMS_COUNT 以上）の枠は消す。v0.36.0 のセーブで どうぐ の 1 枠が
	//  道具 0x1C20・数 17452 に壊れ「？？？？ ×?52」と出た（書いた処理は未特定・docs/research/2026-10-10_bag-garbage-item.md）。
	//  バッグを開くたびにここを通るので、壊れたセーブも開けば直る
	for (i = 0; i < capacity; ++i)
	{
		if (slots[i].itemId >= ITEMS_COUNT)
		{
			slots[i].itemId = ITEM_NONE;
			slots[i].quantity = 0;
		}
	}

	if (tmp == NULL)
	{
		// 空き枠を詰め、選択ページの道具を順番を保って前へ移す。
		for (i = 0; i < capacity; ++i)
			if (slots[i].itemId != ITEM_NONE)
				slots[n++] = slots[i];
		for (i = n; i < capacity; ++i)
		{
			slots[i].itemId = ITEM_NONE;
			slots[i].quantity = 0;
		}
		shown = 0;
		for (i = 0; i < n; ++i)
		{
			if (HackBagPage_Of(slots[i].itemId) == page)
			{
				struct ItemSlot selected = slots[i];
				u32 j;
				for (j = i; j > shown; --j)
					slots[j] = slots[j - 1];
				slots[shown++] = selected;
			}
		}
		return shown;
	}

	for (i = 0; i < capacity; ++i) //見せる道具
	{
		if (slots[i].itemId != ITEM_NONE && HackBagPage_Of(slots[i].itemId) == page)
			tmp[n++] = slots[i];
	}
	shown = n;
	for (i = 0; i < capacity; ++i) //隠す道具
	{
		if (slots[i].itemId != ITEM_NONE && HackBagPage_Of(slots[i].itemId) != page)
			tmp[n++] = slots[i];
	}
	for (i = 0; i < n; ++i)
		slots[i] = tmp[i];
	for (; i < capacity; ++i)
	{
		slots[i].itemId = ITEM_NONE;
		slots[i].quantity = 0;
	}

	Free(tmp);
	return shown;
}

static void ResetItemsPocketCursor(void)
{
	gBagMenuState.cursorPos[0] = 0;
	gBagMenuState.itemsAbove[0] = 0;
}

void HackTrainingPouch_Reset(void);

//ポーチで「もどる」（B）を押したとき: 閉じずに ふだんのバッグの たいせつなもの へ戻る（わざマシンケースと同じ・2026-10-06
//ユーザー「印たてたときのもどるで解除できる？」）。そのバッグを閉じると今までどおりスタートメニュー（フィールドへ）
static void CB2_ReturnFromPouch(void)
{
	HackTrainingPouch_Reset();
	gBagMenuState.pocket = POCKET_KEY_ITEMS - 1;
	CB2_BagMenuFromStartMenu();
}

static void CB2_OpenBagAsPouch(void)
{
	VarSet(HACK_VAR_BAG_PAGE, BAG_PAGE_TRAINING);
	ResetItemsPocketCursor();
	GoToBagMenu(0, POCKET_ITEMS - 1, CB2_ReturnFromPouch);
}

static void Task_OpenPouchFromField(u8 taskId)
{
	if (!gPaletteFade->active)
	{
		CleanupOverworldWindowsAndTilemaps();
		VarSet(HACK_VAR_BAG_PAGE, BAG_PAGE_TRAINING);
		ResetItemsPocketCursor();
		GoToBagMenu(0, POCKET_ITEMS - 1, CB2_ReturnFromPouch);
		DestroyTask(taskId);
	}
}

void FieldUseFunc_TrainingPouch(u8 taskId)
{
	if (gTasks[taskId].data[3] == 0) //バッグの中から
	{
		ItemMenu_SetExitCallback(CB2_OpenBagAsPouch);
		ItemMenu_StartFadeToExitCallback(taskId);
	}
	else //フィールドから（SELECT に登録）
	{
		FadeScreen(FADE_TO_BLACK, 0);
		gTasks[taskId].func = Task_OpenPouchFromField;
	}
}

//ポーチの印を下ろす（フィールドの毎コマと、スタートメニューを出したとき＝src/bw_start_menu.c・2026-10-06
//ユーザー「バッグをとじてもいくせいポーチのまま」: バッグを閉じるとフィールドではなくスタートメニューへ戻るので、フィールドだけでは下りなかった）
void HackTrainingPouch_Reset(void)
{
	if (FlagGet(HACK_FLAG_TRAINING_POUCH))
	{
		FlagClear(HACK_FLAG_TRAINING_POUCH);
		ResetItemsPocketCursor();
	}
}

//フィールドの毎コマ（src/overworld_speedup.c）: ポーチの印を下ろす
//★ 2026-10-11 ユーザー「いくせいポーチをままからもらえなくして」→ 育成の道具を持ったときに自動で渡すのをやめた
//  （お母さんの贈り物の直後に入るので、お母さんからもらったように見えていた）。育成の道具はバッグの「いくせい」のページで見られる。
//  前のセーブで持っているポーチはそのまま使える
void HackTrainingPouch_FieldTick(void)
{
	HackTrainingPouch_Reset();
}

/* ---- 2026-10-10 ページの切り替えと見出し ---- */
extern const u8 gText_BagPage_Other[], gText_BagPage_Medicine[], gText_BagPage_Held[], gText_BagPage_Battle[], gText_BagPage_Training[];
static const u8 *const sBagPageNames[BAG_PAGE_COUNT] =
{
	gText_BagPage_Other, gText_BagPage_Medicine, gText_BagPage_Held, gText_BagPage_Battle, gText_BagPage_Training,
};

#define sBagMenuDisplayPtr (*(u8**) 0x0203AC88)                                    //素の sBagMenuDisplay（+5 の下 4 ビットが pocketSwitchMode・+0xA から nItems[3]・+0xD から maxShowed[3]）
#define sVanillaPocketNames ((const u8* const*) 0x08413BA4)                          //素の sPocketNames（PrintBagPocketName 081092E8 のリテラル語）
#define VanillaGetLRKeysPressed ((u8 (*)(void)) (0x080C0800 | 1))
#define VanillaSwitchPockets ((void (*)(u8, s16, bool16)) (0x08109CC8 | 1))
#define VanillaTask_AnimateSwitchPockets ((TaskFunc) (0x08109D90 | 1))
#define VanillaBagPrintTextOnWin1CenteredColor0 ((void (*)(const u8*, u8)) (0x0810C454 | 1))
void StoreBagItemCount(void);   //src/item.c
u16 GetNumItemsInPocket(u8 pocket);   //src/item.c
#define SE_BAG_POCKET 0xF6   //素の ProcessPocketSwitchInput が鳴らす音
#define BAG_TASK_RESET_CURSOR 9    //バッグのタスクの data[9]（素は 0〜8・10〜13・14/15 は SetTaskFuncWithFollowupFunc の戻り先＝pokefirered item_menu.c・task.c）: ページを替えた次のコマでカーソルを 0 に

//functionrewrites: PrintBagPocketName 081092E8
void HackBagPage_PrintPocketName(void)
{
	u8 pocket = gBagMenuState.pocket;
	FillWindowPixelBuffer(2, 0);
	VanillaBagPrintTextOnWin1CenteredColor0(pocket == POCKET_ITEMS - 1 ? sBagPageNames[CurrentPage()] : sVanillaPocketNames[pocket], pocket);
}

//どうぐ のポケットのままページを替える（一覧を消して数え直し、向き 0 の切り替えで作り直す）
static void ChangePage(u8 taskId, u8 page)
{
	u8* disp = sBagMenuDisplayPtr;
	u16 n;

	PlaySE(SE_BAG_POCKET);
	VanillaSwitchPockets(taskId, 0, FALSE);   //今の一覧を片づける（カーソルの位置も書き戻す）
	VarSet(HACK_VAR_BAG_PAGE, page);
	StoreBagItemCount();                      //新しいページの道具を前へ寄せて数える
	n = GetNumItemsInPocket(POCKET_ITEMS - 1);
	disp[0xA] = n;
	disp[0xD] = (n + 1 > 6) ? 6 : n + 1;
	gBagMenuState.cursorPos[0] = 0;
	gBagMenuState.itemsAbove[0] = 0;
	/* ★ 2026-10-11 ユーザーの動画「持ち物欄の一番下が永遠といけるようになってる・いろんな欄がバグる」:
	   ここで 0 を返すと、素の入力の処理（Task_BagMenu_HandleInput）はそのまま ListMenu_ProcessInput と ListMenuGetScrollAndRow を
	   片づけたばかりの一覧（data[0]）で呼び、古いスクロールの位置で上の 0 を書き戻していた → 新しいページの一覧が数より下までスクロールし、
	   前の名前の残り・崩れた行が出て、そこで道具を選ぶと数の外の枠を書き換えた。
	   左右・L・R のキーを消して古い一覧を動かさず、0 に戻すのは次のコマ（Task_AnimateSwitchPockets が頭でここを呼ぶ）でやり直す */
	gMain.newKeys &= ~(DPAD_LEFT | DPAD_RIGHT | L_BUTTON | R_BUTTON);
	gMain.newAndRepeatedKeys &= ~(DPAD_LEFT | DPAD_RIGHT | L_BUTTON | R_BUTTON);
	gTasks[taskId].data[BAG_TASK_RESET_CURSOR] = TRUE;
}

//functionrewrites: ProcessPocketSwitchInput 08109C60（0 = そのまま・1 = 左のポケットへ・2 = 右のポケットへ）
u8 HackBagPage_ProcessPocketSwitchInput(u8 taskId, u8 pocketId)
{
	u8 lr, page = CurrentPage();
	bool8 animating = gTasks[taskId].func == VanillaTask_AnimateSwitchPockets;

	if (animating && gTasks[taskId].data[BAG_TASK_RESET_CURSOR])   //ページを替えた次のコマ（一覧を作り直す前）
	{
		gTasks[taskId].data[BAG_TASK_RESET_CURSOR] = FALSE;
		gBagMenuState.cursorPos[0] = 0;
		gBagMenuState.itemsAbove[0] = 0;
	}

	if ((sBagMenuDisplayPtr[5] & 0xF) != 0)   //並べ替え・入れ替えの途中
		return 0;
	lr = VanillaGetLRKeysPressed();
	if (JOY_NEW(DPAD_LEFT) || lr == MENU_L_PRESSED)
	{
		if (pocketId == POCKET_ITEMS - 1)
		{
			if (page > 0 && !animating)
				ChangePage(taskId, page - 1);
			return 0;
		}
		PlaySE(SE_BAG_POCKET);
		if (pocketId == POCKET_KEY_ITEMS - 1)
			VarSet(HACK_VAR_BAG_PAGE, BAG_PAGE_TRAINING);   //たいせつなもの の左は いくせい（数え直しは切り替えの中で）
		return 1;
	}
	if (JOY_NEW(DPAD_RIGHT) || lr == MENU_R_PRESSED)
	{
		if (pocketId == POCKET_ITEMS - 1 && page < BAG_PAGE_COUNT - 1)
		{
			if (!animating)
				ChangePage(taskId, page + 1);
			return 0;
		}
		if (pocketId >= POCKET_POKE_BALLS - 1)
			return 0;
		PlaySE(SE_BAG_POCKET);
		return 2;
	}
	return 0;
}
