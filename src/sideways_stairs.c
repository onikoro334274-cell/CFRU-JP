#include "defines.h"
#include "../include/event_object_movement.h"
#include "../include/fieldmap.h"
#include "../include/constants/event_object_movement_constants.h"
#include "../include/constants/flags.h"
#include "../include/new/overworld.h"
#include "Tables/sideways_stairs_table.h"

bool8 ShouldPlayerRun(u16 heldKeys);     /* src/overworld.c（CFRU・自動ダッシュ FLAG_AUTO_RUN も見る）*/

/*
 * 斜めの階段（2026-09-29 ユーザー「1から順番に進めて」・シンオウの Neo Platinum の地図が約 320 マスで使う）
 * docs/research/2026-09-29_sinnoh-johto-outdoor.md の「斜めの階段」
 *
 * Spherical Ice の「[FR] Sideways Stairs」（参考/!資料/sideways_stairs.asm・英語版 FR 用の ASM）を CFRU の形で書き直した。
 *   - 挙動 0xB0〜0xB5 のマスに立って東西に歩くと、見た目は東西を向いたまま斜めに 1 マス動く（元の ASM の change_movement_main と同じ表）:
 *       B0: 西 → 北西・東 → 南東   B1: 西 → 北西            B2: 東 → 南東
 *       B3: 西 → 南西・東 → 北東   B4: 東 → 北東            B5: 西 → 南西
 *   - ぶつかる判定も斜めの先で行う（ふさがっていたら横へは歩かずぶつかる・元の ASM は MoveCoords に渡す向きを斜めに変えている）
 *   - 動きの番号は **CFRU の番号**（include/constants/event_object_movement_constants.h の 0xC2〜0xE1）。
 *     CFRU の連れ歩き（src/follow_me.c の DetermineFollowerState）がこの番号を前提に書かれている（元の ASM は 0xAA〜0xB5）
 *   - 動きの表を 0xAA 個 → 0xE2 個に広げる（repoints で表を指す 3 か所を付け替え）。番号だけあって中身の無い 0xAA〜0xC1・0xCA〜0xD1 は
 *     すぐ終わる動き
 *   - 主人公の歩きの振り分けの表（sPlayerNotOnBikeFuncs 0831F89C）の「歩く」（0805B291）を SidewaysStairs_PlayerNotOnBikeMoving に
 *     差し替え（repoints）。階段でなければ素の関数をそのまま呼ぶ
 * ★ 自転車はまだ（元の ASM は自転車のぶつかる判定にも差し込んでいる・次の段）。人（NPC）は元の ASM と同じく対応しない
 * ★ 状態は持たない（6.13）
 */

#define SIDEWAYS_BEHAVIOUR_0 0xB0

// 素の関数（日本語版・英語版 pokefirered.map の番地 −0x740/−0x744 を中身のバイト列で確かめた）
#define InitNpcForMovement_          ((void (*)(struct EventObject*, struct Sprite*, u8, u8)) (0x08063F38 | 1))
#define SetEventObjectDirection_     ((void (*)(struct EventObject*, u8)) (0x0805F49C | 1))
#define SetStepAnimHandleAlternation_ ((void (*)(struct EventObject*, struct Sprite*, u8)) (0x08062E14 | 1))
#define UpdateMovementNormal_        ((bool8 (*)(struct EventObject*, struct Sprite*)) (0x08064048 | 1))
#define CheckForObjectEventCollision_ ((u8 (*)(struct EventObject*, s16, s16, u8, u8)) (0x0805B464 | 1))
#define PlayerNotOnBikeCollide_      ((void (*)(u8)) (0x0805BA8C | 1))
#define PlayerSetAnimId_             ((void (*)(u8, u8)) (0x0805B8E0 | 1))
#define PlayerNotOnBikeMoving_Vanilla ((void (*)(u8, u16)) (0x0805B290 | 1))
#define MapGridGetMetatileBehaviorAt_ ((u8 (*)(s16, s16)) (0x08058834 | 1))
#define FlagGet_                     ((bool8 (*)(u16)) (0x0806DEC4 | 1))
#define GetMoveDirectionAnimNum_     ((u8 (*)(u8)) (0x08062D00 | 1))
#define GetMoveDirectionFastAnimNum_ ((u8 (*)(u8)) (0x08062D10 | 1))
#define GetRunningDirectionAnimNum_  ((u8 (*)(u8)) (0x08062DE0 | 1))

