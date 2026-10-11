#include "defines.h"
#include "../include/bg.h"
#include "../include/event_data.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/menu.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/random.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/constants/songs.h"
#include "../include/new/dns.h"     //TransferPlttBuffer

void __attribute__((long_call)) ScanlineEffect_Stop(void);      /* BPRJ.ld 08087A48 */

/*
 * BW 風の電話帳とフィールドの着信（pm-start/poke-call の src/phone.c を CFRU の名前で書き直した・2026-10-11
 * ユーザー「poke-call これ取り込んで」「番号を交換する相手：まずはママだけ」）。
 *
 *  - スタートメニューの「でんわ」（src/bw_start_menu.c）→ CB2_HackPhoneBook。登録した相手を上下で選んで A でかける
 *    （フィールドに戻ってから、かける台本＝assembly/hack_phone/phone_scripts.s）。B でスタートメニューへ戻る
 *  - フィールドの毎コマ（src/overworld_speedup.c）→ HackPhone_FieldTick。登録した相手からかかってくる。
 *    ★ 2026-10-11 ユーザー「でんわの頻度がたかいので調整 30分とか」: プレイ時間（gSaveBlock2）が 30 分の倍数になった瞬間（秒が 0）に 1 回。
 *    タスクの数で待つ（poke-call の 30〜98 秒）と地図を移るたびに数え直して短すぎた。セーブの変数は増やさない
 *    （台本が動いている・暗転中・バトル施設の挑戦中は数えない。地図やメニューが替わると待ち時間を引き直す＝タスクが消えるため）
 *  - 番号の交換はママの台本の頭（assembly/kanto/kanto_home_scripts.s）。印は予約フラグ 0x12CA
 *  - お母さんの貯金の電話（src/hack_mom_savings.c）は今までどおり別
 *  - 状態はタスクの data[] だけに持つ（CFRU は書き換えられる static を置けない・CLAUDE.md 6.13）
 */

#define HACK_FLAG_PHONE_MOM 0x12CA         //ママの番号を登録した（build_hoenn_scripts.py の HOENN_FLAG_END の下）
#define FLAG_BATTLE_FACILITY 0x930

extern const u8 gText_HackPhone_Title[], gText_HackPhone_Book[], gText_HackPhone_Registered[], gText_HackPhone_Empty[];
extern const u8 gText_HackPhone_Help[], gText_HackPhone_NameMom[];
extern const u8 HackScr_PhoneMomOutgoing[], HackScr_PhoneMomIncoming[];

struct HackPhoneContact
{
	u16 flag;
	const u8 *name;
	const u8 *outgoing;
	const u8 *incoming;
};

static const struct HackPhoneContact sContacts[] =
{
	{HACK_FLAG_PHONE_MOM, gText_HackPhone_NameMom, HackScr_PhoneMomOutgoing, HackScr_PhoneMomIncoming},
};
#define PHONE_COUNT NELEMS(sContacts)

/* ---- 電話帳の画面 ---- */
enum { STATE, SELECTED, COUNT, CONTACTS };     //data[]（CONTACTS から登録した相手の番号）
enum { BOOK, EXIT, CALL };

static const struct BgTemplate sBgTemplates[] =
{
	{.bg = 0, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0},
};

static const struct WindowTemplate sWindowTemplates[] =
{
	{.bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 30, .height = 20, .paletteNum = 15, .baseBlock = 1},
	DUMMY_WIN_TEMPLATE,
};

//poke-call と同じ 11 色（0 透明・1 白・2 濃い灰・3 水色・4 黄・5 紺・6 濃い灰・7 青・8 明るい灰・9 黒・10 緑）
static const u16 sPalette[16] = {0, 0x7FFF, 0x1084, 0x7FE0, 0x03FF, 0x0842, 0x18C6, 0x3180, 0x6739, 0x0421, 0x56A0};

static void Print(const u8 *text, u8 x, u8 y, u8 color, u8 bg)
{
	u8 colors[3];

	colors[0] = bg;
	colors[1] = color;
	colors[2] = (bg == 1) ? 8 : 2;
	AddTextPrinterParameterized3(0, 2, x, y, colors, 0, text);
}

static void Rect(u8 color, u16 x, u16 y, u16 w, u16 h)
{
	FillWindowPixelRect(0, color, x, y, w, h);
}

