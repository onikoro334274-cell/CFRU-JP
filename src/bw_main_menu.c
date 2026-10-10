#include "defines.h"
#include "../include/bg.h"
#include "../include/decompress.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/menu.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/pokedex.h"
#include "../include/pokemon_icon.h"
#include "../include/region_map.h"
#include "../include/save.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/constants/songs.h"

/*
 * ★ 2026-10-10 ユーザー「BW系これになおして」→「全部そちらに置き換える」・画像 Start.png（BW のタイトルのあとのメニュー）:
 * タイトルのあとのメニュー（つづきから・さいしょから・ふしぎなもらいもの・せってい）を BW の絵に。
 * bw-reloaded（https://github.com/pm-start/bw-reloaded 0e7888f）の src/main_menu.c を CFRU の名前で書き直したもの
 * （絵は C-injections-FR-Main-Menu-BW・クレジットは tools/bwreload/CREDITS_bw-reloaded.md）。
 *
 * - 入口: 素の 0800C1A0（メニューの窓を描く関数）の中・CFRU の時計の注意のフック（0800C1A4）が戻る所 0800C1AC に hooks
 *   （assembly/hack_bwreload/bw_main_menu_hook.s）。時計の注意が出たときは CFRU のフックがそちらへ行くので、ここには来ない
 * - タスクの data[0]＝メニューの形（素と同じ: 0 さいしょから だけ・1 つづきから・2 ふしぎなもらいもの つき）・data[1]＝カーソル
 *   ＝素の A を押したときの処理（0800C471）がそのまま読める。選んだら素の処理へ任せる（つづきから・さいしょから・ふしぎなもらいもの は素のまま）
 * - せってい は自前の BW のせってい画面（src/bw_option_menu.c）へ・戻り先はここ（BwMainMenuReopen）。B はタイトルへ
 * - このハックに合わせた所: バッジは 3 地方の数（GetAllBadgeCount）・ずかんは 4 けたまで・主人公の絵は BW の主人公
 *   （GetPlayerAvatarGraphicsIdByStateIdAndGender）・地名は GetMapName（地方の読み替えは中のフック）
 */

extern const u8 gBwrMainBgPal[], gBwrMainIdlePals[], gBwrMainSelectedPal[];
extern const u8 gBwrMainTiles1[], gBwrMainTiles2[], gBwrMainMapBg[];
extern const u8 gBwrMainMapContinue[], gBwrMainMapNewGame[], gBwrMainMapGift[];
extern const u8 gText_BwrMain_Continue[], gText_BwrMain_NewGame[], gText_BwrMain_Gift[], gText_BwrMain_Options[];
extern const u8 gText_BwrMain_Team[], gText_BwrMain_Badges[], gText_BwrMain_Dex[], gText_BwrMain_Time[];
extern void CB2_BwOptionMenu(void);   //src/bw_option_menu.c
u8 GetAllBadgeCount(void);            //src/badge_count.c
void HackMG_Receive(void);            //src/hack_mystery_gift.c（印・レポート・結果の文を gStringVar4 に）
u16 __attribute__((long_call)) GetNationalPokedexCount(u8 caseId);
void __attribute__((long_call)) TransferPlttBuffer(void);

#define Task_HandleMainMenuAPressed ((TaskFunc) (0x0800C470 | 1))   //素（A を押したあと・data[0]／data[1] を読む）
#define CB2_InitTitleScreen ((MainCallback) (0x080780AC | 1))
#define IsMysteryGiftEnabled ((bool8 (*)(void)) (0x0806DAB0 | 1))   //素（このハックは bytereplacement でいつも TRUE＝ふしぎなもらいもの はいつも出る）

enum { SAVED, SELECTED, ROUTE, AVATAR };
enum { ROUTE_NONE, ROUTE_OPTIONS, ROUTE_TITLE, ROUTE_GIFT };

