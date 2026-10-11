#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/fieldmap.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/random.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/wild_encounter.h"
#include "../include/new/wild_encounter.h"
#include "../include/pokemon.h"
#include "../include/new/frontier.h"
#include "../include/new/overworld.h"
#include "../include/item.h"
#include "../include/new/item.h"
#include "../include/new/util.h"
#include "../include/constants/items.h"
#include "../include/constants/songs.h"
#include "Tables/hoenn_script_flags.h"

/*
 * ホウエンのバトルピラミッド（#5・2026-10-03 ユーザー「全部一気に」→「エメラルドどおりで一気に」）。
 * docs/research/2026-09-28_battle-frontier-npcs.md の「バトルピラミッド」。
 *
 * pokeemerald src/battle_pyramid.c を FRLG（CFRU）の仕組みで書き直したもの:
 *   ・セーブの frontier.* → 予約変数（HOENN_VAR_PYRAMID_DATA の struct HoennPyramidData・連勝は HOENN_VAR_PYRAMID_STREAK）。
 *     何階かは施設で共用の HOENN_VAR_TOWER_BATTLE_NUM・レベルは HOENN_VAR_TOWER_LVL_MODE
 *   ・階の地図（エメラルドの InitBattlePyramidMap / GenerateBattlePyramidFloorLayout）→ 階のマップスクリプトの ON_LOAD から
 *     HoennPyramid_FloorOnLoad。読み込んだあとの地図（素 FRLG の VMap・0x03005080）を 16 部屋で上書きする。
 *     人と道具（LoadBattlePyramidObjectEventTemplates）も同じところで。ワープで入ったときだけ（ON_TRANSITION が一時変数に印）
 *   ・出口（エメラルドは挙動 MB_BATTLE_PYRAMID_WARP）→ gMapHeader.events を出口の部屋ごとの座標イベントの表に差し替える
 *   ・トレーナーの「見つかった」→ CFRU の仕組みのまま。台本は trainerbattle 1・番号は 0x7F0 + 何人目
 *     （src/hoenn_trainer_flags.c が一時フラグ 0x11〜0x18 に読み替え・src/overworld.c の差し込み口 patches/0130 が
 *      戦う前にフロンティアの 300 人（BATTLE_TOWER_SPECIAL_TID）へ差し替える）
 *   ・野生（GenerateBattlePyramidWildMon）→ チューブの野生の差し込み口（src/hoenn_pike.c・patches/0126）から呼ぶ
 *   ・暗さ（pyramidLightRadius）→ 素 FRLG のフラッシュの仕組み。flashLevel を 1 にして、半径だけここの値
 *     （WriteFlashScanlineEffectBuffer を HoennPyramid_WriteFlashBuffer に置き換え・functionrewrites）
 *   ・バトルバッグ（battle_pyramid_bag.c）→ 画面は作らず、CFRU のバッグのポケットの表（0x20397D8・SetMemoryForBagStorage）の
 *     「どうぐ」を 10 枠の置き場（予約変数 HOENN_VAR_PYRAMID_BAG・レベルごと）へ向ける。ほかのポケットは 0 枠。
 *     表が作り直されるたび（ワープ・戦い）に src/item.c の差し込み口（patches/0131）が HoennPyramid_ApplyBag を呼ぶ。
 *     戦いの中でも使える（src/move_menu.c の差し込み口・エメラルドの BATTLE_TYPE_FRONTIER_NO_PYRAMID）
 */

#define PYRAMID_FACILITY 6                  //HOENN_VAR_FRONTIER_FACILITY（src/hoenn_tower.c の HOENN_FACILITY_PYRAMID）
#define MAX_PYRAMID_TRAINERS 8
#define NUM_PYRAMID_FLOOR_SQUARES 16
#define PYRAMID_FLOOR_SQUARES_WIDE 4
#define PYRAMID_FLOOR_SQUARES_HIGH 4
#define NUM_LAYOUT_OFFSETS 8
#define FRONTIER_STAGES_PER_CHALLENGE 7
#define TOTAL_PYRAMID_ROUNDS 20
#define PICKUP_ITEMS_PER_ROUND 10
#define FRONTIER_LVL_50 0
#define FRONTIER_LVL_OPEN 1
#define PYRAMID_LIGHT_MAX 120
#define PYRAMID_TRAINER_TID_BASE 0x7F0      //台本の trainerbattle の番号（src/hoenn_trainer_flags.c・patches/0130 と同じ）
#define PYRAMID_TRAINER_FLAG_BASE 0x11      //一時フラグ 0x11〜0x18（エメラルドの pyramidTrainerFlags・階が変わると消える）
#define VAR_TEMP_OUTCOME 0x4002             //戦いの結果: 1 勝った / 2 トレーナーに負けた / 3 野生に負けた（スクリプトと同じ）
#define VAR_TEMP_SLOT 0x4003                //いま戦っているトレーナーが何人目か
#define VAR_TEMP_EXIT 0x4004                //出口の座標イベントの条件（0 のとき）
#define VAR_TEMP_FRESH 0x4005               //ワープで入った（ON_TRANSITION → ON_LOAD で人と道具を置き、入口に立たせる）
#define MAP_OFFSET 7
#define MAP_OFFSET_W (MAP_OFFSET * 2 + 1)
#define MAP_OFFSET_H (MAP_OFFSET * 2)
#define PYR_METATILE_ID_MASK 0x3FF
#define ELEVATION_COLLISION_MASK 0xFC00

#define OBJ_TRAINERS 0
#define OBJ_ITEMS 1
#define OBJ_POSITIONS_UNIFORM 0
#define OBJ_POSITIONS_IN_AND_NEAR_ENTRANCE 1
#define OBJ_POSITIONS_IN_AND_NEAR_EXIT 2
#define OBJ_POSITIONS_NEAR_ENTRANCE 3
#define OBJ_POSITIONS_NEAR_EXIT 4

#define HINT_EXIT_DIRECTION 0
#define HINT_REMAINING_ITEMS 1
#define HINT_REMAINING_TRAINERS 2
#define HINT_EXIT_SHORT_REMAINING_TRAINERS 3
#define HINT_EXIT_SHORT_REMAINING_ITEMS 4
#define HINT_EXIT_MEDIUM_REMAINING_TRAINERS 5
#define HINT_EXIT_MEDIUM_REMAINING_ITEMS 6
#define HINT_EXIT_FAR_REMAINING_TRAINERS 7
#define HINT_EXIT_FAR_REMAINING_ITEMS 8

#define PYRAMID_LIGHT_SET_RADIUS 0
#define PYRAMID_LIGHT_INCR_RADIUS 1

#define PYRAMID_WILD_MONS_PER_ROUND 8   //1 周の野生の表の数（hoenn_pyramid_data.h・tools/frontier/verify_frontier_pyramid.py が数える）

struct PyramidWildMon
{
	u16 species;
	u8 lvl;
	u8 abilityNum;
	u16 moves[MAX_MON_MOVES];
};

struct PyramidFloorTemplate
{
	u8 numItems;
	u8 numTrainers;
	u8 itemPositions;
	u8 trainerPositions;
	u8 runMultiplier;
	u8 layoutOffsets[NUM_LAYOUT_OFFSETS];
};

#include "hoenn_pyramid_data.h"

