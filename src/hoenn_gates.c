#include "defines.h"
#include "../include/sprite.h"
#include "../include/field_camera.h"
#include "../include/field_player_avatar.h"
#include "../include/fieldmap.h"
#include "../include/event_data.h"
#include "../include/sound.h"
#include "../include/constants/songs.h"

#include "Tables/hoenn_gates.h"

/*
hoenn_gates.c
    ヒワマキジムの回転門（やりたいこと #2）— **第 1 段: 出すだけ**

    ★ 詳細は docs/research/2026-09-22_rotating-gates.md

    ## やること

      ・門 8 個を出す
      ・**押したら回る / 回れない門はぶつかる**（素と同じ）

    ## ★★★ 当たり判定は `GetCollisionAtCoords` のフックで見る

    素 `CheckForRotatingGatePuzzleCollision` は
    **腕があって、しかも回れないときだけ** TRUE（ぶつかる）を返す。
    回れるときは**回して FALSE**なので、主人公はそのまま
    門がどいたマスへ入る（押し戸と同じ動き）。

    ★★ しかも判定は**マスではなく境目**でされている。
      腕は「行 1 と 2 のあいだ」「列 1 と 2 のあいだ」に寝ていて、
      *またぐ*ときだけ止まる（腕の下を横に歩くのは通れる）。
      **だから「腕のマスを通行不可にする」やり方では作れない**
      （4x4 の表を読むと、北へ入るのは行 1・南へ入るのは行 2・
      西へ入るのは列 1・東へ入るのは列 2 と、向きごとに別の場所になる）。

    ★ 2026-09-22 に「十字キーを見て自分で回す」形で試したが、
      **当たり判定が無いので門をすり抜けられた**（実機・ユーザー報告）。
      `hooks` は `LDR/BX` なので**どこへでも届く**（BL の ±4MB は関係ない）。
      素の `GetCollisionAtCoords`(08062F6C) の **08062F78** に置く。

    ★ 素と 1 つだけ違う: 素は**ふつうの当たり判定が「通れる」だったときだけ**
      門を見るが、こちらは**先に**門を見る。門の 4x4 の中は床なので
      実害は無い（そこに人が立っているときだけ、素なら回らない門が回る）。

    ## エメラルドと変えたところ

    ★ **カメラのフックを作らない。** エメラルドは画面が動くたびに
      画面外の門を作り消しする（`RotatingGatePuzzleCameraUpdate`）が、
      それは**スプライト数の節約**のため（トリックハウスは門が多い）。
      **ヒワマキは 8 個だけ**なので、入ったときに全部作って放置でよい。
      `coordOffsetEnabled` がスクロールに追従する。

    ★ **パレットは自前の札で読む。** エメラルドは
      `paletteTag = TAG_NONE` ＋ OAM の `paletteNum = 2` で、
      野外の NPC パレットの 2 番に**たまたま**入っている色を借りている。
      置き場 2〜5 は素の NPC が上書きし合うので危ない（CLAUDE.md #16）。
      絵が使う色は 11/12/13/15 の 4 つだけなので、PNG のパレットから
      取り出して札で読む。`CreateSprite` が札から置き場を引く。

    ★ **状態を持たない**（CLAUDE.md 6.13）。
      「もう出したか」は**自分のコールバックを持つスプライトの数**で見る。
      マップを出ると `ResetSpriteData` で消えるので、勝手に作り直される。
*/

#define HOENN_MAP_OFFSET 7
/* 回す向き（エメラルドの enum と同じ）*/
#define HOENN_GATE_ROT_NONE 0
#define HOENN_GATE_ROT_ACW  1
#define HOENN_GATE_ROT_CW   2
#define HOENN_GATE_ROT_INFO_NONE 255
/* ★ 素の `SE_ROTATING_GATE` は FRLG に無い（効果音は付け替えが要る・6.24）。
   いちばん近いのは鍵の開く音。扉が回る合図として使う。 */
#define HOENN_GATE_SE SE_UNLOCK
#define HOENN_GROUP_BASE 43          /* build_hoenn.py の GROUP_BASE */
#define GATE_SUBPRIORITY 0x94        /* 素と同じ */

