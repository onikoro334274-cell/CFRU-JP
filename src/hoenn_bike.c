#include "defines.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/fieldmap.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/new/ram_locs.h"
#include "Tables/hoenn_script_flags.h"

/*
 * 自転車のモード: 通常 / マッハ / ダート（2026-09-24 ユーザー要望
 * 「ホウエンの自転車屋でじてんしゃにモード追加する NPC 追加。現在の L ボタンのモードで
 *   通常・マッハ自転車・ダート自転車に切り替えられるように」・ダートの技は全部）
 *
 * ・キンセツの自転車屋のカゼノに話しかけると FLAG_RECEIVED_BIKE（= HOENN_FLAG_BIKE_MODES）が立つ
 * ・自転車に乗っているとき L で 通常 → マッハ → ダート → 通常 …（追加前の L は何もしない）
 *     通常  : 素 FRLG の自転車（B を押している間はターボ・CFRU の FLAG_BIKE_TURBO_BOOST は下ろす）
 *     マッハ: 素 FRLG の自転車 ＋ ターボを立てる（速度 4。ぬかるみ坂を登れる・CFRU の ForcedMovement_MuddySlope）
 *     ダート: エメラルドの src/bike.c のダートの処理を移植（ウィリー・ぴょんぴょん・横跳び・
 *             振り向きジャンプ・レール・でこぼこ坂）
 *
 * 素 FRLG の自転車は乗った瞬間に MACH と ACRO の旗を両方立てる（item_use.c の GetOnOffBike）。
 * 毎フレーム（HoennBikeTick）モードに合わせて片方だけにし、絵もモードのものにする。
 *
 * ★ 状態は変数 HOENN_VAR_BIKE_MODE（予約・0x514B）に置く（6.13 — 書き換え可能な static は使えない）:
 *     下位 2 ビット = モード（0 通常 / 1 マッハ / 2 ダート）
 *     上位 8 ビット = A/B/START/SELECT の押しっぱなしの時間（エメラルドの abStartSelectTimerHistory[0]。
 *                     素 FRLG の PlayerAvatar にはこの欄が無い。横跳びの判定は [0] しか見ない）
 * 詳細は docs/research/2026-09-24_bike-modes.md
 */

/* ---- 素 FRLG（BPRJ）の番地（tools/rom/verify_hoenn_bike.py が形を見る）---- */
#define BPRJ_GET_BIKE_TRANSITION_ID    0x080BE394  /* 素の入力の処理の振り分け（表 083AF254 を引く）*/
#define BPRJ_BIKE_TRANSITIONS          0x083AF240  /* 素の自転車の動き 5 本（CFRU のターボのフックはこの中）*/
#define BPRJ_GET_BIKE_COLLISION        0x080BE660  /* 素の GetBikeCollision（割れる氷・ヒワマキの橋・走る禁止まで見る）*/

#define CallGetBikeTransitionId   ((u8 (*)(u8 *, u16, u16))(BPRJ_GET_BIKE_TRANSITION_ID + 1))
#define sVanillaBikeTransitions   ((void (*const *)(u8))BPRJ_BIKE_TRANSITIONS)
#define CallGetBikeCollision      ((u8 (*)(u8))(BPRJ_GET_BIKE_COLLISION + 1))

/* BPRJ.ld に番地のある素の関数（ヘッダに無いものだけ宣言）*/
void __attribute__((long_call)) PlayerSetAnimId(u8 movementActionId, u8 copyableMovement);
void __attribute__((long_call)) PlayerRideWaterCurrent(u8 direction);
u8 __attribute__((long_call)) GetPlayerMovementDirection(void);
void __attribute__((long_call)) MoveCoords(u8 direction, s16 *x, s16 *y);
u8 __attribute__((long_call)) GetJumpMovementAction(u32 direction);
u8 __attribute__((long_call)) GetJump2MovementAction(u32 direction);
u8 __attribute__((long_call)) GetFaceDirectionMovementAction(u32 direction);
u8 __attribute__((long_call)) GetOppositeDirection(u8 direction);
bool8 __attribute__((long_call)) MetatileBehavior_IsRunningDisallowed(u8 behavior);

/* ---- 挙動値（CFRU include/constants/metatile_behaviors.h）---- */
#define HB_MB_BUMPY_SLOPE               0xD3
#define HB_MB_ISOLATED_VERTICAL_RAIL    0xD4
#define HB_MB_ISOLATED_HORIZONTAL_RAIL  0xD5
#define HB_MB_VERTICAL_RAIL             0xD6
#define HB_MB_HORIZONTAL_RAIL           0xD7

/* ---- 素 FRLG の動きの番号（pokefirered include/constants/event_object_movement.h）----
   ★ エメラルドとは 0x88 から番号がずれる（エメラルドには ACRO_END_WHEELIE_MOVE の 4 本が多い）。
     名前で素 FRLG の番号を使う（6.24）*/
#define ACT_WALK_IN_PLACE_FAST_DOWN     0x29
#define ACT_JUMP_IN_PLACE_DOWN_UP       0x56
#define ACT_JUMP_IN_PLACE_UP_DOWN       0x57
#define ACT_JUMP_IN_PLACE_LEFT_RIGHT    0x58
#define ACT_JUMP_IN_PLACE_RIGHT_LEFT    0x59
#define ACT_ACRO_WHEELIE_FACE_DOWN      0x70
#define ACT_ACRO_POP_WHEELIE_DOWN       0x74
#define ACT_ACRO_END_WHEELIE_FACE_DOWN  0x78
#define ACT_ACRO_WHEELIE_HOP_FACE_DOWN  0x7C
#define ACT_ACRO_WHEELIE_HOP_DOWN       0x80
#define ACT_ACRO_WHEELIE_JUMP_DOWN      0x84
#define ACT_ACRO_WHEELIE_IN_PLACE_DOWN  0x88
#define ACT_ACRO_POP_WHEELIE_MOVE_DOWN  0x8C
#define ACT_ACRO_WHEELIE_MOVE_DOWN      0x90