/* ---- 予約変数の置き場（エメラルドの frontier.pyramid* ） ---------------------------------------------- */
struct HoennPyramidData
{
	u16 randoms[4];                       /* 0x51B7〜0x51BA pyramidRandoms */
	u16 prize;                            /* 0x51BB pyramidPrize */
	u16 trainerIds[MAX_PYRAMID_TRAINERS]; /* 0x51BC〜0x51C3 trainerIds（この階のトレーナー・フロンティアの 300 人の番号） */
	u16 lightRadius;                      /* 0x51C4 pyramidLightRadius */
	u16 activeFlags;                      /* 0x51C5 winStreakActiveFlags の STREAK_PYRAMID_50 / _OPEN（bit0 / bit1）・bit15 = バトルバッグを使っている */
};
#define PYRAMID_BAG_IN_USE 0x8000
#define PYRAMID_BAG_ITEMS_COUNT 10
//バトルバッグ（エメラルドの frontier.pyramidBag）: レベルごとに 10 枠・素のバッグと同じ ItemSlot（数は暗号化したまま）・0x51C6〜0x51ED
#define PYRAMID_BAG_SLOTS ((struct ItemSlot *)&gExpandedVars[HOENN_VAR_PYRAMID_BAG - 0x5000])
#define gBagPocketsCFRU ((struct BagPocketsCFRU *)0x20397D8)   //src/item.c の gBagPockets
struct BagPocketsCFRU
{
	struct ItemSlot* itemRam;
	u32 itemAmount;
	struct ItemSlot* keyItemRam;
	u32 keyItemAmount;
	struct ItemSlot* pokeBallRam;
	u32 pokeBallAmount;
	struct ItemSlot* tmRam;
	u32 tmAmount;
	struct ItemSlot* berryRam;
	u32 berryAmount;
};
#define HOENN_PYRAMID_DATA_FIRST 0x51B7
#define HOENN_PYRAMID_DATA_LAST 0x51C5
#define PYR ((struct HoennPyramidData *)&gExpandedVars[HOENN_VAR_PYRAMID_DATA - 0x5000])

struct BackupMapLayoutFR { s32 width; s32 height; u16 *map; };
#define VMap (*(struct BackupMapLayoutFR *)0x03005080)   //素 FRLG の VMap（MapGridGetMetatileIdAt 08058704 が読む）

/* ---- 素 FRLG ---------------------------------------------------------------------------------------- */
/* ★ 2026-10-04 ユーザー「ムロジムなんか真っ暗らしい」: 素の表 0838D658 は 5 段。ムロジムは段 1〜7 を使うので、
   9 段に広げた表（src/hoenn_flash.c・素の関数のリテラル 0807E7DC を repoints で向けていたが、この関数を置き換えたので届かない）を引く */
extern const u16 gHoennFlashLevelToRadius[9];
#define SetFlashScanlineEffectWindowBoundaries ((void (*)(u16 *, s32, s32, s32))(0x0807E55C | 1))
#define ShowFieldMessageFR ((void (*)(const u8 *))(0x08068CFC | 1))

/* ---- 自前の他のファイル ------------------------------------------------------------------------------ */
extern u16 HoennFrontier_ChooseTrainer(u16 challengeNum, u16 battleNum); //src/hoenn_frontier.c
extern void HoennTower_SetBattleLevel(void);                              //src/hoenn_tower.c
extern const u8 HoennPyramid_EventScript_ItemBall[];                      //assembly/frontier/pyramid_scripts.s
extern const u8 HoennPyramid_EventScript_WarpToNextFloor[];
extern const u8* const gHoennPyramidTrainerScripts[MAX_PYRAMID_TRAINERS];

bool8 HoennPyramid_OnFloor(void);
bool8 HoennHill_OnFloor(void); //src/hoenn_trainer_hill.c

static u8 LvlMode(void)
{
	return VarGet(HOENN_VAR_TOWER_LVL_MODE) != 0 ? FRONTIER_LVL_OPEN : FRONTIER_LVL_50;
}

static u16 Floor(void)
{
	return VarGet(HOENN_VAR_TOWER_BATTLE_NUM);
}

static u16 WinStreak(void)
{
	return VarGet(HOENN_VAR_PYRAMID_STREAK + LvlMode());
}

static u32 Round(void)
{
	u32 round = (WinStreak() / FRONTIER_STAGES_PER_CHALLENGE) % TOTAL_PYRAMID_ROUNDS;
	return MathMin(round, TOTAL_PYRAMID_ROUNDS - 1);
}

//階の地図にいるか（エメラルドの CurrentBattlePyramidLocation の FLOOR・地図の番号で見る）
bool8 HoennPyramid_OnFloor(void)
{
	return gSaveBlock1->location.mapGroup == HOENN_PYRAMID_FLOOR_GROUP && gSaveBlock1->location.mapNum == HOENN_PYRAMID_FLOOR_NUM;
}

static bool8 OnTop(void)
{
	return gSaveBlock1->location.mapGroup == HOENN_PYRAMID_FLOOR_GROUP && gSaveBlock1->location.mapNum == HOENN_PYRAMID_TOP_NUM;
}

/* ---- 階の型・部屋の並び（エメラルドどおり） ----------------------------------------------------------- */
static u8 GetPyramidFloorTemplateId(void)
{
	int i;
	int rand = PYR->randoms[3] % 100;
	int floor = MathMin(Floor(), FRONTIER_STAGES_PER_CHALLENGE - 1);

	for (i = sFloorTemplateOffsets[floor]; i < (int) NELEMS(sPyramidFloorTemplateOptions); i++)
	{
		if (rand < sPyramidFloorTemplateOptions[i][0])
			return sPyramidFloorTemplateOptions[i][1];
	}
	return 0;
}

static void GetPyramidFloorLayoutOffsets(u8 *layoutOffsets)
{
	int i;
	int rand = (PYR->randoms[0]) | (PYR->randoms[1] << 16);
	u8 id = GetPyramidFloorTemplateId();

	for (i = 0; i < NUM_PYRAMID_FLOOR_SQUARES; i++)
	{
		layoutOffsets[i] = sPyramidFloorTemplates[id].layoutOffsets[((rand) % NUM_LAYOUT_OFFSETS + NUM_LAYOUT_OFFSETS) % NUM_LAYOUT_OFFSETS];
		rand >>= 3;
		if (i == 7)
		{
			rand = (PYR->randoms[2]) | (PYR->randoms[3] << 16);
			rand >>= 8;
		}
	}
}

static void GetPyramidEntranceAndExitSquareIds(u8 *entranceSquareId, u8 *exitSquareId)
{
	*entranceSquareId = PYR->randoms[3] % NUM_PYRAMID_FLOOR_SQUARES;
	*exitSquareId = PYR->randoms[0] % NUM_PYRAMID_FLOOR_SQUARES;

	if (*entranceSquareId == *exitSquareId)
	{
		*entranceSquareId = (PYR->randoms[3] + 1) % NUM_PYRAMID_FLOOR_SQUARES;
		*exitSquareId = (PYR->randoms[0] + NUM_PYRAMID_FLOOR_SQUARES - 1) % NUM_PYRAMID_FLOOR_SQUARES;
	}
}

static const struct MapLayout* SquareLayout(u8 offset)
{
	return Overworld_GetMapHeaderByGroupAndId(PYRAMID_SQUARE_GROUP, PYRAMID_SQUARE_NUM + offset)->mapLayout;
}

/* ---- 出口（座標イベント 1 つ・出口の部屋の 0x28E のマス） -------------------------------------------- */
//出口の位置は「置き場 16 × 部屋の型 16」の ROM の表（src/hoenn_pyramid_data.h の gHoennPyramidFloorEvents・生成）から選ぶ

static void SetFloorEvents(void)
{
	u8 entrance, exit;
	u8 offsets[NUM_PYRAMID_FLOOR_SQUARES];

	GetPyramidFloorLayoutOffsets(offsets);
	GetPyramidEntranceAndExitSquareIds(&entrance, &exit);
	gMapHeader.events = (const struct MapEvents*) &gHoennPyramidFloorEvents[exit][offsets[exit]];
}

