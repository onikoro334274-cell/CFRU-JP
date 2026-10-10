#include "defines.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/fieldmap.h"
#include "../include/field_weather.h"
#include "../include/sprite.h"
#include "../include/script.h"
#include "../include/constants/metatile_behaviors.h"

#include "Tables/hoenn_islands.h"

/*
hoenn_islands.c
    さいはてのことうのミュウ（2026-09-29・やりたいこと #2）

    ユーザー「ホウエンリーグ2回目の強化リーグをクリアするとフラグをたててさいはてのことうとみなみのことうに行ける」
    （ミュウは「エメラルドどおり」）。移植元は pokeemerald src/faraway_island.c。

    ## エメラルドの仕組み
      ・ミュウの動きの型は MOVEMENT_TYPE_COPY_PLAYER_OPPOSITE_IN_GRASS。主人公が 1 歩動き始めるたびに
        CopyablePlayerMovement_WalkNormal が GetMewMoveDirection で逃げる向きを決める（草のマスだけ・主人公から離れる）
      ・歩数（VAR_FARAWAY_ISLAND_STEP_COUNTER）が 8 の倍数の歩だけ姿が見え、9 の倍数の歩は動かない、
        4 の倍数の歩だけ草が揺れる（DoFlaggedGroundEffects を止める）
      ・話しかけると SetMewAboveGrass（歩数を 0xFFFF にしてかくれんぼを止め、足元に草のスプライトを置く）→ 戦闘

    ## こちらのやり方（FRLG の動きのコードは触らない）
      ・ミュウの動きの型は MOVEMENT_TYPE_NONE（自分では動かない）。毎フレームのタスク
        （src/hoenn_steps.c の HoennStepTick）から HoennFarawayMewTick を呼び、主人公のマスが変わった回に
        逃げる向きを決めて「持たせる動き」（EventObjectSetHeldMovement）で 1 歩歩かせる
      ・草を揺らさない歩は、ミュウの triggerGroundEffectsOnMove / OnStop を毎フレーム落とす
        （動きの頭の 1 フレームあとに地面の効果が走るので、その前に消える）
      ・★ 書き換えられる static は使えない（CLAUDE.md 6.13）。状態は一時変数（0x4000〜・地図に入ると 0）に置く:
          VAR_MEW_STEPS  歩数（エメラルドの VAR_FARAWAY_ISLAND_STEP_COUNTER。地図に入ると 0）
          VAR_MEW_GRASS  草のスプライトの番号
          VAR_MEW_PEND   主人公が動いたのに、ミュウがまだ前の 1 歩の途中で動かせていない（1 = 待ち）
          VAR_MEW_SHAKE  この歩で草を揺らすか（1 = 揺らす）
      ・エメラルドと違うところ:
          - ミュウは主人公の 1 フレームあとに動き出す（タスクが主人公の動きを見てから）
          - ミュウにぶつかったときの音の違い（エメラルドはミュウにぶつかると音を鳴らさない）は入れていない
          - いあいぎりで草を刈るとミュウが消える（HideMewWhenGrassCut）は入れていない
            （FRLG のいあいぎりはホウエンの長い草のメタタイルを刈れない＝草が残るので要らない・推測。実機で確かめる）
*/

#define DIR_NONE  0
#define DIR_SOUTH 1
#define DIR_NORTH 2
#define DIR_WEST  3
#define DIR_EAST  4

#define MB_LONG_GRASS MB_VERY_TALL_GRASS  /* 0x03。build_emerald_tileset.py の表でそのまま（エメラルドの IsPokeGrass は 0x02 と 0x03）*/

