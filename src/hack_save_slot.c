#include "defines.h"
#include "../include/bg.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/malloc.h"
#include "../include/menu.h"
#include "../include/palette.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/text_window.h"
#include "../include/constants/rgb.h"
#include "../include/constants/songs.h"
#include "hack_ext_save.h"

/*
 * ぼうけんのしょ 2 つ（ドラクエ式・2026-10-09 ユーザー「セーブデータを２個用意できる　１ゲーム対して２個イメージとしてはドラクエ」
 * →「B: 先に 6MB に広げてから 2 つ目のセーブを作る」→「やって」）。docs/research/2026-10-09_save-6mb-two-slots.md
 *
 * - 直した mGBA（tools/mgba/save1m.patch）の 6MB のフラッシュを半分に分ける: ぼうけんのしょ 1 はバンク 0〜47（今までと同じ場所）、
 *   2 はバンク 48〜95。素のフラッシュの読み書きは全部 SwitchFlashBank（081C27DC）でバンクを選ぶので（呼び元 6 か所・
 *   ReadFlash／VerifyFlashSector／書き込み／区画の消去）、そこを差し替えて 2 のときだけ 48 を足す（hooks）。
 *   本体の 128KB・ボックスの 40〜50 箱（追加の区画）・殿堂入りまで、全部がそのまま 2 の側に分かれる。
 * - どちらを使うかは IWRAM の 1 語（HACK_SAVE_SLOT_WORD＝素の link.c の使われない詰め物 gLinkFiller3・英語版 0x03003F94 の +0x40。
 *   まわりの gBlockRequestType 03003FD0・gLastSendQueueCount 03003FE0 は ROM の参照の数で合う・03003FD1〜FDF への参照は 0 件）。
 *   印の値 SLOT2_MAGIC のときだけ 2。EWRAM を消してセーブを読み直す素の ReloadSave（通信のエラー）でも残る。
 *   ソフトリセット（RegisterRamReset）で 0 に戻る＝ぼうけんのしょ 1。
 * - 選ぶ画面: タイトルで START → 素はセーブを読んでメインメニューへ（08078A28 の SetMainCallback2(0800BCE1)）。その行き先の語
 *   （08078A54）を CB2_HackSaveSlotSelect に（repoints）。使えないとき（下）は何も出さずにすぐメインメニューへ。
 * - ★ 使えるのは 6MB 版の mGBA だけ。前の 1MB 版はバンク 16 以上への切り替えを黙って無視し、今のバンク（ぼうけんのしょ 1 の場所）に
 *   書いてしまう。実機・素の mGBA は 128KB しか無い。→ 64MB の後ろ半分が見える（HackExtSave_Available）うえで、
 *   バンク 2 と 50 の同じ所を読み比べて確かめる（HackSaveSlot_Probe）。
 */

#define HACK_SAVE_SLOT_WORD (*(vu32*) 0x03003FD4)   //gLinkFiller3（上）
#define SLOT2_MAGIC 0x534C5432                      //'SLT2'＝ぼうけんのしょ 2。ほかの値は全部 1
#define SLOT2_BANK_OFFSET 48
#define PROBE_SECTOR_1 32                           //追加の区画 0（バンク 2 の頭）＝確かめ用に取ってある（src/hack_ext_save.c）
#define PROBE_SECTOR_2 (PROBE_SECTOR_1 + SLOT2_BANK_OFFSET * 16)
#define PROBE_OFFSET 0xFF0
#define PROBE_SIZE 16