/* ---- 地図（エメラルドの GenerateBattlePyramidFloorLayout） --------------------------------------------- */
static void GenerateBattlePyramidFloorLayout(bool8 setPlayerPosition)
{
	int y, x;
	int i;
	u8 entranceSquareId, exitSquareId;
	u8 floorLayoutOffsets[NUM_PYRAMID_FLOOR_SQUARES];

	GetPyramidFloorLayoutOffsets(floorLayoutOffsets);
	GetPyramidEntranceAndExitSquareIds(&entranceSquareId, &exitSquareId);
	for (i = 0; i < NUM_PYRAMID_FLOOR_SQUARES; i++)
	{
		u16 *map;
		const struct MapLayout *mapLayout = SquareLayout(floorLayoutOffsets[i]);
		const u16 *layoutMap = mapLayout->map;

		VMap.width = mapLayout->width * PYRAMID_FLOOR_SQUARES_WIDE + MAP_OFFSET_W;
		VMap.height = mapLayout->height * PYRAMID_FLOOR_SQUARES_HIGH + MAP_OFFSET_H;
		map = VMap.map;
		map += VMap.width * (MAP_OFFSET + (i / PYRAMID_FLOOR_SQUARES_WIDE * mapLayout->height))
		    +  MAP_OFFSET + (i % PYRAMID_FLOOR_SQUARES_WIDE * mapLayout->width);
		for (y = 0; y < mapLayout->height; y++)
		{
			for (x = 0; x < mapLayout->width; x++)
			{
				if ((layoutMap[x] & PYR_METATILE_ID_MASK) != PYRAMID_METATILE_EXIT)
				{
					map[x] = layoutMap[x];
				}
				else if (i != exitSquareId)
				{
					if (i == entranceSquareId && setPlayerPosition)
					{
						gSaveBlock1->pos.x = (mapLayout->width * (i % PYRAMID_FLOOR_SQUARES_WIDE)) + x;
						gSaveBlock1->pos.y = (mapLayout->height * (i / PYRAMID_FLOOR_SQUARES_WIDE)) + y;
					}
					map[x] = (layoutMap[x] & ELEVATION_COLLISION_MASK) | PYRAMID_METATILE_FLOOR;
				}
				else
				{
					map[x] = layoutMap[x];
				}
			}
			map += MAP_OFFSET_W + (mapLayout->width * PYRAMID_FLOOR_SQUARES_WIDE);
			layoutMap += mapLayout->width;
		}
	}
}

/* ---- 人と道具（エメラルドの LoadBattlePyramidObjectEventTemplates） ------------------------------------- */
static u16 GetUniqueTrainerId(u8 objectEventId)
{
	int i;
	u16 trainerId;
	u32 challengeNum = WinStreak() / FRONTIER_STAGES_PER_CHALLENGE;
	u32 floor = Floor();

	if (floor == FRONTIER_STAGES_PER_CHALLENGE)
		++challengeNum;
	do
	{
		trainerId = HoennFrontier_ChooseTrainer(challengeNum, floor);
		if (trainerId >= NUM_SPECIAL_TOWER_TRAINERS)
			trainerId = Random() % NUM_SPECIAL_TOWER_TRAINERS;
		for (i = 0; i < objectEventId; i++)
		{
			if (PYR->trainerIds[i] == trainerId)
				break;
		}
	} while (i != objectEventId);

	return trainerId;
}

static void SetTemplateGfx(struct EventObjectTemplate *t, u16 gfx)
{
	t->graphicsIdLowerByte = gfx & 0xFF;
	t->graphicsIdUpperByte = gfx >> 8;
}

//置けたら FALSE（エメラルドと同じ向き）
static bool8 TrySetPyramidObjectEventPositionAtCoords(u8 objType, u8 x, u8 y, u8 *floorLayoutOffsets, u8 squareId, u8 objectEventId)
{
	int i, j;
	const struct PyramidSquareObject *objs = sPyramidSquareObjects[floorLayoutOffsets[squareId]];
	u8 count = sPyramidSquareObjectCount[floorLayoutOffsets[squareId]];
	struct EventObjectTemplate *floorEvents = gSaveBlock1->eventObjectTemplates;

	for (i = 0; i < count; i++)
	{
		if (objs[i].x != x || objs[i].y != y)
			continue;

		if (objType != OBJ_TRAINERS || objs[i].isItem)
		{
			if (objType != OBJ_ITEMS || !objs[i].isItem)
				continue;
		}

		//同じ位置に置いていないか
		for (j = 0; j < objectEventId; j++)
		{
			if (floorEvents[j].x == x + ((squareId % 4) * 8) && floorEvents[j].y == y + ((squareId / 4) * 8))
				break;
		}

		if (j == objectEventId)
		{
			struct EventObjectTemplate *t = &floorEvents[objectEventId];

			Memset(t, 0, sizeof(*t));
			t->localId = objectEventId + 1;
			t->x = x + (squareId % 4) * 8;
			t->y = y + (squareId / 4) * 8;
			t->elevation = objs[i].elevation;
			t->movementType = objs[i].movementType;
			t->movementRangeX = objs[i].rangeX;
			t->movementRangeY = objs[i].rangeY;
			if (objs[i].isItem)
			{
				SetTemplateGfx(t, PYRAMID_GFX_ITEM_BALL);
				t->script = HoennPyramid_EventScript_ItemBall;
			}
			else
			{
				u16 id = GetUniqueTrainerId(objectEventId);

				SetTemplateGfx(t, gSpecialTowerTrainers[id].owNum);
				t->trainerType = 1; //TRAINER_TYPE_NORMAL
				t->trainerRange_berryTreeId = objs[i].sight;
				t->script = gHoennPyramidTrainerScripts[objectEventId];
				PYR->trainerIds[objectEventId] = id;
			}
			return FALSE;
		}
	}

	return TRUE;
}

static bool8 TrySetPyramidObjectEventPositionInSquare(u8 objType, u8 *floorLayoutOffsets, u8 squareId, u8 objectEventId)
{
	int x, y;

	if (PYR->randoms[0] & 1)
	{
		for (y = 7; y > -1; y--)
		{
			for (x = 7; x > -1; x--)
			{
				if (!TrySetPyramidObjectEventPositionAtCoords(objType, x, y, floorLayoutOffsets, squareId, objectEventId))
					return FALSE;
			}
		}
	}
	else
	{
		for (y = 0; y < 8; y++)
		{
			for (x = 0; x < 8; x++)
			{
				if (!TrySetPyramidObjectEventPositionAtCoords(objType, x, y, floorLayoutOffsets, squareId, objectEventId))
					return FALSE;
			}
		}
	}

	return TRUE;
}

static void SetPyramidObjectPositionsUniformly(u8 objType)
{
	int i;
	int numObjects;
	int objectStartIndex;
	int squareId;
	u32 bits = 0;
	u8 id = GetPyramidFloorTemplateId();
	u8 floorLayoutOffsets[NUM_PYRAMID_FLOOR_SQUARES];

	GetPyramidFloorLayoutOffsets(floorLayoutOffsets);
	squareId = PYR->randoms[2] % NUM_PYRAMID_FLOOR_SQUARES;
	if (objType == OBJ_TRAINERS)
	{
		numObjects = sPyramidFloorTemplates[id].numTrainers;
		objectStartIndex = 0;
	}
	else
	{
		numObjects = sPyramidFloorTemplates[id].numItems;
		objectStartIndex = sPyramidFloorTemplates[id].numTrainers;
	}

	for (i = 0; i < numObjects; i++)
	{
		do
		{
			do
			{
				if (bits & 1)
				{
					if (!((1 << squareId) & PYR->randoms[3]))
						bits |= 2;
				}
				else
				{
					if ((1 << squareId) & PYR->randoms[3])
						bits |= 2;
				}
				if (++squareId >= NUM_PYRAMID_FLOOR_SQUARES)
					squareId = 0;

				if (squareId == PYR->randoms[2] % NUM_PYRAMID_FLOOR_SQUARES)
				{
					if (bits & 1)
						bits |= 6;
					else
						bits |= 1;
				}
			} while (!(bits & 2));

		} while (!(bits & 4) && TrySetPyramidObjectEventPositionInSquare(objType, floorLayoutOffsets, squareId, objectStartIndex + i));
		bits &= 1;
	}
}

