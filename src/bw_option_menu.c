#include "defines.h"
#include "../include/bg.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/menu.h"
#include "../include/palette.h"
#include "../include/sprite.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/text_window.h"
#include "../include/window.h"
#include "../include/gba/macro.h"
#include "../include/gba/m4a_internal.h"
#include "../include/new/dns.h"
#include "../include/event_data.h"
#include "Tables/hoenn_script_flags.h"   /* HOENN_VAR_OVERWORLD_SPEED */
#include "hack_battle_speed.h"
#include "hack_difficulty.h"

/*
    BW 風オプション画面（せってい）— #19 の追加分

    元: https://github.com/Shiny-Miner/FR-OptionMenu（作者 Compumaxx・訳 Shiny-Miner）。
    `参考/github/FR-OptionMenu/src/option_menu.c` を BPRJ + CFRU に合わせて書き直した。
    経緯: docs/research/2026-09-19_bw-option-menu.md

    ## 素 FRLG との関係

    元のパッチは、素のスタートメニューが「せってい」で飛ぶ先のリテラル 1 語
    （BPRE 0806F534 = CB2_OptionsMenuFromStartMenu）を自分の CB2 に差し替えるだけ。
    こちらは BW 風スタートメニュー（src/bw_start_menu.c）から**直接この CB2 を呼ぶ**ので
    ROM には 1 バイトも触らない。★ タイトル画面（つづきから／せってい）の「せってい」は
    素の画面のまま（元のパッチも同じ）。
    戻り先は gMain.savedCallback（スタートメニューが CB2_ReturnToFieldWithOpenMenu を入れる）。

    ## 中身
      - 項目は 6 つ（はなしのはやさ／せんとうアニメ／せんとうルール／おと／
        いどうそくど（元はボタンモード・下）／わく）。読み書きするのは gSaveBlock2 の素の欄（いどうそくどだけ変数）
      - 見た目: 暗い画面（bg1 のタイルマップ 6 枚を選択中の行で切り替える）、
        上に「せってい」と「A ほぞん / B もどる」、下の枠に説明（項目ごと）。
        枠は「わく」の選択で差し替わる（素の GetWindowFrameTilesPal）
      - 文字の色は先頭に FC 01 色 / FC 02 00 を付けて決める（元の {COLOR n}{HIGHLIGHT 0} と同じ）。
        選んでいる選択肢 1（白）、選んでいない選択肢と説明 5（灰）、題 2

    ## CFRU との関係
      - 元の `sArrowPressed`（書き込み可能な static）はタスクの data[8] に置き換えた（6.13）
      - gPaletteFade は CFRU ではポインタ
      - フォントは 2（日本語の標準。CFRU の時計と同じ）。選択肢の x は日本語の幅に合わせて動かした

    ## いどうそくど（2026-10-01）
      ユーザー「その倍速を設定に組み込んでほしい」「ボタンモード潰していいよ」。
      5 行目の「ボタンモード」を**フィールドの倍速（1〜4 ばい）**に差し替えた（行の数と背景の絵はそのまま）。
      値は変数 HOENN_VAR_OVERWORLD_SPEED（倍速の処理は src/overworld_speedup.c）。
      ★ gSaveBlock2 のボタンモードはもう書かない（セーブに入っている値のまま・タイトル画面の素の「せってい」では変えられる）

    ## たいせんそくど・スクロール（2026-10-09）
      ユーザー「後設定スクロールできない？」→ 増やす項目「たいせんそくど」（戦闘の 1〜4 ばい・src/hack_battle_speed.c）。
      画面に出る行は 6 つのまま（背景の絵 6 枚＝選んでいる行の位置）。7 行目からは上下でスクロールする（TD_TOP＝いちばん上の項目）。
      項目を足すときは MENUITEM_* と sItemNames・sDescriptions・TD_* を同じ並びで足す（TD_TEXTSPEED + 項目の番号が値の置き場）。
*/