#define HB_SE_WALL_HIT  7       /* pokefirered include/constants/songs.h */
#define HB_SE_LEDGE     10
#define HB_SE_BIKE_HOP  28

/* エメラルドの ACRO_STATE_* / ACRO_TRANS_*（src/bike.c）*/
enum { ACRO_STATE_NORMAL, ACRO_STATE_TURNING, ACRO_STATE_WHEELIE_STANDING, ACRO_STATE_BUNNY_HOP,
       ACRO_STATE_WHEELIE_MOVING, ACRO_STATE_SIDE_JUMP, ACRO_STATE_TURN_JUMP };
enum { ACRO_TRANS_FACE_DIRECTION, ACRO_TRANS_TURN_DIRECTION, ACRO_TRANS_MOVING, ACRO_TRANS_NORMAL_TO_WHEELIE,
       ACRO_TRANS_WHEELIE_TO_NORMAL, ACRO_TRANS_WHEELIE_IDLE, ACRO_TRANS_WHEELIE_HOPPING_STANDING,
       ACRO_TRANS_WHEELIE_HOPPING_MOVING, ACRO_TRANS_SIDE_JUMP, ACRO_TRANS_TURN_JUMP,
       ACRO_TRANS_WHEELIE_MOVING, ACRO_TRANS_WHEELIE_RISING_MOVING, ACRO_TRANS_WHEELIE_LOWERING_MOVING };

/* 素 FRLG の COLLISION_*（pokefirered include/global.fieldmap.h。エメラルドと同じ番号）*/
#define COL_NONE                     0
#define COL_OBJECT_EVENT             4
#define COL_STOP_SURFING             5
#define COL_LEDGE_JUMP               6
#define COL_PUSHED_BOULDER           7
#define COL_DIRECTIONAL_STAIR_WARP   8   /* エメラルドの COLLISION_ROTATING_GATE と同じ番号 */
#define COL_WHEELIE_HOP              9
#define COL_ISOLATED_VERTICAL_RAIL   10
#define COL_ISOLATED_HORIZONTAL_RAIL 11
#define COL_VERTICAL_RAIL            12
#define COL_HORIZONTAL_RAIL          13
#define COL_IMPASSABLE               2

#define HB_SPEED_STANDING 0
#define HB_SPEED_NORMAL   1

/* 素 FRLG の PLAYER_AVATAR_FLAG_*（MACH = 1<<1 / ACRO = 1<<2）*/
#define HB_FLAG_MACH  (1 << 1)
#define HB_FLAG_ACRO  (1 << 2)
#define HB_FLAG_BIKE  (HB_FLAG_MACH | HB_FLAG_ACRO)

enum { BIKE_MODE_NORMAL, BIKE_MODE_MACH, BIKE_MODE_DIRT, BIKE_MODE_COUNT };

extern const u8 gHoennBikeScript_ModeNormal[];
extern const u8 gHoennBikeScript_ModeMach[];
extern const u8 gHoennBikeScript_ModeDirt[];

bool8 HoennBikeCycleModeAllowed(void);
bool8 HoennBikeCycleMode(void);
void HoennBikeTick(void);
bool8 SidewaysStairs_TryBikeStep(u8 direction);   /* src/sideways_stairs.c */
u16 HoennBikeModeGfx(u8 gender, u16 normalGfx);
void HoennMovePlayerOnBike(u8 direction, u16 newKeys, u16 heldKeys);
bool8 HoennCanBikeFaceDirectionOnRail(u8 direction, u8 behavior);
bool8 HoennIsPlayerNotUsingAcroBikeOnBumpySlope(void);
void HoennBike_HandleBumpySlopeJump(void);
bool8 HoennMB_IsBumpySlope(u8 b);
bool8 HoennMB_IsIsolatedVerticalRail(u8 b);
bool8 HoennMB_IsIsolatedHorizontalRail(u8 b);
bool8 HoennMB_IsVerticalRail(u8 b);
bool8 HoennMB_IsHorizontalRail(u8 b);

/* ---- 本物の判定（素の空の判定 5 つ 08059AC8〜08059AD8 の代わり）----
   素の CheckAcroBikeCollision（0805B668）は判定の表 0831F8B0（5 件）を引き、当たった番号の
   0831F8C4 の値（9〜13 = COLLISION_WHEELIE_HOP / *_RAIL）を返す。表を指すリテラル 0805B690 を
   repoints でこちらの表へ向ける（6.11）。*/
bool8 HoennMB_IsBumpySlope(u8 b)              { return b == HB_MB_BUMPY_SLOPE; }
bool8 HoennMB_IsIsolatedVerticalRail(u8 b)    { return b == HB_MB_ISOLATED_VERTICAL_RAIL; }
bool8 HoennMB_IsIsolatedHorizontalRail(u8 b)  { return b == HB_MB_ISOLATED_HORIZONTAL_RAIL; }
bool8 HoennMB_IsVerticalRail(u8 b)            { return b == HB_MB_VERTICAL_RAIL; }
bool8 HoennMB_IsHorizontalRail(u8 b)          { return b == HB_MB_HORIZONTAL_RAIL; }

