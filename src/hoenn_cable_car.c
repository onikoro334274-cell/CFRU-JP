#include "defines.h"
#include "../include/bg.h"
#include "../include/field_weather.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/malloc.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/random.h"
#include "../include/scanline_effect.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "../include/event_object_movement.h"
#include "../include/gba/macro.h"
#include "../include/new/character_customization.h"
#include "../include/new/dns.h"        /* TransferPlttBuffer */
#include "../include/new/dynamic_ow_pals.h"

/*
 * ロープウェイ（112 ばんどうろ ⇔ えんとつやま）の演出
 * （#2・2026-09-24 ユーザー要望「フエンタウンに行くロープウェイの実装とアニメーションの移植」）
 *
 * 移植元は pokeemerald の src/cable_car.c（`special CableCar`）。素 FRLG にはこの画面が無い。
 * 駅のスクリプトは tools/rom/build_hoenn_scripts.py が移植し、
 *   special CableCarWarp -> callnative HoennNative_CableCarWarp
 *   special CableCar     -> callnative HoennNative_CableCar
 * に差し替える。素材は src/Tables/hoenn_cable_car_gfx.c（tools/cablecar/build_cable_car.py）。
 *
 * ★ エメラルドとの違い（詳細は docs/research/2026-09-24_cable-car.md）
 *  - 状態の入れ物のポインタを static に置かない（CLAUDE.md 6.13 — CFRU では書き換え可能な
 *    static は ROM に置かれて黙って捨てられる）。何もしない「預かり」タスクの data[] に置く
 *  - 素材は生のまま ROM に置くので、タイルマップを Malloc して展開しない（ROM を直に読む）
 *  - 小数（0.14f / 0.067f）は整数の割り算にした（切り捨ての結果は 0〜600 フレームで同じ）
 *  - BPRJ.ld に番地が無い素の関数（6.29）は、実測した番地で直に呼ぶ
 *  - 主人公の絵はエメラルドのユウキ／ハルカではなく、いまの主人公の歩きの絵
 *  - 山道の通行人はエメラルドと同じ 3 種（ジグザグマは原本でも出ない）を素 FRLG の番号で
 */

/* ---- 素 FRLG（BPRJ）の番地（実測。tools/cablecar/verify_cable_car.py が先頭の命令を見る）---- */
#define BPRJ_START_WEATHER      0x08079368  /* StartWeather（FuncIsActiveTask(Task_WeatherMain) から）*/
#define BPRJ_WARP_INTO_MAP      0x08054C38  /* WarpIntoMap（CB2_LoadMap の直前に呼ばれる 3 命令の関数）*/
#define BPRJ_FADE_OUT_BGM       0x08071560  /* FadeOutBGM（FadeOutMapMusic の中から）*/
#define BPRJ_SPRITE_OFS_X       0x02021B44  /* gSpriteCoordOffsetX（6.30）*/
#define BPRJ_SPRITE_OFS_Y       0x02021B46  /* gSpriteCoordOffsetY */
/* ★★ 灰の天気の 4 関数（素の sWeatherFuncs[7]・083899B4 にあった値）。
   CFRU の REPLACE_ASH_WEATHER_WITH_WHITE_SANDSTORM が表の 7 番を**白いすなあらし**に
   書き換えている（bytereplacement 083899B4。CFRU は 7 番を「雪」として使う）ので、
   天気 7 を頼むとすなあらしが出る（2026-09-24 実機）。処理そのものは ROM に残っているので、
   天気の仕組みは晴れ（2）のまま回し、灰はこちらから直に呼ぶ */
#define BPRJ_ASH_INIT_VARS      0x0807C3BC
#define BPRJ_ASH_MAIN           0x0807C448
#define BPRJ_ASH_INIT_ALL       0x0807C418
#define BPRJ_ASH_FINISH         0x0807C4F4

/* bg.h の宣言は #if の中にあって見えない（bw_frontier_pass.c と同じ扱い。BPRJ.ld 08001FD4）*/
void __attribute__((long_call)) UnsetBgTilemapBuffer(u8 bg);

#define CcStartWeather     ((void (*)(void))(BPRJ_START_WEATHER + 1))
#define CcWarpIntoMap      ((void (*)(void))(BPRJ_WARP_INTO_MAP + 1))
#define CcFadeOutBGM       ((void (*)(u8))(BPRJ_FADE_OUT_BGM + 1))
#define CcAshInitVars      ((void (*)(void))(BPRJ_ASH_INIT_VARS + 1))
#define CcAshMain          ((void (*)(void))(BPRJ_ASH_MAIN + 1))
#define CcAshInitAll       ((void (*)(void))(BPRJ_ASH_INIT_ALL + 1))
#define CcAshFinish        ((bool8 (*)(void))(BPRJ_ASH_FINISH + 1))

/* 灰を自前で回すときの段（CableCar.ashState）*/
enum { ASH_OFF, ASH_ACTIVE, ASH_FINISHING, ASH_DONE };
#define sCcSpriteOfsX      (*(s16 *)BPRJ_SPRITE_OFS_X)
#define sCcSpriteOfsY      (*(s16 *)BPRJ_SPRITE_OFS_Y)

#define CC_WEATHER_SUNNY        2
#define CC_WEATHER_VOLCANIC_ASH 7

/* 素 FRLG の野外の絵（pokefirered include/constants/event_objects.h）*/
#define CC_GFX_HIKER     56
#define CC_GFX_CAMPER    39
#define CC_GFX_PICNICKER 40
#define CC_ANIM_GO_WEST  6
#define CC_ANIM_GO_EAST  7

/* ★ 山道の通行人が出る割合（1/N）。エメラルドは 1/64 でほとんど見られないので、
   ユーザー要望（2026-09-24「ポケモンや やまおとこが出てくるように」）で毎回出す */
#define CC_HIKER_CHANCE  1
/* ★★ 通行人は**集団**で出す（2026-09-24 ユーザー要望「賑やかな方がいい。ジグザグマの群れとか
   やまおとこの集団とか」）。1 回に 1〜CC_MAX_GROUPS 組、組ごとに種類をランダムに決め、
   人は 2〜4 人・ジグザグマは 3〜6 匹。組の中は歩く線の上に**うしろ（画面の外側）へ**並べ、
   全員いっしょに歩き出させる（斜面の傾きは横 4 : 縦 1）。
   ★ スプライトは 64 個まで。演出はもともと 32 個（灰 20・ケーブル 9・ゴンドラ・扉・主人公）
     使うので、最大 3 組 x 6 = 18 を足しても 50 個で収まる */
