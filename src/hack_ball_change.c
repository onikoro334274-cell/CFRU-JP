#include "defines.h"
#include "../include/item.h"
#include "../include/pokemon.h"
#include "../include/new/catching.h"

/*
hack_ball_change.c
    クチバのポケモンだいすきクラブの「ボールを入れ替えてくれる人」（2026-10-04）。
    ユーザー「ポケモンの捕まえたときのボールを手持ちの選択したボール変更してくれるNPCつくって配置して」。
    スクリプトは assembly/kanto/kanto_ball_change.s。

    入り: 0x8004 = 手持ちの番号（special 0x9F で選んだ）・0x800E = バッグで選んだ道具（special 0xB1・0 ならやめた）
    出口: LASTRESULT 0 = 入れ替えた（ボールを 1 個使う）・1 = 選ばなかった・2 = ボールではない・3 = もう同じボール
    ★ ボールの種類（MON_DATA_POKEBALL）だけ書き換える。PID・個体値には触らない（色違いはそのまま・6.5）
*/

void HackBallChange_Apply(void)
{
	u16 item = Var800E;
	struct Pokemon *mon;
	u8 ball;

	if (item == ITEM_NONE || Var8004 >= PARTY_SIZE)
	{
		gSpecialVar_LastResult = 1;
		return;
	}
	if (GetPocketByItemId(item) != POCKET_POKEBALLS)
	{
		gSpecialVar_LastResult = 2;
		return;
	}

	mon = &gPlayerParty[Var8004];
	ball = ItemIdToBallId(item);
	if (GetMonData(mon, MON_DATA_POKEBALL, NULL) == ball)
	{
		gSpecialVar_LastResult = 3;
		return;
	}

	if (!RemoveBagItem(item, 1))
	{
		gSpecialVar_LastResult = 1;
		return;
	}
	SetMonData(mon, MON_DATA_POKEBALL, &ball);
	gSpecialVar_LastResult = 0;
}
