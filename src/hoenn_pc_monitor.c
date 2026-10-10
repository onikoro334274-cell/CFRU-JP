#include "defines.h"
#include "../include/sprite.h"

#include "Tables/hoenn_pc_monitor.h"

/*
hoenn_pc_monitor.c
    ポケモンセンターの回復モニターをホウエンの絵にする（やりたいこと #2）。

    ★ 詳細は docs/research/2026-09-21_pc-monitor.md

    ## なぜ要るか（2026-09-21 にユーザーが実機で発見）

    「回復時にモニターのアニメーションがカントーのものがでるので
    ホウエンと差異がある」。

    ホウエンのポケモンセンターは**地形だけ**移植してあるので回復機は
    エメラルドの絵。ところが回復の演出は素 FRLG の `FldEff_PokecenterHeal`
    が出しており、**カントーの縦長モニター**を決め打ちで置く。
    ホウエンの回復機は**横長の小さい画面**なので形が合わない。

    ## 直し方 — フックも `functionrewrites` も使わない

    素のスプライトは `images`（コマの絵の表）を**スプライト構造体に
    持っている**。だから**毎フレーム、素の表を指しているスプライトを
    見つけて貼り替える**だけでよい。ROM は 1 バイトも触らない。

      素の表 `sPicTable_PokecenterMonitor` = `0x08392D38`
      （`tools/pcmonitor/build_pc_monitor.py` が毎回 ROM で確かめる）

    ★ 絵は 32x16 が 4 コマで**素とまったく同じ形**なので、
      OAM もアニメも座標も素のまま使える。
      エメラルドの絵は 24x16 なので**左に寄せて**置いてあり、
      素の x=128（中心）で 112〜135 に出る ＝ エメラルドが x=124 に
      置くのと同じ位置になる。

    ★ 呼び元は `src/hoenn_steps.c` の毎フレームの処理。
      書き換え可能な static は使っていない（CLAUDE.md 6.13）。
*/

/* ホウエンの地図は素 FRLG の 43 グループのうしろ（build_hoenn.py GROUP_BASE）*/
#define HOENN_PC_MONITOR_GROUP_BASE 43

void HoennPcMonitorTick(void);

void HoennPcMonitorTick(void)
{
	u8 i;

	if (gSaveBlock1->location.mapGroup < HOENN_PC_MONITOR_GROUP_BASE)
		return;

	for (i = 0; i < MAX_SPRITES; ++i)
	{
		if (!gSprites[i].inUse)
			continue;
		if (gSprites[i].images
		    == (const struct SpriteFrameImage*) HOENN_PC_MONITOR_PICTABLE)
			gSprites[i].images = sHoennPcMonitorPics;
	}
}