bool8 (*const gHoennBikeTrickJudges[5])(u8) =
{
	HoennMB_IsBumpySlope,
	HoennMB_IsIsolatedVerticalRail,
	HoennMB_IsIsolatedHorizontalRail,
	HoennMB_IsVerticalRail,
	HoennMB_IsHorizontalRail,
};

/* ---- モード ---- */
static u8 GetBikeMode(void)
{
	u8 mode = VarGet(HOENN_VAR_BIKE_MODE) & 3;
	if (!FlagGet(HOENN_FLAG_BIKE_MODES) || mode >= BIKE_MODE_COUNT)
		return BIKE_MODE_NORMAL;
	return mode;
}

static void SetBikeMode(u8 mode)
{
	VarSet(HOENN_VAR_BIKE_MODE, (VarGet(HOENN_VAR_BIKE_MODE) & 0xFF00) | mode);
}

static u8 GetABSSTimer(void)
{
	return VarGet(HOENN_VAR_BIKE_MODE) >> 8;
}

static void SetABSSTimer(u8 t)
{
	VarSet(HOENN_VAR_BIKE_MODE, (VarGet(HOENN_VAR_BIKE_MODE) & 0xFF) | (t << 8));
}

/* L ボタンでモードを切り替えられるか（自転車屋でモードを付けてもらったか）*/
bool8 HoennBikeCycleModeAllowed(void)
{
	return FlagGet(HOENN_FLAG_BIKE_MODES);
}

/* L ボタン（src/read_keys.c の StartLButtonFunc から・自転車に乗っているときだけ）。
   追加前は何もしない（ユーザー判断 2026-09-24）*/
bool8 HoennBikeCycleMode(void)
{
	u8 mode;

	if (!FlagGet(HOENN_FLAG_BIKE_MODES))
		return FALSE;

	mode = GetBikeMode() + 1;
	if (mode >= BIKE_MODE_COUNT)
		mode = BIKE_MODE_NORMAL;
	SetBikeMode(mode);
	#ifdef FLAG_BIKE_TURBO_BOOST
	if (mode == BIKE_MODE_MACH)
		FlagSet(FLAG_BIKE_TURBO_BOOST);
	else
		FlagClear(FLAG_BIKE_TURBO_BOOST);
	#endif

	if (mode == BIKE_MODE_MACH)
		ScriptContext1_SetupScript(gHoennBikeScript_ModeMach);
	else if (mode == BIKE_MODE_DIRT)
		ScriptContext1_SetupScript(gHoennBikeScript_ModeDirt);
	else
		ScriptContext1_SetupScript(gHoennBikeScript_ModeNormal);
	return TRUE;
}

/* 自転車の絵をモードで選ぶ（src/character_customization.c から。normalGfx は通常の絵）。
   マッハ・ダートの絵は表 8 の 16〜19 番（tools/bwplayer/build_bw_walk.py）*/
u16 HoennBikeModeGfx(u8 gender, u16 normalGfx)
{
	switch (GetBikeMode())
	{
		case BIKE_MODE_MACH:
			return gender ? 0x0811 : 0x0810;
		case BIKE_MODE_DIRT:
			return gender ? 0x0813 : 0x0812;
		default:
			return normalGfx;
	}
}

static void BikeClearAcroState(void)
{
	gPlayerAvatar->acroBikeState = 0;
	gPlayerAvatar->newDirBackup = 0;
	gPlayerAvatar->bikeFrameCounter = 0;
	gPlayerAvatar->bikeSpeed = HB_SPEED_STANDING;
}

/* ダートの入力の履歴（エメラルドの Bike_TryAcroBikeHistoryUpdate）。
   ★ エメラルドは PlayerStep の中で**毎フレーム**呼ぶ。1 マスの途中でも止めない（横跳びの判定の時間がずれる）*/
static void BikeUpdateAcroHistory(void)
{
	u16 held = gMain.heldKeys;
	u8 dir = DIR_NONE, i;
	u8 abss;

	if (held & DPAD_UP)         dir = DIR_NORTH;
	else if (held & DPAD_DOWN)  dir = DIR_SOUTH;
	else if (held & DPAD_LEFT)  dir = DIR_WEST;
	else if (held & DPAD_RIGHT) dir = DIR_EAST;

	if (dir == (gPlayerAvatar->directionHistory & 0xF))
	{
		if (gPlayerAvatar->dirTimerHistory[0] < 0xFF)
			gPlayerAvatar->dirTimerHistory[0]++;
	}
	else
	{
		gPlayerAvatar->directionHistory = (gPlayerAvatar->directionHistory << 4) | (dir & 0xF);
		for (i = 7; i != 0; i--)
			gPlayerAvatar->dirTimerHistory[i] = gPlayerAvatar->dirTimerHistory[i - 1];
		gPlayerAvatar->dirTimerHistory[0] = 1;
		gPlayerAvatar->bikeSpeed = HB_SPEED_STANDING;
	}

	abss = held & (A_BUTTON | B_BUTTON | SELECT_BUTTON | START_BUTTON);
	if (abss == (gPlayerAvatar->abStartSelectHistory & 0xF))
	{
		u8 t = GetABSSTimer();
		if (t < 0xFF)
			SetABSSTimer(t + 1);
	}
	else
	{
		gPlayerAvatar->abStartSelectHistory = (gPlayerAvatar->abStartSelectHistory << 4) | (abss & 0xF);
		SetABSSTimer(1);
		gPlayerAvatar->bikeSpeed = HB_SPEED_STANDING;
	}
}

