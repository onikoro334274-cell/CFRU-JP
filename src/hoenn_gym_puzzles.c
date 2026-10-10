#include "defines.h"
#include "../include/fieldmap.h"
#include "../include/field_camera.h"
#include "../include/sound.h"
#include "../include/constants/songs.h"
#include "../include/constants/metatile_behaviors.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/constants/event_object_movement_constants.h"

#include "Tables/hoenn_gym_puzzles.h"

/*
hoenn_gym_puzzles.c
    ホウエンのジムの「仕掛け」（やりたいこと #2）。

    ★ 詳細は docs/research/2026-09-21_gym-puzzles.md

    ## なぜ要るか（2026-09-21 にユーザーが実機で発見）

    エメラルドのジムの仕掛けは `special` が**メタタイル番号を書き換えて**
    実現している。素 FRLG にその処理が無いので
    `build_hoenn_scripts.py` が命令ごと落としており、

      キンセツ … スイッチを踏んでも**バリアが動かない**
      ルネ     … **氷が割れない**

    どちらも**ジムを抜けられない**（詰み）。そこで C で書き直した。

    ## 呼ばれ方

    `special` の表を広げる代わりに **`callnative`**（命令 35）を使う。
    `build_hoenn_scripts.py` の `SPECIAL_MISSING` が
    `special XXX` を `callnative HoennNative_XXX` に差し替える。
    ★ `callnative` に渡すのは**関数の番地 + 1**（Thumb の印）。
      `scripts.s` 側で `.4byte HoennNative_… + 1` と書いてある。

    ## 番号は必ず詰め直したものを使う

    メタタイル番号はタイルセットごとに詰め直してあるので、
    エメラルドの番号をそのまま書くと**まったく別の絵**になる（6.19 節）。
    `src/Tables/hoenn_gym_puzzles.h` が
    `tools/rom/build_hoenn_gym_puzzles.py` の生成物で、詰め直し済み。

    ★ 書き換えるメタタイルは**マップのどのマスにも出てこない**ので、
      `build_hoenn_scripts.PUZZLE_META` から `hoenn_plan.py` に渡して
      枠に残してもらっている。忘れると番号が付かず仕掛けが動かない。

    ★ 書き込み可能な static は無い（6.13）。
*/

#define MT_ID_MASK 0x03FF

/* ★★★ `ScriptMovement_StartObjectMovementScript` は **番地で呼ぶ**。
   CFRU の `include/script_movement.h` には宣言だけあって中身も番地も無く、
   `BPRJ.ld` にも載っていない（名前は英語版由来で `sub_80D444C` などと
   ずれている）。そのままだとリンクで
   `undefined reference to ScriptMovement_StartObjectMovementScript` になる
   （2026-09-21 に実際に出た）。

   番地の出し方（推測ではなく実測）:
     イベント命令表 `0x08162CC4` の **79 番 `applymovement`**（`0806AAC1`）を
     逆アセンブルし、引数をそろえたあとの `BL` を見る。
     `applymovementat`（80 番）も**同じ番地**を呼んでいることで裏を取った。
   引数は decomp と同じ `(localId, mapNum, mapGroup, 動作の並び)`
     r1 = [gSaveBlock1 + 5] = mapNum / r2 = [+4] = mapGroup で一致。

   ★ `verify_gym_scripts.py` が素 ROM の先頭 4 半語を毎回突き合わせる。 */
#define HOENN_SCRIPT_MOVEMENT_START 0x08096E60
#define HoennScriptMovementStart 	((bool8 (*)(u8, u8, u8, const u8 *))(HOENN_SCRIPT_MOVEMENT_START + 1))

void HoennNative_PetalburgGymOpenRoomDoors(void);
void HoennNative_MauvilleGymPressSwitch(void);
void HoennNative_MauvilleGymSetDefaultBarriers(void);
void HoennNative_MauvilleGymDeactivatePuzzle(void);
void HoennSootopolisIceStep(s16 x, s16 y, u8 behavior);

