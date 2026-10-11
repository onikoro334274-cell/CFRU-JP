#include "defines.h"
#include "../include/bg.h"
#include "../include/decompress.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/malloc.h"
#include "../include/menu.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/region_map.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/text_window.h"
#include "../include/window.h"
#include "../include/constants/flags.h"
#include "../include/constants/songs.h"
#include "../include/gba/macro.h"
#include "../include/new/dns.h"
#include "Tables/hoenn_script_flags.h"   /* HOENN_FLAG_CAN_GO_HOENN */

/*
    BW 風スタートメニュー（#19 の 2 つ目）

    元: https://github.com/ansh746/start-menu（bw ブランチ・作者 ansh860 / LibertyTwins）。
    `参考/github/start-menu-bw/src/start_menu.c` を BPRJ + CFRU に合わせて書き直したもの。
    経緯と番地の根拠: docs/research/2026-09-19_bw-start-menu.md

    ## 素 FRLG との関係（functionrewrites 2 本・元と同じ）

      ShowStartMenu（BPRJ 0806EA4C・START を押したとき）
        → BwShowStartMenu。通信中とユニオンルームでは素の中身をそのまま再現して
          素のスタートメニュー（CFRU の start_menu.c の hooks が効く）を出す。
          それ以外は画面を丸ごとこのメニューにする（重ね窓ではなく全画面）。
      CB2_ReturnToFieldWithOpenMenu（BPRJ 08056168・小メニューから戻るとき）
        → BwReturnToFieldWithOpenMenu。カーソル位置を Var8000/8001 から戻して
          このメニューを開き直す。通信中は素の中身を再現。

    ★ どちらも 20 バイト以上の関数で、潰す範囲に別の関数の入口は無い
      （tools/bwstartmenu/verify_bw_start_menu.py が毎回見る。6.22）。

    ## CFRU 側との関係

      - CFRU の start_menu.c（SetUpStartMenu のフック・時計・PokeTools の小メニュー）は
        **素のメニューの中**で動くものなので、全画面のこのメニューでは通らない。
        時計はここで出す（gClock は同じ）。DexNav は選択肢として**この表に足した**
        （FLAG_SYS_DEXNAV とずかんの旗が立っているとき）。
      - 状態の入れ物のポインタは 0x203E038（CFRU が sDexNavStatePtr / gNewBS と
        兼用している「大きな画面の状態」用の語。メニューを開いている間はどちらも
        使われない。DexNav へ行くときは先にこちらを Free してから渡す）

    ## 素の JP 版の番地（BPRJ.ld に無いもの。verify が毎回確かめる）
      CB2_OpenPokedexFromStartMenu 081034E4 / CB2_PartyMenuFromStartMenu 081277E8 / CB2_BagMenuFromStartMenu 081089BC /
      CB2_OptionsMenuFromStartMenu 08087F6C / ShowPlayerTrainerCard 0808B16C /
      CB2_ReturnToFieldFadeFromBlack 080561BC / CreateStartMenuTask 0806E97C（引数 0806E9E5）/
      SafariZoneRetirePrompt 080A2210 / DecompressAndCopyTileDataToVram 080F78D0 /
      FreeTempTileDataBuffersIfPossible 080F7884 / FieldCB_ReturnToFieldOpenStartMenu 0807DAEC /
      gHelpSystemEnabled 03005F1C / gNumSafariBalls 0203990C / gSafariZoneStepCounter 0203990E

    ## 状態を持たない
    書き込み可能な static は無い（6.13）。状態は Calloc した入れ物（EWRAM）。
*/

/* ---- 素の関数（BPRJ.ld に無いもの）---- */
#define BwSm_CB2_OpenPokedexFromStartMenu ((MainCallback)(0x081034E4 + 1))
#define BwSm_CB2_PartyMenuFromStartMenu   ((MainCallback)(0x081277E8 + 1))
#define BwSm_CB2_BagMenuFromStartMenu     ((MainCallback)(0x081089BC + 1))
#define BwSm_CB2_ReturnToFieldFadeFromBlack ((MainCallback)(0x080561BC + 1))
#define BwSm_ShowPlayerTrainerCard ((void (*)(MainCallback))(0x0808B16C + 1))

/* ★ フロンティアパス（やりたいこと #18・src/bw_frontier_pass.c）。
     `0x5C`（ミナモへ行ける＝ななしまクリア）が立つと、この項目が
     素のトレーナーカードから**こちらに入れ替わる**。
     フラグの番号は生成物から取る（船と条件を食い違わせないため）。 */
void CB2_BwFrontierPass(void);
#define BwSm_CreateStartMenuTask ((void (*)(void *))(0x0806E97C + 1))
#define BwSm_VanillaStartMenuTaskFunc ((void *)(0x0806E9E4 + 1))
#define BwSm_SafariZoneRetirePrompt ((void (*)(void))(0x080A2210 + 1))
#define BwSm_DecompressAndCopyTileDataToVram \
    ((void (*)(u8, const void *, u32, u16, u8))(0x080F78D0 + 1))
#define BwSm_FreeTempTileDataBuffersIfPossible ((bool8 (*)(void))(0x080F7884 + 1))
#define BwSm_FieldCB_ReturnToFieldOpenStartMenu ((void *)(0x0807DAEC + 1))
#define BwSm_FieldClearVBlankHBlankCallbacks ((void (*)(void))(0x0805627C + 1))
#define gHelpSystemEnabled (*((bool8 *)0x03005F1C))
#define gNumSafariBalls (*((u8 *)0x0203990C))
#define gSafariZoneStepCounter (*((u16 *)0x0203990E))
#define gFieldCallback2Ptr (*((void **)0x03005064))