/* 毎フレーム（src/hoenn_steps.c の HoennStepTick から）*/
void HoennBikeTick(void)
{
	u8 want, mode;
	struct EventObject *player;
	u16 gfx, cur;

	if (!(gPlayerAvatar->flags & HB_FLAG_BIKE))
		return;

	mode = GetBikeMode();
	want = (mode == BIKE_MODE_DIRT) ? HB_FLAG_ACRO : HB_FLAG_MACH;

	#ifdef FLAG_BIKE_TURBO_BOOST
	/* ターボの旗はマッハのときだけ（前のセーブで L のターボを立てたままでも、ここでそろえる）*/
	if ((mode == BIKE_MODE_MACH) != (FlagGet(FLAG_BIKE_TURBO_BOOST) != 0))
	{
		if (mode == BIKE_MODE_MACH)
			FlagSet(FLAG_BIKE_TURBO_BOOST);
		else
			FlagClear(FLAG_BIKE_TURBO_BOOST);
	}
	#endif

	if (mode == BIKE_MODE_DIRT && (gPlayerAvatar->flags & HB_FLAG_ACRO))
		BikeUpdateAcroHistory();

	/* 1 マスの途中や、演出で止められている間は替えない */
	if (gPlayerAvatar->tileTransitionState != 0 || gPlayerAvatar->preventStep)
		return;

	player = &gEventObjects[gPlayerAvatar->eventObjectId];
	if ((gPlayerAvatar->flags & HB_FLAG_BIKE) != want)
	{
		gPlayerAvatar->flags = (gPlayerAvatar->flags & ~HB_FLAG_BIKE) | want;
		/* ★ 素 FRLG の入力の処理は 3 つ（0〜2）しかない。ダートの状態（〜6）のまま戻すと表の外を引く */
		BikeClearAcroState();
	}

	gfx = GetPlayerAvatarGraphicsIdByStateIdAndGender(PLAYER_AVATAR_STATE_BIKE, gPlayerAvatar->gender);
	cur = (player->graphicsIdUpperByte << 8) | player->graphicsIdLowerByte;
	if (cur != gfx)
		EventObjectSetGraphicsId(player, gfx);

}

/* ---- 主人公を動かす小さな関数（素 FRLG の field_player_avatar.c と同じ中身）---- */
static u8 ActByDir(u8 base, u8 direction)
{
	return base + (direction ? direction - 1 : 0);
}

static void PlayerFaceDirection(u8 direction)       { PlayerSetAnimId(GetFaceDirectionMovementAction(direction), 1); }
static void PlayerJumpLedge(u8 direction)           { PlaySE(HB_SE_LEDGE); PlayerSetAnimId(GetJump2MovementAction(direction), 8); }
static void PlayerIdleWheelie(u8 direction)         { PlayerSetAnimId(ActByDir(ACT_ACRO_WHEELIE_FACE_DOWN, direction), 1); }
static void PlayerStartWheelie(u8 direction)        { PlayerSetAnimId(ActByDir(ACT_ACRO_POP_WHEELIE_DOWN, direction), 1); }
static void PlayerEndWheelie(u8 direction)          { PlayerSetAnimId(ActByDir(ACT_ACRO_END_WHEELIE_FACE_DOWN, direction), 1); }
static void PlayerStandingHoppingWheelie(u8 direction) { PlaySE(HB_SE_BIKE_HOP); PlayerSetAnimId(ActByDir(ACT_ACRO_WHEELIE_HOP_FACE_DOWN, direction), 1); }
static void PlayerMovingHoppingWheelie(u8 direction)   { PlaySE(HB_SE_BIKE_HOP); PlayerSetAnimId(ActByDir(ACT_ACRO_WHEELIE_HOP_DOWN, direction), 2); }
static void PlayerLedgeHoppingWheelie(u8 direction)    { PlaySE(HB_SE_BIKE_HOP); PlayerSetAnimId(ActByDir(ACT_ACRO_WHEELIE_JUMP_DOWN, direction), 8); }
static void PlayerWheelieInPlace(u8 direction)      { PlaySE(HB_SE_WALL_HIT); PlayerSetAnimId(ActByDir(ACT_ACRO_WHEELIE_IN_PLACE_DOWN, direction), 2); }
static void PlayerPopWheelieWhileMoving(u8 direction) { PlayerSetAnimId(ActByDir(ACT_ACRO_POP_WHEELIE_MOVE_DOWN, direction), 2); }
static void PlayerWheelieMove(u8 direction)         { PlayerSetAnimId(ActByDir(ACT_ACRO_WHEELIE_MOVE_DOWN, direction), 2); }
/* ★ 素 FRLG にはエメラルドの ACRO_END_WHEELIE_MOVE（走りながらウィリーを下ろす）が無い。ふつうに進む */
static void PlayerEndWheelieWhileMoving(u8 direction) { PlayerRideWaterCurrent(direction); }

static void PlayerAcroTurnJump(u8 direction)
{
	static const u8 sTurnJump[5] = { ACT_JUMP_IN_PLACE_UP_DOWN, ACT_JUMP_IN_PLACE_UP_DOWN,
	                                 ACT_JUMP_IN_PLACE_DOWN_UP, ACT_JUMP_IN_PLACE_RIGHT_LEFT,
	                                 ACT_JUMP_IN_PLACE_LEFT_RIGHT };
	PlaySE(HB_SE_BIKE_HOP);
	PlayerSetAnimId(sTurnJump[direction < 5 ? direction : 0], 1);
}

