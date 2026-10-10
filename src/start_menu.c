#include "defines.h"
#include "../include/link.h"
#include "../include/menu.h"
#include "../include/menu_helpers.h"
#include "../include/new_menu_helpers.h"
#include "../include/rtc.h"
#include "../include/safari_zone.h"
#include "../include/script.h"
#include "../include/start_menu.h"
#include "../include/string_util.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/constants/flags.h"
#include "../include/constants/songs.h"

#include "../include/new/dexnav.h"

/*
start_menu.c
	Functions to redo how the start menu is generated, and
	associated functions such as safari steps/ball count.
*/

enum
{
	STARTMENU_POKEDEX,
	STARTMENU_POKEMON,
	STARTMENU_BAG,
	STARTMENU_PLAYER,
	STARTMENU_SAVE,
	STARTMENU_OPTION,
	STARTMENU_EXIT,
	STARTMENU_RETIRE_SAFARI,
	STARTMENU_PLAYER_LINK,
	STARTMENU_DEXNAV,
	STARTMENU_EXIT_RIGHT,
	STARTMENU_EXIT_LEFT,
	MAX_STARTMENU_ITEMS
};

enum STARTMENU_TOOLS
{
	START_MENU_NORMAL,
	START_MENU_TOOLS,
};

extern const u8 gText_MenuPokedex[];
extern const u8 gText_MenuPokemon[];
extern const u8 gText_MenuPlayer[];
extern const u8 gText_MenuSave[];
extern const u8 gText_MenuOption[];
extern const u8 gText_MenuExit[];
extern const u8 gText_MenuExitRight[];
extern const u8 gText_MenuExitLeft[];
extern const u8 gText_MenuRetire[];
extern const u8 gText_MenuDexNav[];
extern const u8 gText_MenuBag[];
extern const u8 gText_MenuCube[];
#ifdef UNBOUND
#define gText_MenuBag gText_MenuCube
#endif

extern const u8 gText_PokedexDescription[];
extern const u8 gText_PokemonDescription[];
extern const u8 gText_BagDescription[];
extern const u8 gText_PlayerDescription[];
extern const u8 gText_SaveDescription[];
extern const u8 gText_OptionDescription[];
extern const u8 gText_ExitDescription[];
extern const u8 gText_RetireDescription[];
extern const u8 gText_PlayerDescription[];
extern const u8 gText_DexNavDescription[];

extern bool8 (*sStartMenuCallback)(void);
extern u8 sStartMenuCursorPos;
extern u8 sNumStartMenuItems;
extern u8 sStartMenuOrder[];
extern s8 sDrawStartMenuState[2];
extern u8 sStartMenuOpen;

//Vanilla functions:
void __attribute__((long_call)) SetUpStartMenu_Link(void);
void __attribute__((long_call)) SetUpStartMenu_UnionRoom(void);
void __attribute__((long_call)) DestroyHelpMessageWindow_(void);
void __attribute__((long_call)) HideStartMenu(void);
bool8 __attribute__((long_call)) StartMenuPokedexCallback(void);
bool8 __attribute__((long_call)) StartMenuPokemonCallback(void);
bool8 __attribute__((long_call)) StartMenuBagCallback(void);
bool8 __attribute__((long_call)) StartMenuPlayerCallback(void);
bool8 __attribute__((long_call)) StartMenuSaveCallback(void);
bool8 __attribute__((long_call)) StartMenuOptionCallback(void);
bool8 __attribute__((long_call)) StartMenuExitCallback(void);
bool8 __attribute__((long_call)) StartMenuSafariZoneRetireCallback(void);
bool8 __attribute__((long_call)) StartMenuLinkModePlayerCallback(void);
bool8 __attribute__((long_call)) StartMenuQuestLogCallback(void);
void __attribute__((long_call)) AppendToStartMenuItems(u8 action);
void __attribute__((long_call)) DestroySafariZoneStatsWindow(void);
s8 __attribute__((long_call)) PrintStartMenuItems(s8* cursor_p, u8 nitems);
void __attribute__((long_call)) StartMenu_FadeScreenIfLeavingOverworld(void);
bool8 __attribute__((long_call)) StartMenuPokedexSanityCheck(void);
void __attribute__((long_call)) CloseStartMenu(void);
void __attribute__((long_call)) PrintTextOnHelpMessageWindow(const u8 * text, u8 mode);

