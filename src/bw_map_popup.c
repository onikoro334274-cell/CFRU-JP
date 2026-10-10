#include "defines.h"
#include "../include/bg.h"
#include "../include/event_data.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/menu.h"
#include "../include/palette.h"
#include "../include/region_map.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/new/ram_locs.h"

/*
 * ★ 2026-10-10 ユーザー「BW系これになおして」→「全部そちらに置き換える」・動画（マサラタウンの地名が黒い帯で出る）:
 * 地名の帯を BW の黒い上下の帯に。bw-reloaded（https://github.com/pm-start/bw-reloaded 0e7888f）の src/map_popup.c を
 * CFRU の名前で書き直したもの（元の絵は Bw-map-pops・クレジットは tools/bwreload/CREDITS_bw-reloaded.md）。
 *
 * - 素の ShowMapNamePopup（08097B00）・DismissMapNamePopup（08097CDC）・IsMapNamePopupTaskActive（08097D10）を functionrewrites で置き換える
 * - 上の帯（窓 0・y 0〜2）に地名と階、下の帯（y 17〜19）は絵だけ。30 コマ待ってから 2 画素ずつ滑り込み、120 コマ出して滑り出る
 * - 帯を滑らせるのは BG0 の縦のずらし（HBlank で 80 行目までと 160 行目からは +、間は −）。前の HBlank の処理は残して先に呼ぶ
 * - 地名は GetMapName（ホウエン・シンオウ・キタカミの読み替えは中の HoennTownMapNameHook がする）
 * - 状態はタスクの data[]（6.13）。窓は DMA の写しが終わってから外す
 */

extern const u8 gBwrPopupPrimary[];
extern const u8 gBwrPopupSecondary[];
extern const u8 gBwrPopupPal[];
extern const u8 gText_BwrPopupRooftop[];

#define QUEST_LOG_STATE (*(vu8 *) 0x0203AD72)   //素の gQuestLogState（2・3 は ふりかえり の再生中）
#define FLAG_HIDE_MAP_NAME_POPUP 0x4000

enum { STATE, TIMER, OFFSET, RESHOW, PRIMARY, SECONDARY, PAL_FADE, WINDOWS, OLD_HBLANK_LO, OLD_HBLANK_HI, OLD_IE, OLD_DISPSTAT, HOOKED };
enum { DELAY, SLIDE_IN, SHOWN, SLIDE_OUT, CLEAR, FREE };

static void Task_BwMapPopup(u8 taskId);
static void HBlankCB_BwMapPopup(void);

static const struct WindowTemplate sPrimaryWindow = {.bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 30, .height = 3, .paletteNum = 14, .baseBlock = 1};
static const struct WindowTemplate sSecondaryWindow = {.bg = 0, .tilemapLeft = 0, .tilemapTop = 17, .width = 30, .height = 3, .paletteNum = 14, .baseBlock = 91};

static IntrCallback OldHBlank(const s16 *data)
{
	return (IntrCallback) ((u32) (u16) data[OLD_HBLANK_LO] | ((u32) (u16) data[OLD_HBLANK_HI] << 16));
}

static void InstallHBlank(s16 *data)
{
	u32 previous = (u32) gMain.hblankCallback;
	data[OLD_HBLANK_LO] = (s16) previous;
	data[OLD_HBLANK_HI] = (s16) (previous >> 16);
	data[OLD_IE] = REG_IE & INTR_FLAG_HBLANK;
	data[OLD_DISPSTAT] = REG_DISPSTAT & DISPSTAT_HBLANK_INTR;
	data[HOOKED] = TRUE;
	SetHBlankCallback(HBlankCB_BwMapPopup);
	REG_DISPSTAT |= DISPSTAT_HBLANK_INTR;
	REG_IE |= INTR_FLAG_HBLANK;
}

static void RestoreHBlank(s16 *data)
{
	if (data[HOOKED] && gMain.hblankCallback == HBlankCB_BwMapPopup)
	{
		SetHBlankCallback(OldHBlank(data));
		if (!data[OLD_IE])
			REG_IE &= ~INTR_FLAG_HBLANK;
		if (!data[OLD_DISPSTAT])
			REG_DISPSTAT &= ~DISPSTAT_HBLANK_INTR;
		SetGpuReg(REG_OFFSET_BG0VOFS, 0);
		ChangeBgY(0, 0, 0);
	}
	data[HOOKED] = FALSE;
}