#define FLASH_BASE ((vu8*) 0x0E000000)
#define ReadFlashVanilla ((void (*)(u16, u32, void*, u32)) (0x081C2A54 | 1))
#define SetSaveBlocksPointers ((void (*)(void)) (0x0804B810 | 1))
#define ResetMenuAndMonGlobals ((void (*)(void)) (0x080542EC | 1))
#define Save_ResetSaveCounters ((void (*)(void)) (0x080DA738 | 1))
#define Sav2_ClearSetDefault ((void (*)(void)) (0x080542DC | 1))
#define Save_LoadGameDataVanilla ((u8 (*)(u8)) (0x080DB4E4 | 1))
#define SetPokemonCryStereoVanilla ((void (*)(u32)) (0x081C2790 | 1))
#define MallocInitVanilla ((void (*)(void*, u32)) (0x08002B80 | 1))
#define CB2_InitMainMenu ((MainCallback) (0x0800BCE0 | 1))
#define gSaveFileStatusVanilla (*(vu16*) 0x030053F0)
#define HEAP_START ((void*) 0x02000000)
#define SLOT_HEAP_SIZE 0x1B000                      //08078A1E mov r1,#0xD8 / lsl r1,#9（patches/0288 HQ ミキサーで 0x1C000 → 0x1B000・後ろ 4KB は gSoundInfo）
#define SAVE_NORMAL 0
#define SAVE_STATUS_EMPTY 0
#define SAVE_STATUS_OK 1
#define SAVE_STATUS_INVALID 2

extern const u8 gText_HackSlot_Title[];
extern const u8 gText_HackSlot_Slot1[];
extern const u8 gText_HackSlot_Slot2[];
extern const u8 gText_HackSlot_Empty[];
extern const u8 gText_HackSlot_Broken[];
extern const u8 gText_HackSlot_NoTitle[];
extern const u8 gText_HackSlot_NoLine1[];
extern const u8 gText_HackSlot_NoLine2[];
extern const u8 gText_HackSlot_NoPressA[];

//---------------------------------------------------------------- フラッシュ

static u8 CurrentSlot(void)
{
	return HACK_SAVE_SLOT_WORD == SLOT2_MAGIC;
}

static void SetSlot(u8 slot)
{
	HACK_SAVE_SLOT_WORD = slot ? SLOT2_MAGIC : 0;
}

//素の SwitchFlashBank（081C27DC）の代わり（hooks）。ぼうけんのしょ 2 のときだけ 48 バンク後ろ
void HackSaveSlot_SwitchFlashBank(u8 bank)
{
	if (CurrentSlot())
		bank += SLOT2_BANK_OFFSET;
	FLASH_BASE[0x5555] = 0xAA;
	FLASH_BASE[0x2AAA] = 0x55;
	FLASH_BASE[0x5555] = 0xB0;
	FLASH_BASE[0x0000] = bank;
}

static bool8 SameBytes(const u8* a, const u8* b, u32 n)
{
	u32 i;

	for (i = 0; i < n; ++i)
		if (a[i] != b[i])
			return FALSE;
	return TRUE;
}

//6MB 版の mGBA か（バンク 50 へ切り替えられるか）。ぼうけんのしょ 1 のまま呼ぶ
static bool8 HackSaveSlot_Probe(void)
{
	u8 a[PROBE_SIZE], b[PROBE_SIZE];
	u8* sector;
	u32 i;
	bool8 ok;

	if (!HackExtSave_Available())
		return FALSE;
	ReadFlashVanilla(PROBE_SECTOR_1, PROBE_OFFSET, a, PROBE_SIZE);
	ReadFlashVanilla(PROBE_SECTOR_2, PROBE_OFFSET, b, PROBE_SIZE);   //前の版は切り替えを無視＝a と同じものが読める
	if (!SameBytes(a, b, PROBE_SIZE))
		return TRUE;

	//同じだった（どちらも消えたまま など）→ 確かめ用の区画（バンク 2）に印を書いて読み比べる（1 度だけ・あとは違うまま）
	sector = Malloc(0x1000);
	if (sector == NULL)
		return FALSE;
	for (i = 0; i < 0x1000; ++i)
		sector[i] = 0xFF;
	for (i = 0; i < PROBE_SIZE; ++i)
		sector[PROBE_OFFSET + i] = 0x50 + i + b[i];      //必ず b と違う値
	ok = HackExtSave_WriteSector(PROBE_SECTOR_1 - 32, sector);
	Free(sector);
	if (!ok)
		return FALSE;
	ReadFlashVanilla(PROBE_SECTOR_1, PROBE_OFFSET, a, PROBE_SIZE);
	ReadFlashVanilla(PROBE_SECTOR_2, PROBE_OFFSET, b, PROBE_SIZE);
	return !SameBytes(a, b, PROBE_SIZE);
}