//Exported functions:
void BuildStartMenuActions(void);

//---------------------------------------------------------------------------
// スタートメニューの時計（左上に AM12:56 と出す）
//
// 時刻は gClock（global.h）。RtcCalcLocalTime() が read_keys.c から毎フレーム
// 呼ばれていて 1 秒ごとに更新される。StartCB_HandleInput の先頭にある
// ForceClockUpdate() は元々この用途のために置かれていたもの。
//
// ウィンドウはサファリゾーンの歩数ウィンドウと**同じ枠を借りる**。
// 本家のテンプレート（ROM 0x0836B3BC）が
//   bg0 / 左1 / 上1 / 幅10 / 高4 / パレット15 / baseBlock 0x8
// で、**スタートメニューと同時に表示される実績がある**ため、
// VRAM のタイルがぶつからないことが確かめられている。
// （スタートメニュー本体は baseBlock 0x13D・幅7 で、重ならない）
//
// そのぶん、サファリゾーンでは本家の歩数ウィンドウが同じ枠を使うので
// 時計は出さない。通信中・ユニオンルームでも出さない。
//---------------------------------------------------------------------------
#define CLOCK_WINDOW_BASE_BLOCK 0x8	//サファリの歩数ウィンドウと同じ
#define CLOCK_WINDOW_FONT 2			//スタートメニューの項目と同じフォント
//幅は決め打ちにせず、GetStringWidth で実際の描画幅から出す（下の GetClockWindowWidth）

#define sClockWindowId (*((u8*) 0x203E03F))		//空き RAM（BPRJ.ld に割り当て無し）
#define sClockLastMinute (*((u8*) 0x203E040))	//同上。描き直しの判定用

//文字コードは charmap.tbl より。A=0xBB M=0xC7 P=0xCA :=0xF0
static const u8 sText_ClockAM[] = {0xBB, 0xC7, EOS};
static const u8 sText_ClockPM[] = {0xCA, 0xC7, EOS};
static const u8 sText_ClockColon[] = {0xF0, EOS};

//This file's functions:
static void SetUpStartMenu_NormalField(void);
static bool8 ShouldShowStartMenuClock(void);
static u8* BuildClockString(u8* str, u8 hour24, u8 minute);
static u8 GetClockWindowWidth(void);
static void PrintStartMenuClock(void);
static void ShowStartMenuClock(void);
static void DestroyStartMenuClock(void);
static void SetUpStartMenu_SafariZone(void);
static void BuildPokeToolsMenu(void);
static bool8 CloseAndReloadStartMenu(void);
static bool8 ReloadStartMenu(void);
static bool8 ReloadStartMenuItems(void);

const struct MenuAction sStartMenuActionTable[] =
{
	[STARTMENU_POKEDEX] = {gText_MenuPokedex, {.u8_void = StartMenuPokedexCallback}},
	[STARTMENU_POKEMON] = {gText_MenuPokemon, {.u8_void = StartMenuPokemonCallback}},
	[STARTMENU_BAG] = {gText_MenuBag, {.u8_void = StartMenuBagCallback}},
	[STARTMENU_PLAYER] = {gText_MenuPlayer, {.u8_void = StartMenuPlayerCallback}},
	[STARTMENU_SAVE] = {gText_MenuSave, {.u8_void = StartMenuSaveCallback}},
	[STARTMENU_OPTION] = {gText_MenuOption, {.u8_void = StartMenuOptionCallback}},
	[STARTMENU_EXIT] = {gText_MenuExit, {.u8_void = StartMenuExitCallback}},
	[STARTMENU_RETIRE_SAFARI] = {gText_MenuRetire, {.u8_void = StartMenuSafariZoneRetireCallback}},
	[STARTMENU_PLAYER_LINK] = {gText_MenuPlayer, {.u8_void = StartMenuLinkModePlayerCallback}},
	[STARTMENU_DEXNAV] = {gText_MenuDexNav, {.u8_void = StartMenuDexNavCallback}},
	[STARTMENU_EXIT_RIGHT] = {gText_MenuExitRight, {.u8_void = StartMenuExitCallback}},
	[STARTMENU_EXIT_LEFT] = {gText_MenuExitLeft, {.u8_void = StartMenuExitCallback}},
};