static u8 PlayerFacingDirection(void)
{
	return gEventObjects[gPlayerAvatar->eventObjectId].facingDirection;
}

static void SetPlayerDirection(struct EventObject *obj, u8 direction)
{
	obj->previousMovementDirection = obj->facingDirection;
	if (!obj->facingDirectionLocked)
		obj->facingDirection = direction;
	obj->movementDirection = direction;
}

static void Bike_SetBikeStill(void)
{
	gPlayerAvatar->bikeFrameCounter = 0;
	gPlayerAvatar->bikeSpeed = HB_SPEED_STANDING;
}

/* ---- レール・でこぼこ坂 ---- */
bool8 HoennCanBikeFaceDirectionOnRail(u8 direction, u8 behavior)
{
	if (direction == DIR_EAST || direction == DIR_WEST)
	{
		if (HoennMB_IsIsolatedVerticalRail(behavior) || HoennMB_IsVerticalRail(behavior))
			return FALSE;
	}
	else
	{
		if (HoennMB_IsIsolatedHorizontalRail(behavior) || HoennMB_IsHorizontalRail(behavior))
			return FALSE;
	}
	return TRUE;
}

bool8 HoennIsPlayerNotUsingAcroBikeOnBumpySlope(void)
{
	if ((gPlayerAvatar->flags & HB_FLAG_ACRO)
	&& HoennMB_IsBumpySlope(gEventObjects[gPlayerAvatar->eventObjectId].currentMetatileBehavior))
		return FALSE;
	return TRUE;
}

void HoennBike_HandleBumpySlopeJump(void)
{
	s16 x, y;

	if (gPlayerAvatar->flags & HB_FLAG_ACRO)
	{
		PlayerGetDestCoords(&x, &y);
		if (HoennMB_IsBumpySlope(MapGridGetMetatileBehaviorAt(x, y)))
		{
			gPlayerAvatar->acroBikeState = ACRO_STATE_WHEELIE_STANDING;
			/* 素 FRLG の PlayerUseAcroBikeOnBumpySlope（0805C4F4）は空関数。エメラルドと同じ中身をここで */
			PlayerSetAnimId(ActByDir(ACT_ACRO_WHEELIE_IN_PLACE_DOWN, GetPlayerMovementDirection()), 2);
		}
	}
}

/* エメラルドの GetBikeCollision: 素の同じ関数（080BE660）を呼び、当たったら止める */
static u8 GetBikeCollision(u8 direction)
{
	u8 collision = CallGetBikeCollision(direction);
	if (collision)
		Bike_SetBikeStill();
	return collision;
}

static bool8 WillPlayerCollideWithCollision(u8 collision, u8 direction)
{
	if (direction == DIR_NORTH || direction == DIR_SOUTH)
	{
		if (collision == COL_ISOLATED_VERTICAL_RAIL || collision == COL_VERTICAL_RAIL)
			return FALSE;
	}
	else if (collision == COL_ISOLATED_HORIZONTAL_RAIL || collision == COL_HORIZONTAL_RAIL)
	{
		return FALSE;
	}
	return TRUE;
}

/* ---- エメラルドのダートの入力の処理（src/bike.c）---- */
static u8 CheckMovementInputAcroBike(u8 *newDirection, u16 newKeys, u16 heldKeys);

static bool8 HasPlayerInputTakenLongerThan4(void)
{
	/* sAcroBikeJumpTimerList = {4, 0}（[0] しか見ない）*/
	return gPlayerAvatar->dirTimerHistory[0] <= 4 && GetABSSTimer() <= 4;
}

static u8 AcroBike_GetJumpDirection(void)
{
	u32 dirHistory = gPlayerAvatar->directionHistory & 0xF;
	u32 abHistory = gPlayerAvatar->abStartSelectHistory & 0xF;

	if (abHistory == B_BUTTON && dirHistory >= DIR_SOUTH && dirHistory <= DIR_EAST
	&& HasPlayerInputTakenLongerThan4())
		return dirHistory;
	return 0;
}

static u8 AcroBikeHandleInputNormal(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
	u8 direction = GetPlayerMovementDirection();

	gPlayerAvatar->bikeFrameCounter = 0;
	if (*newDirection == DIR_NONE)
	{
		if (newKeys & B_BUTTON)
		{
			*newDirection = direction;
			gPlayerAvatar->runningState = 0;
			gPlayerAvatar->acroBikeState = ACRO_STATE_WHEELIE_STANDING;
			return ACRO_TRANS_NORMAL_TO_WHEELIE;
		}
		*newDirection = direction;
		gPlayerAvatar->runningState = 0;
		return ACRO_TRANS_FACE_DIRECTION;
	}
	if (*newDirection == direction && (heldKeys & B_BUTTON) && gPlayerAvatar->bikeSpeed == HB_SPEED_STANDING)
	{
		gPlayerAvatar->bikeSpeed++;
		gPlayerAvatar->acroBikeState = ACRO_STATE_WHEELIE_MOVING;
		return ACRO_TRANS_WHEELIE_RISING_MOVING;
	}
	if (*newDirection != direction && gPlayerAvatar->runningState != 2)
	{
		gPlayerAvatar->acroBikeState = ACRO_STATE_TURNING;
		gPlayerAvatar->newDirBackup = *newDirection;
		gPlayerAvatar->runningState = 0;
		return CheckMovementInputAcroBike(newDirection, newKeys, heldKeys);
	}
	gPlayerAvatar->runningState = 2;
	return ACRO_TRANS_MOVING;
}

