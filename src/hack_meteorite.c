#include "defines.h"
#include "../include/item.h"
#include "../include/event_data.h"
#include "../include/pokedex.h"
#include "../include/pokemon.h"
#include "../include/constants/items.h"
#include "../include/constants/species.h"

/*
    いんせき（2026-10-09 ユーザー「デオキシスゲットしたらいんせき手に入るようにして」「ミミッキュ氏のやつ」
    「それっておもくなるから」「入手フラグたてるときでいいよ」）。資料: docs/research/2026-10-09_meteorite.md

    いんせきでデオキシスのフォルムを変える処理は、ミミッキュ氏（@pkhk_BeGiNNeR）の「CFRU-JP_変更ファイル」の機能 3（patches/0008・
    src/party_menu.c の ITEM_METEORITE）。ここでは渡すだけ:
    図鑑の「つかまえた」を初めて立てるとき（CFRU の HandleSetPokedexFlag・src/util.c・patches/0248）に、デオキシスならバッグに入れる。
    捕まえたとき（catching.c）と、CFRU がポケモンを渡すとき（SetMonPokedexFlags）がここを通る。
    ★ 2026-10-09 ユーザー「ロトムもどうように」: ロトムなら ロトムのカタログ（同じ 0008）
    ★ 2026-10-09 ユーザー「持っている場合はママに話しかけるともらえるように」「ポケモンをね」: 前から図鑑に「つかまえた」があるのに
      道具を持っていないとき（この変更より前のセーブなど）は、お母さんが渡す（HackMeteorite_MomCheck・assembly/kanto/kanto_home_scripts.s）
    ★ 状態は持たない（CLAUDE.md 6.13）。だいじなものなので、持っていれば渡さない
*/

static void GiveKeyItem(u16 item)
{
	if (!CheckBagHasItem(item, 1))
		AddBagItem(item, 1);
}

void HackMeteorite_OnCaught(u16 nationalNum)
{
	if (nationalNum == SpeciesToNationalPokedexNum(SPECIES_DEOXYS))
		GiveKeyItem(ITEM_METEORITE);
	else if (nationalNum == SpeciesToNationalPokedexNum(SPECIES_ROTOM))   //★ 2026-10-09 ユーザー「ロトムもどうように」: ロトムのカタログ（patches/0008）
		GiveKeyItem(ITEM_ROTOM_CATALOG);
}

//お母さんに話しかけたとき（callasm）: 0x8004 = いんせきを渡すか・0x8005 = ロトムのカタログを渡すか・LASTRESULT = どちらかを渡すか
static bool8 NeedsItem(u16 species, u16 item)
{
	return GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_CAUGHT) && !CheckBagHasItem(item, 1);
}

void HackMeteorite_MomCheck(void)
{
	Var8004 = NeedsItem(SPECIES_DEOXYS, ITEM_METEORITE);
	Var8005 = NeedsItem(SPECIES_ROTOM, ITEM_ROTOM_CATALOG);
	gSpecialVar_LastResult = Var8004 || Var8005;
}