const u8* const sStartMenuDescPointers[] =
{
	gText_PokedexDescription,
	gText_PokemonDescription,
	gText_BagDescription,
	gText_PlayerDescription,
	gText_SaveDescription,
	gText_OptionDescription,
	gText_ExitDescription,
	gText_RetireDescription,
	gText_PlayerDescription,
	gText_DexNavDescription,
	gText_ExitDescription,
	gText_ExitDescription,
};

static bool8 CanSetUpSecondaryStartMenu(void)
{
	#ifdef FLAG_SYS_DEXNAV
	if (FlagGet(FLAG_SYS_DEXNAV) && FlagGet(FLAG_SYS_POKEDEX_GET))
		return TRUE;
	#endif

	#ifdef FLAG_SYS_QUEST_LOG
	if (FlagGet(FLAG_SYS_QUEST_LOG))
		return TRUE;
	#endif

	return FALSE;
}

static void SetUpStartMenu_NormalField(void)
{
	if (FlagGet(FLAG_SYS_POKEDEX_GET))
		AppendToStartMenuItems(STARTMENU_POKEDEX);

	if (FlagGet(FLAG_SYS_POKEMON_GET))
		AppendToStartMenuItems(STARTMENU_POKEMON);

	#ifdef FLAG_SYS_BAG_HIDE
		if (!FlagGet(FLAG_SYS_BAG_HIDE))
	#endif
			AppendToStartMenuItems(STARTMENU_BAG);

	#ifdef FLAG_SYS_PLAYER_HIDE
		if (!FlagGet(FLAG_SYS_PLAYER_HIDE))
	#endif
			AppendToStartMenuItems(STARTMENU_PLAYER);

	#ifdef FLAG_SYS_SAVE_HIDE
		if (!FlagGet(FLAG_SYS_SAVE_HIDE))
	#endif
			AppendToStartMenuItems(STARTMENU_SAVE);

	//These two are always present
	AppendToStartMenuItems(STARTMENU_OPTION);

	if  (sStartMenuOpen == START_MENU_NORMAL && CanSetUpSecondaryStartMenu())
		AppendToStartMenuItems(STARTMENU_EXIT_RIGHT);
	else
		AppendToStartMenuItems(STARTMENU_EXIT);
}

static void SetUpStartMenu_SafariZone(void)
{
	AppendToStartMenuItems(STARTMENU_RETIRE_SAFARI);

	if (FlagGet(FLAG_SYS_POKEDEX_GET))
		AppendToStartMenuItems(STARTMENU_POKEDEX);

	#ifdef FLAG_SYS_POKEMON_GET
	if (FlagGet(FLAG_SYS_POKEMON_GET))
	#endif
		AppendToStartMenuItems(STARTMENU_POKEMON);

	#ifdef FLAG_SYS_BAG_GET
		if (FlagGet(FLAG_SYS_BAG_GET))
	#endif
			AppendToStartMenuItems(STARTMENU_BAG);

	#ifdef FLAG_SYS_PLAYER_GET
		if (FlagGet(FLAG_SYS_PLAYER_GET))
	#endif
			AppendToStartMenuItems(STARTMENU_PLAYER);

	AppendToStartMenuItems(STARTMENU_OPTION);

	if  (sStartMenuOpen == START_MENU_NORMAL && CanSetUpSecondaryStartMenu())
		AppendToStartMenuItems(STARTMENU_EXIT_RIGHT);
	else
		AppendToStartMenuItems(STARTMENU_EXIT);
}