#define CC_MAX_GROUPS    3
/* ★ 色違いのジグザグマ（2026-09-24 ユーザー要望「低確率にはしたくない」）。1 匹ずつ 1/N。
   群れの中に色違いが混ざる。絵は同じで、パレットだけ別の枠に読んだ色違いに差し替える */
#define CC_SHINY_CHANCE  3

#define GOING_DOWN (Var8004 != 0)
#define STATE_END  0xFF

/* スプライトの札（素のどの札とも当たらない所）*/
#define TAG_CC_CAR   0x5CC1
#define TAG_CC_DOOR  0x5CC2
#define TAG_CC_CABLE 0x5CC3
#define TAG_CC_ZIG_SHINY 0x5CC4

/* ---- 素材（src/Tables/hoenn_cable_car_gfx.c）---- */
extern const u8 gHoennCableCarBgTiles[0x4000];
extern const u16 gHoennCableCarBgPal[64];
extern const u8 gHoennCableCarGfx[0x800];
extern const u8 gHoennCableCarDoorGfx[0x40];
extern const u8 gHoennCableCarCableGfx[0x80];
extern const u16 gHoennCableCarPal[16];
extern const u16 gHoennCableCar_Ground[];
extern const u16 gHoennCableCar_Trees[];
extern const u16 gHoennCableCar_BgMountains[];
extern const u16 gHoennCableCar_PylonTop[];
extern const u16 gHoennCableCar_PylonPole[];
extern const u16 gHoennCableCarMusic;
extern const u16 gHoennCableCarZigzagoonGfx;
extern const u16 gHoennCableCarZigzagoonShinyPal[16];
extern const u8 gHoennCableCarStations[2][2];

struct CableCar
{
	u8 bgTaskId;
	u8 state;
	u8 weather;
	u16 weatherDelay;
	u16 timer;
	u8 bg0HorizontalOffset;
	u8 bg0VerticalOffset;
	u8 bg1HorizontalOffset;
	u8 bg1VerticalOffset;
	u8 bg3HorizontalOffset;
	u8 bg3VerticalOffset;
	u8 groundTileIdx;
	u8 groundSegmentXStart;
	u8 groundSegmentYStart;
	u8 groundTilemapOffset;
	u8 groundXOffset;
	u8 groundYOffset;
	u8 groundXBase;
	u8 groundYBase;
	u8 ashState;
	u16 groundTileBuffer[9][12];
	u16 bgTilemapBuffers[4][32 * 32];
};

void HoennNative_CableCar(void);
void HoennNative_CableCarWarp(void);

static void Task_LoadCableCar(u8 taskId);
static void Task_CableCarHolder(u8 taskId);
static void CB2_LoadCableCar(void);
static void CB2_CableCar(void);
static void CB2_EndCableCar(void);
static void VBlankCB_CableCar(void);
static void Task_CableCar(u8 taskId);
static void Task_AnimateBgGoingUp(u8 taskId);
static void Task_AnimateBgGoingDown(u8 taskId);
static void SetBgRegs(bool8 active);
static void CreateCableCarSprites(void);
static void InitGroundTilemapData(bool8 goingDown);
static void AnimateGroundGoingUp(void);
static void AnimateGroundGoingDown(void);
static void DrawNextGroundSegmentGoingUp(void);
static void DrawNextGroundSegmentGoingDown(void);
static void SpriteCB_CableCar(struct Sprite *sprite);
static void SpriteCB_Player(struct Sprite *sprite);
static void SpriteCB_HikerGoingUp(struct Sprite *sprite);
static void SpriteCB_HikerGoingDown(struct Sprite *sprite);

static const struct BgTemplate sBgTemplates[4] =
{
	{ .bg = 0, .charBaseIndex = 0, .mapBaseIndex = 28, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0 },
	{ .bg = 1, .charBaseIndex = 0, .mapBaseIndex = 29, .screenSize = 0, .paletteMode = 0, .priority = 2, .baseTile = 0 },
	{ .bg = 2, .charBaseIndex = 0, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 3, .baseTile = 0 },
	{ .bg = 3, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0 },
};

static const struct SpriteSheet sSpriteSheets[] =
{
	{ gHoennCableCarGfx,      0x800, TAG_CC_CAR },
	{ gHoennCableCarDoorGfx,  0x40,  TAG_CC_DOOR },
	{ gHoennCableCarCableGfx, 0x80,  TAG_CC_CABLE },
};

static const struct OamData sOam_CableCar =
{
	.affineMode = ST_OAM_AFFINE_DOUBLE,
	.objMode = ST_OAM_OBJ_NORMAL,
	.shape = SPRITE_SHAPE(64x64),
	.size = SPRITE_SIZE(64x64),
	.priority = 2,
};

static const struct OamData sOam_CableCarDoor =
{
	.affineMode = ST_OAM_AFFINE_DOUBLE,
	.objMode = ST_OAM_OBJ_NORMAL,
	.shape = SPRITE_SHAPE(16x8),
	.size = SPRITE_SIZE(16x8),
	.priority = 2,
};

static const struct OamData sOam_Cable =
{
	.affineMode = ST_OAM_AFFINE_DOUBLE,
	.objMode = ST_OAM_OBJ_NORMAL,
	.shape = SPRITE_SHAPE(16x16),
	.size = SPRITE_SIZE(16x16),
	.priority = 2,
};

static const struct SpriteTemplate sSpriteTemplate_CableCar =
{
	.tileTag = TAG_CC_CAR,
	.paletteTag = TAG_CC_CAR,
	.oam = &sOam_CableCar,
	.anims = gDummySpriteAnimTable,
	.images = NULL,
	.affineAnims = gDummySpriteAffineAnimTable,
	.callback = SpriteCB_CableCar,
};

static const struct SpriteTemplate sSpriteTemplate_Door =
{
	.tileTag = TAG_CC_DOOR,
	.paletteTag = TAG_CC_CAR,
	.oam = &sOam_CableCarDoor,
	.anims = gDummySpriteAnimTable,
	.images = NULL,
	.affineAnims = gDummySpriteAffineAnimTable,
	.callback = SpriteCB_CableCar,
};

static const struct SpriteTemplate sSpriteTemplate_Cable =
{
	.tileTag = TAG_CC_CABLE,
	.paletteTag = TAG_CC_CAR,
	.oam = &sOam_Cable,
	.anims = gDummySpriteAnimTable,
	.images = NULL,
	.affineAnims = gDummySpriteAffineAnimTable,
	.callback = SpriteCallbackDummy,
};