#define gSpritePalette_GeneralFieldEffect1 ((const struct SpritePalette*) 0x083693D0)  /* 素 FRLG・札 0x1005（ROM で実測）*/
#define FLDEFFOBJ_LONG_GRASS 15
#define gFieldEffectObjectTemplatePointers ((const struct SpriteTemplate* const*) 0x08364098)  /* src/field_effects.c と同じ */
#define SetSpritePosToOffsetMapCoords_s16 ((void (*)(s16*, s16*, s16, s16)) (0x08063484 | 1))  /* 素 FRLG（BPRJ.ld）*/
#define GetOppositeDirection_ ((u8 (*)(u8)) (0x08063D40 | 1))

/* 岩の位置（エメラルドの sFarawayIslandRockCoords・MAP_OFFSET 7 を足した座標）*/
static const s16 sRockCoords[4][2] =
{
	{14 + 7,  9 + 7},
	{18 + 7,  9 + 7},
	{ 9 + 7, 10 + 7},
	{13 + 7, 13 + 7},
};

static bool8 IsFarawayInterior(void)
{
	return gSaveBlock1->location.mapGroup == HOENN_FARAWAY_INTERIOR_GROUP
	    && gSaveBlock1->location.mapNum == HOENN_FARAWAY_INTERIOR_NUM;
}

static struct EventObject* GetMew(void)
{
	u8 id = GetEventObjectIdByLocalIdAndMap(HOENN_FARAWAY_MEW_LOCALID, gSaveBlock1->location.mapNum, gSaveBlock1->location.mapGroup);
	if (id >= EVENT_OBJECTS_COUNT)
		return NULL;
	return &gEventObjects[id];
}

/* エメラルドの CanMewMoveToCoords: 主人公の立っていない草のマス */
static bool8 CanMewMoveToCoords(s16 x, s16 y)
{
	struct EventObject* player = &gEventObjects[gPlayerAvatar->eventObjectId];
	u8 b;

	if (player->currentCoords.x == x && player->currentCoords.y == y)
		return FALSE;
	b = MapGridGetMetatileBehaviorAt(x, y);
	return b == MB_TALL_GRASS || b == MB_LONG_GRASS;
}

/* 向きの候補（エメラルドの sMewDirectionCandidates。static にできないので呼ぶ側の配列）*/
struct MewMove
{
	struct EventObject* mew;
	s16 dx;                 /* 主人公の元のマス - ミュウ（エメラルドの sPlayerToMewDeltaX）*/
	s16 dy;
	u16 steps;
	u8 cand[4];
};

static bool8 ShouldMoveNorth(struct MewMove* m, u8 index)
{
	if (m->dy > 0 && CanMewMoveToCoords(m->mew->currentCoords.x, m->mew->currentCoords.y - 1))
	{
		m->cand[index] = DIR_NORTH;
		return TRUE;
	}
	return FALSE;
}

static bool8 ShouldMoveEast(struct MewMove* m, u8 index)
{
	if (m->dx < 0 && CanMewMoveToCoords(m->mew->currentCoords.x + 1, m->mew->currentCoords.y))
	{
		m->cand[index] = DIR_EAST;
		return TRUE;
	}
	return FALSE;
}

static bool8 ShouldMoveSouth(struct MewMove* m, u8 index)
{
	if (m->dy < 0 && CanMewMoveToCoords(m->mew->currentCoords.x, m->mew->currentCoords.y + 1))
	{
		m->cand[index] = DIR_SOUTH;
		return TRUE;
	}
	return FALSE;
}

static bool8 ShouldMoveWest(struct MewMove* m, u8 index)
{
	if (m->dx > 0 && CanMewMoveToCoords(m->mew->currentCoords.x - 1, m->mew->currentCoords.y))
	{
		m->cand[index] = DIR_WEST;
		return TRUE;
	}
	return FALSE;
}

static u8 RandomCandidate(struct MewMove* m, u8 num)
{
	return m->cand[m->steps % num];
}