static void SetAndDraw(int x, int y, u16 metatile)
{
	MapGridSetMetatileIdAt(x, y, metatile);
	/* ★ 素は `DrawWholeMapView` を使うが、CFRU のヘッダでは
	   宣言が外れているので 1 マスずつ描き直す（結果は同じ）。 */
	CurrentMapDrawMetatileAt(x, y);
}

/* トウカジムの引き戸を開ける（素 PetalburgGymSetDoorMetatiles）

   `Var8004` が部屋の番号（1〜8）。その部屋の引き戸を「開いた形」にする。

   ★★★ **これが無いと入口の部屋から出られない**（2026-09-21 実機）。
     スクリプトの `setmetatile` が置き換えるのは部屋の入口の**飾り**だけで、
     通り抜ける**引き戸そのもの**は素の `special` の中に
     座標が決め打ちで入っていた。

   ★ 引き戸は**縦 2 マス**。上下で別のメタタイルを置く。
     素は「上の番号 + 8」で下を出すが、**こちらは詰め直しているので
     その足し算は成り立たない**（6.19 節と同じ罠）。表から両方引く。

   ★ 素と同じく**両方のマスに通行不可**を付ける。
     ドアのワープは「向いた先」で判定されるので、通行不可でも通れる。

   ★ 絵の描き直しはここではやらない。呼び元のスクリプトが
     `special DrawWholeMapView` を続けて撃つ（素と同じ順）。
     ON_LOAD から呼ばれることがあるので、1 マスずつ描くのは避ける。 */
void HoennNative_PetalburgGymOpenRoomDoors(void)
{
	u8 room = (u8) Var8004;
	u8 i;

	if (room == 0 || room > HOENN_PETALBURG_ROOMS)
		return;
	--room;

	for (i = 0; i < 2; ++i)
	{
		int x, y;

		if (sHoennPetalburgDoorX[room][i] == 0xFF)
			continue;
		x = sHoennPetalburgDoorX[room][i] + HOENN_MAP_OFFSET;
		y = sHoennPetalburgDoorY[room] + HOENN_MAP_OFFSET;
		MapGridSetMetatileIdAt(x, y,
				       HOENN_MT_PETALBURG_DOOR_UP
				       | HOENN_PUZZLE_IMPASSABLE);
		MapGridSetMetatileIdAt(x, y + 1,
				       HOENN_MT_PETALBURG_DOOR_LOW
				       | HOENN_PUZZLE_IMPASSABLE);
	}
}

/* 踏んだスイッチだけ沈め、ほかは上げる（素 MauvilleGymPressSwitch）*/
void HoennNative_MauvilleGymPressSwitch(void)
{
	u8 i;

	for (i = 0; i < HOENN_MAUVILLE_SWITCHES; ++i)
	{
		int x = sHoennMauvilleSwitchXY[i][0] + HOENN_MAP_OFFSET;
		int y = sHoennMauvilleSwitchXY[i][1] + HOENN_MAP_OFFSET;

		SetAndDraw(x, y, (i == Var8004) ? HOENN_MT_MAUVILLE_PRESSED
					       : HOENN_MT_MAUVILLE_RAISED);
	}
}

/* 表を引いて 1 マス置き換える。置き換えたら TRUE */
static bool8 SwapOne(int x, int y, const u16 table[][2], u8 count)
{
	u16 now = MapGridGetMetatileIdAt(x, y) & MT_ID_MASK;
	u8 i;

	for (i = 0; i < count; ++i)
	{
		if ((table[i][0] & MT_ID_MASK) == now)
		{
			SetAndDraw(x, y, table[i][1]);
			return TRUE;
		}
	}
	return FALSE;
}