/* ---- 状態の入れ物（預かりタスクの data[0..1] にポインタを置く）---- */
static struct CableCar *CC(void)
{
	u8 taskId = FindTaskIdByFunc(Task_CableCarHolder);

	if (taskId == 0xFF)
		return NULL;
	return (struct CableCar *)(((u32)(u16)gTasks[taskId].data[1] << 16) | (u16)gTasks[taskId].data[0]);
}

static void Task_CableCarHolder(unusedArg u8 taskId)
{
}

/* ---- タイルマップ（自前の入れ物に直に書く。32x32 で折り返す）---- */
static void CopyRect(u8 bg, const u16 *src, u8 left, u8 top, u8 width, u8 height)
{
	u16 *dst = CC()->bgTilemapBuffers[bg];
	u8 x, y;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			dst[(((top + y) & 31) << 5) | ((left + x) & 31)] = *src++;
}

static void FillRect(u8 bg, u16 tile, u8 left, u8 top, u8 width, u8 height)
{
	u16 *dst = CC()->bgTilemapBuffers[bg];
	u8 x, y;

	for (y = 0; y < height; y++)
		for (x = 0; x < width; x++)
			dst[(((top + y) & 31) << 5) | ((left + x) & 31)] = tile;
}

/* ★★ CFRU の動的パレットは置き場の帳簿が 2 冊ある（src/dynamic_ow_pals.c）。
   天気のタスクが動いている（＝野外）あいだは sPalRefs、そうでないときは素の PalTags。
   ResetSpriteData が sPalRefs を空にした（0800779C のフック）あと:
     1. StartWeather の天気のパレット（0x1200）とゴンドラのパレットは素の PalTags で枠 0・1 に載る
     2. 主人公の絵は sPalRefs で空き枠を探す → 帳簿が空なので**枠 0（天気）を上書き**
     3. 灰の粒は sPalRefs で 0x1200 を探して見つからず、空き枠の**枠 1（ゴンドラ）に天気の
        既定パレット（9〜11 番が赤紫）を読む**（2026-09-24 実機「灰が降ると色がおかしくなる」）
   → PalTags に載っている枠を、主人公を作る前に sPalRefs にも同じ枠で書き写す */
/* 帳簿の番地は src/dynamic_ow_pals.c の中でしか定義されていない（同じ値。検算が突き合わせる）*/
struct CcPalRef		/* src/dynamic_ow_pals.c の struct PalRef と同じ形（検算が突き合わせる）*/
{
	u8 Type;
	u8 Count;
	u16 PalTag;
};
#define sPalRefs ((struct CcPalRef *) 0x203B74C)

static void SyncPalRefs(void)
{
	u8 i;

	for (i = 0; i < 16; i++)
	{
		if (PalTags[i] != 0xFFFF && sPalRefs[i].Type == PalTypeUnused)
		{
			sPalRefs[i].Type = (PalTags[i] == 0x1200) ? PalTypeWeather : PalTypeOther;
			sPalRefs[i].PalTag = PalTags[i];
			sPalRefs[i].Count = 1;
		}
	}
}

/* ★★ 2026-09-26 実機「上りでキャンプガールの集団がいるときに灰が降ってくると、キャンプガールだけ赤紫になる」
   （主人公とゴンドラはふつう）。
   灰の粒は sPalRefs で 0x1200 を探し、見つからないと**空いている枠に天気の既定パレット（9〜11 番が赤紫）**を読む
   （dynamic_ow_pals.c の GetPalSlotMisc）。赤紫になったのがキャンプガールだけなので、灰が降り出したとき
   天気の枠もキャンプガールの枠も帳簿の上では空きになっていた。CFRU はスプライトを消すたびに
   その枠の数を 1 減らし（DeleteOBJFreeTiles のフック）、0 で枠を空きにするので、この画面では数が実物とずれる。
   → 灰を作る直前に、今ある全部のスプライトの枠を数え直して帳簿を作り直す。天気の枠（PalTags の 0x1200）は
      必ず 0x1200 として残し、数は 1 多くしておく（灰の粒が消えても空きにならないように）*/
static void RebuildPalRefs(void)
{
	u8 counts[16];
	u8 i, weatherSlot = 0xFF;

	for (i = 0; i < 16; i++)
	{
		counts[i] = 0;
		if (PalTags[i] == 0x1200)
			weatherSlot = i;
	}
	for (i = 0; i < MAX_SPRITES; i++)
	{
		if (gSprites[i].inUse)
			counts[gSprites[i].oam.paletteNum & 0xF]++;
	}
	for (i = 0; i < 16; i++)
	{
		if (i == weatherSlot)
		{
			sPalRefs[i].Type = PalTypeWeather;
			sPalRefs[i].PalTag = 0x1200;
			sPalRefs[i].Count = counts[i] + 1;
		}
		else if (counts[i] != 0)
		{
			if (sPalRefs[i].Type == PalTypeUnused)
			{
				sPalRefs[i].Type = PalTypeOther;
				sPalRefs[i].PalTag = (PalTags[i] != 0xFFFF) ? PalTags[i] : 0;
			}
			sPalRefs[i].Count = counts[i];
		}
	}
}

/* 素 FRLG の SetCurrentAndNextWeatherNoDelay は static なので書き直した（雨の音止めは不要）*/
static void SetCurrentAndNextWeatherNoDelay(u8 weather)
{
	gWeatherPtr->currWeather = weather;
	gWeatherPtr->nextWeather = weather;
	gWeatherPtr->readyForInit = TRUE;
}

/* ================================================================ */

/* 駅のスクリプトから（素の special CableCarWarp）*/
void HoennNative_CableCarWarp(void)
{
	const u8 *st = gHoennCableCarStations[GOING_DOWN ? 0 : 1];

	SetWarpDestination(st[0], st[1], -1, 6, 4);
}

/* 駅のスクリプトから（素の special CableCar）*/
void HoennNative_CableCar(void)
{
	ScriptContext2_Enable();
	CreateTask(Task_LoadCableCar, 1);
	BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
}

static void Task_LoadCableCar(u8 taskId)
{
	if (!gPaletteFade->active)
	{
		SetMainCallback2(CB2_LoadCableCar);
		DestroyTask(taskId);
	}
}