//---------------------------------------------------------------- 画面

struct SlotInfo
{
	u8 status;
	u8 name[8];
	u16 hours;
	u8 minutes;
};

struct SlotScreen
{
	struct SlotInfo info[2];
	u8 cursor;
	u8 exiting;
	u8 unavailable;   //6MB 版の mGBA でない＝「さいしんの mGBA を」の案内だけ出す（2026-10-10）
};

enum
{
	WIN_MAIN,
	WIN_COUNT,
};

static const struct BgTemplate sBgTemplates[] =
{
	{.bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0},
};

static const struct WindowTemplate sWindowTemplates[] =
{
	{.bg = 0, .tilemapLeft = 2, .tilemapTop = 2, .width = 26, .height = 16, .paletteNum = 15, .baseBlock = 1},
	DUMMY_WIN_TEMPLATE,
};

#define FRAME_TILE 0x1C0
#define FRAME_PAL 14

extern bool16 HoennFrontier_InitWindowsZeroed(const struct WindowTemplate *templates);   //src/hoenn_frontier.c
void __attribute__((long_call)) TransferPlttBuffer(void);

//画面の中身はヒープに（タスクの data[0..1] にその番地）
static struct SlotScreen* ScreenOf(u8 taskId)
{
	return (struct SlotScreen*) (((u32) (u16) gTasks[taskId].data[0]) | ((u32) (u16) gTasks[taskId].data[1] << 16));
}

static void ReadSlotInfo(struct SlotInfo* info, u8 slot)
{
	u32 i;

	SetSlot(slot);
	Save_ResetSaveCounters();
	Save_LoadGameDataVanilla(SAVE_NORMAL);
	info->status = gSaveFileStatusVanilla;
	for (i = 0; i < 8; ++i)
		info->name[i] = gSaveBlock2->playerName[i];
	info->name[7] = EOS;
	info->hours = gSaveBlock2->playTimeHours;
	info->minutes = gSaveBlock2->playTimeMinutes;
}