void __attribute__((long_call)) ScanlineEffect_Stop(void);      /* BPRJ.ld 08087A48 */

/* ---- 絵（src/bw_option_menu_gfx.c・生成物）---- */
extern const u8 gBwOptTiles[];
extern const u8 gBwOptMap0[];
extern const u8 gBwOptMap1[];
extern const u8 gBwOptMap2[];
extern const u8 gBwOptMap3[];
extern const u8 gBwOptMap4[];
extern const u8 gBwOptMap5[];
extern const u8 gBwOptTextPal[];

/* ---- 文字列（strings/bw_option_menu.string）---- */
extern const u8 gText_BwOpt_Title[];
extern const u8 gText_BwOpt_Save[];
extern const u8 gText_BwOpt_Back[];
extern const u8 gText_BwOpt_TextSpeed[];
extern const u8 gText_BwOpt_BattleScene[];
extern const u8 gText_BwOpt_BattleStyle[];
extern const u8 gText_BwOpt_Sound[];
extern const u8 gText_BwOpt_FieldSpeed[];
extern const u8 gText_BwOpt_Difficulty[];
extern const u8 gText_BwOpt_BattleSpeed[];
extern const u8 gText_BwOpt_Moemon[];
extern const u8 gText_BwOpt_MoemonOn[];
extern const u8 gText_BwOpt_MoemonOff[];
extern const u8 gText_BwOpt_Habitat[];
extern const u8 gText_BwOpt_HabitatOrig[];
extern const u8 gText_BwOpt_WonderTrade[];
extern const u8 gText_BwOpt_Medium[];
extern const u8 gText_BwOpt_Easy[];
extern const u8 gText_BwOpt_Hard[];
extern const u8 gText_BwOpt_MomoIce[];
extern const u8 gText_BwOpt_Slow[];
extern const u8 gText_BwOpt_Mid[];
extern const u8 gText_BwOpt_Fast[];
extern const u8 gText_BwOpt_On[];
extern const u8 gText_BwOpt_Off[];
extern const u8 gText_BwOpt_Shift[];
extern const u8 gText_BwOpt_Set[];
extern const u8 gText_BwOpt_Mono[];
extern const u8 gText_BwOpt_Stereo[];
extern const u8 gText_BwOpt_Speed1[];
extern const u8 gText_BwOpt_Speed2[];
extern const u8 gText_BwOpt_Speed3[];
extern const u8 gText_BwOpt_Speed4[];
extern const u8 gText_BwOpt_DescTextSpeed[];
extern const u8 gText_BwOpt_DescBattleScene[];
extern const u8 gText_BwOpt_DescBattleStyle[];
extern const u8 gText_BwOpt_DescSound[];
extern const u8 gText_BwOpt_DescFieldSpeed[];
extern const u8 gText_BwOpt_DescDifficulty[];
extern const u8 gText_BwOpt_DescBattleSpeed[];
extern const u8 gText_BwOpt_DescMoemon[];
extern const u8 gText_BwOpt_DescHabitat[];
extern const u8 gText_BwOpt_DescWonderTrade[];
extern const u8 gText_BwOpt_DescSave[];
extern const u8 gText_BwOpt_DescCancel[];

#define BWOPT_FONT 2
#define BWOPT_FRAME_COUNT 10             /* 素 FRLG の「わく」の数（タイプ 1〜10） */
#define BWOPT_SPEED_COUNT 4              /* いどうそくど 1〜4 ばい */
#define BWOPT_COLOR_TITLE 2
#define BWOPT_COLOR_ON 1                 /* 選んでいる選択肢（白） */
#define BWOPT_COLOR_OFF 5                /* 選んでいない選択肢・説明（灰） */
#define BWOPT_FRAME_TILE 0x1A2           /* 枠の見本のタイル（bg2・charBase 0）*/
#define BWOPT_FRAME_PAL 14
#define BWOPT_VISIBLE_ROWS 6             /* 画面に出る行（背景の絵の数） */