static u8 AcroBikeHandleInputTurning(u8 *newDirection, unusedArg u16 newKeys, unusedArg u16 heldKeys)
{
	u8 direction;

	*newDirection = gPlayerAvatar->newDirBackup;
	gPlayerAvatar->bikeFrameCounter++;
	if (gPlayerAvatar->bikeFrameCounter > 6)
	{
		gPlayerAvatar->runningState = 1;
		gPlayerAvatar->acroBikeState = ACRO_STATE_NORMAL;
		Bike_SetBikeStill();
		return ACRO_TRANS_TURN_DIRECTION;
	}
	direction = GetPlayerMovementDirection();
	if (*newDirection == AcroBike_GetJumpDirection())
	{
		Bike_SetBikeStill();
		gPlayerAvatar->bikeSpeed = HB_SPEED_NORMAL;
		if (*newDirection == GetOppositeDirection(direction))
		{
			gPlayerAvatar->acroBikeState = ACRO_STATE_TURN_JUMP;
			return ACRO_TRANS_TURN_JUMP;
		}
		gPlayerAvatar->runningState = 2;
		gPlayerAvatar->acroBikeState = ACRO_STATE_SIDE_JUMP;
		return ACRO_TRANS_SIDE_JUMP;
	}
	*newDirection = direction;
	return ACRO_TRANS_FACE_DIRECTION;
}

static u8 AcroBikeHandleInputWheelieStanding(u8 *newDirection, unusedArg u16 newKeys, u16 heldKeys)
{
	u8 direction = GetPlayerMovementDirection();
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	gPlayerAvatar->runningState = 0;
	if (heldKeys & B_BUTTON)
	{
		gPlayerAvatar->bikeFrameCounter++;
	}
	else
	{
		gPlayerAvatar->bikeFrameCounter = 0;
		if (!HoennMB_IsBumpySlope(obj->currentMetatileBehavior))
		{
			*newDirection = direction;
			gPlayerAvatar->acroBikeState = ACRO_STATE_NORMAL;
			Bike_SetBikeStill();
			return ACRO_TRANS_WHEELIE_TO_NORMAL;
		}
	}
	if (gPlayerAvatar->bikeFrameCounter >= 40)
	{
		*newDirection = direction;
		gPlayerAvatar->acroBikeState = ACRO_STATE_BUNNY_HOP;
		Bike_SetBikeStill();
		return ACRO_TRANS_WHEELIE_HOPPING_STANDING;
	}
	if (*newDirection == direction)
	{
		gPlayerAvatar->runningState = 2;
		gPlayerAvatar->acroBikeState = ACRO_STATE_WHEELIE_MOVING;
		Bike_SetBikeStill();
		return ACRO_TRANS_WHEELIE_MOVING;
	}
	if (*newDirection == 0)
	{
		*newDirection = direction;
		return ACRO_TRANS_WHEELIE_IDLE;
	}
	gPlayerAvatar->runningState = 1;
	return ACRO_TRANS_WHEELIE_IDLE;
}

static u8 AcroBikeHandleInputBunnyHop(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
	u8 direction = GetPlayerMovementDirection();
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!(heldKeys & B_BUTTON))
	{
		Bike_SetBikeStill();
		if (HoennMB_IsBumpySlope(obj->currentMetatileBehavior))
		{
			gPlayerAvatar->acroBikeState = ACRO_STATE_WHEELIE_STANDING;
			return CheckMovementInputAcroBike(newDirection, newKeys, heldKeys);
		}
		*newDirection = direction;
		gPlayerAvatar->runningState = 0;
		gPlayerAvatar->acroBikeState = ACRO_STATE_NORMAL;
		return ACRO_TRANS_WHEELIE_TO_NORMAL;
	}
	if (*newDirection == DIR_NONE)
	{
		*newDirection = direction;
		gPlayerAvatar->runningState = 0;
		return ACRO_TRANS_WHEELIE_HOPPING_STANDING;
	}
	if (*newDirection != direction && gPlayerAvatar->runningState != 2)
	{
		gPlayerAvatar->runningState = 1;
		return ACRO_TRANS_WHEELIE_HOPPING_STANDING;
	}
	gPlayerAvatar->runningState = 2;
	return ACRO_TRANS_WHEELIE_HOPPING_MOVING;
}

static u8 AcroBikeHandleInputWheelieMoving(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
	u8 direction = PlayerFacingDirection();
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!(heldKeys & B_BUTTON))
	{
		Bike_SetBikeStill();
		if (!HoennMB_IsBumpySlope(obj->currentMetatileBehavior))
		{
			gPlayerAvatar->acroBikeState = ACRO_STATE_NORMAL;
			if (*newDirection == DIR_NONE)
			{
				*newDirection = direction;
				gPlayerAvatar->runningState = 0;
				return ACRO_TRANS_WHEELIE_TO_NORMAL;
			}
			if (*newDirection != direction && gPlayerAvatar->runningState != 2)
			{
				gPlayerAvatar->runningState = 0;
				return ACRO_TRANS_WHEELIE_TO_NORMAL;
			}
			gPlayerAvatar->runningState = 2;
			return ACRO_TRANS_WHEELIE_LOWERING_MOVING;
		}
		gPlayerAvatar->acroBikeState = ACRO_STATE_WHEELIE_STANDING;
		return CheckMovementInputAcroBike(newDirection, newKeys, heldKeys);
	}
	if (*newDirection == DIR_NONE)
	{
		*newDirection = direction;
		gPlayerAvatar->acroBikeState = ACRO_STATE_WHEELIE_STANDING;
		gPlayerAvatar->runningState = 0;
		Bike_SetBikeStill();
		return ACRO_TRANS_WHEELIE_IDLE;
	}
	if (direction != *newDirection && gPlayerAvatar->runningState != 2)
	{
		gPlayerAvatar->runningState = 0;
		return ACRO_TRANS_WHEELIE_IDLE;
	}
	gPlayerAvatar->runningState = 2;
	return ACRO_TRANS_WHEELIE_MOVING;
}