extern bool8 StartMenuDexNavCallback(void);    /* src/dexnav.c */
extern bool8 StartMenuAchievementsCallback(void);    /* src/achievements.c（2026-10-10 じっせき） */
extern void CB2_BwOptionMenu(void);            /* src/bw_option_menu.c */
void __attribute__((long_call)) ScanlineEffect_Stop(void);      /* BPRJ.ld 08087A48 */
bool8 __attribute__((long_call)) InUnionRoom(void);             /* BPRJ.ld 0811B914 */

/* ---- 絵（graphics/BwStartMenu/ から grit が作る）---- */
extern const u8 BwSmBgTiles[];
extern const u8 BwSmBgMap[];
extern const u16 BwSmBgPal[];
extern const u8 BwSmPanelTiles[];
extern const u16 BwSmPanelPal[];
extern const u8 BwSmExitTiles[];
extern const u16 BwSmExitPal[];
extern const u8 BwSmScrollbarTiles[];
extern const u16 BwSmScrollbarPal[];
extern const u8 BwSmPokedexTiles[];
extern const u16 BwSmPokedexPal[];
extern const u8 BwSmPokemonTiles[];
extern const u16 BwSmPokemonPal[];
extern const u8 BwSmBagTiles[];
extern const u16 BwSmBagPal[];
extern const u8 BwSmPlayerTiles[];
extern const u16 BwSmPlayerPal[];
extern const u8 BwSmSaveTiles[];
extern const u16 BwSmSavePal[];
extern const u8 BwSmOptionsTiles[];
extern const u16 BwSmOptionsPal[];
extern const u8 BwSmDexnavTiles[];
extern const u16 BwSmDexnavPal[];
extern const u8 BwSmAchievementsTiles[];
extern const u16 BwSmAchievementsPal[];
extern const u8 BwSmPhoneTiles[];
extern const u16 BwSmPhonePal[];
extern const u8 gText_BwSm_Phone[];
extern void CB2_HackPhoneBook(void);    /* src/hack_phone.c（2026-10-11 でんわ） */

/* ---- 文字列（strings/bw_start_menu.string）---- */
extern const u8 gText_BwSm_Pokedex[];
extern const u8 gText_BwSm_Pokemon[];
extern const u8 gText_BwSm_Bag[];
extern const u8 gText_BwSm_Save[];
extern const u8 gText_BwSm_Option[];
extern const u8 gText_BwSm_Retire[];
extern const u8 gText_BwSm_DexNav[];
extern const u8 gText_BwSm_Achievements[];
extern const u8 gText_BwSm_Sun[];
extern const u8 gText_BwSm_Mon[];
extern const u8 gText_BwSm_Tue[];
extern const u8 gText_BwSm_Wed[];
extern const u8 gText_BwSm_Thu[];
extern const u8 gText_BwSm_Fri[];
extern const u8 gText_BwSm_Sat[];
extern const u8 gText_BwSm_AM[];
extern const u8 gText_BwSm_PM[];
/* ★ 空白とコロンは文字列ファイルに書かない。CFRU の文字列コンパイラは行の空白を削るので
   「 」は空の文字列になり、ラベルが次の文字列を指してしまう（2026-09-19 実機で
   時計の欄に「ボール …/」が出た）。文字コードは charmap.tbl より（空白 0x00・コロン 0xF0）*/
static const u8 sText_BwSm_Space[] = {0x00, EOS};
static const u8 sText_BwSm_Colon[] = {0xF0, EOS};
extern const u8 gText_BwSm_SafariStats[];

/* ---- スクリプト（assembly/bwstartmenu/bw_start_menu_scripts.s）---- */
extern const u8 BwSmScript_SaveGame[];
extern const u8 BwSmScript_Retire[];

/* ---- 置き方（元と同じ）---- */
#define PANEL_X 34
#define PANEL_Y 41
#define HSPACING 20
#define VSPACING 6
#define BWSM_FONT 2                      /* 日本語の標準フォント（CFRU の時計と同じ）*/
#define BWSM_ON_SCREEN 6                 /* 2 列 x 3 段 */
#define BWSM_STATE_PTR 0x203E038         /* CFRU の sDexNavStatePtr / gNewBS と兼用の語 */

enum
{
    GFXTAG_PANEL = 0x7B00,               /* 他の画面のタグとぶつからないよう大きめ */
    GFXTAG_EXIT,
    GFXTAG_POKEDEX,
    GFXTAG_POKEMON,
    GFXTAG_BAG,
    GFXTAG_PLAYER,
    GFXTAG_SAVE,
    GFXTAG_OPTIONS,
    GFXTAG_SCROLLBAR,
    GFXTAG_RETIRE,
    GFXTAG_DEXNAV,
    GFXTAG_ACHIEVEMENTS,
    GFXTAG_PHONE,             /* 2026-10-11 でんわ（src/hack_phone.c） */
};

enum
{
    BWSM_POKEDEX,
    BWSM_POKEMON,
    BWSM_BAG,
    BWSM_PLAYER,
    BWSM_SAVE,
    BWSM_OPTION,
    BWSM_DEXNAV,
    BWSM_ACHIEVEMENTS,      /* 2026-10-10 じっせき（src/achievements.c） */
    BWSM_PHONE,             /* 2026-10-11 でんわ（pm-start/poke-call・src/hack_phone.c） */
    BWSM_RETIRE,
    BWSM_MAX_ITEMS,
};

enum
{
    WIN_TOPBAR_TIME,
    WIN_TOPBAR,
    WIN_ITEMS,
    WIN_BOTTOMBAR,
    WIN_COUNT,
};