static u8 GetValidMewMoveDirection(struct MewMove* m)
{
	struct EventObject* mew = m->mew;
	u8 count = 0;
	u8 i;

	for (i = 0; i < 4; ++i)
		m->cand[i] = DIR_NONE;
	if (CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y - 1))
		m->cand[count++] = DIR_NORTH;
	if (CanMewMoveToCoords(mew->currentCoords.x + 1, mew->currentCoords.y))
		m->cand[count++] = DIR_EAST;
	if (CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y + 1))
		m->cand[count++] = DIR_SOUTH;
	if (CanMewMoveToCoords(mew->currentCoords.x - 1, mew->currentCoords.y))
		m->cand[count++] = DIR_WEST;
	if (count > 1)
		return m->cand[m->steps % count];
	return m->cand[0];
}

/* エメラルドの GetMewMoveDirection（中身はそのまま・状態は m に）*/
static u8 GetMewMoveDirection(struct MewMove* m)
{
	struct EventObject* player = &gEventObjects[gPlayerAvatar->eventObjectId];
	struct EventObject* mew = m->mew;
	s16 px = player->previousCoords.x;
	s16 py = player->previousCoords.y;
	u8 i;
	bool8 safe;

	m->dx = px - mew->currentCoords.x;
	m->dy = py - mew->currentCoords.y;
	for (i = 0; i < 4; ++i)
		m->cand[i] = DIR_NONE;

	if (px == player->currentCoords.x && py == player->currentCoords.y)
		return DIR_NONE;    /* 向きを変えただけ */

	/* 8 歩に 1 回だけ姿が見える */
	mew->invisible = (m->steps % 8 == 0) ? FALSE : TRUE;

	/* 見えた次の 1 歩は動かない */
	if (m->steps % 9 == 0)
		return DIR_NONE;

	/* 岩と主人公にはさまれないように */
	for (i = 0; i < 4; ++i)
	{
		if (px == sRockCoords[i][0])
		{
			safe = FALSE;
			if (py < sRockCoords[i][1])
			{
				if (mew->currentCoords.y <= sRockCoords[i][1])
					safe = TRUE;
			}
			else if (mew->currentCoords.y >= sRockCoords[i][1])
				safe = TRUE;

			if (!safe)
			{
				if (m->dx > 0)
				{
					if (mew->currentCoords.x + 1 == px && CanMewMoveToCoords(mew->currentCoords.x + 1, mew->currentCoords.y))
						return DIR_EAST;
				}
				else if (m->dx < 0)
				{
					if (mew->currentCoords.x - 1 == px && CanMewMoveToCoords(mew->currentCoords.x - 1, mew->currentCoords.y))
						return DIR_WEST;
				}

				if (mew->currentCoords.x == px)
				{
					if (m->dy > 0)
					{
						if (CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y - 1))
							return DIR_NORTH;
					}
					else if (CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y + 1))
						return DIR_SOUTH;
				}
			}
		}

		if (py == sRockCoords[i][1])
		{
			safe = FALSE;
			if (px < sRockCoords[i][0])
			{
				if (mew->currentCoords.x <= sRockCoords[i][0])
					safe = TRUE;
			}
			else if (mew->currentCoords.x >= sRockCoords[i][0])
				safe = TRUE;

			if (!safe)
			{
				if (m->dy > 0)
				{
					if (mew->currentCoords.y + 1 == py && CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y + 1))
						return DIR_SOUTH;
				}
				else if (m->dy < 0)
				{
					if (mew->currentCoords.y - 1 == py && CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y - 1))
						return DIR_NORTH;
				}

				if (mew->currentCoords.y == py)
				{
					if (m->dx > 0)
					{
						if (CanMewMoveToCoords(mew->currentCoords.x - 1, mew->currentCoords.y))
							return DIR_WEST;
					}
					else if (CanMewMoveToCoords(mew->currentCoords.x + 1, mew->currentCoords.y))
						return DIR_EAST;
				}
			}
		}
	}

	/* 主人公から離れる向き（2 つあれば歩数で選ぶ）*/
	if (ShouldMoveNorth(m, 0))
	{
		if (ShouldMoveEast(m, 1) || ShouldMoveWest(m, 1))
			return RandomCandidate(m, 2);
		return DIR_NORTH;
	}
	if (ShouldMoveSouth(m, 0))
	{
		if (ShouldMoveEast(m, 1) || ShouldMoveWest(m, 1))
			return RandomCandidate(m, 2);
		return DIR_SOUTH;
	}
	if (ShouldMoveEast(m, 0))
	{
		if (ShouldMoveNorth(m, 1) || ShouldMoveSouth(m, 1))
			return RandomCandidate(m, 2);
		return DIR_EAST;
	}
	if (ShouldMoveWest(m, 0))
	{
		if (ShouldMoveNorth(m, 1) || ShouldMoveSouth(m, 1))
			return RandomCandidate(m, 2);
		return DIR_WEST;
	}

	/* 離れられないとき: 同じ行なら上下へ */
	if (m->dy == 0)
	{
		if (player->currentCoords.y > mew->currentCoords.y && CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y - 1))
			return DIR_NORTH;
		if (player->currentCoords.y < mew->currentCoords.y && CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y + 1))
			return DIR_SOUTH;
		if (CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y - 1))
			return DIR_NORTH;
		if (CanMewMoveToCoords(mew->currentCoords.x, mew->currentCoords.y + 1))
			return DIR_SOUTH;
	}

	/* 同じ列なら左右へ */
	if (m->dx == 0)
	{
		if (player->currentCoords.x > mew->currentCoords.x && CanMewMoveToCoords(mew->currentCoords.x - 1, mew->currentCoords.y))
			return DIR_WEST;
		if (player->currentCoords.x < mew->currentCoords.x && CanMewMoveToCoords(mew->currentCoords.x + 1, mew->currentCoords.y))
			return DIR_EAST;
		if (CanMewMoveToCoords(mew->currentCoords.x + 1, mew->currentCoords.y))
			return DIR_EAST;
		if (CanMewMoveToCoords(mew->currentCoords.x - 1, mew->currentCoords.y))
			return DIR_WEST;
	}

	return GetValidMewMoveDirection(m);
}