static void CB2_LoadCableCar(void)
{
	struct CableCar *cc;
	u8 i, taskId;

	switch (gMain.state)
	{
	case 0:
	default:
		SetVBlankCallback(NULL);
		SetBgRegs(FALSE);
		ScanlineEffect_Stop();
		DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
		DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
		DmaFill16(3, 0, (void *)PLTT, PLTT_SIZE);
		gMain.state++;
		break;
	case 1:
		ResetSpriteData();
		ResetTasks();
		FreeAllSpritePalettes();
		ClearAllPalRefs();		/* ResetSpriteData も消すが、念のため（上の SyncPalRefs の説明）*/
		ResetPaletteFade();
		/* ★ ResetTasks のあとで入れ物を作る（預かりタスクが消されないように）*/
		cc = AllocZeroed(sizeof(struct CableCar));
		taskId = CreateTask(Task_CableCarHolder, 0xFF);
		gTasks[taskId].data[0] = (u32)cc;
		gTasks[taskId].data[1] = (u32)cc >> 16;
		CcStartWeather();
		for (i = 0; i < NUM_ASH_SPRITES; i++)
			gWeatherPtr->sprites.s2.ashSprites[i] = NULL;

		ResetMapMusic();
		ResetBgsAndClearDma3BusyFlags(0);
		InitBgsFromTemplates(0, sBgTemplates, NELEMS(sBgTemplates));
		for (i = 0; i < 4; i++)
			SetBgTilemapBuffer(i, cc->bgTilemapBuffers[i]);
		sCcSpriteOfsX = sCcSpriteOfsY = 0;
		gMain.state++;
		break;
	case 2:
		for (i = 0; i < NELEMS(sSpriteSheets); i++)
			LoadSpriteSheet(&sSpriteSheets[i]);
		{
			struct SpritePalette pal = { gHoennCableCarPal, TAG_CC_CAR };
			LoadSpritePalette(&pal);
		}
		{
			struct SpritePalette shiny = { gHoennCableCarZigzagoonShinyPal, TAG_CC_ZIG_SHINY };
			LoadSpritePalette(&shiny);
		}
		SyncPalRefs();		/* 天気（StartWeather の 0x1200）とゴンドラの枠を sPalRefs にも */
		DmaCopy16(3, gHoennCableCarBgTiles, (void *)VRAM, sizeof(gHoennCableCarBgTiles));
		gMain.state++;
		break;
	case 3:
		LoadPalette(gHoennCableCarBgPal, 0, 4 * 32);
		gMain.state++;
		break;
	case 4:
		CreateCableCarSprites();
		RunTasks();
		SyncPalRefs();		/* 主人公・通行人の枠も */
		gMain.state++;
		break;
	case 5:
		cc = CC();
		if (cc->weather == CC_WEATHER_VOLCANIC_ASH)
		{
			gMain.state++;
		}
		else if (gWeatherPtr->sprites.s2.ashSprites[0])
		{
			for (i = 0; i < NUM_ASH_SPRITES; i++)
			{
				if (gWeatherPtr->sprites.s2.ashSprites[i])
					gWeatherPtr->sprites.s2.ashSprites[i]->oam.priority = 0;
			}
			gMain.state++;
		}
		else
		{
			/* ★ エメラルドは灰が出るまでここで待つ。天気のタスクを回さないと出ないので回す */
			RunTasks();
			AnimateSprites();
			BuildOamBuffer();
		}
		break;
	case 6:
		CopyRect(1, gHoennCableCar_Trees, 0, 17, 32, 15);
		CopyRect(2, gHoennCableCar_BgMountains, 0, 0, 30, 20);
		CopyRect(3, gHoennCableCar_PylonTop, 0, 0, 5, 2);
		CopyRect(3, gHoennCableCar_PylonPole, 0, 2, 2, 20);
		gMain.state++;
		break;
	case 7:
		InitGroundTilemapData(GOING_DOWN);
		CopyRect(0, gHoennCableCar_Ground + 0x48, 0, 14, 12, 3);
		CopyRect(0, gHoennCableCar_Ground + 0x6C, 12, 17, 12, 3);
		CopyRect(0, gHoennCableCar_Ground + 0x90, 24, 20, 12, 3);
		CopyRect(0, gHoennCableCar_Ground + 0x0, 0, 17, 12, 3);
		CopyRect(0, gHoennCableCar_Ground + 0x24, 0, 20, 12, 3);
		CopyRect(0, gHoennCableCar_Ground + 0x0, 12, 20, 12, 3);
		CopyRect(0, gHoennCableCar_Ground + 0x24, 12, 23, 12, 3);
		CopyRect(0, gHoennCableCar_Ground + 0x0, 24, 23, 12, 3);
		gMain.state++;
		break;
	case 8:
		BeginNormalPaletteFade(0xFFFFFFFF, 3, 16, 0, RGB_BLACK);
		FadeInNewBGM(gHoennCableCarMusic, 1);
		SetBgRegs(TRUE);
		gMain.state++;
		break;
	case 9:
		cc = CC();
		IntrEnable(INTR_FLAG_VBLANK);
		SetVBlankCallback(VBlankCB_CableCar);
		SetMainCallback2(CB2_CableCar);
		CreateTask(Task_CableCar, 0);
		if (!GOING_DOWN)
			cc->bgTaskId = CreateTask(Task_AnimateBgGoingUp, 1);
		else
			cc->bgTaskId = CreateTask(Task_AnimateBgGoingDown, 1);
		break;
	}
}