enum
{
    BG_TEXT,
    BG_1,
    BG_2,
    BG_BACKGROUND,
};

struct BwSmState
{
    u8 cursor;
    u8 scroll;
    u8 numItems;
    u8 numOnScreen;
    u8 items[BWSM_MAX_ITEMS];
    u8 onScreen[BWSM_ON_SCREEN];
    u8 iconSpriteIds[BWSM_ON_SCREEN];
    u8 *bgTilemapBuffer;
    u8 runItem;             /* 0 でなければ 番号 + 1 を選んだことにして動かす（SELECT でレポート・L でずかんナビ・2026-10-05）*/
};

#define sState (*((struct BwSmState **)BWSM_STATE_PTR))

struct BwSmOption
{
    const u8 *text;         /* NULL なら主人公の名前 */
    u16 flag;               /* 0 でなければ、立っているときだけ出す */
    const u8 *script;       /* 使うならこれ。フィールドに戻ってから走る */
    MainCallback func;      /* 使うならこれ */
    const struct SpriteSheet sheet;
    const struct SpritePalette pal;
};

static const struct BgTemplate sBgTemplates[] =
{
    [BG_TEXT] = {.bg = BG_TEXT, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0},
    [BG_1] = {.bg = BG_1, .charBaseIndex = 1, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0},
    [BG_2] = {.bg = BG_2, .charBaseIndex = 2, .mapBaseIndex = 29, .screenSize = 0, .paletteMode = 0, .priority = 2, .baseTile = 0},
    [BG_BACKGROUND] = {.bg = BG_BACKGROUND, .charBaseIndex = 3, .mapBaseIndex = 28, .screenSize = 0, .paletteMode = 0, .priority = 3, .baseTile = 0},
};

static const struct WindowTemplate sWindowTemplates[] =
{
    /* ★ 時計の窓は元の 10 タイルだと日本語フォントで「ど PM 9:05」が切れる（2026-09-19 実機）。
       14 タイルに広げ、右の帯の窓を 16 タイルに詰めた（VRAM: 1+56=57, 57+64=121 < 123）*/
    [WIN_TOPBAR_TIME] = {.bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = 0, .width = 14, .height = 4, .paletteNum = 14, .baseBlock = 1},
    [WIN_TOPBAR] = {.bg = BG_TEXT, .tilemapLeft = 14, .tilemapTop = 0, .width = 16, .height = 4, .paletteNum = 14, .baseBlock = 57},
    [WIN_ITEMS] = {.bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = 4, .width = 30, .height = 12, .paletteNum = 14, .baseBlock = 123},
    [WIN_BOTTOMBAR] = {.bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = 16, .width = 30, .height = 4, .paletteNum = 14, .baseBlock = 484},
    DUMMY_WIN_TEMPLATE,
};

static const struct TextColor sWhiteText = {TEXT_COLOR_TRANSPARENT, TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GREY};

/* ---- スプライト ---- */
static const struct OamData sPanelOam = {.affineMode = ST_OAM_AFFINE_OFF, .objMode = ST_OAM_OBJ_BLEND, .shape = SPRITE_SHAPE(64x64), .size = SPRITE_SIZE(64x64), .priority = 1};
static const struct OamData sExitOam = {.affineMode = ST_OAM_AFFINE_OFF, .objMode = ST_OAM_OBJ_NORMAL, .shape = SPRITE_SHAPE(16x16), .size = SPRITE_SIZE(16x16), .priority = 1};
static const struct OamData sIconOam = {.affineMode = ST_OAM_AFFINE_OFF, .objMode = ST_OAM_OBJ_NORMAL, .shape = SPRITE_SHAPE(32x32), .size = SPRITE_SIZE(32x32), .priority = 1};

static const union AnimCmd sAnimPanelUnselected1[] = {ANIMCMD_FRAME(0, 0), ANIMCMD_END};
static const union AnimCmd sAnimPanelUnselected2[] = {ANIMCMD_FRAME(64, 0), ANIMCMD_END};
static const union AnimCmd sAnimPanelSelected1[] = {ANIMCMD_FRAME(128, 0), ANIMCMD_END};
static const union AnimCmd sAnimPanelSelected2[] = {ANIMCMD_FRAME(192, 0), ANIMCMD_JUMP(0)};
static const union AnimCmd *const sAnimTablePanel1[] = {sAnimPanelUnselected1, sAnimPanelSelected1};
static const union AnimCmd *const sAnimTablePanel2[] = {sAnimPanelUnselected2, sAnimPanelSelected2};
static const union AnimCmd sAnimExit[] = {ANIMCMD_FRAME(0, 0), ANIMCMD_END};
static const union AnimCmd sAnimExitSelected[] = {ANIMCMD_FRAME(4, 0), ANIMCMD_END};
static const union AnimCmd *const sAnimTableExit[] = {sAnimExit, sAnimExitSelected};
static const union AnimCmd sAnimIcon[] = {ANIMCMD_FRAME(0, 0), ANIMCMD_END};
static const union AnimCmd sAnimIconSelected[] = {ANIMCMD_FRAME(16, 0), ANIMCMD_END};
static const union AnimCmd *const sAnimTableIcon[] = {sAnimIcon, sAnimIconSelected};
static const union AnimCmd sAnimScrollbar[] = {ANIMCMD_FRAME(0, 0), ANIMCMD_END};
static const union AnimCmd *const sAnimTableScrollbar[] = {sAnimScrollbar};

static void PanelCallback(struct Sprite *sprite);
static void IconCallback(struct Sprite *sprite);
static void ScrollbarCallback(struct Sprite *sprite);