/* バリアを切り替える（素 MauvilleGymSetDefaultBarriers）*/
void HoennNative_MauvilleGymSetDefaultBarriers(void)
{
	int x, y;

	for (y = HOENN_MAUVILLE_Y0 + HOENN_MAP_OFFSET;
	     y < HOENN_MAUVILLE_Y1 + HOENN_MAP_OFFSET; ++y)
	{
		for (x = HOENN_MAUVILLE_X0 + HOENN_MAP_OFFSET;
		     x < HOENN_MAUVILLE_X1 + HOENN_MAP_OFFSET; ++x)
		{
			/* ★ 床だけは「1 つ上が何か」で行き先が変わるので先に見る */
			if ((MapGridGetMetatileIdAt(x, y) & MT_ID_MASK)
			    == HOENN_MT_MAUVILLE_FLOOR)
			{
				u16 above = MapGridGetMetatileIdAt(x, y - 1) & MT_ID_MASK;

				SetAndDraw(x, y,
					   (above == HOENN_MT_MAUVILLE_FLOOR_ABOVE
					    ? HOENN_MT_MAUVILLE_FLOOR_YES
					    : HOENN_MT_MAUVILLE_FLOOR_NO)
					   | HOENN_PUZZLE_IMPASSABLE);
				continue;
			}
			SwapOne(x, y, sHoennMauvilleToggle,
				ARRAY_COUNT(sHoennMauvilleToggle));
		}
	}
}

/* バリアを全部消す（素 MauvilleGymDeactivatePuzzle）*/
void HoennNative_MauvilleGymDeactivatePuzzle(void)
{
	int x, y;
	u8 i;

	for (i = 0; i < HOENN_MAUVILLE_SWITCHES; ++i)
		SetAndDraw(sHoennMauvilleSwitchXY[i][0] + HOENN_MAP_OFFSET,
			   sHoennMauvilleSwitchXY[i][1] + HOENN_MAP_OFFSET,
			   HOENN_MT_MAUVILLE_PRESSED);

	for (y = HOENN_MAUVILLE_Y0 + HOENN_MAP_OFFSET;
	     y < HOENN_MAUVILLE_Y1 + HOENN_MAP_OFFSET; ++y)
		for (x = HOENN_MAUVILLE_X0 + HOENN_MAP_OFFSET;
		     x < HOENN_MAUVILLE_X1 + HOENN_MAP_OFFSET; ++x)
			SwapOne(x, y, sHoennMauvilleClear,
				ARRAY_COUNT(sHoennMauvilleClear));
}

/* ルネジムの氷（素 SootopolisGymIcePerStepCallback）

   薄氷を踏む -> ひび / ひびを踏む -> 割れて通れなくなる。
   `src/hoenn_steps.c` の 1 歩ごとの処理から、**動いた回だけ**呼ばれる。

   ★ 判定は**挙動値**で行う。`MB_THIN_ICE`(0x26) / `MB_CRACKED_ICE`(0x27) は
     エメラルドと素 FRLG で**同じ値**（変換表で確認済み）。

   ★★ **割れた氷を踏んだ場所に立ったままになる**が、これは素と同じ。
     GBA の通行判定は「そのマスへ入るとき」に見るので、
     立っている本人は動けなくならない。

   ★ **素と違うところ**: エメラルドは踏んだ場所を覚えていて
     （`MarkIcePuzzleCoordVisited`）、出入りしても割れたままにする。
     こちらは覚えないので、**部屋を出入りすると氷が元に戻る**。
     解けなくなることは無い（やり直しになるだけ）。 */