static void PrintScreen(struct SlotScreen* s)
{
	static const u8 sCursor[] = {0xEF, EOS};   //▶（素の文字コード）
	static const u8 sColon[] = {0xF0, EOS};    //:
	u8 buf[40];
	u8* p;
	u32 k;

	FillWindowPixelBuffer(WIN_MAIN, PIXEL_FILL(1));
	if (s->unavailable)
	{
		static const u8 sRed[3] = {TEXT_COLOR_WHITE, TEXT_COLOR_RED, TEXT_COLOR_LIGHT_RED};   //★ 2026-10-10 ユーザー「赤文字で」
		AddTextPrinterParameterized3(WIN_MAIN, 2, 8, 1, sRed, 0xFF, gText_HackSlot_NoTitle);
		AddTextPrinterParameterized(WIN_MAIN, 2, gText_HackSlot_NoLine1, 8, 36, 0xFF, NULL);
		AddTextPrinterParameterized(WIN_MAIN, 2, gText_HackSlot_NoLine2, 8, 52, 0xFF, NULL);
		AddTextPrinterParameterized(WIN_MAIN, 2, gText_HackSlot_NoPressA, 8, 100, 0xFF, NULL);
		CopyWindowToVram(WIN_MAIN, 3);
		return;
	}
	AddTextPrinterParameterized(WIN_MAIN, 2, gText_HackSlot_Title, 8, 1, 0xFF, NULL);
	for (k = 0; k < 2; ++k)
	{
		u8 y = 28 + k * 36;
		const struct SlotInfo* info = &s->info[k];

		if (s->cursor == k)
			AddTextPrinterParameterized(WIN_MAIN, 2, sCursor, 4, y, 0xFF, NULL);
		AddTextPrinterParameterized(WIN_MAIN, 2, k == 0 ? gText_HackSlot_Slot1 : gText_HackSlot_Slot2, 16, y, 0xFF, NULL);
		if (info->status == SAVE_STATUS_OK)
		{
			p = StringCopy(buf, info->name);
			*p++ = 0;      //空白
			*p++ = 0;
			p = ConvertIntToDecimalStringN(p, info->hours, STR_CONV_MODE_LEFT_ALIGN, 3);
			p = StringCopy(p, sColon);
			ConvertIntToDecimalStringN(p, info->minutes, STR_CONV_MODE_LEADING_ZEROS, 2);
			AddTextPrinterParameterized(WIN_MAIN, 2, buf, 32, y + 16, 0xFF, NULL);
		}
		else
		{
			AddTextPrinterParameterized(WIN_MAIN, 2, info->status == SAVE_STATUS_EMPTY ? gText_HackSlot_Empty : gText_HackSlot_Broken,
			                            32, y + 16, 0xFF, NULL);
		}
	}
	CopyWindowToVram(WIN_MAIN, 3);
}