static void Frame(u16 x, u16 y, u16 w, u16 h, u8 fill)
{
	Rect(8, x, y, w, h);
	Rect(9, x + 1, y + 1, w - 2, h - 2);
	Rect(fill, x + 2, y + 2, w - 4, h - 4);
}

static void Receiver(u16 x, u16 y, u8 color)    //受話器の絵（poke-call の receiver と同じ形）
{
	Rect(color, x, y, 10, 6);
	Rect(color, x + 3, y + 5, 7, 8);
	Rect(color, x + 7, y + 12, 7, 6);
	Rect(color, x + 12, y + 17, 8, 6);
	Rect(color, x + 19, y + 19, 6, 9);
	Rect(color, x + 19, y + 25, 11, 6);
}

static void Draw(s16 *d)
{
	u32 j, top;
	u8 number[3];

	FillWindowPixelBuffer(0, 0x55);
	Rect(6, 0, 0, 240, 23);
	Rect(3, 0, 22, 240, 1);
	Print(gText_HackPhone_Title, 10, 5, 1, 6);
	Print(gText_HackPhone_Book, 150, 5, 8, 6);
	Rect(6, 0, 138, 240, 22);
	Rect(8, 0, 138, 240, 1);
	Frame(7, 28, 144, 104, 5);
	Frame(157, 28, 76, 104, 5);
	Receiver(180, 44, 3);
	Print(gText_HackPhone_Registered, 172, 85, 8, 5);
	number[0] = 0xA1 + d[COUNT];   //全角の数字（0xA1 = ０）
	number[1] = EOS;
	Print(number, 190, 103, 1, 5);
	if (d[COUNT] == 0)
	{
		Frame(7, 56, 226, 46, 1);
		Print(gText_HackPhone_Empty, 12, 68, 9, 1);
	}
	top = (d[SELECTED] >= 6) ? 6 : 0;
	for (j = top; j < (u32) d[COUNT] && j < top + 6; ++j)
	{
		u16 y = 32 + (j - top) * 16;
		bool8 selected = (j == (u32) d[SELECTED]);

		if (selected)
		{
			Rect(7, 10, y, 138, 16);
			Rect(3, 10, y, 3, 16);
		}
		else
			Rect(6, 14, y + 15, 130, 1);
		Print(sContacts[d[CONTACTS + j]].name, 22, y, 1, selected ? 7 : 5);
	}
	Print(gText_HackPhone_Help, 8, 143, 1, 6);
	CopyWindowToVram(0, COPYWIN_BOTH);
}