static const struct SpriteSheet sPanelSheet = {BwSmPanelTiles, (64 * 64 * 4) / 2, GFXTAG_PANEL};
static const struct SpritePalette sPanelPal = {BwSmPanelPal, GFXTAG_PANEL};
static const struct SpriteTemplate sPanel1Template = {GFXTAG_PANEL, GFXTAG_PANEL, &sPanelOam, sAnimTablePanel1, NULL, gDummySpriteAffineAnimTable, PanelCallback};
static const struct SpriteTemplate sPanel2Template = {GFXTAG_PANEL, GFXTAG_PANEL, &sPanelOam, sAnimTablePanel2, NULL, gDummySpriteAffineAnimTable, PanelCallback};
static const struct SpriteSheet sExitSheet = {BwSmExitTiles, 16 * 16, GFXTAG_EXIT};
static const struct SpritePalette sExitPal = {BwSmExitPal, GFXTAG_EXIT};
static const struct SpriteTemplate sExitTemplate = {GFXTAG_EXIT, GFXTAG_EXIT, &sExitOam, sAnimTableExit, NULL, gDummySpriteAffineAnimTable, PanelCallback};
static const struct SpriteSheet sScrollbarSheet = {BwSmScrollbarTiles, 32 * 16, GFXTAG_SCROLLBAR};
static const struct SpritePalette sScrollbarPal = {BwSmScrollbarPal, GFXTAG_SCROLLBAR};
static const struct SpriteTemplate sScrollbarTemplate = {GFXTAG_SCROLLBAR, GFXTAG_SCROLLBAR, &sIconOam, sAnimTableScrollbar, NULL, gDummySpriteAffineAnimTable, ScrollbarCallback};

#define ICON(tag) {tag, tag, &sIconOam, sAnimTableIcon, NULL, gDummySpriteAffineAnimTable, IconCallback}
static const struct SpriteTemplate sIconTemplates[BWSM_MAX_ITEMS] =
{
    [BWSM_POKEDEX] = ICON(GFXTAG_POKEDEX),
    [BWSM_POKEMON] = ICON(GFXTAG_POKEMON),
    [BWSM_BAG] = ICON(GFXTAG_BAG),
    [BWSM_PLAYER] = ICON(GFXTAG_PLAYER),
    [BWSM_SAVE] = ICON(GFXTAG_SAVE),
    [BWSM_OPTION] = ICON(GFXTAG_OPTIONS),
    [BWSM_DEXNAV] = ICON(GFXTAG_DEXNAV),
    [BWSM_ACHIEVEMENTS] = ICON(GFXTAG_ACHIEVEMENTS),
    [BWSM_PHONE] = ICON(GFXTAG_PHONE),
    [BWSM_RETIRE] = {GFXTAG_RETIRE, GFXTAG_RETIRE, &sExitOam, sAnimTableExit, NULL, gDummySpriteAffineAnimTable, PanelCallback},
};

static void BwSm_PlayerCard(void);
static void BwSm_Options(void);
static void BwSm_DexNav(void);
static void BwSm_Achievements(void);
static void BwSm_Phone(void);

static const struct BwSmOption sOptions[BWSM_MAX_ITEMS] =
{
    [BWSM_POKEDEX] = {gText_BwSm_Pokedex, FLAG_SYS_POKEDEX_GET, NULL, BwSm_CB2_OpenPokedexFromStartMenu,
                      {BwSmPokedexTiles, 32 * 32, GFXTAG_POKEDEX}, {BwSmPokedexPal, GFXTAG_POKEDEX}},
    [BWSM_POKEMON] = {gText_BwSm_Pokemon, FLAG_SYS_POKEMON_GET, NULL, BwSm_CB2_PartyMenuFromStartMenu,
                      {BwSmPokemonTiles, 32 * 32, GFXTAG_POKEMON}, {BwSmPokemonPal, GFXTAG_POKEMON}},
    [BWSM_BAG] = {gText_BwSm_Bag, 0, NULL, BwSm_CB2_BagMenuFromStartMenu,
                  {BwSmBagTiles, 32 * 32, GFXTAG_BAG}, {BwSmBagPal, GFXTAG_BAG}},
    [BWSM_PLAYER] = {NULL, 0, NULL, BwSm_PlayerCard,
                     {BwSmPlayerTiles, 32 * 32, GFXTAG_PLAYER}, {BwSmPlayerPal, GFXTAG_PLAYER}},
    [BWSM_SAVE] = {gText_BwSm_Save, 0, BwSmScript_SaveGame, NULL,
                   {BwSmSaveTiles, 32 * 32, GFXTAG_SAVE}, {BwSmSavePal, GFXTAG_SAVE}},
    [BWSM_OPTION] = {gText_BwSm_Option, 0, NULL, BwSm_Options,
                     {BwSmOptionsTiles, 32 * 32, GFXTAG_OPTIONS}, {BwSmOptionsPal, GFXTAG_OPTIONS}},
    [BWSM_DEXNAV] = {gText_BwSm_DexNav, FLAG_SYS_DEXNAV, NULL, BwSm_DexNav,
                     {BwSmDexnavTiles, 32 * 32, GFXTAG_DEXNAV}, {BwSmDexnavPal, GFXTAG_DEXNAV}},
    [BWSM_ACHIEVEMENTS] = {gText_BwSm_Achievements, 0, NULL, BwSm_Achievements,
                           {BwSmAchievementsTiles, 32 * 32, GFXTAG_ACHIEVEMENTS}, {BwSmAchievementsPal, GFXTAG_ACHIEVEMENTS}},
    [BWSM_PHONE] = {gText_BwSm_Phone, 0, NULL, BwSm_Phone,
                    {BwSmPhoneTiles, 32 * 32, GFXTAG_PHONE}, {BwSmPhonePal, GFXTAG_PHONE}},
    [BWSM_RETIRE] = {gText_BwSm_Retire, FLAG_SYS_SAFARI_MODE, BwSmScript_Retire, NULL,
                     {BwSmExitTiles, 16 * 16, GFXTAG_RETIRE}, {BwSmExitPal, GFXTAG_RETIRE}},
};