enum
{
    TD_MENUSELECTION,
    TD_TEXTSPEED,
    TD_BATTLESCENE,
    TD_BATTLESTYLE,
    TD_SOUND,
    TD_FIELDSPEED,                       /* 元はボタンモード */
    TD_DIFFICULTY,                       /* 元はわく（2026-10-03） */
    TD_BATTLESPEED,                      /* 2026-10-09 */
#ifndef HACK_NO_MOEMON
    TD_MOEMON,                           /* 2026-10-10 Moémon の絵（もとは たいせつなもの Moemonボタン） */
#endif
    TD_HABITAT,                          /* 2026-10-10 せいそくち（もとは たいせつなもの せいそくスイッチ） */
    TD_WONDERTRADE,                      /* 2026-10-10 ミラクルこうかんの場面 */
    TD_TIMER,
    TD_ARROW_PRESSED,                    /* 元の sArrowPressed（static）の代わり */
    TD_TOP,                              /* スクロール: いちばん上に出ている項目 */
};

enum
{
    MENUITEM_TEXTSPEED,
    MENUITEM_BATTLESCENE,
    MENUITEM_BATTLESTYLE,
    MENUITEM_SOUND,
    MENUITEM_FIELDSPEED,                 /* 元はボタンモード */
    MENUITEM_DIFFICULTY,                 /* 元はわく（2026-10-03・なんいど ふつう／やさしい） */
    MENUITEM_BATTLESPEED,                /* 2026-10-09 たいせんそくど */
#ifndef HACK_NO_MOEMON
    MENUITEM_MOEMON,                     /* 2026-10-10 だす／ださない＝旗 HACK_FLAG_MOEMON_ON（オミット版には無い） */
#endif
    MENUITEM_HABITAT,                    /* 2026-10-10 いれかえ／もとのまま＝旗 HACK_FLAG_HABITAT_ORIGINAL */
    MENUITEM_WONDERTRADE,                /* 2026-10-10 みる／みない＝旗 HACK_FLAG_WONDER_SKIP（src/hack_wonder_trade.c） */
    MENUITEM_COUNT,
};

enum
{
    DESC_SAVE = MENUITEM_COUNT,
    DESC_CANCEL,
};

enum
{
    WIN_TEXT,
    WIN_FRAME,
};

static const u8 *const sItemNames[MENUITEM_COUNT] =
{
    gText_BwOpt_TextSpeed, gText_BwOpt_BattleScene, gText_BwOpt_BattleStyle,
    gText_BwOpt_Sound, gText_BwOpt_FieldSpeed, gText_BwOpt_Difficulty, gText_BwOpt_BattleSpeed,
#ifndef HACK_NO_MOEMON
    gText_BwOpt_Moemon,
#endif
    gText_BwOpt_Habitat,
    gText_BwOpt_WonderTrade,
};

static const u8 *const sDescriptions[] =
{
    gText_BwOpt_DescTextSpeed, gText_BwOpt_DescBattleScene, gText_BwOpt_DescBattleStyle,
    gText_BwOpt_DescSound, gText_BwOpt_DescFieldSpeed, gText_BwOpt_DescDifficulty, gText_BwOpt_DescBattleSpeed,
#ifndef HACK_NO_MOEMON
    gText_BwOpt_DescMoemon,
#endif
    gText_BwOpt_DescHabitat,
    gText_BwOpt_DescWonderTrade,
    gText_BwOpt_DescSave, gText_BwOpt_DescCancel,
};

static const u8 *const sMaps[BWOPT_VISIBLE_ROWS] =   /* 選んでいる行の位置（項目 - TD_TOP）で選ぶ */
{
    gBwOptMap0, gBwOptMap1, gBwOptMap2, gBwOptMap3, gBwOptMap4, gBwOptMap5,
};