static void HBlankCB_BwMapPopup(void)
{
	u8 taskId = FindTaskIdByFunc(Task_BwMapPopup);
	s16 *data;
	IntrCallback previous;
	u16 line;

	if (taskId >= NUM_TASKS)
	{
		//★ 2026-10-10 帯が出ている間にタスクごと消された（BW のスタートメニューなど全画面の画面は開くとき ResetTasks する＝
		//  RestoreHBlank が呼ばれない）。BG0 の縦のずれが最後に書いた値のまま残り、メニューの文字がずれた（ユーザーの動画・
		//  地図に入った直後にメニュー）。画面側の値（SetGpuReg のバッファ）に戻し、この HBlank の処理を外す
		REG_BG0VOFS = GetGpuReg(REG_OFFSET_BG0VOFS);
		if (gMain.hblankCallback == HBlankCB_BwMapPopup)
			SetHBlankCallback(NULL);
		return;
	}
	data = gTasks[taskId].data;
	previous = OldHBlank(data);
	if (previous != NULL && previous != HBlankCB_BwMapPopup)
		previous();
	line = REG_VCOUNT & 0xFF;
	REG_BG0VOFS = (line < 80 || line >= 160) ? data[OFFSET] : (512 - data[OFFSET]) & 511;
}

static void PrintName(u8 windowId)
{
	u8 text[104];
	u32 i;
	s8 floor;

	text[0] = EXT_CTRL_CODE_BEGIN; text[1] = 1; text[2] = 2;   //字の色 2（白）
	text[3] = EXT_CTRL_CODE_BEGIN; text[4] = 2; text[5] = 0;   //地の色 0（透明）
	text[6] = EXT_CTRL_CODE_BEGIN; text[7] = 3; text[8] = 1;   //影の色 1
	GetMapName(text + 9, gMapHeader.regionMapSectionId, 0);
	for (i = 9; i < 80 && text[i] != EOS; ++i)
		;
	floor = (s8) gMapHeader.floorNum;
	if (floor != 0)
	{
		text[i++] = 0;   //空白
		if (floor == 127)
		{
			const u8 *s = gText_BwrPopupRooftop;
			while (*s != EOS && i < 101)
				text[i++] = *s++;
		}
		else
		{
			u32 tens = 0;
			if (floor < 0)
			{
				text[i++] = 0xBC;   //B（地下）
				floor = -floor;
			}
			if (floor > 99)
				floor = 99;
			while (floor >= 10)
			{
				floor -= 10;
				++tens;
			}
			if (tens)
				text[i++] = 0xA1 + tens;
			text[i++] = 0xA1 + floor;
			text[i++] = 0xC0;   //F
		}
	}
	text[i] = EOS;
	AddTextPrinterParameterized(windowId, 2, text, 8, 2, TEXT_SPEED_FF, NULL);
}

static bool8 CreateWindows(s16 *data)
{
	u8 a = AddWindow(&sPrimaryWindow), b;
	u32 i;

	if (a == 0xFF)
		return FALSE;
	b = AddWindow(&sSecondaryWindow);
	if (b == 0xFF)
	{
		RemoveWindow(a);
		return FALSE;
	}
	data[PRIMARY] = a;
	data[SECONDARY] = b;
	data[WINDOWS] = TRUE;
	//★ 2026-10-10 ユーザーの動画（そらをとぶで グレンじま に着くと下の帯が白・上の帯が透けた）: 帯を出すのは 30 コマ待ってから
	//  なので、素と同じ「呼ばれたときの引数」で決めると暗転が終わったあとに暗転前の入れ物（Unfaded）だけへ書いて画面に出なかった。
	//  いま暗転しているかで決める（暗転中は終わりに写されるので Unfaded だけ・暗転していなければ両方）
	if (data[PAL_FADE] || !gPaletteFade->active)
		LoadPalette(gBwrPopupPal, 14 * 16, 32);
	else
		for (i = 0; i < 16; ++i)
			gPlttBufferUnfaded[14 * 16 + i] = ((const u16 *) gBwrPopupPal)[i];
	CopyToWindowPixelBuffer(a, gBwrPopupPrimary, 30 * 3 * 32, 0);
	CopyToWindowPixelBuffer(b, gBwrPopupSecondary, 30 * 3 * 32, 0);
	PrintName(a);
	PutWindowTilemap(a);
	PutWindowTilemap(b);
	CopyWindowToVram(a, 3);
	CopyWindowToVram(b, 3);
	InstallHBlank(data);
	return TRUE;
}