static const u8 *const sDayNames[] =
{
    gText_BwSm_Sun, gText_BwSm_Mon, gText_BwSm_Tue, gText_BwSm_Wed, gText_BwSm_Thu, gText_BwSm_Fri, gText_BwSm_Sat,
};

/* ---- 前方宣言 ---- */
static void CB2_BwStartMenu(void);
void HackTrainingPouch_Reset(void);   /* src/hack_training_pouch.c */
static void Task_FadeIn(u8 taskId);
static void Task_WaitForKeyPress(u8 taskId);
static void Task_FadeOut(u8 taskId);
static void Task_RunOption(u8 taskId);

/* ================================================================ */

static void ClearVramOamPlttRegs(void)
{
    DmaFill16(3, 0, (void *)VRAM, VRAM_SIZE);
    DmaFill32(3, 0, (void *)OAM, OAM_SIZE);
    DmaFill16(3, 0, (void *)PLTT, PLTT_SIZE);
    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BG0CNT, 0);
    SetGpuReg(REG_OFFSET_BG0HOFS, 0);
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG1CNT, 0);
    SetGpuReg(REG_OFFSET_BG1HOFS, 0);
    SetGpuReg(REG_OFFSET_BG1VOFS, 0);
    SetGpuReg(REG_OFFSET_BG2CNT, 0);
    SetGpuReg(REG_OFFSET_BG2HOFS, 0);
    SetGpuReg(REG_OFFSET_BG2VOFS, 0);
    SetGpuReg(REG_OFFSET_BG3CNT, 0);
    SetGpuReg(REG_OFFSET_BG3HOFS, 0);
    SetGpuReg(REG_OFFSET_BG3VOFS, 0);
    SetGpuReg(REG_OFFSET_WIN0H, 0);
    SetGpuReg(REG_OFFSET_WIN0V, 0);
    SetGpuReg(REG_OFFSET_WININ, 0);
    SetGpuReg(REG_OFFSET_WINOUT, 0);
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    SetGpuReg(REG_OFFSET_BLDALPHA, 0);
    SetGpuReg(REG_OFFSET_BLDY, 0);
}

static void ClearTasksAndGraphicalStructs(void)
{
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    ResetTempTileDataBuffers();
    ResetPaletteFade();
    FreeAllSpritePalettes();
}