/* ★★ 描く側が足す「ずれ」。**`gTotalCameraPixelOffset` とは 40 画素違う**
     （素 FRLG の `UpdateCameraPanning`(0805A6E4):
       `gSpriteCoordOffsetY = gTotalCameraPixelOffsetY - 縦のパン - 8`
      で、野外の縦のパンは 32）。

   2026-09-22 に**これを取り違えて実機で出した**: 画面の上に 10 マスも
   外れた門が「まだ見えている」と判定され、**GBA の OAM の y は 8 ビット**
   なので 256 で回り込んで**画面の下の黒い所に門が出た**
   （ユーザー「ヒワマキジムの変なところに門がある」）。

   ★ `gSpriteCoordOffsetX/Y` は `BPRJ.ld` に無い（6.29 節）ので**番地で読む**。
     番地は `UpdateCameraPanning` が書いている先（0805A6F2〜）から取った。
     `verify_rotating_gates.py` がその命令の並びを毎回確かめる。 */
#define HOENN_GATE_COORD_OFFSET_X (*(s16*) 0x02021B44)
#define HOENN_GATE_COORD_OFFSET_Y (*(s16*) 0x02021B46)
/* 素 `GetCollisionAtCoords` の中の、フックを置く所と戻り先 */
#define HOENN_GATE_COLLISION_RESUME  0x08062F81   /* 潰した 4 命令の次 */
#define HOENN_GATE_COLLISION_BLOCKED 0x08062F9F   /* MOV r0,#1 → 出口 */

void HoennGatesTick(void);
bool8 HoennGateCollisionCheck(struct EventObject* obj, s16 x, s16 y,
                             u8 dir);
static void HoennGateSpriteCallback(struct Sprite* sprite);

static const struct OamData sHoennGateOamLarge =
{
	.affineMode = ST_OAM_AFFINE_NORMAL,
	.objMode = ST_OAM_OBJ_NORMAL,
	.bpp = ST_OAM_4BPP,
	.shape = 0,             /* 正方形 */
	.size = 3,              /* 64x64 */
	.priority = 2,
};

static const struct OamData sHoennGateOamSmall =
{
	.affineMode = ST_OAM_AFFINE_NORMAL,
	.objMode = ST_OAM_OBJ_NORMAL,
	.bpp = ST_OAM_4BPP,
	.shape = 0,             /* 正方形 */
	.size = 2,              /* 32x32 */
	.priority = 2,
};

static const union AnimCmd sHoennGateAnim[] =
{
	ANIMCMD_FRAME(0, 0),
	ANIMCMD_END,
};

static const union AnimCmd* const sHoennGateAnimTable[] =
{
	sHoennGateAnim,
};

static const struct SpriteTemplate sHoennGateTemplate =
{
	.tileTag = HOENN_GATE_TILE_TAG,
	.paletteTag = HOENN_GATE_PAL_TAG,
	.oam = &sHoennGateOamLarge,
	.anims = sHoennGateAnimTable,
	.images = NULL,
	.affineAnims = sHoennGateAffineTable,
	.callback = HoennGateSpriteCallback,
};

static const struct SpritePalette sHoennGatePal =
{
	sHoennGatePalette, HOENN_GATE_PAL_TAG,
};

/* 形ごとの絵。`hoenn_gates.h` が使う形だけ出している */
static const struct SpriteSheet sHoennGateSheets[] =
{
	{sHoennGateTiles_T1, sizeof(sHoennGateTiles_T1),
	 HOENN_GATE_TILE_TAG + HOENN_GATE_SHAPE_T1},
	{sHoennGateTiles_T2, sizeof(sHoennGateTiles_T2),
	 HOENN_GATE_TILE_TAG + HOENN_GATE_SHAPE_T2},
	{sHoennGateTiles_L4, sizeof(sHoennGateTiles_L4),
	 HOENN_GATE_TILE_TAG + HOENN_GATE_SHAPE_L4},
};