static bool8 SetPyramidObjectPositionsInAndNearSquare(u8 objType, u8 squareId)
{
	int i;
	int objectStartIndex;
	int borderedIndex = 0;
	int r7 = 0;
	int numPlacedObjects = 0;
	int numObjects;
	u8 id = GetPyramidFloorTemplateId();
	u8 floorLayoutOffsets[NUM_PYRAMID_FLOOR_SQUARES];

	GetPyramidFloorLayoutOffsets(floorLayoutOffsets);
	if (objType == OBJ_TRAINERS)
	{
		numObjects = sPyramidFloorTemplates[id].numTrainers;
		objectStartIndex = 0;
	}
	else
	{
		numObjects = sPyramidFloorTemplates[id].numItems;
		objectStartIndex = sPyramidFloorTemplates[id].numTrainers;
	}

	for (i = 0; i < numObjects; i++)
	{
		if (r7 == 0)
		{
			if (TrySetPyramidObjectEventPositionInSquare(objType, floorLayoutOffsets, squareId, objectStartIndex + i))
				r7 = 1;
			else
				numPlacedObjects++;
		}
		if (r7 & 1)
		{
			if (TrySetPyramidObjectEventPositionInSquare(objType, floorLayoutOffsets, sBorderedSquareIds[squareId][borderedIndex], objectStartIndex + i))
			{
				do
				{
					borderedIndex++;
					if (sBorderedSquareIds[squareId][borderedIndex] == 0xFF || borderedIndex >= 4)
						borderedIndex = 0;
					r7 += 2;
				} while (r7 >> 1 != 4 && TrySetPyramidObjectEventPositionInSquare(objType, floorLayoutOffsets, sBorderedSquareIds[squareId][borderedIndex], objectStartIndex + i));
				numPlacedObjects++;
			}
			else
			{
				borderedIndex++;
				if (sBorderedSquareIds[squareId][borderedIndex] == 0xFF || borderedIndex >= 4)
					borderedIndex = 0;
				numPlacedObjects++;
			}
		}

		if (r7 >> 1 == 4)
			break;

		r7 &= 1;
	}

	return (numObjects / 2) > numPlacedObjects;
}

static bool8 SetPyramidObjectPositionsNearSquare(u8 objType, u8 squareId)
{
	int i;
	int objectStartIndex;
	int borderOffset = 0;
	int numPlacedObjects = 0;
	int r8 = 0;
	int numObjects;
	u8 id = GetPyramidFloorTemplateId();
	u8 floorLayoutOffsets[NUM_PYRAMID_FLOOR_SQUARES];

	GetPyramidFloorLayoutOffsets(floorLayoutOffsets);
	if (objType == OBJ_TRAINERS)
	{
		numObjects = sPyramidFloorTemplates[id].numTrainers;
		objectStartIndex = 0;
	}
	else
	{
		numObjects = sPyramidFloorTemplates[id].numItems;
		objectStartIndex = sPyramidFloorTemplates[id].numTrainers;
	}

	for (i = 0; i < numObjects; i++)
	{
		if (TrySetPyramidObjectEventPositionInSquare(objType, floorLayoutOffsets, sBorderedSquareIds[squareId][borderOffset], objectStartIndex + i))
		{
			do
			{
				borderOffset++;
				if (sBorderedSquareIds[squareId][borderOffset] == 0xFF || borderOffset >= 4)
					borderOffset = 0;
				r8++;
			} while (r8 != 4 && TrySetPyramidObjectEventPositionInSquare(objType, floorLayoutOffsets, sBorderedSquareIds[squareId][borderOffset], objectStartIndex + i));
			numPlacedObjects++;
		}
		else
		{
			borderOffset++;
			if (sBorderedSquareIds[squareId][borderOffset] == 0xFF || borderOffset >= 4)
				borderOffset = 0;
			numPlacedObjects++;
		}

		if (r8 == 4)
			break;
	}

	return (numObjects / 2) > numPlacedObjects;
}

//人と道具を置く。置き場の残り（地図の人の数 PYRAMID_MAX_OBJECTS まで）は画面の外へ
static void LoadBattlePyramidObjectEventTemplates(void)
{
	int i;
	u8 id;
	u8 entranceSquareId, exitSquareId;
	struct EventObjectTemplate *t = gSaveBlock1->eventObjectTemplates;

	for (i = 0; i < MAX_PYRAMID_TRAINERS; i++)
		PYR->trainerIds[i] = 0xFFFF;

	id = GetPyramidFloorTemplateId();
	GetPyramidEntranceAndExitSquareIds(&entranceSquareId, &exitSquareId);
	for (i = 0; i < 64; i++)
	{
		Memset(&t[i], 0, sizeof(t[i]));
		t[i].x = 0x7FFF;
		t[i].y = 0x7FFF;
		SetTemplateGfx(&t[i], PYRAMID_GFX_ITEM_BALL);
		t[i].script = HoennPyramid_EventScript_ItemBall;
		t[i].localId = (i < PYRAMID_MAX_OBJECTS) ? i + 1 : 0;
	}
	for (i = 0; i < 2; i++)
	{
		u8 objectPositionsType;

		if (i == OBJ_TRAINERS)
			objectPositionsType = sPyramidFloorTemplates[id].trainerPositions;
		else
			objectPositionsType = sPyramidFloorTemplates[id].itemPositions;

		switch (objectPositionsType) {
			case OBJ_POSITIONS_UNIFORM:
				SetPyramidObjectPositionsUniformly(i);
				break;
			case OBJ_POSITIONS_IN_AND_NEAR_ENTRANCE:
				if (SetPyramidObjectPositionsInAndNearSquare(i, entranceSquareId))
					SetPyramidObjectPositionsUniformly(i);
				break;
			case OBJ_POSITIONS_IN_AND_NEAR_EXIT:
				if (SetPyramidObjectPositionsInAndNearSquare(i, exitSquareId))
					SetPyramidObjectPositionsUniformly(i);
				break;
			case OBJ_POSITIONS_NEAR_ENTRANCE:
				if (SetPyramidObjectPositionsNearSquare(i, entranceSquareId))
					SetPyramidObjectPositionsUniformly(i);
				break;
			case OBJ_POSITIONS_NEAR_EXIT:
				if (SetPyramidObjectPositionsNearSquare(i, exitSquareId))
					SetPyramidObjectPositionsUniformly(i);
				break;
		}
	}
}

/* ---- 階のマップスクリプト ------------------------------------------------------------------------------ */
//ON_TRANSITION（ワープで入ったときだけ）: 明かりは半径 32 から（エメラルドの SetLightRadius）・暗くする・人と道具を置く印
void HoennPyramid_FloorOnTransition(void)
{
	PYR->lightRadius = 32;
	gSaveBlock1->flashLevel = 1;
	VarSet(VAR_TEMP_FRESH, 1);
}

//ON_LOAD（ワープでも「つづきから」でも）: 地図を 16 部屋で作り直す。ワープで入ったときは人と道具を置き、入口に立たせる
void HoennPyramid_FloorOnLoad(void)
{
	bool8 fresh = VarGet(VAR_TEMP_FRESH) != 0;

	if (fresh)
	{
		LoadBattlePyramidObjectEventTemplates();
		VarSet(VAR_TEMP_FRESH, 0);
	}
	GenerateBattlePyramidFloorLayout(fresh);
	SetFloorEvents();
}