static const struct WindowTemplate sWindowTemplates[] =
{
    [WIN_TEXT] = {.bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 30, .height = 20, .paletteNum = 5, .baseBlock = 0},
    [WIN_FRAME] = {.bg = 2, .tilemapLeft = 0, .tilemapTop = 14, .width = 30, .height = 6, .paletteNum = BWOPT_FRAME_PAL, .baseBlock = 0},
    DUMMY_WIN_TEMPLATE,
};

static const struct BgTemplate sBgTemplates[] =
{
    {.bg = 0, .charBaseIndex = 1, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0},
    {.bg = 1, .charBaseIndex = 0, .mapBaseIndex = 7, .screenSize = 0, .paletteMode = 0, .priority = 2, .baseTile = 0},
    {.bg = 2, .charBaseIndex = 0, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0},
};

static void Task_FadeIn(u8 taskId);
static void Task_ProcessInput(u8 taskId);
static void Task_Save(u8 taskId);
static void Task_Cancel(u8 taskId);
static void Task_FadeOut(u8 taskId);
static void DrawChoicesFor(u8 item, u8 selection, u8 top);

/* ---- 文字 ---- */

/* 先頭に色の指定を付けて描く。元の {COLOR n}{HIGHLIGHT 0} と同じ並び */
static void PrintColored(const u8 *text, u8 x, u8 y, u8 color)
{
    u8 dst[96];
    u8 *p = dst;
    u16 i;

    *p++ = 0xFC; *p++ = 0x01; *p++ = color;
    *p++ = 0xFC; *p++ = 0x02; *p++ = 0;
    for (i = 0; text[i] != EOS && i < 80; ++i)
        *p++ = text[i];
    *p = EOS;
    AddTextPrinterParameterized(WIN_TEXT, BWOPT_FONT, dst, x, y, TEXT_SPEED_FF, NULL);
}

static void DrawChoice(const u8 *text, u8 x, u8 y, bool8 on)
{
    PrintColored(text, x, y + 1, on ? BWOPT_COLOR_ON : BWOPT_COLOR_OFF);
}

static void ClearDescription(void)
{
    FillWindowPixelRect(WIN_TEXT, 0, 8, 120, 224, 32);
}

static void ShowDescription(u8 which)
{
    ClearDescription();
    PrintColored(sDescriptions[which], 12, 120, BWOPT_COLOR_OFF);
    CopyWindowToVram(WIN_TEXT, 2);
}

static void DrawFramePreview(void)
{
    /*                          bg  tile                 x   y   w   h  pal */
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 0,  0, 14,  1,  1, BWOPT_FRAME_PAL);
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 1,  1, 14, 28,  1, BWOPT_FRAME_PAL);
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 2, 29, 14,  1,  1, BWOPT_FRAME_PAL);
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 3,  0, 15,  1,  4, BWOPT_FRAME_PAL);
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 4,  1, 15, 28,  4, BWOPT_FRAME_PAL);
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 5, 29, 15,  1,  4, BWOPT_FRAME_PAL);
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 6,  0, 19,  1,  1, BWOPT_FRAME_PAL);
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 7,  1, 19, 28,  1, BWOPT_FRAME_PAL);
    FillBgTilemapBufferRect(2, BWOPT_FRAME_TILE + 8, 29, 19,  1,  1, BWOPT_FRAME_PAL);
    CopyBgTilemapBufferToVram(2);
}

static void LoadFrameGfx(u8 frameType)
{
    const struct TilesPal *frame = GetWindowFrameTilesPal(frameType);

    LoadBgTiles(2, frame->tiles, 0x120, BWOPT_FRAME_TILE);
    LoadPalette(frame->pal, BWOPT_FRAME_PAL * 16, 0x20);
}