static void CB2_CableCar(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void CB2_EndCableCar(void)
{
	struct CableCar *cc = CC();
	u8 i;

	HideBg(0);
	HideBg(1);
	HideBg(2);
	HideBg(3);
	SetBgRegs(FALSE);
	sCcSpriteOfsX = 0;
	SetCurrentAndNextWeatherNoDelay(0);
	for (i = 0; i < NUM_ASH_SPRITES; i++)
		gWeatherPtr->sprites.s2.ashSprites[i] = NULL;

	ResetTasks();
	ResetSpriteData();
	ResetPaletteFade();
	for (i = 0; i < 4; i++)
		UnsetBgTilemapBuffer(i);
	ResetBgsAndClearDma3BusyFlags(0);
	if (cc != NULL)
		Free(cc);
	DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
	DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
	DmaFill16(3, 0, (void *)PLTT, PLTT_SIZE);
	CcWarpIntoMap();
	gFieldCallback = NULL;
	SetMainCallback2(CB2_LoadMap);
}

static void Task_CableCar(u8 taskId)
{
	struct CableCar *cc = CC();
	u8 i = 0;

	cc->timer++;
	switch (cc->state)
	{
	case 0:
		/* 天気を変える時を待つ（上り: 350 フレームで灰が降り出す / 下り: 265 で晴れる）*/
		if (cc->timer == cc->weatherDelay)
		{
			if (cc->weather == CC_WEATHER_VOLCANIC_ASH)
			{
				RebuildPalRefs();           /* 灰の粒が 0x1200 の枠を見つけられるように（上の説明）*/
				CcAshInitVars();            /* 上り: 灰が降り出す */
				cc->ashState = ASH_ACTIVE;
			}
			else
			{
				gWeatherPtr->finishStep = 0; /* 下り: 灰を薄めて消す（素の Ash_Finish）*/
				cc->ashState = ASH_FINISHING;
			}
			cc->state = 1;
		}
		break;
	case 1:
		switch (cc->weather)
		{
		case CC_WEATHER_VOLCANIC_ASH:
			if (gWeatherPtr->sprites.s2.ashSprites[0] != NULL && gWeatherPtr->sprites.s2.ashSprites[0]->oam.priority != 0)
			{
				for (; i < NUM_ASH_SPRITES; i++)
				{
					if (gWeatherPtr->sprites.s2.ashSprites[i])
						gWeatherPtr->sprites.s2.ashSprites[i]->oam.priority = 0;
				}
				cc->state = 2;
			}
			break;
		case CC_WEATHER_SUNNY:
			if (cc->ashState == ASH_DONE)
			{
				cc->state = 2;
			}
			else if (cc->timer >= cc->weatherDelay + 8)
			{
				for (; i < NUM_ASH_SPRITES; i++)
				{
					if (gWeatherPtr->sprites.s2.ashSprites[i])
						gWeatherPtr->sprites.s2.ashSprites[i]->invisible ^= 1;
				}
			}
			break;
		}
		/* ★★ 逃げ道（2026-09-24 実機「無限に登っていく」）。エメラルドは灰の粒ができるのを
		   待ってから 2 へ進み、2 は timer == 570 の 1 フレームでしか暗転しない。
		   1 回目は天気 7 が白いすなあらしで灰の粒が 1 つもできず、ここで永久に待っていた。
		   粒ができなくても、灰が消えきらなくても、570 で必ず終わる */
		if (cc->state == 1 && cc->timer >= 570)
			cc->state = 2;
		break;
	case 2:
		if (cc->timer >= 570)	/* == だと 570 を過ぎてから来たとき二度と通らない */
		{
			cc->state = 3;
			BeginNormalPaletteFade(0xFFFFFFFF, 3, 0, 16, RGB_BLACK);
			CcFadeOutBGM(4);
		}
		break;
	case 3:
		if (!gPaletteFade->active)
			cc->state = STATE_END;
		break;
	case STATE_END:
		SetVBlankCallback(NULL);
		DestroyTask(taskId);
		DestroyTask(cc->bgTaskId);
		SetMainCallback2(CB2_EndCableCar);
		break;
	}
}

/* 素の Task_WeatherMain が灰に対してやること（main か、終わらせている間は finish）*/
static void RunAsh(struct CableCar *cc)
{
	if (cc->state == STATE_END)
		return;
	if (cc->ashState == ASH_ACTIVE)
		CcAshMain();
	else if (cc->ashState == ASH_FINISHING && !CcAshFinish())
		cc->ashState = ASH_DONE;
}

static void Task_AnimateBgGoingUp(unusedArg u8 taskId)
{
	struct CableCar *cc = CC();

	if (cc->state != STATE_END)
	{
		cc->bg3HorizontalOffset--;
		if ((cc->timer % 2) == 0)
			cc->bg3VerticalOffset--;

		if ((cc->timer % 8) == 0)
		{
			cc->bg1HorizontalOffset--;
			cc->bg1VerticalOffset--;
		}

		switch (cc->bg3HorizontalOffset)
		{
		case 175:
			FillRect(3, 0, 0, 22, 2, 10);
			break;
		case 40:
			FillRect(3, 0, 3, 0, 2, 2);
			break;
		case 32:
			FillRect(3, 0, 2, 0, 1, 2);
			break;
		case 16:
			CopyRect(3, gHoennCableCar_PylonTop, 0, 0, 5, 2);
			CopyRect(3, gHoennCableCar_PylonPole, 0, 2, 2, 30);
			cc->bg3VerticalOffset = 64;
			break;
		}
	}

	AnimateGroundGoingUp();
	sCcSpriteOfsX = (sCcSpriteOfsX + 1) % 128;
	RunAsh(cc);
}

static void Task_AnimateBgGoingDown(unusedArg u8 taskId)
{
	struct CableCar *cc = CC();

	if (cc->state != STATE_END)
	{
		cc->bg3HorizontalOffset++;
		if ((cc->timer % 2) == 0)
			cc->bg3VerticalOffset++;

		if ((cc->timer % 8) == 0)
		{
			cc->bg1HorizontalOffset++;
			cc->bg1VerticalOffset++;
		}

		switch (cc->bg3HorizontalOffset)
		{
		case 176:
			CopyRect(3, gHoennCableCar_PylonPole, 0, 2, 2, 30);
			break;
		case 16:
			FillRect(3, 0, 2, 0, 3, 2);
			FillRect(3, 0, 0, 22, 2, 10);
			cc->bg3VerticalOffset = 192;
			break;
		case 32:
			FillRect(3, gHoennCableCar_PylonTop[2], 2, 0, 1, 1);
			FillRect(3, gHoennCableCar_PylonTop[3], 3, 0, 1, 1);
			FillRect(3, gHoennCableCar_PylonTop[7], 2, 1, 1, 1);
			FillRect(3, gHoennCableCar_PylonTop[8], 3, 1, 1, 1);
			break;
		case 40:
			FillRect(3, gHoennCableCar_PylonTop[4], 4, 0, 1, 1);
			FillRect(3, gHoennCableCar_PylonTop[9], 4, 1, 1, 1);
			break;
		}
	}

	AnimateGroundGoingDown();
	if (cc->timer < cc->weatherDelay)
		sCcSpriteOfsX = (sCcSpriteOfsX + 247) % 248;
	else
		gWeatherPtr->ashBaseSpritesX = (gWeatherPtr->ashBaseSpritesX + 247) % 248;
	RunAsh(cc);
}

static void VBlankCB_CableCar(void)
{
	struct CableCar *cc = CC();

	CopyBgTilemapBufferToVram(0);
	CopyBgTilemapBufferToVram(3);
	SetGpuReg(REG_OFFSET_BG3HOFS, cc->bg3HorizontalOffset);
	SetGpuReg(REG_OFFSET_BG3VOFS, cc->bg3VerticalOffset);
	SetGpuReg(REG_OFFSET_BG1HOFS, cc->bg1HorizontalOffset);
	SetGpuReg(REG_OFFSET_BG1VOFS, cc->bg1VerticalOffset);
	SetGpuReg(REG_OFFSET_BG0HOFS, cc->bg0HorizontalOffset);
	SetGpuReg(REG_OFFSET_BG0VOFS, cc->bg0VerticalOffset);
	LoadOam();
	ProcessSpriteCopyRequests();
	TransferPlttBuffer();
}

/* ---- スプライト ---- */
#define sXPos data[0]
#define sYPos data[1]

/* 0.14f * timer / 0.067f * timer の切り捨てを整数で（エメラルドは (u8) にしてから引く）*/
#define CAR_DX(t) ((u8)(((u32)(t) * 14) / 100))
#define CAR_DY(t) ((u8)(((u32)(t) * 67) / 1000))

static void SpriteCB_CableCar(struct Sprite *sprite)
{
	struct CableCar *cc = CC();

	if (cc->state != STATE_END)
	{
		if (!GOING_DOWN)
		{
			sprite->pos1.x = sprite->sXPos - CAR_DX(cc->timer);
			sprite->pos1.y = sprite->sYPos - CAR_DY(cc->timer);
		}
		else
		{
			sprite->pos1.x = sprite->sXPos + CAR_DX(cc->timer);
			sprite->pos1.y = sprite->sYPos + CAR_DY(cc->timer);
		}
	}
}

#define sState data[2]
#define sTimer data[3]

static void SpriteCB_Player(struct Sprite *sprite)
{
	struct CableCar *cc = CC();

	if (cc->state != STATE_END)
	{
		if (!GOING_DOWN)
		{
			sprite->pos1.x = sprite->sXPos - CAR_DX(cc->timer);
			sprite->pos1.y = sprite->sYPos - CAR_DY(cc->timer);
		}
		else
		{
			sprite->pos1.x = sprite->sXPos + CAR_DX(cc->timer);
			sprite->pos1.y = sprite->sYPos + CAR_DY(cc->timer);
		}

		/* ゴンドラの揺れに合わせて上下する */
		switch (sprite->sState)
		{
		case 0:
			sprite->pos2.y = 17;
			if (sprite->sTimer++ > 9)
			{
				sprite->sTimer = 0;
				sprite->sState++;
			}
			break;
		default:
			sprite->pos2.y = 16;
			if (sprite->sTimer++ > 9)
			{
				sprite->sTimer = 0;
				sprite->sState = 0;
			}
			break;
		}
	}
}

#undef sState
#undef sTimer

#define sHTimer   data[0]
#define sSameDir  data[1]
#define sDelay    data[2]

static void SpriteCB_HikerGoingUp(struct Sprite *sprite)
{
	if (sprite->sHTimer == 0)
	{
		sprite->pos1.x += 2 * sprite->centerToCornerVecX;
		sprite->pos1.y += 16 + sprite->centerToCornerVecY;
	}

	if (++sprite->sHTimer >= sprite->sDelay)
	{
		switch (sprite->sSameDir)
		{
		case FALSE:
			sprite->pos1.x++;
			if ((sprite->sHTimer % 4) == 0)
				sprite->pos1.y++;
			break;
		case TRUE:
			if ((sprite->sHTimer % 2) != 0)
			{
				sprite->pos1.x++;
				if ((sprite->pos1.x % 4) == 0)
					sprite->pos1.y++;
			}
			break;
		}

		if (sprite->pos1.y > 160)
			DestroySprite(sprite);
	}
}

static void SpriteCB_HikerGoingDown(struct Sprite *sprite)
{
	if (sprite->sHTimer == 0)
		sprite->pos1.y += 16 + sprite->centerToCornerVecY;

	if (++sprite->sHTimer >= sprite->sDelay)
	{
		switch (sprite->sSameDir)
		{
		case FALSE:
			sprite->pos1.x--;
			if ((sprite->sHTimer % 4) == 0)
				sprite->pos1.y--;
			break;
		case TRUE:
			if ((sprite->sHTimer % 2) != 0)
			{
				sprite->pos1.x--;
				if ((sprite->pos1.x % 4) == 0)
					sprite->pos1.y--;
			}
			break;
		}

		if (sprite->pos1.y < 80)
			DestroySprite(sprite);
	}
}

static void SetBgRegs(bool8 active)
{
	struct CableCar *cc;

	SetGpuReg(REG_OFFSET_WININ, 0);
	SetGpuReg(REG_OFFSET_WINOUT, 0);
	SetGpuReg(REG_OFFSET_WIN0H, 0);
	SetGpuReg(REG_OFFSET_WIN1H, 0);
	SetGpuReg(REG_OFFSET_WIN0V, 0);
	SetGpuReg(REG_OFFSET_WIN1V, 0);
	if (!active)
	{
		SetGpuReg(REG_OFFSET_DISPCNT, 0);
		SetGpuReg(REG_OFFSET_BG3CNT, 0);
		SetGpuReg(REG_OFFSET_BG2CNT, 0);
		SetGpuReg(REG_OFFSET_BG1CNT, 0);
		SetGpuReg(REG_OFFSET_BG0CNT, 0);
		SetGpuReg(REG_OFFSET_BG3HOFS, 0);
		SetGpuReg(REG_OFFSET_BG3VOFS, 0);
		SetGpuReg(REG_OFFSET_BG2HOFS, 0);
		SetGpuReg(REG_OFFSET_BG2VOFS, 0);
		SetGpuReg(REG_OFFSET_BG1HOFS, 0);
		SetGpuReg(REG_OFFSET_BG1VOFS, 0);
		SetGpuReg(REG_OFFSET_BG0HOFS, 0);
		SetGpuReg(REG_OFFSET_BG0VOFS, 0);
		SetGpuReg(REG_OFFSET_BLDCNT, 0);
		return;
	}

	cc = CC();
	if (!GOING_DOWN)
	{
		cc->bg3HorizontalOffset = 176;
		cc->bg3VerticalOffset = 16;
		cc->bg1HorizontalOffset = 0;
		cc->bg1VerticalOffset = 80;
		cc->bg0VerticalOffset = 0;
	}
	else
	{
		cc->bg3HorizontalOffset = 96;
		cc->bg3VerticalOffset = 232;
		cc->bg1HorizontalOffset = 0;
		cc->bg1VerticalOffset = 4;
		cc->bg0VerticalOffset = 0;
	}

	SetGpuReg(REG_OFFSET_BG3HOFS, cc->bg3HorizontalOffset);
	SetGpuReg(REG_OFFSET_BG3VOFS, cc->bg3VerticalOffset);
	SetGpuReg(REG_OFFSET_BG2HOFS, 0);
	SetGpuReg(REG_OFFSET_BG2VOFS, 0);
	SetGpuReg(REG_OFFSET_BG1HOFS, cc->bg1HorizontalOffset);
	SetGpuReg(REG_OFFSET_BG1VOFS, cc->bg1VerticalOffset);
	SetGpuReg(REG_OFFSET_BG0HOFS, cc->bg0HorizontalOffset);
	SetGpuReg(REG_OFFSET_BG0VOFS, cc->bg0VerticalOffset);
	SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
	CopyBgTilemapBufferToVram(1);
	CopyBgTilemapBufferToVram(2);
	ShowBg(0);
	ShowBg(1);
	ShowBg(2);
	ShowBg(3);
	SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_TGT2_ALL);
}

