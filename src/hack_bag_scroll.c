#include "defines.h"
#include "../include/item_menu.h"
#include "../include/new/item.h"

/*
 * バッグを開いたときの一覧の位置を必ず範囲に収める（2026-10-03）。
 *
 * ユーザーの動画（mGBA）「たいせつなもので とじる に合わせて閉じる → 新しいたいせつなものをもらう → バッグを開くと
 * とじる の下に文字が出て、カーソルが とじる の下へ進む（11 行目で固まる）」。
 *
 * 素の UpdatePocketScrollPositions（BPRJ 0810957C・pokefirered item_menu.c）は、カーソルの行（itemsAbove）が
 * 3 より下なら「行を 1 つ上げて一覧を 1 つ下げる」を、一覧の下端がちょうど とじる に合う（==）まで繰り返す。
 * 開く前の位置がもう範囲の外だと、ちょうど合うことがなく下げすぎる（== で止めるので行き過ぎを戻さない）。
 * CFRU の PocketCalculateInitialCursorPosAndItemsAbove（src/item.c）も行を「道具の数」にしてしまい、窓の行数を超えうる。
 * → 素の繰り返しは同じに残し、最後に必ず
 *      一覧のずれ + 窓の行数 ≤ 道具の数 + 1（とじる まで）・行 < 窓の行数・ずれ + 行 ≤ 道具の数
 *    に収める。functionrewrites で素の関数を丸ごと置き換える（中の hooks の OpenBagBugFix3 81095B8 は通らなくなる）。
 *
 * 状態は持たない（6.13）。
 */

#define BAG_POCKETS 3
#define sBagMenuDisplayPtr (*(u8 **)0x0203AC88)    /* BPRJ.ld の sBagMenuDisplay（struct BagMenuAlloc *） */
#define BAG_MAX_SHOWED(p) (sBagMenuDisplayPtr[0x0D + (p)])   /* maxShowed[3]（素の 08109928 の add #13） */

static void ClampPocket(u8 p)
{
	u16 total = GetNumItemsInPocket(p) + 1;   /* とじる まで */
	u16 shown = BAG_MAX_SHOWED(p);
	u16 *scroll = &gBagMenuState.cursorPos[p];
	u16 *row = &gBagMenuState.itemsAbove[p];

	if (shown == 0)
		shown = 1;
	if (shown > total)
		shown = total;
	if (*scroll + shown > total)
		*scroll = total - shown;
	if (*row >= shown)
		*row = shown - 1;
	if (*scroll + *row >= total)
		*row = total - 1 - *scroll;
}

void HackBag_UpdatePocketScrollPositions(void)
{
	u8 p;

	for (p = 0; p < BAG_POCKETS; ++p)
	{
		u16 total = GetNumItemsInPocket(p) + 1;

		ClampPocket(p);
		/* 素と同じ: カーソルが 4 行目より下なら、一覧の下端が とじる に届くまで一覧を下げて行を上げる */
		while (gBagMenuState.itemsAbove[p] > 3
		    && gBagMenuState.cursorPos[p] + BAG_MAX_SHOWED(p) < total)
		{
			--gBagMenuState.itemsAbove[p];
			++gBagMenuState.cursorPos[p];
		}
		ClampPocket(p);
	}
}