static void DrawTexts(void)
{
    u8 i;

    FillWindowPixelBuffer(WIN_TEXT, PIXEL_FILL(0));
    PrintColored(gText_BwOpt_Title, 8, 1, BWOPT_COLOR_TITLE);
    /* ★ A / B のボタンの絵は**背景の絵に焼き込まれている**（格子の x タイル 16 = 画面 x128〜135 と
       x タイル 23 = 画面 x184〜191）。文字で描くと 2 つ出る（実機 2026-09-20）。言葉だけをその右へ置く */
    PrintColored(gText_BwOpt_Save, 138, 1, BWOPT_COLOR_TITLE);
    PrintColored(gText_BwOpt_Back, 194, 1, BWOPT_COLOR_TITLE);
    ShowDescription(0);
    for (i = 0; i < MENUITEM_COUNT && i < BWOPT_VISIBLE_ROWS; ++i)
        PrintColored(sItemNames[i], 8, (i * 16) + 17, BWOPT_COLOR_ON);
    CopyWindowToVram(WIN_TEXT, 2);
}

/* スクロールしたとき: 行の場所を消して、出ている項目の名前と選択肢を描き直す */
static void DrawRows(const s16 *data)
{
    u8 top = data[TD_TOP];
    u8 i;

    FillWindowPixelRect(WIN_TEXT, 0, 0, 16, 240, BWOPT_VISIBLE_ROWS * 16);
    for (i = top; i < MENUITEM_COUNT && i < top + BWOPT_VISIBLE_ROWS; ++i)
    {
        PrintColored(sItemNames[i], 8, ((i - top) * 16) + 17, BWOPT_COLOR_ON);
        DrawChoicesFor(i, data[TD_TEXTSPEED + i], top);
    }
    CopyWindowToVram(WIN_TEXT, 2);
}

/* ---- 選択肢 ---- */

static void DrawChoices3(const u8 *a, const u8 *b, const u8 *c, u8 x1, u8 x2, u8 x3, u8 y, u8 sel)
{
    DrawChoice(a, x1, y, sel == 0);
    DrawChoice(b, x2, y, sel == 1);
    DrawChoice(c, x3, y, sel == 2);
}

static void DrawChoices2(const u8 *a, const u8 *b, u8 x1, u8 x2, u8 y, u8 sel)
{
    DrawChoice(a, x1, y, sel == 0);
    DrawChoice(b, x2, y, sel == 1);
}

/* やさしい・ほどほど・ふつう・むずかしい・ももアイス。保存値の対応は hack_difficulty.h。 */

/* 横に並ばない選択肢は今の値だけを出す（左右で切り替え） */
static void DrawValueChoice(const u8 *text, u8 y)
{
    FillWindowPixelRect(WIN_TEXT, 0, 104, y, 120, 16);
    DrawChoice(text, 150, y, TRUE);
}

static void DrawDifficultyChoice(u8 selection, u8 y)
{
    static const u8 *const sNames[DIFFICULTY_COUNT] =
    {
        gText_BwOpt_Easy, gText_BwOpt_Medium, gText_BwOpt_Mid, gText_BwOpt_Hard, gText_BwOpt_MomoIce,
    };

    DrawValueChoice(sNames[selection < DIFFICULTY_COUNT ? selection : 2], y);
}

static void DrawSpeedChoice(u8 selection, u8 y)
{
    static const u8 *const sSpeeds[BWOPT_SPEED_COUNT] =
    {
        gText_BwOpt_Speed1, gText_BwOpt_Speed2, gText_BwOpt_Speed3, gText_BwOpt_Speed4,
    };

    DrawValueChoice(sSpeeds[selection < BWOPT_SPEED_COUNT ? selection : 0], y);
}

