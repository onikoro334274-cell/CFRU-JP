#include "defines.h"
#include "../include/list_menu.h"
#include "../include/event_data.h"

/*
 * タマムシデパート屋上の のどが かわいた女の子の選択肢（2026-10-04・assembly/kanto/kanto_thirsty_girl.s から callasm）。
 * CFRU が素の選択肢の表の 0x20〜0x25 番を gMultiChoice に差し替えているので、素の飲み物の並び（ROM に残っている）を
 * gMultiChoice に写す。Var8004 = 素の選択肢の番号（0x20〜0x25）
 */

static const struct { u32 list; u8 count; } sThirstyGirlLists[] =
{
	{0x083A510C, 2},	// 0x20 おいしいみず / やめる
	{0x083A511C, 2},	// 0x21 サイコソーダ / やめる
	{0x083A512C, 3},	// 0x22 おいしいみず / サイコソーダ / やめる
	{0x083A5144, 2},	// 0x23 ミックスオレ / やめる
	{0x083A5154, 3},	// 0x24 おいしいみず / ミックスオレ / やめる
	{0x083A516C, 3},	// 0x25 サイコソーダ / ミックスオレ / やめる
};

void HackThirstyGirl_LoadChoices(void)
{
	u32 i, k = Var8004 - 0x20;
	const struct ListMenuItem* src;

	if (k >= ARRAY_COUNT(sThirstyGirlLists))
		return;
	src = (const struct ListMenuItem*) sThirstyGirlLists[k].list;
	for (i = 0; i < sThirstyGirlLists[k].count; ++i)
	{
		gMultiChoice[i].name = src[i].name;
		gMultiChoice[i].id = i;
	}
	Var8006 = 0;	// 幅は素の表（CFRU の幅の hooks は Var8006 が 0 なら素の幅）
}