static const struct BgTemplate sBgTemplates[] =
{
	{.bg = 0, .charBaseIndex = 2, .mapBaseIndex = 31, .priority = 0},
	{.bg = 1, .charBaseIndex = 0, .mapBaseIndex = 29, .priority = 1},
	{.bg = 2, .charBaseIndex = 1, .mapBaseIndex = 30, .priority = 2},
};
static const struct WindowTemplate sWindowTemplates[] =
{
	{.bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 30, .height = 20, .paletteNum = 15, .baseBlock = 0},
	DUMMY_WIN_TEMPLATE,
};
static const u16 sTextPal[] = {0, 0x7BDE, 0x4631, 0x7E87, 0x51A7};
static const u16 sFemaleNamePal[] = {0x2D3F, 0x2115};

static void Task_BwMainInput(u8 taskId);

static void Print(const u8 *text, u8 x, u8 y, u8 color)
{
	u8 buffer[96];
	u32 i = 6;
	buffer[0] = EXT_CTRL_CODE_BEGIN; buffer[1] = 1; buffer[2] = color;
	buffer[3] = EXT_CTRL_CODE_BEGIN; buffer[4] = 2; buffer[5] = 0;
	while (*text != EOS && i < 94)
		buffer[i++] = *text++;
	buffer[i] = EOS;
	AddTextPrinterParameterized(0, 2, buffer, x, y + 1, TEXT_SPEED_FF, NULL);
}

static void PrintStat(const u8 *label, u32 value, u8 digits, u8 y)
{
	u8 buffer[48];
	u8 *p = StringCopy(buffer, label);
	ConvertIntToDecimalStringN(p, value, STR_CONV_MODE_LEADING_ZEROS, digits);
	Print(buffer, 128, y, 1);
}

static void IconCallback(struct Sprite *sprite)
{
	u8 taskId = FindTaskIdByFunc(Task_BwMainInput);
	if (taskId < NUM_TASKS && gTasks[taskId].data[SAVED] && !gTasks[taskId].data[SELECTED])
		UpdateMonIconFrame(sprite);
}

static void DrawParty(s16 *data)
{
	u32 i, count = gPlayerPartyCount;

	data[AVATAR] = AddPseudoEventObject(GetPlayerAvatarGraphicsIdByStateIdAndGender(PLAYER_AVATAR_STATE_NORMAL, gSaveBlock2->playerGender),
	                                    SpriteCallbackDummy, 40, 40, 0);
	if (data[AVATAR] < MAX_SPRITES)
	{
		gSprites[data[AVATAR]].oam.priority = 0;
		StartSpriteAnimIfDifferent(&gSprites[data[AVATAR]], 4);
	}
	LoadMonIconPalettes();
	if (count > PARTY_SIZE)
		count = PARTY_SIZE;
	for (i = 0; i < count; ++i)
		CreateMonIcon(GetMonData(&gPlayerParty[i], MON_DATA_SPECIES2, NULL), IconCallback, 40 + i * 32, 88, 0,
		              GetMonData(&gPlayerParty[i], MON_DATA_PERSONALITY, NULL), TRUE);
}

static void PrintStats(void)
{
	u8 time[48], name[64];
	u8 *p;
	u32 badges, hours, minutes;
	const struct MapHeader *header;

	Print(gSaveBlock2->playerName, 72, 40, 3);
	Print(gText_BwrMain_Team, 24, 56, 1);
	badges = GetAllBadgeCount();
	PrintStat(gText_BwrMain_Badges, badges, badges >= 10 ? 2 : 1, 24);
	if (FlagGet(FLAG_SYS_POKEDEX_GET))
	{
		u32 dex = GetNationalPokedexCount(FLAG_GET_CAUGHT);
		PrintStat(gText_BwrMain_Dex, dex, dex >= 1000 ? 4 : 3, 40);
	}
	hours = gSaveBlock2->playTimeHours;
	minutes = gSaveBlock2->playTimeMinutes;
	if (hours > 999)
		hours = 999;
	if (minutes > 59)
		minutes = 59;
	p = StringCopy(time, gText_BwrMain_Time);
	p = ConvertIntToDecimalStringN(p, hours, STR_CONV_MODE_LEADING_ZEROS, 3);
	*p++ = 0xF0;   //：
	ConvertIntToDecimalStringN(p, minutes, STR_CONV_MODE_LEADING_ZEROS, 2);
	Print(time, 128, 56, 1);
	header = Overworld_GetMapHeaderByGroupAndId(gSaveBlock1->location.mapGroup, gSaveBlock1->location.mapNum);
	if (header != NULL)
	{
		GetMapName(name, header->regionMapSectionId, 0);
		Print(name, 128, 8, 1);
	}
}