void HoennSootopolisIceStep(s16 x, s16 y, u8 behavior)
{
	/* ★ 2026-10-04 ユーザー「氷が割れないらしい」: これはどの地図でも呼ばれる（src/hoenn_steps.c）。
	   4 のしまの いてだきのどうくつ（素の IcefallCaveIcePerStepCallback）の薄氷を先にルネの部品に書き換えて、
	   素の処理が割れなくなっていた → ルネジムの地図だけで動かす */
	if (gSaveBlock1->location.mapGroup != HOENN_SOOTOPOLIS_GYM_GROUP
	 || gSaveBlock1->location.mapNum != HOENN_SOOTOPOLIS_GYM_NUM)
		return;

	if (behavior == MB_THIN_ICE)
	{
		/* ★★★ **歩数を数えないと階段が出ない。**
		   マップスクリプト（ON_FRAME）が 8 / 28 / 67 で階段を開け、
		   0 になると氷を踏み抜いて落ちる。素と同じ数え方。 */
		VarSet(HOENN_VAR_ICE_STEP_COUNT,
		       VarGet(HOENN_VAR_ICE_STEP_COUNT) + 1);
		PlaySE(SE_ICE_CRACK);
		SetAndDraw(x, y, HOENN_MT_SOOTOPOLIS_CRACKED);
	}
	else if (behavior == MB_CRACKED_ICE)
	{
		VarSet(HOENN_VAR_ICE_STEP_COUNT, 0);
		PlaySE(SE_ICE_BREAK);
		SetAndDraw(x, y,
			   HOENN_MT_SOOTOPOLIS_BROKEN | HOENN_PUZZLE_IMPASSABLE);
	}
}

/* ============================================================================
   トクサネジムの回る床（素 src/rotating_tile_puzzle.c）

   床のスイッチを踏むと、その色の矢印の上に立っている **NPC が 1 マス進み、
   向きも 1 つ左に回る**。メタタイルは 1 つも書き換えない。

   ★★★ **状態を持たない**（6.13 節）。素は `AllocZeroed` した表に
     「動かした NPC と前の向き」を覚えるが、こちらは覚えない。
     理由は 2 つ:
       ① decomp のコメントどおり**回転は必ず反時計回り**なので、
          前の向きは要らない（`TurnRotatingTileObjects` の
          「tileDifference は必ず -1 か 3」の説明）。
       ② 動かす対象は「その色の矢印の上にいる NPC」で、
          動いたあとも**同じ色の矢印の上**にいる。だから
          `TurnRotatingTileObjects` も同じ条件で探し直せる。
     色は スクリプトが `Var8004` に入れて渡す。

   ★ 素の番号の引き算（(番号 - 黄の右) / 8）は**使えない**。
     メタタイルを詰め直しているので `sHoennMossdeepArrows` を引く。

   ★ 動作（`MOVEMENT_ACTION_*`）は**エメラルドと素 FRLG で番号が違う**。
     歩くほうは +8（EM 8〜11 -> FR 16〜19）、向くほうは同じ（0〜3）。
     `LOCK_ANIM`/`UNLOCK_ANIM` は素 FRLG に無いので入れていない。 */

#define MV_STEP_END      254
#define MV_FACE_DOWN     0
#define MV_FACE_UP       1
#define MV_FACE_LEFT     2
#define MV_FACE_RIGHT    3
#define MV_WALK_DOWN     16
#define MV_WALK_UP       17
#define MV_WALK_LEFT     18
#define MV_WALK_RIGHT    19

/* 矢印の向き（0 右 / 1 下 / 2 左 / 3 上）ごとの、進む先と歩く動作 */
static const s8 sArrowDx[4] = { 1, 0, -1, 0 };
static const s8 sArrowDy[4] = { 0, 1, 0, -1 };
static const u8 sArrowWalk[4][2] =
{
	{MV_WALK_RIGHT, MV_STEP_END},
	{MV_WALK_DOWN, MV_STEP_END},
	{MV_WALK_LEFT, MV_STEP_END},
	{MV_WALK_UP, MV_STEP_END},
};

/* いまの向き（DIR_*）-> 反時計回りに 1 つ回った先の「向く動作」 */
static const u8 sTurnCcw[5][2] =
{
	{MV_FACE_DOWN, MV_STEP_END},    /* 0: 無し（来ない） */
	{MV_FACE_RIGHT, MV_STEP_END},   /* DIR_SOUTH -> 右 */
	{MV_FACE_LEFT, MV_STEP_END},    /* DIR_NORTH -> 左 */
	{MV_FACE_DOWN, MV_STEP_END},    /* DIR_WEST  -> 下 */
	{MV_FACE_UP, MV_STEP_END},      /* DIR_EAST  -> 上 */
};
/* 同じ並びの `MOVEMENT_TYPE_*`（NPC の既定の向き。素と同じ番号） */
static const u8 sTurnCcwType[5] =
{
	MOVEMENT_TYPE_FACE_DOWN,
	MOVEMENT_TYPE_FACE_RIGHT,
	MOVEMENT_TYPE_FACE_LEFT,
	MOVEMENT_TYPE_FACE_DOWN,
	MOVEMENT_TYPE_FACE_UP,
};