//ON_RESUME: 出口の座標イベントを付け直す（戦いから戻ると地図の見出しが ROM のものに戻ることがある）
void HoennPyramid_FloorOnResume(void)
{
	SetFloorEvents();
	HoennTower_SetBattleLevel();
}

/* ---- 戦いの結果（src/overworld.c の差し込み口 patches/0130 と src/hoenn_pike.c から） -------------------------- */
//trainerbattle の番号 0x7F0 + n → フロンティアの 300 人へ（BattleSetup_ConfigureTrainerBattle の中・patches/0130）
void HoennPyramid_ResolveOpponent(void)
{
	u16 slot;

	if (!HoennPyramid_OnFloor())
		return;
	slot = gTrainerBattleOpponent_A - PYRAMID_TRAINER_TID_BASE;
	if (slot >= MAX_PYRAMID_TRAINERS)
		return;
	VarSet(VAR_TEMP_SLOT, slot);
	VarSet(VAR_FACILITY_TRAINER_ID, PYR->trainerIds[slot]);
	gTrainerBattleOpponent_A = BATTLE_TOWER_SPECIAL_TID;
}

//src/hoenn_trainer_flags.c: ピラミッドのトレーナーの「戦った」印は一時フラグ（エメラルドの pyramidTrainerFlags・階が変わると消える）
//→ フラグの番号（0 = ピラミッドのトレーナーではない）
u16 HoennPyramid_TrainerFlag(u16 trainerId)
{
	if (trainerId >= PYRAMID_TRAINER_TID_BASE && trainerId < PYRAMID_TRAINER_TID_BASE + MAX_PYRAMID_TRAINERS)
		return PYRAMID_TRAINER_FLAG_BASE + (trainerId - PYRAMID_TRAINER_TID_BASE);
	if (trainerId == BATTLE_TOWER_SPECIAL_TID && HoennPyramid_OnFloor())
		return PYRAMID_TRAINER_FLAG_BASE + MathMin(VarGet(VAR_TEMP_SLOT), MAX_PYRAMID_TRAINERS - 1);
	return 0;
}

//見つかったときの曲（CFRU の SetUpTrainerEncounterMusic の値・patches/0130）
u8 HoennPyramid_EncounterMusicId(u16 trainerId)
{
	if (trainerId == BATTLE_TOWER_SPECIAL_TID && HoennPyramid_OnFloor())
		return sPyramidTrainerMusic[VarGet(VAR_FACILITY_TRAINER_ID) % 300];
	if (trainerId == BATTLE_TOWER_SPECIAL_TID && HoennHill_OnFloor()) //★ 2026-10-03 トレーナーヒルの人は ふつうの曲（素 FRLG の 0・特別なトレーナーの番号では gTrainers を引けない）
		return 0;
	return GetTrainerEncounterMusicId(trainerId);
}

//トレーナー戦の終わり（エメラルドの CB2_EndTrainerBattle の InBattlePyramid: 負けても全滅にせず地図へ戻る）
void HoennPyramid_CB2_EndTrainerBattle(void)
{
	if (gBattleOutcome == B_OUTCOME_WON)
	{
		VarSet(VAR_TEMP_OUTCOME, 1);
		SetTrainerFlags();
	}
	else if (gBattleOutcome == B_OUTCOME_LOST || gBattleOutcome == B_OUTCOME_DREW || gBattleOutcome == B_OUTCOME_FORFEITED)
		VarSet(VAR_TEMP_OUTCOME, 2);
	SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
}

//野生の戦いの終わり（src/hoenn_pike.c の HoennPike_CB2_EndWildBattle から）
void HoennPyramid_RecordWildOutcome(void)
{
	if (gBattleOutcome == B_OUTCOME_WON || gBattleOutcome == B_OUTCOME_CAUGHT)
		VarSet(VAR_TEMP_OUTCOME, 1);
	else if (gBattleOutcome == B_OUTCOME_LOST || gBattleOutcome == B_OUTCOME_DREW)
		VarSet(VAR_TEMP_OUTCOME, 3);
}

//トレーナーに勝ったあと（エメラルドの MarkPyramidTrainerAsBattled: 歩き回るように）
void HoennPyramid_MarkTrainerBattled(void)
{
	struct EventObject *obj = &gEventObjects[gSelectedEventObject];
	u8 localId = gSpecialVar_LastTalked;

	obj->movementType = 2 /*MOVEMENT_TYPE_WANDER_AROUND*/;
	if (localId >= 1 && localId <= 64)
		gSaveBlock1->eventObjectTemplates[localId - 1].movementType = 2 /*MOVEMENT_TYPE_WANDER_AROUND*/;
	obj->initialCoords.x = obj->currentCoords.x;
	obj->initialCoords.y = obj->currentCoords.y;
}

/* ---- 野生（エメラルドの GenerateBattlePyramidWildMon） ----------------------------------------------------- */
//草むらの表の代わり: 12 匹の枠は 1〜12 番の種類（出たあと GenerateWildMon が「種類 - 1」番目の子に入れ替える）
//★ 2026-10-11 1 周の表は 8 匹なので、9〜12 番（枠の 1 割）は 8 匹の中に折りたたむ（前は表の外を読んで Lv200 になった）
static const struct WildPokemon sPyramidWildPlaceholders[12] =
{
	{50, 50, 1}, {50, 50, 2}, {50, 50, 3}, {50, 50, 4}, {50, 50, 5}, {50, 50, 6},
	{50, 50, 7}, {50, 50, 8}, {50, 50, 9}, {50, 50, 10}, {50, 50, 11}, {50, 50, 12},
};
static const struct WildPokemonInfo sPyramidWildInfo = {4, sPyramidWildPlaceholders};

const struct WildPokemonInfo* HoennPyramid_WildMonsInfo(u8 type)
{
	if (type != LAND_MONS_HEADER)
		return NULL;
	return &sPyramidWildInfo;
}