/* ★ 日本語の幅に合わせた x。3 文字（約 36 画素）が重ならないよう 40 ずつ空ける。y は画面の行（項目 - top）で決める */
static void DrawChoicesFor(u8 item, u8 selection, u8 top)
{
    u8 y;

    if (item < top || item >= top + BWOPT_VISIBLE_ROWS)
        return;
    y = (item - top + 1) * 16;
    switch (item)
    {
    case MENUITEM_TEXTSPEED:
        DrawChoices3(gText_BwOpt_Slow, gText_BwOpt_Mid, gText_BwOpt_Fast, 104, 148, 192, y, selection);
        break;
    case MENUITEM_BATTLESCENE:
        DrawChoices2(gText_BwOpt_On, gText_BwOpt_Off, 112, 162, y, selection);
        break;
    case MENUITEM_BATTLESTYLE:
        DrawChoices2(gText_BwOpt_Shift, gText_BwOpt_Set, 112, 172, y, selection);
        break;
    case MENUITEM_SOUND:
        DrawChoices2(gText_BwOpt_Mono, gText_BwOpt_Stereo, 112, 172, y, selection);
        break;
    case MENUITEM_FIELDSPEED:
    case MENUITEM_BATTLESPEED:
        DrawSpeedChoice(selection, y);
        break;
    case MENUITEM_DIFFICULTY:
        DrawDifficultyChoice(selection, y);
        break;
#ifndef HACK_NO_MOEMON
    case MENUITEM_MOEMON:
        DrawChoices2(gText_BwOpt_MoemonOn, gText_BwOpt_MoemonOff, 112, 162, y, selection);
        break;
#endif
    case MENUITEM_HABITAT:
        DrawChoices2(gText_BwOpt_Shift, gText_BwOpt_HabitatOrig, 112, 172, y, selection);
        break;
    case MENUITEM_WONDERTRADE:
        DrawChoices2(gText_BwOpt_On, gText_BwOpt_Off, 112, 162, y, selection);
        break;
    }
    CopyWindowToVram(WIN_TEXT, 2);
}

static u8 CycleInput(u8 selection, u8 count, s16 *arrow)
{
    if (JOY_NEW(DPAD_RIGHT))
    {
        selection = (selection + 1 < count) ? selection + 1 : 0;
        *arrow = TRUE;
    }
    if (JOY_NEW(DPAD_LEFT))
    {
        selection = (selection != 0) ? selection - 1 : count - 1;
        *arrow = TRUE;
    }
    return selection;
}

static u8 ProcessInputFor(u8 item, u8 selection, s16 *arrow)
{
    u8 prev = selection;

    switch (item)
    {
    case MENUITEM_TEXTSPEED:
        selection = CycleInput(selection, 3, arrow);
        break;
    case MENUITEM_BATTLESCENE:
    case MENUITEM_BATTLESTYLE:
        selection = CycleInput(selection, 2, arrow);
        break;
    case MENUITEM_SOUND:
        selection = CycleInput(selection, 2, arrow);
        if (selection != prev)
            SetPokemonCryStereo(selection);
        break;
    case MENUITEM_FIELDSPEED:
    case MENUITEM_BATTLESPEED:
        selection = CycleInput(selection, BWOPT_SPEED_COUNT, arrow);
        break;
    case MENUITEM_DIFFICULTY:
        selection = CycleInput(selection, DIFFICULTY_COUNT, arrow);
        break;
#ifndef HACK_NO_MOEMON
    case MENUITEM_MOEMON:
#endif
    case MENUITEM_HABITAT:
    case MENUITEM_WONDERTRADE:
        selection = CycleInput(selection, 2, arrow);
        break;
    }
    return selection;
}

/* ---- 画面の流れ ---- */