static u8 CreateCarPart(const struct SpriteTemplate *template, s16 x, s16 y, u8 subpriority, s16 x2, s16 y2)
{
	u8 spriteId = CreateSprite(template, x, y, subpriority);

	if (spriteId != MAX_SPRITES)
	{
		gSprites[spriteId].pos2.x = x2;
		gSprites[spriteId].pos2.y = y2;
		gSprites[spriteId].sXPos = x;
		gSprites[spriteId].sYPos = y;
	}
	return spriteId;
}

static void CreateCableCarSprites(void)
{
	struct CableCar *cc = CC();
	u8 spriteId;
	u8 i;
	u16 rval = Random();
	u16 playerGfx = GetPlayerAvatarGraphicsIdByStateIdAndGender(PLAYER_AVATAR_STATE_NORMAL, gSaveBlock2->playerGender);
	/* ★ エメラルドは ARRAY_COUNT - 1 のバグでジグザグマが出ない。こちらは 4 種とも出す
	   （ジグザグマはエメラルドの絵を表 7 に入れたもの。番号は生成物から読むので実行時に組む）*/
	u16 hikerGfx[4];
	static const s16 sHikerCoords[2][2] = {
		{   0,  80 }, /* 上り */
		{ 240, 146 }, /* 下り */
	};
	static const u8 sHikerDelay[4] = { 0, 60, 120, 170 };
	/* 1 組の人数（最小・最大）と間隔（画素）。[3] がジグザグマ */
	static const u8 sGroupMin[4] = { 2, 2, 2, 3 };
	static const u8 sGroupMax[4] = { 4, 4, 4, 6 };
	static const u8 sGroupGap[4] = { 18, 18, 18, 22 };
	s16 px, py;
	u8 g, groups, firstDelay;
	u8 shinySlot = IndexOfSpritePaletteTag(TAG_CC_ZIG_SHINY);

	hikerGfx[0] = CC_GFX_HIKER;
	hikerGfx[1] = CC_GFX_CAMPER;
	hikerGfx[2] = CC_GFX_PICNICKER;
	hikerGfx[3] = gHoennCableCarZigzagoonGfx;

	if (!GOING_DOWN)
	{
		px = 200; py = 73;
		CreateCarPart(&sSpriteTemplate_CableCar, 176, 43, 0x67, 32, 32);
		CreateCarPart(&sSpriteTemplate_Door, 200, 99, 0x65, 8, 4);
		cc->weather = CC_WEATHER_VOLCANIC_ASH;
		cc->weatherDelay = 350;
		SetCurrentAndNextWeatherNoDelay(CC_WEATHER_SUNNY);
	}
	else
	{
		CopyRect(0, gHoennCableCar_Ground + 0x24, 24, 26, 12, 3);
		px = 128; py = 39;
		CreateCarPart(&sSpriteTemplate_CableCar, 104, 9, 0x67, 32, 32);
		CreateCarPart(&sSpriteTemplate_Door, 128, 65, 0x65, 8, 4);
		cc->weather = CC_WEATHER_SUNNY;
		cc->weatherDelay = 265;
		SetCurrentAndNextWeatherNoDelay(CC_WEATHER_SUNNY);
		CcAshInitAll();                    /* 下りは灰の中から始まる（素の天気 7 の initAll）*/
		cc->ashState = ASH_ACTIVE;
	}

	/* 主人公（エメラルドはゴンドラより先に作る。重なりの順を同じにする）*/
	spriteId = AddPseudoEventObject(playerGfx, SpriteCB_Player, px, py, 102);
	if (spriteId != MAX_SPRITES)
	{
		gSprites[spriteId].oam.priority = 2;
		gSprites[spriteId].pos2.x = 8;
		gSprites[spriteId].pos2.y = 16;
		gSprites[spriteId].sXPos = px;
		gSprites[spriteId].sYPos = py;
	}

	for (i = 0; i < 9; i++)
	{
		spriteId = CreateSprite(&sSpriteTemplate_Cable, 16 * i + 96, 8 * i - 8, 0x68);
		if (spriteId != MAX_SPRITES)
		{
			gSprites[spriteId].pos2.x = 8;
			gSprites[spriteId].pos2.y = 8;
		}
	}

	/* 山道に通行人が出る（エメラルドは 1/64 で 1 人だけ）*/
	if ((rval % CC_HIKER_CHANCE) != 0)
		return;

	groups = 1 + Random() % CC_MAX_GROUPS;
	firstDelay = Random() % 4;
	for (g = 0; g < groups; g++)
	{
		/* ★ 種類・人数・向きは組ごとに別の乱数で（エメラルドは 1 つの乱数から取るので、
		   出たときは必ず 64 の倍数＝向きも待ち時間もいつも同じだった）*/
		u8 kind = Random() % 4;
		u8 count = sGroupMin[kind] + Random() % (sGroupMax[kind] - sGroupMin[kind] + 1);
		bool8 sameDir = Random() % 2;
		u8 delay = sHikerDelay[(firstDelay + g) % 4];	/* 組どうしは出発をずらす */
		u8 k;

		for (k = 0; k < count; k++)
		{
			/* 先頭のうしろ（画面の外側）へ、歩く線に沿って並べる。少しだけ揺らす */
			s16 back = k * sGroupGap[kind] + (k ? (s16)(Random() % 5) - 2 : 0);
			s16 x, y;
			struct Sprite *s;

			if (!GOING_DOWN)
			{
				x = sHikerCoords[0][0] - back;
				y = sHikerCoords[0][1] - back / 4;
			}
			else
			{
				x = sHikerCoords[1][0] + back;
				y = sHikerCoords[1][1] + back / 4;
			}

			spriteId = AddPseudoEventObject(hikerGfx[kind],
			                                GOING_DOWN ? SpriteCB_HikerGoingDown : SpriteCB_HikerGoingUp,
			                                x, y, 106);
			if (spriteId == MAX_SPRITES)
				return;		/* スプライトが尽きたらそこまで */

			s = &gSprites[spriteId];
			s->oam.priority = 2;
			s->pos2.x = -s->centerToCornerVecX;
			s->pos2.y = -s->centerToCornerVecY;
			if (sameDir)
			{
				StartSpriteAnim(s, GOING_DOWN ? CC_ANIM_GO_EAST : CC_ANIM_GO_WEST);
				s->sSameDir = TRUE;
				s->pos1.y += 2;
			}
			else
			{
				StartSpriteAnim(s, GOING_DOWN ? CC_ANIM_GO_WEST : CC_ANIM_GO_EAST);
				s->sSameDir = FALSE;
			}
			s->sDelay = delay;
			if (kind == 3 && shinySlot != 0xFF && (Random() % CC_SHINY_CHANCE) == 0)
				s->oam.paletteNum = shinySlot;	/* 色違い */
		}
	}
}

