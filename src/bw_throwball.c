#include "defines.h"
#include "../include/sprite.h"
#include "../include/pokemon.h"
#include "../include/battle.h"
#include "../include/task.h"
#include "../include/new/catching.h"
#include "bw_throwball_pos.h"

/*
bw_throwball.c
	#20 投げる前の「手に持っているボール」を出す。

★★ **なぜ要るか**

	BW の後ろ姿は**手にボールを握った絵**になっている。
	素 FRLG の後ろ姿は持っておらず、**ボールはゲームが別のスプライトで描く**
	（`StartSendOutAnim` -> `DoPokeballSendOutAnimation` が (48,70) に作る）。
	そのまま入れるとボールが 2 つになるので、
	`tools/bwplayer/build_bw_trainer.py` が**絵のボールを消している**。

	ただし消しただけだと、振りかぶっている間は手が空になる。
	そこで**振りかぶり〜離すまでの 32 フレームだけ**、
	本物のボール（そのポケモンが入っているボール）を手の位置に出す。

★ **時刻**（実機の動画 2 本を 1 コマずつ測って確定）

	投げアニメの開始から **32 フレーム目**でゲームのボールが手を離れる。
	こちらのボールはその手前で消す。`BW_HELD_BALL_FRAMES`。

★★ **入れ方（2026-09-21 に直した）**

	投げの開始は **`PlayerHandleIntroTrainerBallThrow`**。
	（`PlayerHandleTrainerSlideBack` は会話のあとに下がる**別の処理**で、
	 最初そちらに入れてしまい**ボールが出なかった**。実機で判明。）

	そこで作られる **`Task_StartSendOutAnim` が 31 フレーム数えてから**
	本物のボールを作る。**この数え上げがそのまま「持っている時間」**なので、
	CFRU が差し替えている `src/multi.c` の `Task_PlayerStartSendOutAnim` の
	`data[1] == 0` のときに、ここの `BwCreateHeldBall()` を呼ぶ。
	★ 素のコードは 1 バイトも触らない（`functionrewrites` も `hooks` も要らない）。

★★ **ボールの種類はそのまま使う。**
	CFRU は `MON_DATA_POKEBALL` に「道具の番号」ではなく**ボールの種類**を入れる。
	`ItemIdToBallId` を通すと二重変換になる（`src/bw_summary_screen.c` と同じ）。
	表は 0〜27 の 28 件なので、外を引かない見張りを置く（6.23）。
*/

#define BW_HELD_BALL_FRAMES 30      /* 32 フレーム目に本物が出る。その手前で消す */
/* ★★ **主人公より「うしろ」に出す。**
	素の並び順は **subpriority が小さいほど手前**（投げたボールは 29、
	主人公の後ろ姿は 30 で、ボールが手前を飛ぶ）。
	最初 29 にしたら**ボールが指の上に乗って「持っている」ように見えなかった**
	（実機 2026-09-21「持っているというよりボールが上に描画されている」）。
	31 にすると、**消した跡の穴からボールが覗き、指がその手前に重なる**。 */
#define BW_HELD_BALL_SUBPRIORITY 31

#define SetSpritePrimaryCoordsFromSecondaryCoords \
	((void (*)(struct Sprite *))(0x080748A0 | 1))

extern const struct SpriteTemplate gBallSpriteTemplates[];

/* data[] の使い方 */
#define tPlayerSpriteId data[0]
#define tTimer          data[1]
#define tDx             data[2]
#define tDy             data[3]

/* 出しているボールを毎フレーム手の位置へ置き直す。
   ★ 主人公は投げながら左へ滑っていくので、**位置は毎フレーム読み直す**。 */
static void BwHeldBallCallback(struct Sprite *sprite)
{
	struct Sprite *player;

	if (sprite->tPlayerSpriteId >= MAX_SPRITES)
	{
		DestroySprite(sprite);
		return;
	}

	player = &gSprites[sprite->tPlayerSpriteId];

	if (++sprite->tTimer > BW_HELD_BALL_FRAMES
	 || !player->inUse
	 || player->invisible)
	{
		DestroySprite(sprite);
		return;
	}

	sprite->pos1.x = player->pos1.x + player->pos2.x + sprite->tDx;
	sprite->pos1.y = player->pos1.y + player->pos2.y + sprite->tDy;
	sprite->invisible = player->invisible;
}