/* かくれんぼの最中か（エメラルドの IsMewPlayingHideAndSeek ＋ 見つかったあと（歩数 0xFFFF）を除く）*/
static bool8 IsMewPlayingHideAndSeek(void)
{
	return IsFarawayInterior()
	    && !FlagGet(HOENN_FLAG_MEW_GONE)
	    && !FlagGet(HOENN_FLAG_HIDE_MEW)
	    && VarGet(VAR_MEW_STEPS) != 0xFFFF;
}

/* 毎フレーム（src/hoenn_steps.c の HoennStepTick）。moved = 主人公のマスが変わった回 */
void HoennFarawayMewTick(bool8 moved)
{
	struct EventObject* mew;
	struct EventObject* player;
	struct MewMove m;
	u8 dir, action;
	s16 x, y;

	if (!IsMewPlayingHideAndSeek())
		return;
	mew = GetMew();
	if (mew == NULL)
		return;

	if (moved)
		VarSet(VAR_MEW_PEND, 1);

	/* 草を揺らさない歩: 地面の効果の合図を落とす（エメラルドの DoFlaggedGroundEffects の頭の return）*/
	if (VarGet(VAR_MEW_SHAKE) == 0)
	{
		mew->triggerGroundEffectsOnMove = FALSE;
		mew->triggerGroundEffectsOnStop = FALSE;
	}

	if (VarGet(VAR_MEW_PEND) == 0)
		return;
	if (mew->heldMovementActive && !mew->heldMovementFinished)
		return;     /* 前の 1 歩の途中。終わってから動かす */
	if (ScriptContext2_IsEnabled())
		return;     /* スクリプト中（見つけた・話しかけた）は動かさない */

	EventObjectClearHeldMovementIfFinished(mew);
	VarSet(VAR_MEW_PEND, 0);

	m.mew = mew;
	m.steps = VarGet(VAR_MEW_STEPS);
	dir = GetMewMoveDirection(&m);
	VarSet(VAR_MEW_SHAKE, (m.steps % 4 == 0) ? 1 : 0);   /* エメラルドの ShouldMewShakeGrass */

	player = &gEventObjects[gPlayerAvatar->eventObjectId];
	if (dir == DIR_NONE)
	{
		/* エメラルド: 主人公と逆の向きを向くだけ（COPY_PLAYER_OPPOSITE）*/
		action = GetFaceDirectionMovementAction(GetOppositeDirection_(player->movementDirection));
	}
	else
	{
		x = mew->currentCoords.x;
		y = mew->currentCoords.y;
		MoveCoords(dir, &x, &y);
		action = GetWalkNormalMovementAction(dir);
		if (GetCollisionAtCoords(mew, x, y, dir) || !CanMewMoveToCoords(x, y))
			action = GetFaceDirectionMovementAction(dir);
	}
	EventObjectSetHeldMovement(mew, action);

	/* エメラルドの UpdateFarawayIslandStepCounter（1 歩ごとに 1 足す・9999 で 0）*/
	if (dir != DIR_NONE || player->previousCoords.x != player->currentCoords.x || player->previousCoords.y != player->currentCoords.y)
	{
		u16 steps = m.steps + 1;
		VarSet(VAR_MEW_STEPS, steps >= 9999 ? 0 : steps);
	}
}