static u8 AcroBikeHandleInputSidewaysJump(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	obj->facingDirectionLocked = 0;
	SetPlayerDirection(obj, obj->facingDirection);
	gPlayerAvatar->acroBikeState = ACRO_STATE_NORMAL;
	return CheckMovementInputAcroBike(newDirection, newKeys, heldKeys);
}

static u8 AcroBikeHandleInputTurnJump(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
	gPlayerAvatar->acroBikeState = ACRO_STATE_NORMAL;
	return CheckMovementInputAcroBike(newDirection, newKeys, heldKeys);
}

static u8 CheckMovementInputAcroBike(u8 *newDirection, u16 newKeys, u16 heldKeys)
{
	switch (gPlayerAvatar->acroBikeState)
	{
		case ACRO_STATE_TURNING:          return AcroBikeHandleInputTurning(newDirection, newKeys, heldKeys);
		case ACRO_STATE_WHEELIE_STANDING: return AcroBikeHandleInputWheelieStanding(newDirection, newKeys, heldKeys);
		case ACRO_STATE_BUNNY_HOP:        return AcroBikeHandleInputBunnyHop(newDirection, newKeys, heldKeys);
		case ACRO_STATE_WHEELIE_MOVING:   return AcroBikeHandleInputWheelieMoving(newDirection, newKeys, heldKeys);
		case ACRO_STATE_SIDE_JUMP:        return AcroBikeHandleInputSidewaysJump(newDirection, newKeys, heldKeys);
		case ACRO_STATE_TURN_JUMP:        return AcroBikeHandleInputTurnJump(newDirection, newKeys, heldKeys);
		default:
			gPlayerAvatar->acroBikeState = ACRO_STATE_NORMAL;
			return AcroBikeHandleInputNormal(newDirection, newKeys, heldKeys);
	}
}

/* ---- エメラルドのダートの動き（src/bike.c）---- */
static void AcroBikeTransition_FaceDirection(u8 direction)
{
	PlayerFaceDirection(direction);
}

static void AcroBikeTransition_TurnDirection(u8 direction)
{
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
		direction = obj->movementDirection;
	PlayerFaceDirection(direction);
}

/* ふつうに進む: 素の BikeTransition_MoveDirection（動きの表 083AF240 の 2 番）と同じ中身
   （レール・段差・割れる氷・岩の階段・CFRU のターボのフック 080BE5F8 まで）なので素を呼ぶ */
static void AcroBikeTransition_Moving(u8 direction)
{
	if (SidewaysStairs_TryBikeStep(direction))   /* 斜めの階段（src/sideways_stairs.c・2026-09-30）*/
		return;
	sVanillaBikeTransitions[2](direction);
}

static void AcroBikeTransition_NormalToWheelie(u8 direction)
{
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
		direction = obj->movementDirection;
	PlayerStartWheelie(direction);
}

static void AcroBikeTransition_WheelieToNormal(u8 direction)
{
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
		direction = obj->movementDirection;
	PlayerEndWheelie(direction);
}

static void AcroBikeTransition_WheelieIdle(u8 direction)
{
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
		direction = obj->movementDirection;
	PlayerIdleWheelie(direction);
}

static void AcroBikeTransition_WheelieHoppingStanding(u8 direction)
{
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
		direction = obj->movementDirection;
	PlayerStandingHoppingWheelie(direction);
}

static void AcroBikeTransition_WheelieHoppingMoving(u8 direction)
{
	u8 collision;
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
	{
		AcroBikeTransition_WheelieHoppingStanding(obj->movementDirection);
		return;
	}
	collision = GetBikeCollision(direction);
	if (collision && collision != COL_WHEELIE_HOP)
	{
		if (collision == COL_LEDGE_JUMP)
		{
			PlayerLedgeHoppingWheelie(direction);
			return;
		}
		if (collision >= COL_STOP_SURFING && collision <= COL_DIRECTIONAL_STAIR_WARP)
			return;
		if (collision < COL_VERTICAL_RAIL)
		{
			AcroBikeTransition_WheelieHoppingStanding(direction);
			return;
		}
	}
	PlayerMovingHoppingWheelie(direction);
}

static void AcroBikeTransition_SideJump(u8 direction)
{
	u8 collision;
	struct EventObject *obj;

	collision = GetBikeCollision(direction);
	if (collision)
	{
		if (collision == COL_PUSHED_BOULDER)
			return;
		if (collision < COL_ISOLATED_VERTICAL_RAIL)
		{
			AcroBikeTransition_TurnDirection(direction);
			return;
		}
		if (WillPlayerCollideWithCollision(collision, direction) == FALSE)
		{
			AcroBikeTransition_TurnDirection(direction);
			return;
		}
	}
	obj = &gEventObjects[gPlayerAvatar->eventObjectId];
	PlaySE(HB_SE_BIKE_HOP);
	obj->facingDirectionLocked = 1;
	PlayerSetAnimId(GetJumpMovementAction(direction), 2);
}

static void AcroBikeTransition_TurnJump(u8 direction)
{
	PlayerAcroTurnJump(direction);
}