static void MainCB2_HackPhoneBook(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void VBlankCB_HackPhoneBook(void)
{
	LoadOam();
	ProcessSpriteCopyRequests();
	TransferPlttBuffer();
}

static void Task_HackPhoneBook(u8 taskId)
{
	s16 *d = gTasks[taskId].data;
	u16 keys;

	if (gPaletteFade->active)
		return;
	if (d[STATE] == EXIT || d[STATE] == CALL)
	{
		const u8 *script = (d[STATE] == CALL) ? sContacts[d[CONTACTS + d[SELECTED]]].outgoing : NULL;

		SetVBlankCallback(NULL);
		DestroyTask(taskId);
		FreeAllWindowBuffers();
		ResetSpriteData();
		FreeAllSpritePalettes();
		if (script != NULL)
		{
			ScriptContext1_SetupScript(script);
			SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
		}
		else
			SetMainCallback2(CB2_ReturnToFieldWithOpenMenu);
		return;
	}
	keys = gMain.newKeys;
	if (keys & B_BUTTON)
		d[STATE] = EXIT;
	else if ((keys & A_BUTTON) && d[COUNT] != 0)
		d[STATE] = CALL;
	else if (d[COUNT] != 0)
	{
		if (keys & DPAD_UP)
			d[SELECTED] = (d[SELECTED] != 0) ? d[SELECTED] - 1 : d[COUNT] - 1;
		if (keys & DPAD_DOWN)
			d[SELECTED] = (d[SELECTED] + 1 < d[COUNT]) ? d[SELECTED] + 1 : 0;
	}
	if (d[STATE] != BOOK)
	{
		PlaySE(SE_SELECT);
		BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
	}
	else if (keys & (DPAD_UP | DPAD_DOWN))
	{
		PlaySE(SE_SELECT);
		Draw(d);
	}
}

void CB2_HackPhoneBook(void)
{
	u32 i;
	u8 taskId;
	s16 *d;

	SetVBlankCallback(NULL);
	SetHBlankCallback(NULL);
	SetGpuReg(REG_OFFSET_DISPCNT, 0);
	DmaFill16(3, 0, (void *) VRAM, VRAM_SIZE);
	ResetBgsAndClearDma3BusyFlags(0);
	InitBgsFromTemplates(0, sBgTemplates, NELEMS(sBgTemplates));
	for (i = 0; i < 4; ++i)
	{
		ChangeBgX(i, 0, 0);
		ChangeBgY(i, 0, 0);
	}
	InitWindows(sWindowTemplates);
	DeactivateAllTextPrinters();
	for (i = REG_OFFSET_BLDCNT; i <= REG_OFFSET_BLDY; i += 2)
		SetGpuReg(i, 0);
	ResetTasks();
	ResetSpriteData();
	FreeAllSpritePalettes();
	ResetPaletteFade();
	ScanlineEffect_Stop();
	LoadPalette(sPalette, 0xF0, sizeof(sPalette));
	taskId = CreateTask(Task_HackPhoneBook, 0);
	d = gTasks[taskId].data;
	for (i = 0; i < PHONE_COUNT; ++i)
		if (FlagGet(sContacts[i].flag))
			d[CONTACTS + d[COUNT]++] = i;
	PutWindowTilemap(0);
	Draw(d);
	ShowBg(0);
	SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_1D_MAP | DISPCNT_BG0_ON | DISPCNT_OBJ_ON);
	BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
	SetVBlankCallback(VBlankCB_HackPhoneBook);
	SetMainCallback2(MainCB2_HackPhoneBook);
}

/* ---- フィールドの着信 ---- */
#define CALL_EVERY_MINUTES 30     //プレイ時間がこの倍数（分）になった瞬間に 1 回（2026-10-11）
#define CALL_LOCKOUT_FRAMES 600   //鳴らしたあと 10 秒は見ない（同じ 1 秒の中で 2 度鳴らさない）

static bool8 IsCallMoment(void)
{
	u32 total = gSaveBlock2->playTimeHours * 60 + gSaveBlock2->playTimeMinutes;

	return total != 0 && total % CALL_EVERY_MINUTES == 0 && gSaveBlock2->playTimeSeconds == 0;
}

static u32 RegisteredCount(void)
{
	u32 i, n = 0;
	for (i = 0; i < PHONE_COUNT; ++i)
		n += FlagGet(sContacts[i].flag) ? 1 : 0;
	return n;
}

static void Task_HackPhoneIncoming(u8 taskId)
{
	s16 *d = gTasks[taskId].data;
	u32 i, count, chosen;

	if (gPaletteFade->active || ScriptContext2_IsEnabled() || gMain.callback2 != CB2_Overworld)
		return;
	if (FlagGet(FLAG_BATTLE_FACILITY))
		return;
	count = RegisteredCount();
	if (count == 0)
		return;
	if (d[0] > 0)
	{
		--d[0];
		return;
	}
	if (!IsCallMoment())
		return;
	d[0] = CALL_LOCKOUT_FRAMES;
	chosen = Random() % count;
	for (i = 0; i < PHONE_COUNT; ++i)
	{
		if (!FlagGet(sContacts[i].flag))
			continue;
		if (chosen != 0)
		{
			--chosen;
			continue;
		}
		ScriptContext2_Enable();
		ScriptContext1_SetupScript(sContacts[i].incoming);
		PlaySE(SE_SELECT);
		return;
	}
}

//src/overworld_speedup.c が毎コマ呼ぶ（フィールドの間だけ）
void HackPhone_FieldTick(void)
{
	if (FindTaskIdByFunc(Task_HackPhoneIncoming) == 0xFF)
	{
		u8 taskId = CreateTask(Task_HackPhoneIncoming, 80);
		if (taskId != 0xFF)
			gTasks[taskId].data[0] = 60;   //地図に入って 1 秒は鳴らさない
	}
}