static void BuildPokeToolsMenu(void)
{
	sNumStartMenuItems = 0;

	#ifdef FLAG_SYS_DEXNAV
	if (FlagGet(FLAG_SYS_DEXNAV) && FlagGet(FLAG_SYS_POKEDEX_GET))
	#endif
		AppendToStartMenuItems(STARTMENU_DEXNAV);

	#ifdef FLAG_SYS_QUEST_LOG
	if (FlagGet(FLAG_SYS_QUEST_LOG))
		AppendToStartMenuItems(STARTMENU_QUEST_LOG);
	#endif

	AppendToStartMenuItems(STARTMENU_EXIT_LEFT);
}

void SetUpStartMenu(void)
{
	sNumStartMenuItems = 0;

	if (IsUpdateLinkStateCBActive())
		SetUpStartMenu_Link();
	else if (InUnionRoom())
		SetUpStartMenu_UnionRoom();
	else if (GetSafariZoneFlag())
		SetUpStartMenu_SafariZone();
	else if (sStartMenuOpen == START_MENU_TOOLS)
		BuildPokeToolsMenu();
	else
		SetUpStartMenu_NormalField();

	ShowStartMenuClock();
}

static bool8 ShouldShowStartMenuClock(void)
{
	//サファリでは本家の歩数ウィンドウが同じ枠を使う。通信中も触らない
	return !GetSafariZoneFlag()
		&& !IsUpdateLinkStateCBActive()
		&& InUnionRoom() != TRUE;
}

//AM12:56 の形に組み立てる。終端を指すポインタを返す
static u8* BuildClockString(u8* str, u8 hour24, u8 minute)
{
	u8 hour = hour24 % 12;

	if (hour == 0)
		hour = 12; //0時台と12時台はどちらも 12 と出す

	str = StringCopy(str, (hour24 < 12) ? sText_ClockAM : sText_ClockPM);
	str = ConvertIntToDecimalStringN(str, hour, STR_CONV_MODE_LEFT_ALIGN, 2);
	str = StringCopy(str, sText_ClockColon);
	str = ConvertIntToDecimalStringN(str, minute, STR_CONV_MODE_LEADING_ZEROS, 2);
	*str = EOS;
	return str;
}

//窓の幅を実際の描画幅から決める。決め打ちだと余白が出るため。
//時刻で幅が変わらないよう、一番広くなる「12時台・2桁の分」で測る
static u8 GetClockWindowWidth(void)
{
	u8 text[16];
	s32 width, other;

	BuildClockString(text, 0, 88);	//AM12:88
	width = GetStringWidth(CLOCK_WINDOW_FONT, text, 0);
	BuildClockString(text, 12, 88);	//PM12:88
	other = GetStringWidth(CLOCK_WINDOW_FONT, text, 0);

	if (other > width)
		width = other;

	return (width + 7) / 8; //ピクセル → タイル（切り上げ）
}

static void PrintStartMenuClock(void)
{
	u8 text[16];

	BuildClockString(text, gClock.hour, gClock.minute);

	FillWindowPixelBuffer(sClockWindowId, PIXEL_FILL(1));
	AddTextPrinterParameterized(sClockWindowId, CLOCK_WINDOW_FONT, text, 0, 2, 0, NULL);
	CopyWindowToVram(sClockWindowId, 2);
	sClockLastMinute = gClock.minute;
}

static void ShowStartMenuClock(void)
{
	struct WindowTemplate template;

	if (!ShouldShowStartMenuClock())
		return;

	template = SetWindowTemplateFields(0, 1, 1, GetClockWindowWidth(), 2, 15, CLOCK_WINDOW_BASE_BLOCK);
	sClockWindowId = AddWindow(&template);
	DrawStdWindowFrame(sClockWindowId, FALSE);
	PutWindowTilemap(sClockWindowId);
	PrintStartMenuClock();
}

static void DestroyStartMenuClock(void)
{
	//出した時と同じ条件でしか消さない。メニューを開いている間に
	//条件が変わることは無いので、これで作った分と対応が取れる
	if (!ShouldShowStartMenuClock())
		return;

	ClearStdWindowAndFrame(sClockWindowId, TRUE);
	RemoveWindow(sClockWindowId);
}