/* 門の位置を決める（素 `GetMapCoordsFromSpritePos` と同じ式）。

   ★★★ **作るときに 1 回だけ呼ぶこと。毎フレーム呼んではいけない。**

   `coordOffsetEnabled` のスプライトは、描くときに
   `+ gSpriteCoordOffset`（＝ `gTotalCameraPixelOffset − パン − 8`）される。
   つまり**カメラの動きは描く側が足してくれる**ので、`pos1` は
   「作った時を基準にした置き場所」を**焼き込んだまま**でよい。

   2026-09-22 に毎フレーム作り直したら、`- gTotalCameraPixelOffset` が
   毎回引き直されて**描く側の足し算とちょうど打ち消し合い**、
   門が `(マス − pos) * 16` ＝ **1 マス歩くまで動かない**絵になった
   （実機の動画を 1 コマずつ測った: 背景が 2 画素/コマ動くあいだ門は止まり、
   マスの境目で 16 画素飛ぶ。8 コマ周期の鋸歯）。

   ★ 焼き込んで大丈夫な理由: `CameraMove()` は `gSaveBlock1->pos` を ±1 する
     ときに `gTotalCameraPixelOffset` も ∓16 するので、
     **`(マス − pos) * 16 − gTotalCameraPixelOffset` は動かない**。 */
static void HoennGatePlace(struct Sprite* sprite)
{
	const struct HoennGateSpot* g = &sHoennGateSpots[sprite->data[0]];
	s16 gx = g->x + HOENN_MAP_OFFSET;
	s16 gy = g->y + HOENN_MAP_OFFSET;

	sprite->pos1.x = (gx - gSaveBlock1->pos.x) * 16
			 - (s16) gTotalCameraPixelOffsetX;
	sprite->pos1.y = (gy - gSaveBlock1->pos.y) * 16
			 - (s16) gTotalCameraPixelOffsetY;
}

/* 画面の外へ出た門を消す（素 `RotatingGate_HideGatesOutsideViewport`）。
   ★ 見るのは**描く側と同じずれ**（上の `HOENN_GATE_COORD_OFFSET_*` の
     説明を読むこと）。ここを `gTotalCameraPixelOffset` で見ると 40 画素甘くなり、
     画面の上に外れた門が **OAM の y の回り込み**で下に出る（6.30 節）。 */
static void HoennGateHide(struct Sprite* sprite)
{
	s16 x = sprite->pos1.x + sprite->pos2.x + sprite->centerToCornerVecX
		+ HOENN_GATE_COORD_OFFSET_X;
	s16 y = sprite->pos1.y + sprite->pos2.y + sprite->centerToCornerVecY
		+ HOENN_GATE_COORD_OFFSET_Y;

	sprite->invisible = (x > 240 + 16 - 1 || x + 64 < -16
			     || y > 160 + 16 - 1 || y + 64 < -16);
}

/* 素 `SpriteCallback_RotatingGate`。`data[1]` に向きが入っていたら回す。
   `data[2]` は回り**始める前**の向き（アフィンアニメの番号に使う）。 */
static void HoennGateSpriteCallback(struct Sprite* sprite)
{
	u8 rot = sprite->data[1];
	u8 orient = sprite->data[2];

	HoennGateHide(sprite);

	if (rot == HOENN_GATE_ROT_ACW)
	{
		PlaySE(HOENN_GATE_SE);
		StartSpriteAffineAnim(sprite, orient + 4);
	}
	else if (rot == HOENN_GATE_ROT_CW)
	{
		PlaySE(HOENN_GATE_SE);
		StartSpriteAffineAnim(sprite, orient + 8);
	}
	sprite->data[1] = HOENN_GATE_ROT_NONE;
}

/* 門 `gateId` のスプライト（無ければ NULL）*/
static struct Sprite* HoennGateSprite(u8 gateId)
{
	u8 i;

	for (i = 0; i < MAX_SPRITES; ++i)
	{
		if (gSprites[i].inUse
		    && gSprites[i].callback == HoennGateSpriteCallback
		    && gSprites[i].data[0] == gateId)
			return &gSprites[i];
	}
	return NULL;
}