static void VBlankCB_BwStartMenu(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void MainCB2_BwStartMenu(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

/* ---- 選択肢の組み立て ---- */

static void ConfigureOnScreen(void)
{
    u8 i, n = 0;

    for (i = 0; i < BWSM_ON_SCREEN; ++i)
    {
        if (i + 2 * sState->scroll >= sState->numItems)
            break;
        sState->onScreen[i] = sState->items[i + sState->scroll * 2];
        ++n;
    }
    sState->numOnScreen = n;
}

static void BuildItems(void)
{
    u8 i, n = 0;
    bool8 safari = FlagGet(FLAG_SYS_SAFARI_MODE);

    for (i = 0; i < BWSM_MAX_ITEMS; ++i)
    {
        if (sOptions[i].flag != 0 && !FlagGet(sOptions[i].flag))
            continue;
        if (safari && (i == BWSM_SAVE || i == BWSM_DEXNAV))
            continue;
        if (i == BWSM_DEXNAV && !FlagGet(FLAG_SYS_POKEDEX_GET))
            continue;
        sState->items[n++] = i;
    }
    sState->numItems = n;
    if (sState->scroll > 0 && 2 * sState->scroll + BWSM_ON_SCREEN > n + 1)
        sState->scroll = 0;
    ConfigureOnScreen();
    if (sState->cursor >= sState->numOnScreen)
        sState->cursor = 0;
}

/* ---- 描く ---- */

static void LoadBgGfx(void)
{
    BwSm_DecompressAndCopyTileDataToVram(BG_BACKGROUND, BwSmBgTiles, 0, 0, 0);
    LZDecompressWram(BwSmBgMap, sState->bgTilemapBuffer);
    LoadPalette(BwSmBgPal, 0, 0x20);
    LoadPalette(stdpal_get(0), 14 * 16, 0x20);      /* 文字の色（白・濃い灰） */
}

static void CleanWindow(u8 windowId)
{
    FillWindowPixelBuffer(windowId, PIXEL_FILL(0));
}

static void CommitWindow(u8 windowId)
{
    PutWindowTilemap(windowId);
    CopyWindowToVram(windowId, COPYWIN_BOTH);
}

static void PrintTime(void)
{
    u8 hour = gClock.hour;
    u8 *p = gStringVar4;

    p = StringCopy(p, sDayNames[gClock.dayOfWeek % 7]);
    p = StringCopy(p, sText_BwSm_Space);
    p = StringCopy(p, (hour >= 12) ? gText_BwSm_PM : gText_BwSm_AM);
    p = StringCopy(p, sText_BwSm_Space);
    hour %= 12;
    if (hour == 0)
        hour = 12;
    p = ConvertIntToDecimalStringN(p, hour, STR_CONV_MODE_LEFT_ALIGN, 2);   /* 頭の空白を入れない */
    p = StringCopy(p, sText_BwSm_Colon);
    p = ConvertIntToDecimalStringN(p, gClock.minute, STR_CONV_MODE_LEADING_ZEROS, 2);
    *p = EOS;
    WindowPrint(WIN_TOPBAR_TIME, BWSM_FONT, 3, 0, &sWhiteText, 0, gStringVar4);
}

static void PrintSafariStats(void)
{
    ConvertIntToDecimalStringN(gStringVar1, gNumSafariBalls, STR_CONV_MODE_RIGHT_ALIGN, 2);
    ConvertIntToDecimalStringN(gStringVar2, gSafariZoneStepCounter, STR_CONV_MODE_RIGHT_ALIGN, 3);
    StringExpandPlaceholders(gStringVar4, gText_BwSm_SafariStats);
    /* 窓は 16 タイル（128 画素）。日本語フォントは広いので空白を詰めて左端から（2026-09-19 実機で切れた）*/
    WindowPrint(WIN_TOPBAR, BWSM_FONT, 0, 0, &sWhiteText, 0, gStringVar4);
}

static void PrintMapName(void)
{
    GetMapName(gStringVar4, GetCurrentRegionMapSectionId(), 0);
    WindowPrint(WIN_BOTTOMBAR, BWSM_FONT, 2, 13, &sWhiteText, 0, gStringVar4);
}

static void PrintItemNames(void)
{
    u8 row, col, n = 0;

    for (row = 0; row < 3 && n < sState->numOnScreen; ++row)
    {
        for (col = 0; col < 2 && n < sState->numOnScreen; ++col)
        {
            u16 x = (PANEL_X + (HSPACING + 64 / 2 + 64) * col) + 8;
            u16 y = (PANEL_Y + (VSPACING + 32) * row) - 40;
            u8 id = sState->onScreen[n];
            /* ★ 名前が NULL の枠は主人公の名前。フロンティアパスに
                 入れ替わったあとも名前のまま（ユーザー判断 2026-09-23）。 */
            const u8 *text = sOptions[id].text;

            if (text == NULL)
                text = gSaveBlock2->playerName;

            WindowPrint(WIN_ITEMS, BWSM_FONT, x, y, &sWhiteText, 0, text);
            ++n;
        }
    }
}

static void DrawPanels(void)
{
    u8 row, col, n = 0, spriteId;

    LoadSpriteSheet(&sPanelSheet);
    LoadSpritePalette(&sPanelPal);
    for (row = 0; row < 3; ++row)
    {
        for (col = 0; col < 2; ++col)
        {
            u16 x = PANEL_X + (HSPACING + 64 / 2 + 62) * col;
            u16 y = PANEL_Y + (VSPACING + 32) * row;
            u8 id1 = CreateSprite(&sPanel1Template, x, y, 0);
            u8 id2 = CreateSprite(&sPanel2Template, x + 64, y, 0);

            gSprites[id1].data[0] = n;
            gSprites[id2].data[0] = n;
            ++n;
        }
    }
    LoadSpritePalette(&sExitPal);
    LoadSpriteSheet(&sExitSheet);
    spriteId = CreateSprite(&sExitTemplate, 240 - 16, 160 - 11, 0);
    gSprites[spriteId].data[0] = 0xFF;
}

static void DrawIcons(void)
{
    u8 row, col, n = 0;

    for (row = 0; row < 3 && n < sState->numOnScreen; ++row)
    {
        for (col = 0; col < 2 && n < sState->numOnScreen; ++col)
        {
            u16 x = (PANEL_X - 11) + (HSPACING - 2 + 64 / 2 + 64) * col;
            u16 y = (PANEL_Y - 3) + (VSPACING + 32) * row;
            u8 id = sState->onScreen[n];
            u8 spriteId;

            LoadSpriteSheet(&sOptions[id].sheet);
            LoadSpritePalette(&sOptions[id].pal);
            spriteId = CreateSprite(&sIconTemplates[id], x, y, 0);
            gSprites[spriteId].data[0] = n;
            gSprites[spriteId].data[1] = 3;
            gSprites[spriteId].data[2] = y;
            gSprites[spriteId].data[3] = -1;
            sState->iconSpriteIds[n] = spriteId;
            ++n;
        }
    }
}

static void CreateScrollbar(void)
{
    LoadSpriteSheet(&sScrollbarSheet);
    LoadSpritePalette(&sScrollbarPal);
    CreateSprite(&sScrollbarTemplate, 240 - 5, 48, 0);
}

static void InitGui(void)
{
    u8 i;

    for (i = 0; i < WIN_COUNT; ++i)
        CleanWindow(i);
    DrawPanels();
    DrawIcons();
    PrintMapName();
    PrintItemNames();
    if (FlagGet(FLAG_SYS_SAFARI_MODE))
        PrintSafariStats();
    PrintTime();
    CreateScrollbar();
    for (i = 0; i < WIN_COUNT; ++i)
        CommitWindow(i);
}

static void RefreshOptions(void)
{
    u8 i;

    CleanWindow(WIN_ITEMS);
    for (i = 0; i < sState->numOnScreen; ++i)
        DestroySpriteAndFreeResources(&gSprites[sState->iconSpriteIds[i]]);
    ConfigureOnScreen();
    DrawIcons();
    PrintItemNames();
    CommitWindow(WIN_ITEMS);
}

/* ---- スプライトの動き ---- */

static void PanelCallback(struct Sprite *sprite)
{
    if (sprite->data[0] >= sState->numOnScreen && sprite->data[0] != 0xFF)
        sprite->invisible = TRUE;
    else
        sprite->invisible = FALSE;
    StartSpriteAnimIfDifferent(sprite, sprite->data[0] == sState->cursor ? 1 : 0);
}

static void IconCallback(struct Sprite *sprite)
{
    if (sprite->data[0] == sState->cursor)
    {
        StartSpriteAnim(sprite, 1);
        if (sprite->data[4] % 5 == 0)
        {
            if (sprite->data[1] == 0)
                sprite->data[3] = 1;
            else if (sprite->data[1] == 3)
                sprite->data[3] = -1;
            sprite->data[1] += sprite->data[3];
            sprite->pos1.y = sprite->data[2] + sprite->data[1];
        }
        sprite->data[4]++;
    }
    else
    {
        sprite->pos1.y = sprite->data[2];
        StartSpriteAnim(sprite, 0);
    }
}

static void ScrollbarCallback(struct Sprite *sprite)
{
    s16 pages = sState->numItems / 2 - 3 + sState->numItems % 2;    /* 送れる回数 */
    s16 y;

    if (pages <= 0)
    {
        sprite->invisible = TRUE;
        return;
    }
    y = 48 + (62 / pages) * sState->scroll;
    if (sState->scroll == pages)
        y = 48 + 62;
    sprite->invisible = FALSE;
    sprite->pos1.y = y;
}

/* ---- 画面の流れ ---- */

static void CB2_BwStartMenu(void)
{
    switch (gMain.state)
    {
    case 0:
    default:
        HackTrainingPouch_Reset();   /* ★ 2026-10-06 いくせいポーチの印を下ろす（バッグを閉じるとここへ戻るので、次に開くバッグは ふだんの表示・ユーザー「バッグをとじてもいくせいポーチのまま」） */
        SetBGMVolume_SuppressHelpSystemReduction(160);
        SetVBlankCallback(NULL);
        ClearVramOamPlttRegs();
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_BG_ALL_ON | DISPCNT_OBJ_ON);
        gMain.state++;
        break;
    case 1:
        ClearTasksAndGraphicalStructs();
        gMain.state++;
        break;
    case 2:
        sState->bgTilemapBuffer = Malloc(0x1000);
        ResetBgsAndClearDma3BusyFlags(0);
        InitBgsFromTemplates(0, sBgTemplates, NELEMS(sBgTemplates));
        SetBgTilemapBuffer(BG_BACKGROUND, sState->bgTilemapBuffer);
        gMain.state++;
        break;
    case 3:
        BuildItems();
        LoadBgGfx();
        gMain.state++;
        break;
    case 4:
        if (!BwSm_FreeTempTileDataBuffersIfPossible())
        {
            ShowBg(BG_TEXT);
            ShowBg(BG_BACKGROUND);
            CopyBgTilemapBufferToVram(BG_BACKGROUND);
            gMain.state++;
        }
        break;
    case 5:
        Free(sState->bgTilemapBuffer);
        sState->bgTilemapBuffer = NULL;
        InitWindows(sWindowTemplates);
        DeactivateAllTextPrinters();
        gMain.state++;
        break;
    case 6:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    case 7:
        SetVBlankCallback(VBlankCB_BwStartMenu);
        InitGui();
        CreateTask(Task_FadeIn, 0);
        SetMainCallback2(MainCB2_BwStartMenu);
        gMain.state = 0;
        break;
    }
}

static void Task_FadeIn(u8 taskId)
{
    if (!gPaletteFade->active)
        gTasks[taskId].func = Task_WaitForKeyPress;
}

static void Task_WaitForKeyPress(u8 taskId)
{
    u8 cursor = sState->cursor;
    s16 pages = sState->numItems / 2 - 3 + sState->numItems % 2;

    if (gClock.second == 0)
    {
        CleanWindow(WIN_TOPBAR_TIME);
        PrintTime();
        CommitWindow(WIN_TOPBAR_TIME);
    }

    if (JOY_NEW(B_BUTTON | START_BUTTON))
    {
        sState->cursor = 0xFF;      /* 出口の印を選択中の色に */
        PlaySE(SE_PC_OFF);
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_FadeOut;
    }
    else if (JOY_NEW(SELECT_BUTTON | L_BUTTON))
    {
        /* ★ 2026-10-05 ショートカット: SELECT＝レポート（ユーザー「レポートのショートカットキー作成 メニュー開いたら」）・
           L＝ずかんナビ（ユーザー「図鑑ナビもショートカットできるように」「図鑑ナビLで」）。いつもの流れ（確認など）を通る。
           その項目が一覧に無いとき（サファリ・図鑑をもらう前など）は何もしない */
        u8 i, want = JOY_NEW(SELECT_BUTTON) ? BWSM_SAVE : BWSM_DEXNAV;
        for (i = 0; i < sState->numItems && sState->items[i] != want; ++i)
            ;
        if (i < sState->numItems)
        {
            PlaySE(SE_SELECT);
            VarSet(0x8000, cursor);
            VarSet(0x8001, sState->scroll);
            sState->runItem = want + 1;
            BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
            gTasks[taskId].func = Task_RunOption;
        }
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        VarSet(0x8000, cursor);
        VarSet(0x8001, sState->scroll);
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_RunOption;
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        if (cursor + 1 < sState->numOnScreen && cursor % 2 == 0)
        {
            PlaySE(SE_SELECT);
            sState->cursor = cursor + 1;
        }
    }
    else if (JOY_NEW(DPAD_LEFT))
    {
        if (cursor >= 1 && cursor % 2 == 1)
        {
            PlaySE(SE_SELECT);
            sState->cursor = cursor - 1;
        }
    }
    else if (JOY_NEW(DPAD_UP))
    {
        if (cursor > 1 && cursor < 4 && sState->scroll > 0)
        {
            PlaySE(SE_SELECT);
            sState->scroll--;
            RefreshOptions();
        }
        else if (cursor >= 2)
        {
            PlaySE(SE_SELECT);
            sState->cursor = cursor - 2;
        }
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        if (cursor > 1 && cursor < 4 && sState->scroll < pages)
        {
            PlaySE(SE_SELECT);
            sState->scroll++;
            RefreshOptions();
        }
        else if (cursor + 2 < sState->numOnScreen)
        {
            PlaySE(SE_SELECT);
            sState->cursor = cursor + 2;
        }
    }
}

static void FreeAndClose(u8 taskId)
{
    Free(sState);
    sState = NULL;
    FreeAllWindowBuffers();
    BGMVolumeMax_EnableHelpSystemReduction();
    DestroyTask(taskId);
}

static void Task_FadeOut(u8 taskId)
{
    if (!gPaletteFade->active)
    {
        gHelpSystemEnabled = TRUE;
        SetMainCallback2(BwSm_CB2_ReturnToFieldFadeFromBlack);
        FreeAndClose(taskId);
    }
}

static void Task_RunOption(u8 taskId)
{
    if (!gPaletteFade->active)
    {
        const struct BwSmOption *opt = &sOptions[sState->runItem != 0 ? sState->runItem - 1 : sState->onScreen[sState->cursor]];

        FreeAndClose(taskId);       /* ★ 先に自分を片づける（DexNav が同じ語を使う） */
        if (opt->func != NULL)
            SetMainCallback2(opt->func);
        else
        {
            ScriptContext1_SetupScript(opt->script);
            SetMainCallback2(BwSm_CB2_ReturnToFieldFadeFromBlack);
        }
    }
}

/* ---- 選択肢の飛び先 ---- */

static void BwSm_PlayerCard(void)
{
    /* ★ ホウエンへ行けるようになったらフロンティアパス、それまでは素のカード。
         素のカードには 1 バイトも触らない（#18 の方針）。 */
    if (FlagGet(HOENN_FLAG_CAN_GO_HOENN))
    {
        SetMainCallback2(CB2_BwFrontierPass);
        gMain.savedCallback = CB2_ReturnToFieldWithOpenMenu;
        return;
    }
    BwSm_ShowPlayerTrainerCard(CB2_ReturnToFieldWithOpenMenu);
}

static void BwSm_Options(void)
{
    /* BW 風の「せってい」（src/bw_option_menu.c）。戻り先は savedCallback */
    SetMainCallback2(CB2_BwOptionMenu);
    gMain.savedCallback = CB2_ReturnToFieldWithOpenMenu;
}

static void BwSm_DexNav(void)
{
    /* CFRU の素のメニュー用の入口をそのまま使う（フェードは済んでいるので 1 回で通る） */
    StartMenuDexNavCallback();
}

static void BwSm_Phone(void)
{
    /* 電話帳（src/hack_phone.c・pm-start/poke-call）。B でこのメニューへ戻る・かけるとフィールドで台本 */
    SetMainCallback2(CB2_HackPhoneBook);
}

static void BwSm_Achievements(void)
{
    /* じっせきの画面（src/achievements.c・CFRU 用の入口をそのまま使う。戻りは CB2_ReturnToFieldWithOpenMenu＝このメニュー） */
    StartMenuAchievementsCallback();
}

void BwSmSafariRetire(void)
{
    BwSm_SafariZoneRetirePrompt();
}

/* ---- functionrewrites の入口 2 本 ---- */

static bool8 UseVanillaStartMenu(void)
{
    return IsUpdateLinkStateCBActive() || InUnionRoom() == TRUE;
}

void BwFollower_FinishBallNow(void); //src/bw_follower.c

/* ShowStartMenu（0806EA4C）の差し替え。START を押したときに呼ばれる */
void BwShowStartMenu(void)
{
    if (UseVanillaStartMenu())
    {
        /* 素の中身をそのまま（逆アセンブル 0806EA4C〜） */
        if (!IsUpdateLinkStateCBActive())
        {
            FreezeEventObjects();
            HandleEnforcedLookDirectionOnPlayerStopMoving();
            StopPlayerAvatar();
        }
        BwSm_CreateStartMenuTask(BwSm_VanillaStartMenuTaskFunc);
        ScriptContext2_Enable();
        return;
    }

    if (gPaletteFade->active)
        return;
    BwFollower_FinishBallNow(); //2026-10-03 連れ歩きのボールの演出を終わらせてから全画面へ（src/bw_follower.c）
    ResetBgsAndClearDma3BusyFlags(0);
    gHelpSystemEnabled = FALSE;
    CleanupOverworldWindowsAndTilemaps();
    sState = Calloc(sizeof(struct BwSmState));
    PlaySE(SE_PC_LOGON);
    SetMainCallback2(CB2_BwStartMenu);
}

/* CB2_ReturnToFieldWithOpenMenu（08056168）の差し替え。小メニューから戻るとき */
void BwReturnToFieldWithOpenMenu(void)
{
    if (UseVanillaStartMenu())
    {
        /* 素の中身をそのまま（逆アセンブル 08056168〜） */
        BwSm_FieldClearVBlankHBlankCallbacks();
        gFieldCallback2Ptr = BwSm_FieldCB_ReturnToFieldOpenStartMenu;
        CB2_ReturnToField();
        return;
    }

    sState = Calloc(sizeof(struct BwSmState));
    sState->cursor = VarGet(0x8000);
    sState->scroll = VarGet(0x8001);
    SetMainCallback2(CB2_BwStartMenu);
}