static void UpdateSelection(s16 *data)
{
	LoadPalette(gBwrMainIdlePals, 16, 96);
	if (data[SAVED] == 2)
		LoadPalette(gBwrMainIdlePals + 64, 64, 32);
	LoadPalette(gBwrMainSelectedPal, 16 + 16 * data[SELECTED], 32);
	if (data[SAVED] && data[AVATAR] < MAX_SPRITES)
		StartSpriteAnimIfDifferent(&gSprites[data[AVATAR]], data[SELECTED] ? 0 : 4);
}

void BwMainMenuReopen(void);

static void Task_BwMainRoute(u8 taskId)
{
	bool8 options;

	if (gPaletteFade->active)
		return;
	options = gTasks[taskId].data[ROUTE] == ROUTE_OPTIONS;
	FreeAllWindowBuffers();
	DestroyTask(taskId);
	if (options)
	{
		gMain.savedCallback = BwMainMenuReopen;
		SetMainCallback2(CB2_BwOptionMenu);
	}
	else
		SetMainCallback2(CB2_InitTitleScreen);
}

//ふしぎなもらいもの: 素のお知らせ（素のメニューの窓に描く）はこの画面の窓と合わないので、ここで受け取って文を出し、A でメニューに戻る
static void Task_BwMainGift(u8 taskId)
{
	s16 *data = gTasks[taskId].data;

	if (data[ROUTE] == ROUTE_GIFT)
	{
		HackMG_Receive();
		FillWindowPixelBuffer(0, PIXEL_FILL(0));
		Print(gStringVar4, 24, 8, 1);
		CopyWindowToVram(0, 3);
		data[ROUTE] = ROUTE_NONE;
		return;
	}
	if (JOY_NEW(A_BUTTON | B_BUTTON))
	{
		PlaySE(SE_SELECT);
		BwMainMenuReopen();
	}
}

static void Task_BwMainInput(u8 taskId)
{
	s16 *data = gTasks[taskId].data;
	s16 last;

	if (gPaletteFade->active)
		return;
	last = data[SAVED] == 2 ? 3 : (data[SAVED] ? 2 : 1);
	if (JOY_NEW(A_BUTTON | START_BUTTON) && data[SAVED] == 2 && data[SELECTED] == 2)
	{
		PlaySE(SE_SELECT);   //ふしぎなもらいもの: 暗くしないでこの画面で受け取る（暗転は始めると止められない）
		data[ROUTE] = ROUTE_GIFT;
		gTasks[taskId].func = Task_BwMainGift;
	}
	else if (JOY_NEW(A_BUTTON | START_BUTTON | B_BUTTON))
	{
		PlaySE(SE_SELECT);
		BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
		if (JOY_NEW(B_BUTTON))
		{
			data[ROUTE] = ROUTE_TITLE;
			gTasks[taskId].func = Task_BwMainRoute;
		}
		else if (data[SELECTED] == last)
		{
			data[ROUTE] = ROUTE_OPTIONS;
			gTasks[taskId].func = Task_BwMainRoute;
		}
		else
			gTasks[taskId].func = Task_HandleMainMenuAPressed;   //data[0] 形・data[1] カーソル（素と同じ並び）
	}
	else if (JOY_NEW(DPAD_UP) && data[SELECTED] > 0)
	{
		--data[SELECTED];
		PlaySE(SE_SELECT);
		UpdateSelection(data);
	}
	else if (JOY_NEW(DPAD_DOWN) && data[SELECTED] < last)
	{
		++data[SELECTED];
		PlaySE(SE_SELECT);
		UpdateSelection(data);
	}
}