/* ---- 足もとの地面（斜めに流れていく）---- */
static void BufferNextGroundSegment(void)
{
	struct CableCar *cc = CC();
	u8 i, j, k;
	u8 offset;

	for (i = 0, k = 0, offset = 0x24 * (cc->groundTilemapOffset + 2); i < 3; i++)
	{
		for (j = 0; j < 12; j++)
		{
			cc->groundTileBuffer[i][j] = gHoennCableCar_Ground[offset++];
			cc->groundTileBuffer[i + 3][j] = gHoennCableCar_Ground[k];
			cc->groundTileBuffer[i + 6][j] = (gHoennCableCar_Ground + 0x24)[k];
			k++;
		}
	}

	cc->groundTilemapOffset = (cc->groundTilemapOffset + 1) % 3;
}

static void AnimateGroundGoingUp(void)
{
	struct CableCar *cc = CC();

	cc->bg0HorizontalOffset = cc->groundXBase - cc->groundXOffset;
	cc->bg0VerticalOffset = cc->groundYBase - cc->groundYOffset;
	cc->groundXOffset++;
	if ((cc->groundXOffset % 4) == 0)
		cc->groundYOffset++;

	if (cc->groundXOffset > 16)
		DrawNextGroundSegmentGoingUp();
}

static void AnimateGroundGoingDown(void)
{
	struct CableCar *cc = CC();

	cc->bg0HorizontalOffset = cc->groundXBase + cc->groundXOffset;
	cc->bg0VerticalOffset = cc->groundYBase + cc->groundYOffset;
	cc->groundXOffset++;
	if ((cc->groundXOffset % 4) == 0)
		cc->groundYOffset++;

	if (cc->groundXOffset > 16)
		DrawNextGroundSegmentGoingDown();
}