static void CB2_HackSaveSlotMain(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void VBlankCB_HackSaveSlot(void)
{
	LoadOam();
	ProcessSpriteCopyRequests();
	TransferPlttBuffer();
}

//選んだぼうけんのしょを読み直してメインメニューへ（素のタイトルの case 2 と同じ並び）
static void LoadChosenAndGoToMainMenu(u8 slot)
{
	SetSlot(slot);
	SetSaveBlocksPointers();
	ResetMenuAndMonGlobals();
	Save_ResetSaveCounters();
	Save_LoadGameDataVanilla(SAVE_NORMAL);
	if (gSaveFileStatusVanilla == SAVE_STATUS_EMPTY || gSaveFileStatusVanilla == SAVE_STATUS_INVALID)
		Sav2_ClearSetDefault();
	SetPokemonCryStereoVanilla(gSaveBlock2->optionsSound);
	MallocInitVanilla(HEAP_START, SLOT_HEAP_SIZE);
	SetMainCallback2(CB2_InitMainMenu);
}

static void Task_HackSaveSlot(u8 taskId)
{
	struct SlotScreen* s = ScreenOf(taskId);

	if (gPaletteFade->active)
		return;
	if (s->exiting)
	{
		u8 slot = s->cursor;

		FreeAllWindowBuffers();
		Free(s);
		DestroyTask(taskId);
		LoadChosenAndGoToMainMenu(slot);
		return;
	}
	if (s->unavailable)
	{
		if (JOY_NEW(A_BUTTON | B_BUTTON | START_BUTTON))   //ぼうけんのしょ 1 のまま進む
		{
			PlaySE(SE_SELECT);
			s->cursor = 0;
			s->exiting = TRUE;
			BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
		}
		return;
	}
	if (JOY_NEW(DPAD_UP | DPAD_DOWN))
	{
		PlaySE(SE_SELECT);
		s->cursor ^= 1;
		PrintScreen(s);
	}
	else if (JOY_NEW(A_BUTTON))
	{
		PlaySE(SE_SELECT);
		s->exiting = TRUE;
		BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
	}
}

static bool8 CreateSlotScreen(u8 wasSlot2, bool8 unavailable)
{
	struct SlotScreen* s = AllocZeroed(sizeof(struct SlotScreen));
	u8 taskId;

	if (s == NULL)
		return FALSE;

	taskId = CreateTask(Task_HackSaveSlot, 0);
	gTasks[taskId].data[0] = (u32) s;
	gTasks[taskId].data[1] = (u32) s >> 16;
	s->unavailable = unavailable;
	if (!unavailable)
	{
		ReadSlotInfo(&s->info[1], 1);
		ReadSlotInfo(&s->info[0], 0);
		s->cursor = wasSlot2;
	}
	return TRUE;
}

static void SkipSlotScreen(u8 slot)
{
	FreeAllWindowBuffers();
	ResetTasks();
	LoadChosenAndGoToMainMenu(slot);
}

void CB2_HackSaveSlotSelect(void)
{
	u8 wasSlot2;
	bool8 unavailable;

	switch (gMain.state)
	{
	case 0:
		//確かめはぼうけんのしょ 1 のまま（2 のままだと読み比べる所も 48 バンク後ろにずれる）
		wasSlot2 = CurrentSlot();
		SetSlot(0);
		//★ 2026-10-10 ユーザー「ぼうけんのしょが出せないときは最新のmGBAを使用してくださいを表示してほしい」:
		//  使えない版（6MB 版でない mGBA・64MB の後ろ半分が見えない）は、黙って飛ばさず案内を出して A でぼうけんのしょ 1 へ
		unavailable = !HackSaveSlot_Probe();
		SetVBlankCallback(NULL);
		SetHBlankCallback(NULL);
		CpuFill32(0, (void*) VRAM, VRAM_SIZE);
		ResetBgsAndClearDma3BusyFlags(0);
		InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
		if (!HoennFrontier_InitWindowsZeroed(sWindowTemplates))
		{
			SkipSlotScreen(unavailable ? 0 : wasSlot2);
			return;
		}
		ChangeBgX(0, 0, 0);
		ChangeBgY(0, 0, 0);
		SetGpuReg(REG_OFFSET_BLDCNT, 0);      //タイトルが白くするのに使った明るさの合成を戻す
		SetGpuReg(REG_OFFSET_BLDALPHA, 0);
		SetGpuReg(REG_OFFSET_BLDY, 0);
		SetGpuReg(REG_OFFSET_MOSAIC, 0);
		SetGpuReg(REG_OFFSET_WIN0H, 0);
		SetGpuReg(REG_OFFSET_WIN0V, 0);
		SetGpuReg(REG_OFFSET_WIN1H, 0);
		SetGpuReg(REG_OFFSET_WIN1V, 0);
		SetGpuReg(REG_OFFSET_WININ, 0);
		SetGpuReg(REG_OFFSET_WINOUT, 0);
		DeactivateAllTextPrinters();
		ResetPaletteFade();
		ResetSpriteData();
		ResetTasks();
		FreeAllSpritePalettes();
		if (!CreateSlotScreen(wasSlot2, unavailable))
		{
			SkipSlotScreen(unavailable ? 0 : wasSlot2);
			return;
		}
		gMain.state++;
		break;
	case 1:
		LoadPalette(stdpal_get(0), 15 * 16, 32);
		TextWindow_SetStdFrame0_WithPal(WIN_MAIN, FRAME_TILE, FRAME_PAL * 16);
		gPlttBufferUnfaded[0] = RGB_BLACK;
		gPlttBufferFaded[0] = RGB_BLACK;
		PutWindowTilemap(WIN_MAIN);
		DrawStdFrameWithCustomTileAndPalette(WIN_MAIN, FALSE, FRAME_TILE, FRAME_PAL);
		PrintScreen(ScreenOf(FindTaskIdByFunc(Task_HackSaveSlot)));
		CopyBgTilemapBufferToVram(0);
		gMain.state++;
		break;
	default:
		ShowBg(0);
		SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_BG0_ON | DISPCNT_OBJ_1D_MAP);
		SetVBlankCallback(VBlankCB_HackSaveSlot);
		BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
		SetMainCallback2(CB2_HackSaveSlotMain);
		break;
	}
}