/* ★★ いまの向きは**一時変数 VAR_TEMP_0〜3 の 8 バイト**に置く（エメラルドと同じ・pokeemerald src/rotating_gate.c の
   GetVarPointer(VAR_TEMP_0)。2026-09-29 ユーザー「ヒワマキジムのギミックがトレーナー戦後もとに戻ってしまう」）。
   以前はスプライトの data[3] だけに持っていたので、戦闘・メニューから戻って野外のスプライトが作り直されると最初の向きに戻った。
   一時変数は**地図に入るたびに 0 に戻り、戦闘・メニューから戻っても残る**（pokefirered src/event_data.c ClearTempFieldEventData は
   ワープ・地図の継ぎ目でだけ呼ばれる）。エメラルドは地図に入るたびに初期化するので、こちらは「向き + 1・0 は最初の向き」で持つ */
#define HOENN_GATE_STATE_VAR 0x4000          /* VAR_TEMP_0（素 FRLG も同じ番号） */

static u8* HoennGateState(void)
{
	return (u8*) GetVarPointer(HOENN_GATE_STATE_VAR);
}

static u8 HoennGateOrientation(u8 gateId)
{
	u8 v = HoennGateState()[gateId];

	return v ? (u8) (v - 1) : sHoennGateSpots[gateId].orient;
}

/* 素 `RotatingGate_HasArm`。腕は**世界の方角**で数え、向きで引き戻す */
static bool8 HoennGateHasArm(u8 gateId, u8 armInfo)
{
	u8 arm = armInfo / 2;
	u8 isLong = armInfo % 2;
	u8 rel = (u8) ((arm - HoennGateOrientation(gateId) + 4) % 4);

	return sHoennGateArmLayout[sHoennGateSpots[gateId].shape][rel * 2 + isLong];
}

/* 素 `RotatingGate_CanRotate`。腕が通る道に壁があったら回れない。
   ★ 素は `== 1` で見ているが decomp の BUGFIX のとおり **0 以外**で見る
     （通行の値は 0〜3 で、1 以外も壁）。 */
static bool8 HoennGateCanRotate(u8 gateId, u8 rot)
{
	const s8 (*pos)[2];
	u8 orient = HoennGateOrientation(gateId);
	u8 shape = sHoennGateSpots[gateId].shape;
	s16 gx = sHoennGateSpots[gateId].x + HOENN_MAP_OFFSET;
	s16 gy = sHoennGateSpots[gateId].y + HOENN_MAP_OFFSET;
	u8 i, j;

	if (rot == HOENN_GATE_ROT_ACW)
		pos = sHoennGateArmPosAcw;
	else if (rot == HOENN_GATE_ROT_CW)
		pos = sHoennGateArmPosCw;
	else
		return FALSE;

	for (i = 0; i < 4; ++i)
	{
		for (j = 0; j < 2; ++j)
		{
			u8 k = (u8) (2 * ((orient + i) % 4) + j);

			if (sHoennGateArmLayout[shape][2 * i + j]
			    && MapGridIsImpassableAt(gx + pos[k][0], gy + pos[k][1]))
				return FALSE;
		}
	}
	return TRUE;
}

/* 素 `RotatingGate_RotateInDirection` */
static void HoennGateRotate(u8 gateId, u8 rot)
{
	struct Sprite* s = HoennGateSprite(gateId);
	u8 orient = HoennGateOrientation(gateId);

	if (s == NULL)
		return;
	s->data[1] = rot;
	s->data[2] = orient;            /* 回り始める前の向き */
	if (rot == HOENN_GATE_ROT_ACW)
		orient = (u8) (orient ? orient - 1 : 3);
	else
		orient = (u8) ((orient + 1) % 4);
	s->data[3] = orient;
	HoennGateState()[gateId] = orient + 1;
}

/* 素 `CheckForRotatingGatePuzzleCollision` そのもの。
   回せたら回して FALSE（通す）、腕があって回せないときだけ TRUE。 */