void BwCreateHeldBall(u8 playerSpriteId)
{
	u16 ballType;
	u8 spriteId;
	struct Pokemon *mon;
	u8 i;

	/* これから出すポケモン = 先頭のタマゴでない生きている子 */
	mon = NULL;
	for (i = 0; i < PARTY_SIZE; ++i)
	{
		struct Pokemon *m = &gPlayerParty[i];

		if (GetMonData(m, MON_DATA_SPECIES2, NULL) == SPECIES_NONE)
			break;
		if (GetMonData(m, MON_DATA_IS_EGG, NULL)
		 || GetMonData(m, MON_DATA_HP, NULL) == 0)
			continue;
		mon = m;
		break;
	}
	if (mon == NULL)
		return;

	/* ★ CFRU はボールの「種類」をそのまま入れている。変換しない */
	ballType = GetMonData(mon, MON_DATA_POKEBALL, NULL);
	if (ballType > BALL_TYPE_DYNAMAX_BALL)       /* 表の外を引かない（6.23）*/
		ballType = BALL_TYPE_POKE_BALL;

	LoadBallGfx(ballType);
	spriteId = CreateSprite(&gBallSpriteTemplates[ballType], 0, 0,
	                        BW_HELD_BALL_SUBPRIORITY);
	if (spriteId >= MAX_SPRITES)
		return;

	gSprites[spriteId].tPlayerSpriteId = playerSpriteId;
	gSprites[spriteId].tTimer = 0;
	if (gSaveBlock2->playerGender != 0)
	{
		gSprites[spriteId].tDx = BW_HELD_BALL_FEMALE_DX;
		gSprites[spriteId].tDy = BW_HELD_BALL_FEMALE_DY;
	}
	else
	{
		gSprites[spriteId].tDx = BW_HELD_BALL_MALE_DX;
		gSprites[spriteId].tDy = BW_HELD_BALL_MALE_DY;
	}
	gSprites[spriteId].callback = BwHeldBallCallback;
	BwHeldBallCallback(&gSprites[spriteId]);    /* 1 フレーム目から手の位置に */
}

#undef tPlayerSpriteId
#undef tTimer
#undef tDx
#undef tDy

/*
★★ 最初のライバル戦（2026-09-25 実機「最初のライバル戦だけ手にボールが無い」）

	最初のライバル戦は `BATTLE_TYPE_FIRST_BATTLE` で、主人公側を**オーキド用の操作**
	（pokefirered `battle_controller_oak_old_man.c`）が動かす。投げの開始
	`OakOldManHandleIntroTrainerBallThrow` が作るタスクは CFRU の `Task_PlayerStartSendOutAnim`
	ではなく、**オーキド用の `Task_StartSendOutAnim`（BPRJ `080EC054`）**。
	中身は同じ「data[1] を 31 まで数えてから本物のボールを出す」形（`CMP r0,#0x1E`）。

	そこで、そのタスクの番地を指すリテラル語 **`080EBE74`**（ROM 全体でこの 1 か所）を
	repoints でこの関数へ向け、**1 フレーム目に手のボールを出してから素のタスクを呼ぶ**。
	★ repoints の 3 列目 `1` は Thumb のビット（nm の番地は偶数のまま）。
	★ 老人の捕獲の説明（BATTLE_TYPE_OLD_MAN_TUTORIAL）はこのタスクを作らない（else 側）ので関係しない。
*/
#define OakTask_StartSendOutAnim ((void (*)(u8))(0x080EC054 | 1))

void BwOakTask_StartSendOutAnim(u8 taskId)
{
	if (gTasks[taskId].data[1] == 0)
		BwCreateHeldBall(gBattlerSpriteIds[gTasks[taskId].data[0]]);
	OakTask_StartSendOutAnim(taskId);
}