void HoennPyramid_GenerateWildMon(void)
{
	u8 name[POKEMON_NAME_LENGTH + 1];
	int i;
	const struct PyramidWildMon *wildMons;
	u32 id;
	s32 lvl;                       //★ 2026-10-11 符号つき（下の引き算が負になると u32 で回り込み、MAX_LEVEL で抑えて Lv200 になった）
	u32 mode = LvlMode();
	u16 round = Round();
	u8 altAbility;

	if (mode != FRONTIER_LVL_50)
		wildMons = sOpenLevelWildMonPointers[round];
	else
		wildMons = sLevel50WildMonPointers[round];

	/* ★ 2026-10-11 ユーザー「レベル 50 で野生がまれに Lv200」: 1 周の表は 8 匹（エメラルドの床の仮の種族も 1〜8＝フシギダネ〜カメール）なのに
	 * 12 まで許していて、表の後ろ（次の周・レベル 50 の 20 周目のあとはオープンの「差」の表）を読んでいた。
	 * ホウエンの野生の生成器・入れ替えた野生で床の種族が 1〜8 でなくなることもあるので、8 匹の中に折りたたむ */
	id = GetMonData(&gEnemyParty[0], MON_DATA_SPECIES, NULL) - 1;
	if (id >= PYRAMID_WILD_MONS_PER_ROUND)
		id %= PYRAMID_WILD_MONS_PER_ROUND;
	SetMonData(&gEnemyParty[0], MON_DATA_SPECIES, &wildMons[id].species);
	GetSpeciesName(name, wildMons[id].species);
	SetMonData(&gEnemyParty[0], MON_DATA_NICKNAME, &name);
	if (mode != FRONTIER_LVL_50)
	{
		//エメラルドの SetFacilityPtrsGetLevel（オープンは手持ちのいちばん高いレベル・下は 60）
		u32 best = 60;
		for (i = 0; i < 3; ++i)
			if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES, NULL) != SPECIES_NONE)
				best = MathMax(best, GetMonData(&gPlayerParty[i], MON_DATA_LEVEL, NULL));
		lvl = (s32) best - wildMons[id].lvl;
		lvl = lvl - 5 + (s32) (Random() % 11);
	}
	else
	{
		lvl = wildMons[id].lvl - 5 + (s32) (Random() % 11);
	}
	if (lvl < 1)
		lvl = 1;
	if (lvl > MAX_LEVEL)
		lvl = MAX_LEVEL;
	SetMonData(&gEnemyParty[0], MON_DATA_EXP, &gExperienceTables[gBaseStats[wildMons[id].species].growthRate][lvl]);

	switch (wildMons[id].abilityNum) {
		case 0:
		case 1:
			altAbility = wildMons[id].abilityNum;
			break;
		case ABILITY_RANDOM:
		default:
			altAbility = (gBaseStats[wildMons[id].species].ability2 != ABILITY_NONE) ? GetMonData(&gEnemyParty[0], MON_DATA_PERSONALITY, NULL) % 2 : 0;
			break;
	}
	SetMonData(&gEnemyParty[0], MON_DATA_ALT_ABILITY, &altAbility);

	for (i = 0; i < MAX_MON_MOVES; i++)
	{
		u16 move = wildMons[id].moves[i];
		u8 pp = gBattleMoves[move].pp;

		SetMonData(&gEnemyParty[0], MON_DATA_MOVE1 + i, &move);
		SetMonData(&gEnemyParty[0], MON_DATA_PP1 + i, &pp);
	}

	if (WinStreak() >= 140) //UBFIX（エメラルドはレベルで連勝の配列を引いていた）
	{
		id = (Random() % 17) + 15;
		for (i = 0; i < NUM_STATS; i++)
			SetMonData(&gEnemyParty[0], MON_DATA_HP_IV + i, &id);
	}
	CalculateMonStats(&gEnemyParty[0]);
}

/* ---- 暗さ（素 FRLG の WriteFlashScanlineEffectBuffer 0807E7A4 を置き換え・functionrewrites） --------------------- */
void HoennPyramid_WriteFlashBuffer(u8 flashLevel)
{
	if (flashLevel)
	{
		s32 radius = HoennPyramid_OnFloor() ? PYR->lightRadius : gHoennFlashLevelToRadius[flashLevel < 9 ? flashLevel : 8];

		SetFlashScanlineEffectWindowBoundaries(&gScanlineEffectRegBuffers[0][0], 120, 80, radius);
		CpuFastSet(&gScanlineEffectRegBuffers[0], &gScanlineEffectRegBuffers[1], 480);
	}
}

/* ---- スクリプトから（エメラルドの CallBattlePyramidFunction・Var8004 で選ぶ） ------------------------------------ */
static void InitPyramidChallenge(void)
{
	u32 lvlMode = LvlMode();

	VarSet(HOENN_VAR_TOWER_BATTLE_NUM, 0);
	if (!(PYR->activeFlags & (1 << lvlMode)))
		VarSet(HOENN_VAR_PYRAMID_STREAK + lvlMode, 0);
	gTrainerBattleOpponent_A = 0;
	gBattleOutcome = 0;
}

#define PYRAMID_DATA_PRIZE 0
#define PYRAMID_DATA_WIN_STREAK 1
#define PYRAMID_DATA_WIN_STREAK_ACTIVE 2
#define PYRAMID_DATA_WIN_STREAK_50 3
#define PYRAMID_DATA_WIN_STREAK_OPEN 4
#define PYRAMID_DATA_WIN_STREAK_ACTIVE_50 5
#define PYRAMID_DATA_WIN_STREAK_ACTIVE_OPEN 6
#define PYRAMID_DATA_TRAINER_FLAGS 7

static void GetBattlePyramidData(void)
{
	u32 lvlMode = LvlMode();

	switch (Var8005) {
		case PYRAMID_DATA_PRIZE:
			gSpecialVar_LastResult = PYR->prize;
			break;
		case PYRAMID_DATA_WIN_STREAK:
			gSpecialVar_LastResult = VarGet(HOENN_VAR_PYRAMID_STREAK + lvlMode);
			break;
		case PYRAMID_DATA_WIN_STREAK_ACTIVE:
			gSpecialVar_LastResult = (PYR->activeFlags >> lvlMode) & 1;
			break;
		case PYRAMID_DATA_WIN_STREAK_50:
			gSpecialVar_LastResult = VarGet(HOENN_VAR_PYRAMID_STREAK + FRONTIER_LVL_50);
			break;
		case PYRAMID_DATA_WIN_STREAK_OPEN:
			gSpecialVar_LastResult = VarGet(HOENN_VAR_PYRAMID_STREAK + FRONTIER_LVL_OPEN);
			break;
		case PYRAMID_DATA_WIN_STREAK_ACTIVE_50:
			gSpecialVar_LastResult = PYR->activeFlags & 1;
			break;
		case PYRAMID_DATA_WIN_STREAK_ACTIVE_OPEN:
			gSpecialVar_LastResult = (PYR->activeFlags >> 1) & 1;
			break;
	}
}

static void SetBattlePyramidData(void)
{
	u32 lvlMode = LvlMode();
	u32 i;

	switch (Var8005) {
		case PYRAMID_DATA_PRIZE:
			PYR->prize = Var8006;
			break;
		case PYRAMID_DATA_WIN_STREAK:
			VarSet(HOENN_VAR_PYRAMID_STREAK + lvlMode, Var8006);
			break;
		case PYRAMID_DATA_WIN_STREAK_ACTIVE:
			if (Var8006)
				PYR->activeFlags |= (1 << lvlMode);
			else
				PYR->activeFlags &= ~(1 << lvlMode);
			break;
		case PYRAMID_DATA_TRAINER_FLAGS: //エメラルドの pyramidTrainerFlags（負けたとき 255 = 全員戦った扱い）
			for (i = 0; i < MAX_PYRAMID_TRAINERS; ++i)
			{
				if (Var8006 & (1 << i))
					FlagSet(PYRAMID_TRAINER_FLAG_BASE + i);
				else
					FlagClear(PYRAMID_TRAINER_FLAG_BASE + i);
			}
			break;
	}
}

static void SetBattlePyramidPrize(void)
{
	if (WinStreak() > 41)
		PYR->prize = sLongStreakRewardItems[Random() % NELEMS(sLongStreakRewardItems)];
	else
		PYR->prize = sShortStreakRewardItems[Random() % NELEMS(sShortStreakRewardItems)];
}

static void GiveBattlePyramidPrize(void)
{
	if (AddBagItem(PYR->prize, 1) == TRUE)
	{
		CopyItemName(PYR->prize, gStringVar1);
		PYR->prize = 0;
		gSpecialVar_LastResult = TRUE;
	}
	else
	{
		gSpecialVar_LastResult = FALSE;
	}
}

static void SeedPyramidFloor(void)
{
	u32 i;

	for (i = 0; i < NELEMS(PYR->randoms); i++)
		PYR->randoms[i] = Random();
	for (i = 0; i < MAX_PYRAMID_TRAINERS; ++i)
		FlagClear(PYRAMID_TRAINER_FLAG_BASE + i);
}