/*
    エメラルドの SetMewAboveGrass（callasm・Var8004 = 0: 見つけたとき / 1: 戦闘から戻ったとき）
*/
void HoennFaraway_SetMewAboveGrass(void)
{
	struct EventObject* mew = GetMew();
	struct Sprite* mewSprite;
	s16 x, y;
	u8 spriteId;

	if (mew == NULL)
		return;
	mewSprite = &gSprites[mew->spriteId];
	mew->invisible = FALSE;
	mew->fixedPriority = TRUE;
	mewSprite->subspriteMode = SUBSPRITES_IGNORE_PRIORITY;

	if (Var8004 == 1)
	{
		mewSprite->subpriority = 1;
		return;
	}

	VarSet(VAR_MEW_STEPS, 0xFFFF);      /* かくれんぼをやめる */
	if (VarGet(0x800C) != DIR_NORTH)     /* VAR_FACING（gSpecialVar_Facing は BPRJ.ld に無いので VarGet で読む）*/
		mewSprite->subpriority = 1;

	LoadSpritePalette(gSpritePalette_GeneralFieldEffect1);
	UpdateSpritePaletteWithWeather(IndexOfSpritePaletteTag(gSpritePalette_GeneralFieldEffect1->tag));

	x = mew->currentCoords.x;
	y = mew->currentCoords.y;
	SetSpritePosToOffsetMapCoords_s16(&x, &y, 8, 8);
	spriteId = CreateSpriteAtEnd(gFieldEffectObjectTemplatePointers[FLDEFFOBJ_LONG_GRASS], x, y, mewSprite->subpriority - 1);
	VarSet(VAR_MEW_GRASS, spriteId);
	if (spriteId != MAX_SPRITES)
	{
		struct Sprite* sprite = &gSprites[spriteId];
		sprite->coordOffsetEnabled = TRUE;
		sprite->oam.priority = 2;
		sprite->callback = SpriteCallbackDummy;
	}
}

/* エメラルドの DestroyMewEmergingGrassSprite（callasm）*/
void HoennFaraway_DestroyGrassSprite(void)
{
	u16 spriteId = VarGet(VAR_MEW_GRASS);

	if (spriteId < MAX_SPRITES)
		DestroySprite(&gSprites[spriteId]);
	VarSet(VAR_MEW_GRASS, MAX_SPRITES);
}
