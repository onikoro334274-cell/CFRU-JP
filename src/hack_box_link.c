#include "defines.h"
#include "../include/item.h"
#include "../include/item_use.h"
#include "../include/script.h"
#include "../include/task.h"

/*
hack_box_link.c
    ボックスリンク（ITEM_BOX_LINK 0x342・たいせつなもの・2026-10-04）。
    ユーザー「ボックス操作できるたいせつなものを作成してほしい」→「回復機能付きでいいや　ポケモンリーグとかで使用できなければいいや」。
    マサキがサントアンヌ号のチケットのあとに渡す（assembly/kanto/kanto_box_link.s）。

    - つかう → バッグを閉じて素の「ポケモンあずかりシステム」（special 0x3C）。素のパソコンと同じ（あずけると回復する）
    - ポケモンリーグ（カントーの四天王・チャンピオン・殿堂・ホウエンの四天王・チャンピオン・通路・殿堂）と
      バトルフロンティアの挑戦中の部屋・トレーナーヒルでは つかえない（バッグで断る・受付やポケセンでは使える）

    ★ 2026-10-04 最初は「アイテムからは元気な手持ちしかあずけられない」にしたが、ユーザー判断で回復ありに変えた。
      CFRU のボックスは 1 匹 58 バイトに縮めて持ち、HP・状態異常・PP が入らない（include/new/pokemon_storage_system.h）。
*/

extern const u8 gText_BoxLink_CantUseHere[];
extern const u8 EventScript_BoxLink[];

#define BoxLinkDisplayCannotUse ((void (*)(u8, u8, u8, const u8 *))(0x080A2398 + 1))

#define KANTO_LEAGUE_GROUP 1          //pokefirered map_groups.json: PokemonLeague_LoreleisRoom〜HallOfFame = 1.75〜1.80
#define KANTO_LEAGUE_FIRST 75
#define KANTO_LEAGUE_LAST 80
#define HOENN_LEAGUE_GROUP (16 + 43)  //エメラルドの群 16（EverGrandeCity_*）+ HOENN_GROUP_BASE
#define HOENN_FRONTIER_GROUP (26 + 43)  //エメラルドの群 26（gMapGroup_SpecialArea）

//挑戦中の部屋（受付・ポケセン・外は使える）: タワー 6〜8・15〜17／ドーム 19〜21／パレス 23〜24／ピラミッド 26〜27／
//アリーナ 29〜30／ファクトリー 32〜33／チューブ 35〜39／トレーナーヒル 61〜65・88（エレベーター）
static const u8 sBoxLinkFrontierRooms[] = {6, 7, 8, 15, 16, 17, 19, 20, 21, 23, 24, 26, 27, 29, 30, 32, 33,
                                           35, 36, 37, 38, 39, 61, 62, 63, 64, 65, 88};

static bool8 BoxLinkForbiddenHere(void)
{
	u8 group = gSaveBlock1->location.mapGroup;
	u8 num = (u8) gSaveBlock1->location.mapNum;

	if (group == KANTO_LEAGUE_GROUP)
		return num >= KANTO_LEAGUE_FIRST && num <= KANTO_LEAGUE_LAST;
	if (group == HOENN_LEAGUE_GROUP)
		return num <= 9 || num == 11;  //四天王・チャンピオンの部屋・通路 1〜5・殿堂（1F の受付・ポケセン・2F は使える）
	if (group == HOENN_FRONTIER_GROUP)
	{
		u32 i;

		for (i = 0; i < NELEMS(sBoxLinkFrontierRooms); ++i)
		{
			if (sBoxLinkFrontierRooms[i] == num)
				return TRUE;
		}
	}
	return FALSE;
}

static void Task_BoxLinkField(u8 taskId)
{
	ScriptContext1_SetupScript(EventScript_BoxLink);
	DestroyTask(taskId);
}

void FieldUseFunc_BoxLink(u8 taskId)
{
	if (BoxLinkForbiddenHere())
	{
		//素の PrintNotTheTimeToUseThat（080A23E4）と同じ道: 080A2398(taskId, 登録して フィールドで使ったか, 字 4, 文)。
		//バッグなら DisplayItemMessageInBag、フィールドなら DisplayItemMessageOnField に分かれる（字 2 だと BW 風バッグで見えなかった）
		BoxLinkDisplayCannotUse(taskId, gTasks[taskId].data[3], 4, gText_BoxLink_CantUseHere);
		return;
	}
	sItemUseOnFieldCB = Task_BoxLinkField;
	SetUpItemUseOnFieldCallback(taskId);
}