static void Task_BwMapPopup(u8 taskId)
{
	s16 *data = gTasks[taskId].data;

	switch (data[STATE])
	{
	case DELAY:
		if (++data[TIMER] >= 30)
		{
			if (!CreateWindows(data))
			{
				DestroyTask(taskId);
				return;
			}
			data[TIMER] = 0;
			data[STATE] = SLIDE_IN;
		}
		break;
	case SLIDE_IN:
		if (IsDma3ManagerBusyWithBgCopy())
			break;
		data[OFFSET] -= 2;
		if (data[OFFSET] <= 0)
		{
			data[OFFSET] = 0;
			data[TIMER] = 0;
			data[STATE] = SHOWN;
		}
		break;
	case SHOWN:
		if (++data[TIMER] >= 120)
			data[STATE] = SLIDE_OUT;
		break;
	case SLIDE_OUT:
		data[OFFSET] += 2;
		if (data[OFFSET] >= 24)
		{
			data[OFFSET] = 24;
			data[STATE] = CLEAR;
		}
		break;
	case CLEAR:
		if (data[WINDOWS])
		{
			ClearWindowTilemap(data[PRIMARY]);
			ClearWindowTilemap(data[SECONDARY]);
			CopyWindowToVram(data[PRIMARY], 1);
			CopyWindowToVram(data[SECONDARY], 1);
		}
		data[STATE] = FREE;
		break;
	case FREE:
		if (IsDma3ManagerBusyWithBgCopy())
			break;
		if (data[WINDOWS])
		{
			RemoveWindow(data[PRIMARY]);
			RemoveWindow(data[SECONDARY]);
			data[WINDOWS] = FALSE;
		}
		RestoreHBlank(data);
		if (data[RESHOW])
		{
			data[RESHOW] = FALSE;
			data[TIMER] = 0;
			data[OFFSET] = 24;
			data[STATE] = DELAY;
		}
		else
			DestroyTask(taskId);
		break;
	}
}

//functionrewrites: ShowMapNamePopup 08097B00
void BwShowMapNamePopup(bool32 palIntoFadedBuffer)
{
	u8 taskId;

	if (FlagGet(FLAG_HIDE_MAP_NAME_POPUP) || QUEST_LOG_STATE == 2 || QUEST_LOG_STATE == 3)
		return;
	taskId = FindTaskIdByFunc(Task_BwMapPopup);
	if (taskId < NUM_TASKS)
	{
		s16 *data = gTasks[taskId].data;
		data[RESHOW] = TRUE;
		data[PAL_FADE] = palIntoFadedBuffer != 0;
		if (data[STATE] < SLIDE_OUT)
			data[STATE] = SLIDE_OUT;
	}
	else
	{
		taskId = CreateTask(Task_BwMapPopup, 90);
		if (taskId >= NUM_TASKS)
			return;
		gTasks[taskId].data[OFFSET] = 24;
		gTasks[taskId].data[PAL_FADE] = palIntoFadedBuffer != 0;
	}
}

//functionrewrites: DismissMapNamePopup 08097CDC
void BwDismissMapNamePopup(void)
{
	u8 taskId = FindTaskIdByFunc(Task_BwMapPopup);
	if (taskId < NUM_TASKS)
	{
		gTasks[taskId].data[RESHOW] = FALSE;
		if (gTasks[taskId].data[STATE] < CLEAR)
			gTasks[taskId].data[STATE] = CLEAR;
	}
}

//functionrewrites: IsMapNamePopupTaskActive 08097D10
bool8 BwIsMapNamePopupTaskActive(void)
{
	return FindTaskIdByFunc(Task_BwMapPopup) < NUM_TASKS;
}