static bool8 HoennGateBlocks(u8 dir, s16 x, s16 y)
{
	u8 i;

	if (dir < DIR_SOUTH || dir > DIR_EAST)
		return FALSE;
	for (i = 0; i < HOENN_GATE_COUNT; ++i)
	{
		s16 gx = sHoennGateSpots[i].x + HOENN_MAP_OFFSET;
		s16 gy = sHoennGateSpots[i].y + HOENN_MAP_OFFSET;

		if (gx - 2 <= x && x <= gx + 1 && gy - 2 <= y && y <= gy + 1)
		{
			u8 cx = (u8) (x - gx + 2);
			u8 cy = (u8) (y - gy + 2);
			u8 info = sHoennGateRotInfo[dir][cy * 4 + cx];
			u8 rot, arm;

			if (info == HOENN_GATE_ROT_INFO_NONE)
				continue;
			rot = (u8) ((info & 0xF0) >> 4);
			arm = (u8) (info & 0x0F);
			if (!HoennGateHasArm(i, arm))
				continue;
			if (!HoennGateCanRotate(i, rot))
				return TRUE;            /* 回せない ＝ ぶつかる */
			HoennGateRotate(i, rot);
			return FALSE;                   /* 回した ＝ 通す */
		}
	}
	return FALSE;
}

static bool8 HoennGateOnPuzzleMap(void)
{
	return gSaveBlock1->location.mapGroup
	       == HOENN_GATE_MAP_GROUP + HOENN_GROUP_BASE
	    && gSaveBlock1->location.mapNum == HOENN_GATE_MAP_NUM;
}

static u8 HoennGateCount(void)
{
	u8 i, n = 0;

	for (i = 0; i < MAX_SPRITES; ++i)
	{
		if (gSprites[i].inUse
		    && gSprites[i].callback == HoennGateSpriteCallback)
			++n;
	}
	return n;
}

static void HoennGateCreateAll(void)
{
	struct SpriteTemplate t = sHoennGateTemplate;
	u8 i;

	for (i = 0; i < ARRAY_COUNT(sHoennGateSheets); ++i)
	{
		if (GetSpriteTileStartByTag(sHoennGateSheets[i].tag) == 0xFFFF)
			LoadSpriteSheet(&sHoennGateSheets[i]);
	}
	if (IndexOfSpritePaletteTag(HOENN_GATE_PAL_TAG) == 0xFF)
		LoadSpritePalette(&sHoennGatePal);

	for (i = 0; i < HOENN_GATE_COUNT; ++i)
	{
		const struct HoennGateSpot* g = &sHoennGateSpots[i];
		u8 id;

		t.tileTag = HOENN_GATE_TILE_TAG + g->shape;
		t.oam = sHoennGateIsSmall[g->shape] ? &sHoennGateOamSmall
						    : &sHoennGateOamLarge;
		id = CreateSprite(&t, 0, 0, GATE_SUBPRIORITY);
		if (id == MAX_SPRITES)
			return;         /* 枠が無い。次のフレームでまた試す */

		gSprites[id].data[0] = i;
		gSprites[id].data[1] = HOENN_GATE_ROT_NONE;
		gSprites[id].data[3] = HoennGateOrientation(i);   /* いまの向き（一時変数から・戦闘のあとも残る）*/
		gSprites[id].coordOffsetEnabled = TRUE;
		HoennGatePlace(&gSprites[id]);     /* ここだけ。毎フレームは不可 */
		HoennGateHide(&gSprites[id]);
		StartSpriteAffineAnim(&gSprites[id], HoennGateOrientation(i));
	}
}

/* `src/hoenn_steps.c` の毎フレームの処理から呼ばれる */
void HoennGatesTick(void)
{
	if (!HoennGateOnPuzzleMap())
		return;
	/* ★ 1 つでも出ていれば触らない（作りかけで暴れないように）。
	     置き直しと画面外の始末はスプライトのコールバックがやる。 */
	if (HoennGateCount() != 0)
		return;
	HoennGateCreateAll();
}

/* `assembly/hoenn/gates_hook.s` から呼ばれる。
   素 `GetCollisionAtCoords`(08062F6C) の途中に置いたフックの中身。

   ★ **主人公のときだけ**見る。素は物体なら何でも見るが、
     こちらは連れ歩き（#16）や NPC に回されると困る。 */
bool8 HoennGateCollisionCheck(struct EventObject* obj, s16 x, s16 y, u8 dir)
{
	if (!HoennGateOnPuzzleMap())
		return FALSE;
	if (obj != &gEventObjects[gPlayerAvatar->eventObjectId])
		return FALSE;
	return HoennGateBlocks(dir, x, y);
}
