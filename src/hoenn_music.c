#include "defines.h"
#include "../include/overworld.h"

#include "Tables/hoenn_music.h"

/*
hoenn_music.c
    118 ばんどうろの曲を「西／東」で振り分ける（やりたいこと #2）。

    ## なぜ要るか（2026-09-22 にユーザーが実機で発見）

    「音楽がセキチクシティになっている」（118 ばんどうろ）。

    エメラルドのマップヘッダの曲番号には**番兵**が入っていることがある。
    118 ばんどうろは `MUS_ROUTE118`(0x7FFF) で、これは曲ではなく
    「**この地図は左右で曲が違う**」という合図。エメラルドは

        pokeemerald/src/overworld.c:1096  GetCurrLocationDefaultMusic()
            if (music != MUS_ROUTE118) return music;
            return (pos.x < 24) ? MUS_ROUTE110 : MUS_ROUTE119;

    と振り分けている。**素 FRLG の `GetLocationMusic` にはこの分岐が無い**
    ので、こちらの生成器は「移植していない曲」とみなして
    カントーの曲（`DEF_MUSIC` = セキチクシティ）に落としていた。

    ## 直し方 — `GetLocationMusic`(0805562C) を丸ごと置き換える

    素の中身は「ヘッダの曲番号をそのまま返す」だけの 32 バイト。

        0805562C  push {lr}
                  ...  warp->mapGroup / warp->mapNum を取り出す
                  bl   Overworld_GetMapHeaderByGroupAndId
                  ldrh r0,[r0,#0x10]
                  pop  {r1} / bx r1
        0805564C  ← 次の関数（GetCurrLocationDefaultMusic）

    ★ `functionrewrites` は入口に **20 バイト**書く（CLAUDE.md 6.22）。
      ここは 32 バイトあり、**0805562C より後ろに BL の飛び先が無い**
      ことを確かめてある（`tools/bgm/verify_hoenn_bgm.py` が毎回見る）。

    ★ 呼び元は 2 つだけ（`GetCurrLocationDefaultMusic` と
      `GetWarpDestinationMusic`）。どちらも番兵以外はそのまま通すので、
      **カントーの挙動は 1 ビットも変わらない。**

    ★ 振り分けに使う座標は、エメラルドと同じく
      **`gSaveBlock1->pos`（いま立っている場所）**。
      エメラルドの `GetWarpDestinationMusic` のほうは「いまカナズミか」で
      分けているが、118 ばんどうろにワープは 1 本も無い
      （道の接続だけで入る）ので、こちらの分岐は使われない。

    ★ 曲が切り替わるのは**地図をまたいだとき**だけで、
      道の途中で x=24 を越えても変わらない。これはエメラルドも同じ
      （どちらも地図の読み込み時にしか評価しない）。
*/

u16 HoennGetLocationMusic(struct WarpData* warp)
{
	struct MapHeader* header =
		Overworld_GetMapHeaderByGroupAndId((u16) warp->mapGroup,
		                                   (u16) warp->mapNum);
	u16 music = header->music;

	if (music == HOENN_MUS_ROUTE118_MARK)
	{
		if (gSaveBlock1->pos.x < HOENN_MUS_ROUTE118_SPLIT_X)
			return HOENN_MUS_ROUTE118_WEST;
		return HOENN_MUS_ROUTE118_EAST;
	}

	return music;
}