void HoennNative_MoveRotatingTileObjects(void);
void HoennNative_TurnRotatingTileObjects(void);

/* その番号の矢印は何色・どの向きか。矢印でなければ FALSE */
static bool8 ArrowAt(u16 metatile, u8 *color, u8 *dir)
{
	u8 i;

	for (i = 0; i < HOENN_MOSSDEEP_ARROWS; ++i)
	{
		if (sHoennMossdeepArrows[i][0] == metatile)
		{
			*color = (u8)sHoennMossdeepArrows[i][1];
			*dir = (u8)sHoennMossdeepArrows[i][2];
			return TRUE;
		}
	}
	return FALSE;
}

/* この地図の NPC の枠 i が「Var8004 の色の矢印」の上にいるか */
static bool8 OnPuzzleArrow(u8 i, u8 *dir)
{
	struct EventObjectTemplate *t = &gSaveBlock1->eventObjectTemplates[i];
	u16 mt = MapGridGetMetatileIdAt(t->x + HOENN_MAP_OFFSET,
					t->y + HOENN_MAP_OFFSET) & MT_ID_MASK;
	u8 color;

	if (!ArrowAt(mt, &color, dir))
		return FALSE;
	return (color == (u8)Var8004);
}

/* 色の矢印に乗っている NPC を 1 マス進める。
   最後に動かした NPC の localId を `Var8005` に置く（`waitmovement` 用）。 */
void HoennNative_MoveRotatingTileObjects(void)
{
	u8 i, dir;

	Var8005 = 0;
	for (i = 0; i < ARRAY_COUNT(gSaveBlock1->eventObjectTemplates); ++i)
	{
		struct EventObjectTemplate *t;

		if (!OnPuzzleArrow(i, &dir))
			continue;
		t = &gSaveBlock1->eventObjectTemplates[i];
		t->x += sArrowDx[dir];
		t->y += sArrowDy[dir];
		if (GetEventObjectIdByLocalIdAndMap(t->localId,
						    gSaveBlock1->location.mapNum,
						    gSaveBlock1->location.mapGroup)
		    != EVENT_OBJECTS_COUNT)
		{
			Var8005 = t->localId;
			HoennScriptMovementStart(
				t->localId, gSaveBlock1->location.mapNum,
				gSaveBlock1->location.mapGroup, sArrowWalk[dir]);
		}
	}
}

/* 進んだ NPC を反時計回りに 1 つ向け直す */
void HoennNative_TurnRotatingTileObjects(void)
{
	u8 i, dir;

	for (i = 0; i < ARRAY_COUNT(gSaveBlock1->eventObjectTemplates); ++i)
	{
		struct EventObjectTemplate *t;
		u8 objId, facing;

		if (!OnPuzzleArrow(i, &dir))
			continue;
		t = &gSaveBlock1->eventObjectTemplates[i];
		objId = GetEventObjectIdByLocalIdAndMap(
			t->localId, gSaveBlock1->location.mapNum,
			gSaveBlock1->location.mapGroup);
		if (objId == EVENT_OBJECTS_COUNT)
			continue;
		facing = gEventObjects[objId].facingDirection;
		if (facing == 0 || facing > 4)
			continue;
		t->movementType = sTurnCcwType[facing];
		HoennScriptMovementStart(
			t->localId, gSaveBlock1->location.mapNum,
			gSaveBlock1->location.mapGroup, sTurnCcw[facing]);
	}
}