enum { SS_SPEED_WALK, SS_SPEED_RUN, SS_SPEED_FAST };
#define MOVE_SPEED_NORMAL_ 0
#define MOVE_SPEED_FAST_1_ 1

typedef bool8 (*MovementActionStep)(struct EventObject*, struct Sprite*);

// ---- 斜めの 1 歩 ----------------------------------------------------------------------------------------------
static bool8 SS_Step1(struct EventObject* obj, struct Sprite* sprite)
{
	if (UpdateMovementNormal_(obj, sprite))
	{
		sprite->data[2] = 2;
		return TRUE;
	}
	return FALSE;
}

static bool8 SS_Start(struct EventObject* obj, struct Sprite* sprite, u8 diagDir, u8 faceDir, u8 kind)
{
	u8 anim;
	InitNpcForMovement_(obj, sprite, diagDir, kind == SS_SPEED_WALK ? MOVE_SPEED_NORMAL_ : MOVE_SPEED_FAST_1_); //位置と絵を斜めに
	SetEventObjectDirection_(obj, faceDir); //見た目の向きは東西（北・南）のまま
	if (kind == SS_SPEED_RUN)
		anim = GetRunningDirectionAnimNum_(faceDir);
	else if (kind == SS_SPEED_FAST)
		anim = GetMoveDirectionFastAnimNum_(faceDir);
	else
		anim = GetMoveDirectionAnimNum_(faceDir);
	SetStepAnimHandleAlternation_(obj, sprite, anim);
	return SS_Step1(obj, sprite);
}

static bool8 SS_Finish(unusedArg struct EventObject* obj, unusedArg struct Sprite* sprite)
{
	return TRUE;
}