//道具の玉（エメラルドの SetPickupItem）→ Var8000 = 道具・Var8001 = 1
static void SetPickupItem(void)
{
	int i;
	int itemIndex;
	int rand;
	u8 id;
	u32 lvlMode = LvlMode();
	u32 floor = MathMin(Floor(), FRONTIER_STAGES_PER_CHALLENGE - 1);
	u32 round = Round();

	id = GetPyramidFloorTemplateId();
	itemIndex = (gSpecialVar_LastTalked - sPyramidFloorTemplates[id].numTrainers) - 1;
	if (itemIndex < 0)
		itemIndex = 0;
	rand = PYR->randoms[(itemIndex / 2) % 4];
	SeedRng2(rand);

	for (i = 0; i < itemIndex + 1; i++)
		rand = Random2() % 100;

	for (i = sPickupItemOffsets[floor]; i < (int) NELEMS(sPickupItemSlots) - 1; i++)
	{
		if (rand < sPickupItemSlots[i][0])
			break;
	}

	if (lvlMode != FRONTIER_LVL_50)
		Var8000 = sPickupItemsLvlOpen[round][sPickupItemSlots[i][1]];
	else
		Var8000 = sPickupItemsLvl50[round][sPickupItemSlots[i][1]];
	Var8001 = 1;
}

//拾った玉を消す（エメラルドの HidePyramidItem: 旗ではなく置き場を画面の外へ）
static void HidePyramidItem(void)
{
	struct EventObjectTemplate *events = gSaveBlock1->eventObjectTemplates;
	int i = 0;

	do
	{
		if (events[i].localId == gSpecialVar_LastTalked)
		{
			events[i].x = 0x7FFF;
			events[i].y = 0x7FFF;
			break;
		}
		i++;
	} while (i < 64 && events[i].localId != 0);
}

static u8 GetPostBattleDirectionHintTextIndex(int *hintType, u8 minDistanceForExitHint, u8 defaultHintType)
{
	int x, y;
	u8 textIndex = 0;
	u16 *map = VMap.map;
	struct EventObject *obj = &gEventObjects[gSelectedEventObject];

	map += VMap.width * MAP_OFFSET + MAP_OFFSET;
	for (y = 0; y < 32; y++)
	{
		for (x = 0; x < 32; x++)
		{
			if ((map[x] & PYR_METATILE_ID_MASK) == PYRAMID_METATILE_EXIT)
			{
				x -= obj->initialCoords.x - MAP_OFFSET;
				y -= obj->initialCoords.y - MAP_OFFSET;
				if (x >= minDistanceForExitHint
				 || x <= -minDistanceForExitHint
				 || y >= minDistanceForExitHint
				 || y <= -minDistanceForExitHint
				 || defaultHintType == HINT_EXIT_DIRECTION)
				{
					if (x > 0 && y > 0)
						textIndex = (x >= y) ? 2 : 3;
					else if (x < 0 && y < 0)
						textIndex = (x > y) ? 0 : 1;
					else if (x == 0)
						textIndex = (y > 0) ? 3 : 0;
					else if (y == 0)
						textIndex = (x > 0) ? 2 : 1;
					else if (x < 0)
						textIndex = (x + y > 0) ? 3 : 1;
					else
						textIndex = (x + y >= 0) ? 2 : 0;
					*hintType = HINT_EXIT_DIRECTION;
				}
				else
				{
					*hintType = defaultHintType;
				}
				return textIndex;
			}
		}
		map += MAP_OFFSET_W + 32;
	}

	return textIndex;
}

//戦ったあとのヒント（エメラルドの ShowPostBattleHintText）
static void ShowPostBattleHintText(void)
{
	int i;
	int hintType;
	u8 id;
	int textGroup;
	int textIndex = 0;
	struct EventObjectTemplate *events = gSaveBlock1->eventObjectTemplates;
	u8 localId = gEventObjects[gSelectedEventObject].localId;
	u16 trainerId = PYR->trainerIds[(localId - 1) % MAX_PYRAMID_TRAINERS];

	textGroup = sPyramidTrainerTextGroup[trainerId % 300];
	hintType = sHintTextTypes[(localId - 1) % MAX_PYRAMID_TRAINERS];
	i = 0;
	while (!i)
	{
		switch (hintType) {
			case HINT_EXIT_DIRECTION:
				textIndex = GetPostBattleDirectionHintTextIndex(&hintType, 8, HINT_EXIT_DIRECTION);
				i = 1;
				break;
			case HINT_REMAINING_ITEMS:
				for (i = 0; i < PYRAMID_MAX_OBJECTS; i++)
				{
					if ((events[i].graphicsIdLowerByte | (events[i].graphicsIdUpperByte << 8)) == PYRAMID_GFX_ITEM_BALL
					&& events[i].x != 0x7FFF && events[i].y != 0x7FFF)
						textIndex++;
				}
				i = 1;
				break;
			case HINT_REMAINING_TRAINERS:
				id = GetPyramidFloorTemplateId();
				textIndex = sPyramidFloorTemplates[id].numTrainers;
				for (i = 0; i < MAX_PYRAMID_TRAINERS; i++)
				{
					if (FlagGet(PYRAMID_TRAINER_FLAG_BASE + i))
						textIndex--;
				}
				textIndex = MathMax(textIndex, 0);
				i = 1;
				break;
			case HINT_EXIT_SHORT_REMAINING_TRAINERS:
				GetPostBattleDirectionHintTextIndex(&hintType, 8, HINT_REMAINING_TRAINERS);
				break;
			case HINT_EXIT_SHORT_REMAINING_ITEMS:
				GetPostBattleDirectionHintTextIndex(&hintType, 8, HINT_REMAINING_ITEMS);
				break;
			case HINT_EXIT_MEDIUM_REMAINING_TRAINERS:
				GetPostBattleDirectionHintTextIndex(&hintType, 16, HINT_REMAINING_TRAINERS);
				break;
			case HINT_EXIT_MEDIUM_REMAINING_ITEMS:
				GetPostBattleDirectionHintTextIndex(&hintType, 16, HINT_REMAINING_ITEMS);
				break;
			case HINT_EXIT_FAR_REMAINING_TRAINERS:
				GetPostBattleDirectionHintTextIndex(&hintType, 24, HINT_REMAINING_TRAINERS);
				break;
			case HINT_EXIT_FAR_REMAINING_ITEMS:
				GetPostBattleDirectionHintTextIndex(&hintType, 24, HINT_REMAINING_ITEMS);
				break;
			default:
				hintType = HINT_EXIT_DIRECTION;
				break;
		}
	}
	textIndex = MathMin(textIndex, hintType == HINT_EXIT_DIRECTION ? 3 : hintType == HINT_REMAINING_ITEMS ? 8 : 7);
	StringExpandPlaceholders(gStringVar4, sPostBattleTexts[textGroup % 6][hintType][textIndex]);
	ShowFieldMessageFR(gStringVar4);
}

static void UpdatePyramidWinStreak(void)
{
	u32 lvlMode = LvlMode();
	u16 streak = VarGet(HOENN_VAR_PYRAMID_STREAK + lvlMode);

	if (streak < 999)
		VarSet(HOENN_VAR_PYRAMID_STREAK + lvlMode, ++streak);
	if (streak > VarGet(HOENN_VAR_PYRAMID_MAX_STREAK + lvlMode))
		VarSet(HOENN_VAR_PYRAMID_MAX_STREAK + lvlMode, streak);
}

static void GetCurrentBattlePyramidLocation(void)
{
	gSpecialVar_LastResult = HoennPyramid_OnFloor() ? 1 : OnTop() ? 2 : 0;
}