/* ★★★ 素 `MetatileBehavior_IsWarpDoor_2`（**08059498**）の差し替え。
   `hooks` で入口 8 バイトを潰して、ここへ飛ばしている（戻らない）。

   ## なぜ要るか（2026-09-22・ユーザーが実機で 2 回報告）

   素 FRLG は**扉まわりの 2 つのことを同じ挙動値 0x69 で見ている**:

     `MetatileBehavior_IsWarpDoor`    … `TryDoorWarp`（**歩いて入る**）
     `MetatileBehavior_IsWarpDoor_2`  … 扉の絵のアニメ ＋
                                        `SetUpWarpExitTask`
                                        （**着いたら扉が開いて一歩出る**）

   エメラルドは別々で、トウカジムの引き戸（`MB_PETALBURG_GYM_DOOR`）は
   **後者だけ**真。だから

     0x69 に付け替える → ぶつかっただけで入ってしまう（実機 1 回目）
     0x00 に落とす     → 戻って来たとき一歩出ず**扉と重なる**（実機 2 回目）

   の**どちらも駄目**だった。専用の値 `HOENN_MB_GYM_DOOR` を与え、
   **こちら（アニメと一歩出るほう）だけ**を広げる。
   `TryDoorWarp` が見る `MetatileBehavior_IsWarpDoor` は**触らない**ので、
   歩いて入れないままになる。

   ★ 潰す 8 バイトは `PUSH {lr} / LSL / LSR / CMP r0,#0x69` の 4 命令で、
     この関数は 08059498〜080594AB の 14 バイト。次の関数は 080594AC。
     内側の分岐（080594A0 → 080594A6）は潰した範囲の外で、
     こちらは元へ戻らないので実行されない。 */
bool8 HoennIsWarpDoorAnim(u8 behavior)
{
	return behavior == HOENN_MB_WARP_DOOR || behavior == HOENN_MB_GYM_DOOR;
}

/* ============================================================================
   トクサネジムの中のワープで、回る床で動かした人を元に戻さない
   （2026-09-29 ユーザー「トクサネジムのギミックがワープで元に戻ってしまうので他の場所に戻れなくなる詰み状態になる」）

   エメラルドはジムの中のワープ床で `DoMossdeepGymWarp` を使い、地図の人の置き場
   （`gSaveBlock1->eventObjectTemplates`・上の HoennNative_MoveRotatingTileObjects が動かした先を書く所）を
   読み直さない（pokeemerald src/field_screen_effect.c・SKIP_OBJECT_EVENT_LOAD）。素 FRLG のワープは必ず読み直す。
   → 素 `LoadObjEventTemplatesFromHeader`（08054828）の入口に assembly/hoenn/mossdeep_gym_warp_hook.s を置き、
     ここが TRUE なら読み直さない。

   ★ 見分け方: 今の地図（ApplyCurrentWarp のあと）と前の地図（`gLastUsedWarp`）がどちらもトクサネジム。
     ジムの外へ出てから戻ると読み直す（エメラルドも同じ）。続きから始めるときはこの関数を通らない。
   ★ `gLastUsedWarp` は CFRU の BPRJ.ld に番地が無い（6.29）。素 `ApplyCurrentWarp`（08054A58）が
     主人公の今の位置を書く先 = リテラル 08054A84 の 0x02031CE8（sWarpDestination 0x02031CF0 の 8 バイト前）で確かめた。 */
#define HOENN_LAST_USED_WARP ((struct WarpData *) 0x02031CE8)

bool8 HoennMossdeepGym_KeepObjTemplates(void)
{
	return gSaveBlock1->location.mapGroup == HOENN_MOSSDEEP_GYM_GROUP
	    && gSaveBlock1->location.mapNum == HOENN_MOSSDEEP_GYM_NUM
	    && HOENN_LAST_USED_WARP->mapGroup == HOENN_MOSSDEEP_GYM_GROUP
	    && HOENN_LAST_USED_WARP->mapNum == HOENN_MOSSDEEP_GYM_NUM;
}