static void DrawNextGroundSegmentGoingUp(void)
{
	struct CableCar *cc = CC();
	u8 i;
	u8 x, y, segY;

	cc->groundXOffset = cc->groundYOffset = 0;
	cc->groundXBase = cc->bg0HorizontalOffset;
	cc->groundYBase = cc->bg0VerticalOffset;
	cc->groundSegmentXStart = (cc->groundSegmentXStart + 30) % 32;
	cc->groundTileIdx -= 2;
	segY = (cc->groundSegmentYStart + 23) % 32;

	for (i = 0; i < 9; i++)
	{
		x = cc->groundSegmentXStart;
		y = (segY + i) % 32;
		FillRect(0, cc->groundTileBuffer[i][cc->groundTileIdx], x, y, 1, 1);
		x = (x + 1) % 32;
		FillRect(0, cc->groundTileBuffer[i][cc->groundTileIdx + 1], x, y, 1, 1);
	}

	x = (cc->groundSegmentXStart + 30) % 32;
	FillRect(0, 0, x, 0, 2, 32);
	if (cc->groundTileIdx == 0)
	{
		cc->groundSegmentYStart = (cc->groundSegmentYStart + 29) % 32;
		cc->groundTileIdx = 12;
		BufferNextGroundSegment();
		x = (cc->groundSegmentYStart + 1) % 32;
		FillRect(0, 0, 0, x, 32, 9);
	}
}

static void DrawNextGroundSegmentGoingDown(void)
{
	struct CableCar *cc = CC();
	u8 i;
	u8 x, y, segY;

	cc->groundXOffset = cc->groundYOffset = 0;
	cc->groundXBase = cc->bg0HorizontalOffset;
	cc->groundYBase = cc->bg0VerticalOffset;
	cc->groundSegmentXStart = (cc->groundSegmentXStart + 2) % 32;
	cc->groundTileIdx += 2;
	segY = cc->groundSegmentYStart;

	for (i = 0; i < 9; i++)
	{
		x = cc->groundSegmentXStart;
		y = (segY + i) % 32;
		FillRect(0, cc->groundTileBuffer[i][cc->groundTileIdx], x, y, 1, 1);
		x = (x + 1) % 32;
		FillRect(0, cc->groundTileBuffer[i][cc->groundTileIdx + 1], x, y, 1, 1);
	}

	y = (cc->groundSegmentYStart + 23) % 32;
	FillRect(0, 0, cc->groundSegmentXStart, y, 2, 9);
	if (cc->groundTileIdx == 10)
	{
		cc->groundSegmentYStart = (cc->groundSegmentYStart + 3) % 32;
		cc->groundTileIdx = -2;
		BufferNextGroundSegment();
	}
}

static void InitGroundTilemapData(bool8 goingDown)
{
	struct CableCar *cc = CC();

	if (!goingDown)
	{
		cc->groundTilemapOffset = 2;
		cc->groundSegmentXStart = 0;
		cc->groundSegmentYStart = 20;
		cc->groundTileIdx = 12;
		BufferNextGroundSegment();
		DrawNextGroundSegmentGoingUp();
	}
	else
	{
		cc->groundTilemapOffset = 2;
		cc->groundSegmentXStart = 28;
		cc->groundSegmentYStart = 20;
		cc->groundTileIdx = 4;
		BufferNextGroundSegment();
		DrawNextGroundSegmentGoingDown();
	}
}