//明かり（エメラルドの UpdatePyramidLightRadius）。INCR は Var8005 歩ぶん 1 ずつ広げる（LASTRESULT が 2 になったら終わり）
static void UpdatePyramidLightRadius(void)
{
	switch (Var8006) {
		case PYRAMID_LIGHT_SET_RADIUS:
			PYR->lightRadius = Var8005;
			break;
		case PYRAMID_LIGHT_INCR_RADIUS:
			switch (gSpecialVar_LastResult) {
				case 0:
					if (!gPaletteFade->active)
					{
						if (PYR->lightRadius >= PYRAMID_LIGHT_MAX)
							PYR->lightRadius = PYRAMID_LIGHT_MAX;
						else
							PlaySE(Var8007);
						gSpecialVar_LastResult++;
					}
					break;
				case 1:
					if (Var8005 != 0)
					{
						Var8005--;
						PYR->lightRadius++;
						if (PYR->lightRadius > PYRAMID_LIGHT_MAX)
						{
							PYR->lightRadius = PYRAMID_LIGHT_MAX;
							gSpecialVar_LastResult++;
						}
						HoennPyramid_WriteFlashBuffer(1);
					}
					else
					{
						gSpecialVar_LastResult = 2;
					}
					break;
			}
			break;
	}
}

static void Task_SetPyramidFloorPalette(u8 taskId)
{
	if (gPaletteFade->active)
	{
		CpuCopy16(sPyramidFloorPal[MathMin(Floor(), 6)], &gPlttBufferUnfaded[PYRAMID_FLOOR_PAL_SLOT * 16], 32);
		DestroyTask(taskId);
	}
}

static void SetPyramidFloorPalette(void)
{
	CreateTask(Task_SetPyramidFloorPalette, 0);
}

static void DoNothing(void)
{
}

static void (*const sBattlePyramidFunctions[])(void) =
{
	InitPyramidChallenge,           //0 INIT
	GetBattlePyramidData,           //1 GET_DATA
	SetBattlePyramidData,           //2 SET_DATA
	DoNothing,                      //3 SAVE（HoennTower_SaveGame を使う）
	SetBattlePyramidPrize,          //4 SET_PRIZE
	GiveBattlePyramidPrize,         //5 GIVE_PRIZE
	SeedPyramidFloor,               //6 SEED_FLOOR
	SetPickupItem,                  //7 SET_ITEM
	HidePyramidItem,                //8 HIDE_ITEM
	DoNothing,                      //9 SET_TRAINERS（相手の表は CFRU の gSpecialTowerTrainers のまま）
	ShowPostBattleHintText,         //10 SHOW_HINT_TEXT
	UpdatePyramidWinStreak,         //11 UPDATE_STREAK
	GetCurrentBattlePyramidLocation,//12 CURRENT_LOCATION
	UpdatePyramidLightRadius,       //13 UPDATE_LIGHT
	DoNothing,                      //14 CLEAR_HELD_ITEMS（バトルバッグは ②）
	SetPyramidFloorPalette,         //15 SET_FLOOR_PALETTE
	DoNothing,                      //16 START_MENU
	DoNothing,                      //17 RESTORE_PARTY（挑戦の手持ちは写し・終わると元に戻る）
};

void HoennPyramid_CallFunction(void)
{
	if (Var8004 < NELEMS(sBattlePyramidFunctions))
		sBattlePyramidFunctions[Var8004]();
}

//受付の占い師のヒント（エメラルドの GetBattlePyramidHint）: Var8004 = 連勝 → LASTRESULT = 周 % 20
void HoennPyramid_GetHint(void)
{
	gSpecialVar_LastResult = (Var8004 / FRONTIER_STAGES_PER_CHALLENGE) % TOTAL_PYRAMID_ROUNDS;
}

/* ---- バトルバッグ（エメラルドの battle_pyramid_bag.c の「中身」だけ） ---------------------------------------- */
//src/item.c の SetMemoryForBagStorage のあと（patches/0131）: バトルバッグを使っている間は「どうぐ」をその 10 枠に・ほかは 0 枠
void HoennPyramid_ApplyBag(void)
{
	if (!(PYR->activeFlags & PYRAMID_BAG_IN_USE))
		return;
	gBagPocketsCFRU->itemRam = PYRAMID_BAG_SLOTS + LvlMode() * PYRAMID_BAG_ITEMS_COUNT;
	gBagPocketsCFRU->itemAmount = PYRAMID_BAG_ITEMS_COUNT;
	gBagPocketsCFRU->keyItemAmount = 0;
	gBagPocketsCFRU->pokeBallAmount = 0;
	gBagPocketsCFRU->tmAmount = 0;
	gBagPocketsCFRU->berryAmount = 0;
}

//src/move_menu.c（patches/0131）: 施設の戦いでもピラミッドの中ならバッグを開ける（エメラルドの BATTLE_TYPE_FRONTIER_NO_PYRAMID）
bool8 HoennPyramid_BagUsableInBattle(void)
{
	return (PYR->activeFlags & PYRAMID_BAG_IN_USE) && (HoennPyramid_OnFloor() || OnTop());
}

//受付でバッグとバトルバッグを取り替える（エメラルドの WalkToPanelAndReceiveBag）。連勝が 0 なら新しいバッグ（いいキズぐすり・ピーピーエイド）
//→ LASTRESULT = 1 新しいバッグ / 0 前のバッグ
void HoennPyramid_TakeBattleBag(void)
{
	u32 i;
	bool8 fresh = (WinStreak() == 0);

	PYR->activeFlags |= PYRAMID_BAG_IN_USE;
	HoennPyramid_ApplyBag();
	if (fresh)
	{
		struct ItemSlot* slots = PYRAMID_BAG_SLOTS + LvlMode() * PYRAMID_BAG_ITEMS_COUNT;

		for (i = 0; i < PYRAMID_BAG_ITEMS_COUNT; ++i)
		{
			slots[i].itemId = ITEM_NONE;
			slots[i].quantity = 0;
		}
		AddBagItem(ITEM_HYPER_POTION, 1);
		AddBagItem(ITEM_ETHER, 1);
	}
	gSpecialVar_LastResult = fresh;
}

//挑戦のポケモンの持ち物をバトルバッグへ（エメラルドの DoBattlePyramidMonsHaveHeldItem + TryStoreHeldItemsInPyramidBag）。
//入った子は一時変数 0x4006 の印（選んだ順・ビット）・LASTRESULT = 0 持ち物なし / 1 全部入った / 2 入りきらない子がいる（その子は持ったまま）
#define VAR_TEMP_STORED 0x4006
void HoennPyramid_StoreHeldItems(void)
{
	u32 i;
	u16 stored = 0, none = TRUE, full = FALSE;

	for (i = 0; i < 3; ++i)
	{
		u16 item = GetMonData(&gPlayerParty[i], MON_DATA_HELD_ITEM, NULL);

		if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES, NULL) == SPECIES_NONE || item == ITEM_NONE)
			continue;
		none = FALSE;
		if (AddBagItem(item, 1))
			stored |= 1 << i;
		else
			full = TRUE;
	}
	VarSet(VAR_TEMP_STORED, stored);
	gSpecialVar_LastResult = none ? 0 : (full ? 2 : 1);
}

//バトルバッグを返してふつうのバッグに戻す（ポケットの表を作り直す）
void HoennPyramid_ReturnBattleBag(void)
{
	PYR->activeFlags &= ~PYRAMID_BAG_IN_USE;
	SetMemoryForBagStorage();
}

//元の手持ちの、挑戦に出た子の持ち物を消す（エメラルドの ClearPyramidPartyHeldItems）。Var8005 = 消す子の印（選んだ順・ビット）
void HoennPyramid_ClearHeldItems(void)
{
	u16 packed = VarGet(HOENN_VAR_TOWER_SELECTED);
	u16 none = ITEM_NONE;
	u32 i;

	for (i = 0; i < 3; ++i)
	{
		u8 slot = (packed >> (3 * i)) & 7;

		if ((Var8005 & (1 << i)) && slot >= 1 && slot <= PARTY_SIZE)
			SetMonData(&gPlayerParty[slot - 1], MON_DATA_HELD_ITEM, &none);
	}
}