// 番号の並び（CFRU の定数と同じ）: 左下・下向き / 左上・上向き / 左下・左向き / 右下・右向き / 右下・下向き / 右上・上向き / 左上・左向き / 右上・右向き
#define SS_ACTION(name, diag, face, kind) \
	static bool8 name##_Step0(struct EventObject* obj, struct Sprite* sprite) { return SS_Start(obj, sprite, diag, face, kind); } \
	static const MovementActionStep name[] = { name##_Step0, SS_Step1, SS_Finish };

#define SS_ACTIONS(prefix, kind) \
	SS_ACTION(prefix##_LeftDownFaceDown,  DIR_SOUTHWEST, DIR_SOUTH, kind) \
	SS_ACTION(prefix##_LeftUpFaceUp,      DIR_NORTHWEST, DIR_NORTH, kind) \
	SS_ACTION(prefix##_LeftDownFaceLeft,  DIR_SOUTHWEST, DIR_WEST,  kind) \
	SS_ACTION(prefix##_RightDownFaceRight, DIR_SOUTHEAST, DIR_EAST, kind) \
	SS_ACTION(prefix##_RightDownFaceDown, DIR_SOUTHEAST, DIR_SOUTH, kind) \
	SS_ACTION(prefix##_RightUpFaceUp,     DIR_NORTHEAST, DIR_NORTH, kind) \
	SS_ACTION(prefix##_LeftUpFaceLeft,    DIR_NORTHWEST, DIR_WEST,  kind) \
	SS_ACTION(prefix##_RightUpFaceRight,  DIR_NORTHEAST, DIR_EAST,  kind)

SS_ACTIONS(sSSWalk, SS_SPEED_WALK)
SS_ACTIONS(sSSRun, SS_SPEED_RUN)
SS_ACTIONS(sSSFast, SS_SPEED_FAST)

static const MovementActionStep sSSNone[] = { SS_Finish };

/* ★ 2026-10-04 マッハ自転車（ユーザー「走る描画がないのが悲しい」）: 動く速さは素の「すべる」と同じ MOVE_SPEED_FASTEST（1 マス 2 コマ）、
   絵は今までのターボのアニメ GO_FASTER（12〜15・2 コマずつ・こぐ絵）。素の「すべる」は GO_FASTEST（1 コマずつ）で、1 マス 2 コマだと
   足を出した絵と立った絵が 1 コマごとに入れ替わるだけで、こいでいるように見えなかった。0xE2〜0xE5（下・上・左・右）・src/overworld.c の MoveOnBike */
#define MOVE_SPEED_FASTEST_ 4
#define ANIM_STD_GO_FASTER_SOUTH_ 12                  /* 南 12・北 13・西 14・東 15（向き 1〜4 + 11） */
static bool8 MachFastest_Start(struct EventObject* obj, struct Sprite* sprite, u8 dir)
{
	u8 prev;
	InitNpcForMovement_(obj, sprite, dir, MOVE_SPEED_FASTEST_);
	/* ★ 同日「全部は動いていない」: 素の SetStepAnimHandleAlternation は、前の 1 歩でアニメが「立った絵」（1・3 番目）まで
	   進んでいたら反対の足にする。1 マス 2 コマだと 1 歩のあいだに最初の絵（2 コマ）から進まず、毎回同じ足の絵だった
	   → 前の 1 歩の足（0・1 番目なら片足・2・3 番目ならもう片足）を見て、自分で反対の足（2 か 0）から始める */
	prev = sprite->animCmdIndex;
	SetStepAnimHandleAlternation_(obj, sprite, ANIM_STD_GO_FASTER_SOUTH_ - 1 + dir);
	SeekSpriteAnim(sprite, (prev < 2) ? 2 : 0);
	return SS_Step1(obj, sprite);
}
#define MACH_ACTION(name, dir) 	static bool8 name##_Step0(struct EventObject* obj, struct Sprite* sprite) { return MachFastest_Start(obj, sprite, dir); } 	static const MovementActionStep name[] = { name##_Step0, SS_Step1, SS_Finish };
MACH_ACTION(sMachFastestDown, DIR_SOUTH)
MACH_ACTION(sMachFastestUp, DIR_NORTH)
MACH_ACTION(sMachFastestLeft, DIR_WEST)
MACH_ACTION(sMachFastestRight, DIR_EAST)

#define SS_ROW(prefix) prefix##_LeftDownFaceDown, prefix##_LeftUpFaceUp, prefix##_LeftDownFaceLeft, prefix##_RightDownFaceRight, \
	prefix##_RightDownFaceDown, prefix##_RightUpFaceUp, prefix##_LeftUpFaceLeft, prefix##_RightUpFaceRight
#define SS_NONE8 sSSNone, sSSNone, sSSNone, sSSNone, sSSNone, sSSNone, sSSNone, sSSNone

// 広げた動きの表（repoints で素の表 0836A644 の代わりに）
const void* const gSidewaysMovementActionFuncs[] =
{
	SIDEWAYS_VANILLA_MOVEMENT_ACTION_TABLE,
	SS_NONE8, SS_NONE8, SS_NONE8,                   //0xAA〜0xC1（CFRU の後ろ向きの歩き・中身は無い）
	SS_ROW(sSSWalk),                                //0xC2〜0xC9
	SS_NONE8,                                       //0xCA〜0xD1
	SS_ROW(sSSRun),                                 //0xD2〜0xD9
	SS_ROW(sSSFast),                                //0xDA〜0xE1
	sMachFastestDown, sMachFastestUp, sMachFastestLeft, sMachFastestRight,   //0xE2〜0xE5 マッハ自転車（1 マス 2 コマ・こぐ絵）
};

// ---- 主人公の歩き ---------------------------------------------------------------------------------------------
//立っているマスと歩く向き → 斜めの向き（0 はふつう）。元の ASM の change_movement_main と同じ
static u8 SidewaysStairs_DiagonalDirection(u8 behaviour, u8 direction)
{
	switch (behaviour - SIDEWAYS_BEHAVIOUR_0) {
		case 0: return direction == DIR_WEST ? DIR_NORTHWEST : direction == DIR_EAST ? DIR_SOUTHEAST : 0;
		case 1: return direction == DIR_WEST ? DIR_NORTHWEST : 0;
		case 2: return direction == DIR_EAST ? DIR_SOUTHEAST : 0;
		case 3: return direction == DIR_WEST ? DIR_SOUTHWEST : direction == DIR_EAST ? DIR_NORTHEAST : 0;
		case 4: return direction == DIR_EAST ? DIR_NORTHEAST : 0;
		case 5: return direction == DIR_WEST ? DIR_SOUTHWEST : 0;
	}
	return 0;
}

static u8 SidewaysStairs_Action(u8 diag, u8 base)
{
	switch (diag) {
		case DIR_SOUTHWEST: return base + 2; //LEFT_DOWN_FACE_LEFT
		case DIR_SOUTHEAST: return base + 3; //RIGHT_DOWN_FACE_RIGHT
		case DIR_NORTHWEST: return base + 6; //LEFT_UP_FACE_LEFT
		default:            return base + 7; //RIGHT_UP_FACE_RIGHT
	}
}

// ---- 自転車（2026-09-30 ユーザー「斜めの階段を自転車で通れるようにする」）----------------------------------------
//元の ASM の bike（PlayerRideWaterCurrent の横取り）と同じく、斜めの速歩き（0xDA〜）で 1 歩。
//呼び元は src/hoenn_bike.c（通常・マッハの「進む」とダートの「ふつうに進む」）。斜めにしないマスなら FALSE（素の処理へ）
#define PlayerOnBikeCollide_ ((void (*)(u8)) (0x0805BA68 | 1))   //英語版 0805C1AC − 0x744（歩きのぶつかり 0805BA8C と同じ形）

bool8 SidewaysStairs_TryBikeStep(u8 direction)
{
	struct EventObject* player = &gEventObjects[gPlayerAvatar->eventObjectId];
	u8 diag = SidewaysStairs_DiagonalDirection(player->currentMetatileBehavior, direction);

	if (diag == 0)
		return FALSE;

	s16 x = player->currentCoords.x + (direction == DIR_EAST ? 1 : -1);
	s16 y = player->currentCoords.y + ((diag == DIR_NORTHWEST || diag == DIR_NORTHEAST) ? -1 : 1);
	if (CheckForObjectEventCollision_(player, x, y, direction, MapGridGetMetatileBehaviorAt_(x, y)) != 0) //斜めの先でぶつかる
		PlayerOnBikeCollide_(direction);
	else
		PlayerSetAnimId_(SidewaysStairs_Action(diag, MOVEMENT_ACTION_WALK_FAST_LEFT_DOWN_FACE_DOWN), 2);
	return TRUE;
}

void SidewaysStairs_PlayerNotOnBikeMoving(u8 direction, u16 heldKeys)
{
	struct EventObject* player = &gEventObjects[gPlayerAvatar->eventObjectId];
	u8 diag = 0;

	if (!(gPlayerAvatar->flags & PLAYER_AVATAR_FLAG_SURFING))
		diag = SidewaysStairs_DiagonalDirection(player->currentMetatileBehavior, direction);

	if (diag == 0)
	{
		PlayerNotOnBikeMoving_Vanilla(direction, heldKeys);
		return;
	}

	s16 x = player->currentCoords.x + (direction == DIR_EAST ? 1 : -1);
	s16 y = player->currentCoords.y + ((diag == DIR_NORTHWEST || diag == DIR_NORTHEAST) ? -1 : 1);
	if (CheckForObjectEventCollision_(player, x, y, direction, MapGridGetMetatileBehaviorAt_(x, y)) != 0) //斜めの先でぶつかる
	{
		PlayerNotOnBikeCollide_(direction);
		return;
	}

	/* ★ 2026-10-01 ユーザー「斜め階段で走れない」: 走るかは CFRU の ShouldPlayerRun（src/overworld.c）で決める。
	   前は「B を押しているか」だけを見ていたので、自動ダッシュ（FLAG_AUTO_RUN・L で切り替え）で B を押さずに
	   走っていると、階段の上だけ歩きになっていた。ShouldPlayerRun は走れない床・ずかんナビ中も見る */
	if (FlagGet_(FLAG_SYS_B_DASH) && ShouldPlayerRun(heldKeys))
	{
		PlayerSetAnimId_(SidewaysStairs_Action(diag, MOVEMENT_ACTION_RUN_LEFT_DOWN_FACE_DOWN), 2);
		gPlayerAvatar->flags |= PLAYER_AVATAR_FLAG_DASH;
	}
	else
		PlayerSetAnimId_(SidewaysStairs_Action(diag, MOVEMENT_ACTION_WALK_NORMAL_LEFT_DOWN_FACE_DOWN), 2);
}