static void MainCB2_BwMain(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void VBlankCB_BwMain(void)
{
	LoadOam();
	ProcessSpriteCopyRequests();
	TransferPlttBuffer();
}

static void BwMainSetup(s16 saved, bool8 releaseWindows)
{
	u8 taskId, bg;
	s16 *data;
	u8 reg;

	SetVBlankCallback(NULL);
	SetHBlankCallback(NULL);
	SetGpuReg(REG_OFFSET_DISPCNT, 0);
	if (releaseWindows)
		FreeAllWindowBuffers();
	ResetTasks();
	ResetSpriteData();
	FreeSpriteTileRanges();
	FreeAllSpritePalettes();
	ResetPaletteFade();
	CpuFill32(0, (void *) VRAM, 0x18000);
	ResetBgsAndClearDma3BusyFlags(0);
	InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
	for (bg = 0; bg < 4; ++bg)
	{
		ChangeBgX(bg, 0, 0);
		ChangeBgY(bg, 0, 0);
	}
	InitWindows(sWindowTemplates);
	DeactivateAllTextPrinters();
	for (reg = REG_OFFSET_WIN0H; reg <= REG_OFFSET_BLDY; reg += 2)
		SetGpuReg(reg, 0);
	SetGpuReg(REG_OFFSET_BLDCNT, 0x442);
	SetGpuReg(REG_OFFSET_BLDALPHA, 0x710);
	LoadPalette(gBwrMainBgPal, 0, 32);
	LoadPalette(sTextPal, 240, sizeof(sTextPal));
	if (gSaveBlock2->playerGender)
		LoadPalette(sFemaleNamePal, 243, sizeof(sFemaleNamePal));
	LZDecompressVram(gBwrMainTiles1, (void *) (VRAM + 0x0000));
	LZDecompressVram(gBwrMainTiles2, (void *) (VRAM + 0x4000));
	LZDecompressVram(gBwrMainMapBg, (void *) (VRAM + 0xF000));
	LZDecompressVram(saved == 2 ? gBwrMainMapGift : (saved ? gBwrMainMapContinue : gBwrMainMapNewGame), (void *) (VRAM + 0xE800));
	taskId = CreateTask(Task_BwMainInput, 0);
	data = gTasks[taskId].data;
	data[SAVED] = saved;
	data[SELECTED] = 0;
	data[AVATAR] = MAX_SPRITES;
	FillWindowPixelBuffer(0, PIXEL_FILL(0));
	if (saved)
	{
		Print(gText_BwrMain_Continue, 24, 8, 1);
		Print(gText_BwrMain_NewGame, 24, saved == 2 ? 104 : 112, 1);
		if (saved == 2)
			Print(gText_BwrMain_Gift, 24, 120, 1);
		Print(gText_BwrMain_Options, 24, 136, 1);
		PrintStats();
		DrawParty(data);
	}
	else
	{
		Print(gText_BwrMain_NewGame, 24, 16, 1);
		Print(gText_BwrMain_Options, 24, 40, 1);
	}
	UpdateSelection(data);
	PutWindowTilemap(0);
	CopyWindowToVram(0, 3);
	ShowBg(0);
	ShowBg(1);
	ShowBg(2);
	SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_BG0_ON | DISPCNT_BG1_ON | DISPCNT_BG2_ON | DISPCNT_OBJ_1D_MAP);
	BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
	SetVBlankCallback(VBlankCB_BwMain);
	SetMainCallback2(MainCB2_BwMain);
}

//hooks 0800C1AC（assembly/hack_bwreload/bw_main_menu_hook.s）から。素のメニューの窓を描く代わり
void BwMainMenuPresent(u8 taskId)
{
	BwMainSetup(gTasks[taskId].data[0], TRUE);
}

//せってい から戻る（gMain.savedCallback）
void BwMainMenuReopen(void)
{
	bool8 saved = gSaveFileStatus == SAVE_STATUS_OK || gSaveFileStatus == 0xFF;
	BwMainSetup(saved && IsMysteryGiftEnabled() ? 2 : saved, FALSE);
}