static void MainCB2_BwOptionMenu(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB_BwOptionMenu(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

void CB2_BwOptionMenu(void)
{
    switch (gMain.state)
    {
    default:
    case 0:
        SetVBlankCallback(NULL);
        SetHBlankCallback(NULL);
        gMain.state++;
        break;
    case 1:
        DmaClearLarge16(3, (void *)VRAM, VRAM_SIZE, 0x1000);
        DmaClear32(3, (void *)OAM, OAM_SIZE);
        DmaClear16(3, (void *)PLTT, PLTT_SIZE);
        SetGpuReg(REG_OFFSET_DISPCNT, 0);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, NELEMS(sBgTemplates));
        ChangeBgX(0, 0, 0); ChangeBgY(0, 0, 0);
        ChangeBgX(1, 0, 0); ChangeBgY(1, 0, 0);
        ChangeBgX(2, 0, 0); ChangeBgY(2, 0, 0);
        ChangeBgX(3, 0, 0); ChangeBgY(3, 0, 0);
        InitWindows(sWindowTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, 0);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        ShowBg(0);
        ShowBg(1);
        ShowBg(2);
        gMain.state++;
        break;
    case 2:
        ResetSpriteData();
        ResetPaletteFade();
        FreeAllSpritePalettes();
        ResetTasks();
        ScanlineEffect_Stop();
        gMain.state++;
        break;
    case 3:
        LoadFrameGfx(gSaveBlock2->optionsWindowFrameType);
        gMain.state++;
        break;
    case 4:
        LZ77UnCompVram(gBwOptTiles, (void *)VRAM);
        LZ77UnCompVram(gBwOptMap0, (void *)(VRAM + 0x3800));
        LoadPalette(gBwOptTextPal, 0x50, 0xE0);
        gMain.state++;
        break;
    case 5:
        PutWindowTilemap(WIN_TEXT);
        DrawTexts();
        PutWindowTilemap(WIN_FRAME);
        DrawFramePreview();
        gMain.state++;
        break;
    case 6:
    {
        u8 taskId = CreateTask(Task_FadeIn, 0);
        s16 *data = gTasks[taskId].data;
        u8 i;

        data[TD_MENUSELECTION] = 0;
        data[TD_TEXTSPEED] = gSaveBlock2->optionsTextSpeed;
        if (data[TD_TEXTSPEED] > OPTIONS_TEXT_SPEED_FAST)
            data[TD_TEXTSPEED] = OPTIONS_TEXT_SPEED_MID;
        data[TD_BATTLESCENE] = gSaveBlock2->optionsBattleSceneOff;
        data[TD_BATTLESTYLE] = gSaveBlock2->optionsBattleStyle;
        data[TD_SOUND] = gSaveBlock2->optionsSound;
        data[TD_FIELDSPEED] = VarGet(HOENN_VAR_OVERWORLD_SPEED);
        if (data[TD_FIELDSPEED] >= BWOPT_SPEED_COUNT)
            data[TD_FIELDSPEED] = 0;
        data[TD_DIFFICULTY] = HackDifficulty_SelectionFromValue(VarGet(HOENN_VAR_DIFFICULTY));
        data[TD_BATTLESPEED] = HackBattleSpeed_Get();
#ifndef HACK_NO_MOEMON
        data[TD_MOEMON] = FlagGet(HACK_FLAG_MOEMON_ON) ? 0 : 1;          /* 0 だす / 1 ださない */
#endif
        data[TD_HABITAT] = FlagGet(HACK_FLAG_HABITAT_ORIGINAL) ? 1 : 0;  /* 0 いれかえ / 1 もとのまま */
        data[TD_WONDERTRADE] = FlagGet(HACK_FLAG_WONDER_SKIP) ? 1 : 0;    /* 0 みる / 1 みない */
        data[TD_ARROW_PRESSED] = FALSE;
        data[TD_TOP] = 0;
        for (i = 0; i < MENUITEM_COUNT; ++i)
            DrawChoicesFor(i, data[TD_TEXTSPEED + i], 0);
        CopyWindowToVram(WIN_TEXT, 3);
        gMain.state++;
        break;
    }
    case 7:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_BwOptionMenu);
        SetMainCallback2(MainCB2_BwOptionMenu);
        return;
    }
}

static void Task_FadeIn(u8 taskId)
{
    if (!gPaletteFade->active)
        gTasks[taskId].func = Task_ProcessInput;
}

