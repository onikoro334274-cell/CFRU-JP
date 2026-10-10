#include "defines.h"
#include "../include/constants/items.h"
#include "Tables/wild_mega_stones.h"

/*
    野生の進化前のポケモンにメガストーンを持たせる（2026-10-01）

    ユーザー「現在野生に登録してある進化前に進化後がメガ進化ポケモンに対応するメガストーンを持っているようにしてほしい」。
    表（src/Tables/wild_mega_stones.h）は tools/megas/build_wild_mega_stones.py が今の野生の表から作る。
    src/build_pokemon.c の GenerateWildMonHeldItem（野生の持ち物・ずかんナビも通る）が最初にここを引き、
    表にある種族なら必ずそのメガストーンを持たせる（ふだんの持ち物の抽選はしない）。
*/

u16 WildMegaStoneFor(u16 species)
{
	u32 lo = 0, hi = WILD_MEGA_STONE_COUNT;

	while (lo < hi)          /* 表は種族の番号の順 */
	{
		u32 mid = (lo + hi) / 2;
		if (sWildMegaStones[mid][0] == species)
			return sWildMegaStones[mid][1];
		if (sWildMegaStones[mid][0] < species)
			lo = mid + 1;
		else
			hi = mid;
	}
	return ITEM_NONE;
}