bool8 StartCB_HandleInput(void)
{
	ForceClockUpdate(); //To help with the clock in the start menu routine

	//分が変わったときだけ描き直す
	if (sClockLastMinute != gClock.minute && ShouldShowStartMenuClock())
		PrintStartMenuClock();

	if (JOY_NEW(DPAD_UP))
	{
		PlaySE(SE_SELECT);
		sStartMenuCursorPos = Menu_MoveCursor(-1);
		#ifndef UNBOUND
		if (!MenuHelpers_LinkSomething() && !InUnionRoom())
		{
			PrintTextOnHelpMessageWindow(sStartMenuDescPointers[sStartMenuOrder[sStartMenuCursorPos]], 2);
		}
		#endif
	}
	else if (JOY_NEW(DPAD_DOWN))
	{
		PlaySE(SE_SELECT);
		sStartMenuCursorPos = Menu_MoveCursor(+1);
		#ifndef UNBOUND
		if (!MenuHelpers_LinkSomething() && !InUnionRoom())
		{
			PrintTextOnHelpMessageWindow(sStartMenuDescPointers[sStartMenuOrder[sStartMenuCursorPos]], 2);
		}
		#endif
	}
	else if (JOY_NEW(DPAD_RIGHT))
	{
		if (sStartMenuOpen == START_MENU_NORMAL && CanSetUpSecondaryStartMenu())
		{
			PlaySE(SE_SELECT);
			sStartMenuCursorPos = 0; //Reset cursor position
			sStartMenuOpen = START_MENU_TOOLS;
			sStartMenuCallback = CloseAndReloadStartMenu;
		}
	}
	else if (JOY_NEW(DPAD_LEFT))
	{
		if (sStartMenuOpen == START_MENU_TOOLS)
		{
			PlaySE(SE_SELECT);
			sStartMenuCursorPos = 0; //Reset cursor position
			sStartMenuOpen = START_MENU_NORMAL;
			sStartMenuCallback = CloseAndReloadStartMenu;
		}
	}
	else if (JOY_NEW(A_BUTTON))
	{
		PlaySE(SE_SELECT);
		if (!StartMenuPokedexSanityCheck())
			return FALSE;
		DestroyStartMenuClock();
		sStartMenuCallback = sStartMenuActionTable[sStartMenuOrder[sStartMenuCursorPos]].func.u8_void;
		StartMenu_FadeScreenIfLeavingOverworld();
		return FALSE;
	}
	else if (JOY_NEW(B_BUTTON | START_BUTTON))
	{
		DestroyStartMenuClock();
		DestroySafariZoneStatsWindow();
		DestroyHelpMessageWindow_();
		CloseStartMenu();
		return TRUE;
	}

	return FALSE;
}

static bool8 CloseAndReloadStartMenu(void)
{
	DestroyStartMenuClock(); //SetUpStartMenu で作り直される
	ClearStdWindowAndFrame(GetStartMenuWindowId(), TRUE);
	RemoveStartMenuWindow();
	sStartMenuCallback = ReloadStartMenu;
	return FALSE;
}

static bool8 ReloadStartMenu(void)
{
	SetUpStartMenu();
	DrawStdWindowFrame(CreateStartMenuWindow(sNumStartMenuItems), FALSE);
	CopyWindowToVram(GetStartMenuWindowId(), 1);
	sDrawStartMenuState[1] = 0;
	sStartMenuCallback = ReloadStartMenuItems;
	return FALSE;
}

static bool8 ReloadStartMenuItems(void)
{
	if (PrintStartMenuItems(&sDrawStartMenuState[1], 2))
	{
		sStartMenuCursorPos = Menu_InitCursor(GetStartMenuWindowId(), 2, 0, 0, 15, sNumStartMenuItems, sStartMenuCursorPos);
		#ifndef UNBOUND
		if (!MenuHelpers_LinkSomething() && InUnionRoom() != TRUE)
		{
			DrawHelpMessageWindowWithText(sStartMenuDescPointers[sStartMenuOrder[sStartMenuCursorPos]]);
		}
		#endif
		CopyWindowToVram(GetStartMenuWindowId(), 1);
		sStartMenuCallback = StartCB_HandleInput;
	}

	return FALSE;
}