static void Task_ProcessInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    u8 item = data[TD_MENUSELECTION];

    if (JOY_NEW(A_BUTTON))
    {
        data[TD_TIMER] = 20;
        gTasks[taskId].func = Task_Save;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        data[TD_TIMER] = 20;
        gTasks[taskId].func = Task_Cancel;
    }
    else if (JOY_NEW(DPAD_UP) || JOY_NEW(DPAD_DOWN))
    {
        if (JOY_NEW(DPAD_UP))
            item = (item > 0) ? item - 1 : MENUITEM_COUNT - 1;
        else
            item = (item + 1 < MENUITEM_COUNT) ? item + 1 : 0;
        data[TD_MENUSELECTION] = item;
        if (item < data[TD_TOP] || item >= data[TD_TOP] + BWOPT_VISIBLE_ROWS)
        {
            data[TD_TOP] = (item < data[TD_TOP]) ? item : item - BWOPT_VISIBLE_ROWS + 1;
            DrawRows(data);
        }
        LZ77UnCompVram(sMaps[item - data[TD_TOP]], (void *)(VRAM + 0x3800));
        ShowDescription(item);
    }
    else
    {
        u8 prev = data[TD_TEXTSPEED + item];

        data[TD_TEXTSPEED + item] = ProcessInputFor(item, prev, &data[TD_ARROW_PRESSED]);
        if (data[TD_TEXTSPEED + item] != prev)
            DrawChoicesFor(item, data[TD_TEXTSPEED + item], data[TD_TOP]);
        if (data[TD_ARROW_PRESSED])
        {
            data[TD_ARROW_PRESSED] = FALSE;
            CopyWindowToVram(WIN_TEXT, 2);
        }
    }
}

static void Task_Save(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (data[TD_TIMER] == 20)
        ShowDescription(DESC_SAVE);
    if (data[TD_TIMER])
    {
        data[TD_TIMER]--;
        return;
    }
    gSaveBlock2->optionsTextSpeed = data[TD_TEXTSPEED];
    gSaveBlock2->optionsBattleSceneOff = data[TD_BATTLESCENE];
    gSaveBlock2->optionsBattleStyle = data[TD_BATTLESTYLE];
    gSaveBlock2->optionsSound = data[TD_SOUND];
    VarSet(HOENN_VAR_OVERWORLD_SPEED, data[TD_FIELDSPEED]);
    HackBattleSpeed_Set(data[TD_BATTLESPEED]);
#ifndef HACK_NO_MOEMON
    if (data[TD_MOEMON] == 0)
        FlagSet(HACK_FLAG_MOEMON_ON);
    else
        FlagClear(HACK_FLAG_MOEMON_ON);
#endif
    if (data[TD_HABITAT] == 1)
        FlagSet(HACK_FLAG_HABITAT_ORIGINAL);
    else
        FlagClear(HACK_FLAG_HABITAT_ORIGINAL);
    if (data[TD_WONDERTRADE] == 1)
        FlagSet(HACK_FLAG_WONDER_SKIP);
    else
        FlagClear(HACK_FLAG_WONDER_SKIP);
    VarSet(HOENN_VAR_DIFFICULTY, HackDifficulty_ValueFromSelection(data[TD_DIFFICULTY]));
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_FadeOut;
}

static void Task_Cancel(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (data[TD_TIMER] == 20)
        ShowDescription(DESC_CANCEL);
    if (data[TD_TIMER])
    {
        data[TD_TIMER]--;
        return;
    }
    SetPokemonCryStereo(gSaveBlock2->optionsSound);
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_FadeOut;
}

static void Task_FadeOut(u8 taskId)
{
    if (!gPaletteFade->active)
    {
        DestroyTask(taskId);
        FreeAllWindowBuffers();
        SetMainCallback2(gMain.savedCallback);
    }
}