static void AcroBikeTransition_WheelieMoving(u8 direction)
{
	u8 collision;
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
	{
		PlayerIdleWheelie(obj->movementDirection);
		return;
	}
	collision = GetBikeCollision(direction);
	if (collision > COL_NONE && collision < COL_VERTICAL_RAIL)
	{
		if (collision == COL_LEDGE_JUMP)
			PlayerLedgeHoppingWheelie(direction);
		else if (collision == COL_WHEELIE_HOP)
			PlayerIdleWheelie(direction);
		else if (collision < COL_STOP_SURFING)
		{
			if (HoennMB_IsBumpySlope(obj->currentMetatileBehavior))
				PlayerIdleWheelie(direction);
			else
				PlayerWheelieInPlace(direction);
		}
		return;
	}
	PlayerWheelieMove(direction);
	gPlayerAvatar->runningState = 2;
}

static void AcroBikeTransition_WheelieRisingMoving(u8 direction)
{
	u8 collision;
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
	{
		PlayerStartWheelie(obj->movementDirection);
		return;
	}
	collision = GetBikeCollision(direction);
	if (collision > COL_NONE && collision < COL_VERTICAL_RAIL)
	{
		if (collision == COL_LEDGE_JUMP)
			PlayerLedgeHoppingWheelie(direction);
		else if (collision == COL_WHEELIE_HOP)
			PlayerIdleWheelie(direction);
		else if (collision < COL_STOP_SURFING)
		{
			if (HoennMB_IsBumpySlope(obj->currentMetatileBehavior))
				PlayerIdleWheelie(direction);
			else
				PlayerWheelieInPlace(direction);
		}
		return;
	}
	PlayerPopWheelieWhileMoving(direction);
	gPlayerAvatar->runningState = 2;
}

static void AcroBikeTransition_WheelieLoweringMoving(u8 direction)
{
	u8 collision;
	struct EventObject *obj = &gEventObjects[gPlayerAvatar->eventObjectId];

	if (!HoennCanBikeFaceDirectionOnRail(direction, obj->currentMetatileBehavior))
	{
		PlayerEndWheelie(obj->movementDirection);
		return;
	}
	collision = GetBikeCollision(direction);
	if (collision > COL_NONE && collision < COL_VERTICAL_RAIL)
	{
		if (collision == COL_LEDGE_JUMP)
			PlayerJumpLedge(direction);
		else if (collision < COL_STOP_SURFING || collision > COL_DIRECTIONAL_STAIR_WARP)
			PlayerEndWheelie(direction);
		return;
	}
	PlayerEndWheelieWhileMoving(direction);
}

static void AcroBikeTransition(u8 trans, u8 direction)
{
	switch (trans)
	{
		case ACRO_TRANS_FACE_DIRECTION:          AcroBikeTransition_FaceDirection(direction); break;
		case ACRO_TRANS_TURN_DIRECTION:          AcroBikeTransition_TurnDirection(direction); break;
		case ACRO_TRANS_MOVING:                  AcroBikeTransition_Moving(direction); break;
		case ACRO_TRANS_NORMAL_TO_WHEELIE:       AcroBikeTransition_NormalToWheelie(direction); break;
		case ACRO_TRANS_WHEELIE_TO_NORMAL:       AcroBikeTransition_WheelieToNormal(direction); break;
		case ACRO_TRANS_WHEELIE_IDLE:            AcroBikeTransition_WheelieIdle(direction); break;
		case ACRO_TRANS_WHEELIE_HOPPING_STANDING: AcroBikeTransition_WheelieHoppingStanding(direction); break;
		case ACRO_TRANS_WHEELIE_HOPPING_MOVING:  AcroBikeTransition_WheelieHoppingMoving(direction); break;
		case ACRO_TRANS_SIDE_JUMP:               AcroBikeTransition_SideJump(direction); break;
		case ACRO_TRANS_TURN_JUMP:               AcroBikeTransition_TurnJump(direction); break;
		case ACRO_TRANS_WHEELIE_MOVING:          AcroBikeTransition_WheelieMoving(direction); break;
		case ACRO_TRANS_WHEELIE_RISING_MOVING:   AcroBikeTransition_WheelieRisingMoving(direction); break;
		case ACRO_TRANS_WHEELIE_LOWERING_MOVING: AcroBikeTransition_WheelieLoweringMoving(direction); break;
	}
}

/* 素の MovePlayerOnBike（080BE35C）の差し替え（functionrewrites）*/
void HoennMovePlayerOnBike(u8 direction, u16 newKeys, u16 heldKeys)
{
	u8 d = direction;

	if (GetBikeMode() == BIKE_MODE_DIRT && (gPlayerAvatar->flags & HB_FLAG_ACRO))
	{
		u8 trans = CheckMovementInputAcroBike(&d, newKeys, heldKeys);
		AcroBikeTransition(trans, d);
		return;
	}

	/* 通常・マッハ: 素の処理そのもの（入力の処理の表 083AF254 → 動きの表 083AF240）。
	   ★ 素の入力の処理は 0〜2 しか無いので、ダートの状態が残っていたら戻す */
	if (gPlayerAvatar->acroBikeState > 2)
		BikeClearAcroState();
	{
		u8 trans = CallGetBikeTransitionId(&d, newKeys, heldKeys);
		if (trans == 2 && SidewaysStairs_TryBikeStep(d))   /* 「進む」で斜めの階段なら斜めの速歩き（src/sideways_stairs.c・2026-09-30）*/
			return;
		sVanillaBikeTransitions[trans](d);
	}
}
