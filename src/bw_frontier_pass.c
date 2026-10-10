#include "defines.h"
#include "../include/bg.h"
#include "../include/event_data.h"
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
#include "../include/constants/songs.h"
#include "../include/gba/macro.h"
#include "../include/new/dns.h"        /* TransferPlttBuffer */
#include "../include/new/frontier.h"
#include "Tables/bw_frontier_pass_gfx.h"
#include "Tables/bw_frontier_pass_bg.h"
#include "Tables/hoenn_script_flags.h"
#include "Tables/johto_flags.h"
#include "Tables/sinnoh_flags.h"

/*
    フロンティアパス（やりたいこと #18）

    設計と決定の経緯: docs/research/2026-09-23_trainer-card-design.md
    絵の出どころ:     docs/research/2026-09-23_frontier-pass-art.md

    ## これは何か

    スタートメニューの主人公の名前の項目は、`0x5C`（ミナモへ行ける＝
    ななしまクリア）が立つと、素のトレーナーカードではなく**この画面**を開く。
    （項目の文字は主人公の名前のまま。ユーザー判断 2026-09-23）

    ★★ **素のトレーナーカードには 1 バイトも触らない。**
      `src/bw_start_menu.c` が呼び先を選ぶだけで、素の画面は無傷。

    ## 画面（2026-09-23 に作り直し。ユーザー「背景が悲しい。
    ## エメラルド参考に。バッジは HGSS を参考に」）

    | 画面 | 絵 |
    |---|---|
    | パス | **日本語版エメラルドのフロンティアパスの背景そのもの**。右上の縮小地図の中身を「3 地方 × 8 個のバッジ入れ」に描き替えた |
    | バッジ（拡大）| **HGSS の「リーグバッジ」をまねた画面**。地方ごとの色の帯・左に地方名の札・枠 8 個 |

    パスで選べるのは 3 つ（上下で動かす。エメラルドと同じく**枠の絵を
    選択中の絵に差し替えて**示す）:
      とじる（右上）/ バッジ（右上の縮小版）/ カード（右下）

    ## BG の割り当て

    | BG | 中身 | タイル | 配置 |
    |---|---|---|---|
    | 0 | 文字 | charBase 3（0xC000〜。窓は 0xE800 の手前＝320 枚まで）| 29 |
    | 1 | バッジ（パスでは 8x8 の縮小版 / 拡大では 16x16）| charBase 2 | 30 |
    | 2 | 背景（パス 540 枚 / 拡大 42 枚。切り替えるたびに積み直す）| charBase 0 | 31 |

    記章は**スプライト**（エメラルドと同じ）。置き場の座標が 8 の倍数に
    揃っていない（タワーは x=20）ので BG のタイルでは置けない。

    ## データの出どころ（3 つで事情が違う）

    | | |
    |---|---|
    | バッジ | `HOENN_FLAG_BADGE1`〜`8`。**ホウエンだけ本物**。ジョウト・シンオウは枠だけ |
    | れんしょう / さいこう | **本物** — `GetBattleTowerStreak()` |
    | ＢＰ / きしょう | **枠だけ** — #5 ができるまで 0 と未獲得のまま |

    ## CFRU との付き合い

    ★ **書き換え可能な static を置かない**（CLAUDE.md 6.13）。
      BG の配置の入れ物は `AllocZeroed` して、ポインタを `0x203E038` に置く
      （スタートメニューが使っている語。スタートメニューは**解放して NULL に
      してから**ここへ渡すので空いている。`src/bw_start_menu.c` の
      `Task_RunOption`）。
    ★★ 文字と背景の色は必ず自分で入れる。`DmaClear16(PLTT)` は本物の
      パレットしか消さず、暗転明けは `gPlttBufferUnfaded` から写される
      （2026-09-23 実機: スタートメニューの色が残って背景が灰色になった）。
    ★★ **BG1 / BG2 は `SetBgTilemapBuffer` を忘れない。** 忘れると
      `FillBgTilemapBufferRect` が何も描かない（2026-09-23 実機:
      バッジも記章も一度も出ていなかった）。
*/

void __attribute__((long_call)) ScanlineEffect_Stop(void);  /* BPRJ.ld 08087A48 */

/* ★ 素のトレーナーカード。`BPRJ.ld` に名前が無いので番地で呼ぶ（6.29 節）。
     src/bw_start_menu.c が使っているのと同じ番地。 */
#define BwFp_ShowPlayerTrainerCard \
    ((void (*)(MainCallback))(0x0808B16C + 1))
/* ★ `include/bg.h` の宣言には long_call が無い（このコードは 0x09000000 台に
     置かれるので BL が届かない）。BPRJ.ld の番地で呼ぶ */
#define BwFp_UnsetBgTilemapBuffer ((void (*)(u8))(0x08001FD4 + 1))

/* ---- 文字列（strings/bw_frontier_pass.string）---- */
extern const u8 gText_BwFp_Title[];
extern const u8 gText_BwFp_Symbols[];
extern const u8 gText_BwFp_Streak[];
extern const u8 gText_BwFp_MaxStreak[];
extern const u8 gText_BwFp_Bp[];
extern const u8 gText_BwFp_Badges[];
extern const u8 gText_BwFp_Hoenn[];
extern const u8 gText_BwFp_Johto[];
extern const u8 gText_BwFp_Sinnoh[];
extern const u8 gText_BwFp_HintPass[];
extern const u8 gText_BwFp_HintBack[];

#define BWFP_FONT 2             /* 日本語の標準（せってい画面と同じ）*/

/* ---- 状態の入れ物（0x203E038 に置く。上の説明）---- */
struct BwFpState
{
    u16 *bg1Buf;
    u16 *bg2Buf;
};
#define BWFP_STATE_PTR 0x203E038
#define sState (*((struct BwFpState **)BWFP_STATE_PTR))

/* ---- BG ---- */
#define BG_TEXT 0
#define BG_BADGE 1
#define BG_ART 2

/* パレットの置き場。0〜7 はパスの背景（エメラルドの 8 本）/
   拡大画面では 0〜3 が帯 */
#define PAL_BADGE 8
#define PAL_TEXT 15

/* BG1 のタイルの置き場。0 は空白にしておく */
#define TILE_BADGE0 1                                       /* 16x16 が 24 個 */
#define TILE_MINI0 (TILE_BADGE0 + BW_FP_NUM_BADGES * 4)     /* 8x8 が 24 個（97〜）*/

/* ---- 文字の色（パレット 15）---- */
static const u16 sTextPal[16] =
{
    RGB(0, 0, 0),       /* 0 透明 */
    RGB(31, 31, 31),    /* 1 白 */
    RGB(10, 10, 12),    /* 2 濃い灰（クリーム色の地に書く字）*/
    RGB(22, 22, 20),    /* 3 薄い灰（その影）*/
    RGB(8, 16, 26),     /* 4 青（白い字の影）*/
};
static const u8 sColorDark[3] = {0, 2, 3};
static const u8 sColorWhite[3] = {0, 1, 4};

/* ---- 窓（全部 BG0。2 つの画面で同じ組を使い、出す窓だけ切り替える）---- */
enum
{
    /* パス（エメラルドの sPassWindowTemplates と同じ位置）*/
    WIN_SYMBOLS,
    WIN_RECORD,
    WIN_POINTS,
    WIN_HINT,
    /* 拡大画面 */
    WIN_BOARD_TITLE,
    WIN_BOARD_HINT,
    WIN_REGION0,
    WIN_REGION1,
    WIN_REGION2,
    WIN_COUNT,
};

#define REGION_WIN_Y(r) (BW_FP_BOARD_TITLE_ROWS + (r) * BW_FP_BAND_ROWS + 1)

static const struct WindowTemplate sWindowTemplates[] =
{
    [WIN_SYMBOLS] = {.bg = BG_TEXT, .tilemapLeft = 2, .tilemapTop = 3,
                     .width = 12, .height = 3, .paletteNum = PAL_TEXT, .baseBlock = 1},
    [WIN_RECORD] = {.bg = BG_TEXT, .tilemapLeft = 2, .tilemapTop = 10,
                    .width = 12, .height = 3, .paletteNum = PAL_TEXT, .baseBlock = 37},
    [WIN_POINTS] = {.bg = BG_TEXT, .tilemapLeft = 2, .tilemapTop = 13,
                    .width = 12, .height = 4, .paletteNum = PAL_TEXT, .baseBlock = 73},
    [WIN_HINT] = {.bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = 18,
                  .width = 30, .height = 2, .paletteNum = PAL_TEXT, .baseBlock = 121},
    [WIN_BOARD_TITLE] = {.bg = BG_TEXT, .tilemapLeft = 1, .tilemapTop = 0,
                         .width = 12, .height = 2, .paletteNum = PAL_TEXT, .baseBlock = 181},
    [WIN_BOARD_HINT] = {.bg = BG_TEXT, .tilemapLeft = 20, .tilemapTop = 0,
                        .width = 10, .height = 2, .paletteNum = PAL_TEXT, .baseBlock = 205},
    [WIN_REGION0] = {.bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = REGION_WIN_Y(0),
                     .width = 6, .height = 3, .paletteNum = PAL_TEXT, .baseBlock = 225},
    [WIN_REGION1] = {.bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = REGION_WIN_Y(1),
                     .width = 6, .height = 3, .paletteNum = PAL_TEXT, .baseBlock = 243},
    [WIN_REGION2] = {.bg = BG_TEXT, .tilemapLeft = 0, .tilemapTop = REGION_WIN_Y(2),
                     .width = 6, .height = 3, .paletteNum = PAL_TEXT, .baseBlock = 261},
    DUMMY_WIN_TEMPLATE,     /* 279 まで。0xE800 の手前（320 枚）に収まる */
};

static const struct BgTemplate sBgTemplates[] =
{
    {.bg = BG_TEXT, .charBaseIndex = 3, .mapBaseIndex = 29, .screenSize = 0,
     .paletteMode = 0, .priority = 0, .baseTile = 0},
    {.bg = BG_BADGE, .charBaseIndex = 2, .mapBaseIndex = 30, .screenSize = 0,
     .paletteMode = 0, .priority = 1, .baseTile = 0},
    {.bg = BG_ART, .charBaseIndex = 0, .mapBaseIndex = 31, .screenSize = 0,
     .paletteMode = 0, .priority = 2, .baseTile = 0},
};

/* ---- 記章（スプライト）---- */
#define TAG_SYMBOL 0x5A50
#define TAG_SYMBOL_GOLD 0x5A51

/* エメラルドの sPassAreasLayout の (xStart + 8, yStart + 6)＝中心 */
static const u8 sSymbolXY[BW_FP_NUM_SYMBOLS][2] =
{
    {28, 56}, {40, 72}, {52, 56}, {64, 72}, {76, 56}, {88, 72}, {100, 56},
};

static const struct OamData sSymbolOam =
{
    .affineMode = ST_OAM_AFFINE_OFF, .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(16x16), .size = SPRITE_SIZE(16x16), .priority = 1,
};
static const union AnimCmd sAnimSymbol0[] = {ANIMCMD_FRAME(0, 0), ANIMCMD_END};
static const union AnimCmd sAnimSymbol1[] = {ANIMCMD_FRAME(4, 0), ANIMCMD_END};
static const union AnimCmd sAnimSymbol2[] = {ANIMCMD_FRAME(8, 0), ANIMCMD_END};
static const union AnimCmd sAnimSymbol3[] = {ANIMCMD_FRAME(12, 0), ANIMCMD_END};
static const union AnimCmd sAnimSymbol4[] = {ANIMCMD_FRAME(16, 0), ANIMCMD_END};
static const union AnimCmd sAnimSymbol5[] = {ANIMCMD_FRAME(20, 0), ANIMCMD_END};
static const union AnimCmd sAnimSymbol6[] = {ANIMCMD_FRAME(24, 0), ANIMCMD_END};
static const union AnimCmd *const sAnimTableSymbol[] =
{
    sAnimSymbol0, sAnimSymbol1, sAnimSymbol2, sAnimSymbol3,
    sAnimSymbol4, sAnimSymbol5, sAnimSymbol6,
};
static const struct SpriteSheet sSymbolSheet =
    {gBwFpSymbolTiles, BW_FP_NUM_SYMBOLS * BW_FP_TILE16, TAG_SYMBOL};
static const struct SpritePalette sSymbolPalSilver = {gBwFpSymbolPalSilver, TAG_SYMBOL};
static const struct SpritePalette sSymbolPalGold = {gBwFpSymbolPalGold, TAG_SYMBOL_GOLD};
static const struct SpriteTemplate sSymbolTemplate =
{
    TAG_SYMBOL, TAG_SYMBOL, &sSymbolOam, sAnimTableSymbol, NULL,
    gDummySpriteAffineAnimTable, SpriteCallbackDummy,
};

/* ---- タスクの欄 ---- */
enum {
    TD_VIEW,                /* 0 = パス / 1 = 拡大 */
    TD_SEL,                 /* パスで選んでいるもの */
    TD_NEXT,                /* 暗転したあと何をするか */
    TD_SYMBOL0,             /* 記章のスプライト番号 7 個（無ければ 0xFF）*/
    TD_WAIT = TD_SYMBOL0 + 7, /* 描き直したあと明るくするまでのコマ */
};

enum { VIEW_PASS, VIEW_BOARD };
/* ★ 並びは画面の上から（上下キーでこの順に動く）*/
enum { SEL_CANCEL, SEL_BADGES, SEL_CARD, SEL_COUNT };
enum { NEXT_NONE, NEXT_BOARD, NEXT_PASS, NEXT_CARD, NEXT_EXIT };

static void Task_FadeIn(u8 taskId);
static void Task_Input(u8 taskId);
static void Task_FadeOut(u8 taskId);
static void CB2_ReturnFromCard(void);

/* ---- 小物 ---------------------------------------------------------- */

#define BWFP_NUM_RIGHT 90   /* 数字の右端（窓は 12 タイル = 96 画素） */

static void Print(u8 win, const u8 *text, u8 x, u8 y, const u8 *color)
{
    AddTextPrinterParameterized3(win, BWFP_FONT, x, y, color, TEXT_SPEED_FF, text);
}

/* 数字を右端 right にそろえて書く。
   ★ 2026-10-07 直した（ユーザー「BPが溜まっているかわからない」）: 日本語版の STR_CONV_MODE_RIGHT_ALIGN は頭を全角の空白で埋める。
     x = 60 から 5 けたで書くと数字が窓（96 画素）の右からはみ出し、れんしょう・さいこう・BP が 1 けたなら何も見えなかった
     （2 けたの BP 14 だけ右端にかろうじて出ていた）→ 左寄せで作り、幅を測って右端にそろえる */
static void PrintNum(u8 win, u16 value, u8 digits, u8 right, u8 y)
{
    u8 buf[8];
    s32 w;

    ConvertIntToDecimalStringN(buf, value, STR_CONV_MODE_LEFT_ALIGN, digits);
    w = GetStringWidth(BWFP_FONT, buf, 0);
    Print(win, buf, (w < right) ? right - w : 0, y, sColorDark);
}

/* 自分の配置の入れ物へ、w x h の配置を写す */
static void CopyRect(u16 *buf, const u16 *src, u8 x, u8 y, u8 w, u8 h)
{
    u8 i, j;

    for (j = 0; j < h; ++j)
        for (i = 0; i < w; ++i)
            buf[(y + j) * 32 + x + i] = src[j * w + i];
}

static void Put16(u8 n, u8 x, u8 y)
{
    u16 t = TILE_BADGE0 + n * 4;

    FillBgTilemapBufferRect(BG_BADGE, t, x, y, 1, 1, PAL_BADGE);
    FillBgTilemapBufferRect(BG_BADGE, t + 1, x + 1, y, 1, 1, PAL_BADGE);
    FillBgTilemapBufferRect(BG_BADGE, t + 2, x, y + 1, 1, 1, PAL_BADGE);
    FillBgTilemapBufferRect(BG_BADGE, t + 3, x + 1, y + 1, 1, 1, PAL_BADGE);
}

/* ---- データの読み出し ----------------------------------------------- */

/* バッジ n（0〜23）を持っているか。★ ホウエンの 8 枚だけが本物 */
static bool8 HasBadge(u8 n)
{
    static const u16 sHoennBadgeFlags[BW_FP_PER_REGION] =
    {
        HOENN_FLAG_BADGE1, HOENN_FLAG_BADGE2, HOENN_FLAG_BADGE3,
        HOENN_FLAG_BADGE4, HOENN_FLAG_BADGE5, HOENN_FLAG_BADGE6,
        HOENN_FLAG_BADGE7, HOENN_FLAG_BADGE8,
    };

    static const u16 sJohtoBadgeFlags[BW_FP_PER_REGION] =   /* 2026-09-30 ジョウトのジム（src/Tables/johto_flags.h・tools/johto/build_johto_gyms.py）*/
    {
        JOHTO_FLAG_BADGE1, JOHTO_FLAG_BADGE2, JOHTO_FLAG_BADGE3,
        JOHTO_FLAG_BADGE4, JOHTO_FLAG_BADGE5, JOHTO_FLAG_BADGE6,
        JOHTO_FLAG_BADGE7, JOHTO_FLAG_BADGE8,
    };

    if (n < BW_FP_PER_REGION)
        return FlagGet(sHoennBadgeFlags[n]);
    static const u16 sSinnohBadgeFlags[BW_FP_PER_REGION] =  /* 2026-10-01 シンオウのジム（src/Tables/sinnoh_flags.h・tools/sinnoh/build_sinnoh_gyms.py）*/
    {
        SINNOH_FLAG_BADGE1, SINNOH_FLAG_BADGE2, SINNOH_FLAG_BADGE3,
        SINNOH_FLAG_BADGE4, SINNOH_FLAG_BADGE5, SINNOH_FLAG_BADGE6,
        SINNOH_FLAG_BADGE7, SINNOH_FLAG_BADGE8,
    };

    if (n < 2 * BW_FP_PER_REGION)
        return FlagGet(sJohtoBadgeFlags[n - BW_FP_PER_REGION]);
    if (n < 3 * BW_FP_PER_REGION)
        return FlagGet(sSinnohBadgeFlags[n - 2 * BW_FP_PER_REGION]);
    return FALSE;
}

/* 記章 n（0〜6）の段階。0＝無し / 1＝銀 / 2＝金 */
static u8 SymbolLevel(u8 n)
{
    return (VarGet(HOENN_VAR_FRONTIER_SYMBOLS) >> (n * 2)) & 3;
}

/* ---- 記章 ----------------------------------------------------------- */

static void CreateSymbols(u8 taskId)
{
    u8 i;

    LoadSpriteSheet(&sSymbolSheet);
    LoadSpritePalette(&sSymbolPalSilver);
    LoadSpritePalette(&sSymbolPalGold);
    for (i = 0; i < BW_FP_NUM_SYMBOLS; ++i)
    {
        u8 level = SymbolLevel(i);
        u8 id = 0xFF;

        if (level != 0)     /* 無いものは出さない（背景の八角形が見える）*/
        {
            struct SpriteTemplate t = sSymbolTemplate;

            if (level >= 2)
                t.paletteTag = TAG_SYMBOL_GOLD;
            id = CreateSprite(&t, sSymbolXY[i][0], sSymbolXY[i][1], 0);
            if (id != MAX_SPRITES)
                StartSpriteAnim(&gSprites[id], i);
            else
                id = 0xFF;
        }
        gTasks[taskId].data[TD_SYMBOL0 + i] = id;
    }
}

static void ShowSymbols(u8 taskId, bool8 show)
{
    u8 i;

    for (i = 0; i < BW_FP_NUM_SYMBOLS; ++i)
    {
        u8 id = gTasks[taskId].data[TD_SYMBOL0 + i];

        if (id != 0xFF)
            gSprites[id].invisible = !show;
    }
}

/* ---- 描く ----------------------------------------------------------- */

static void ClearText(void)
{
    u8 i;

    for (i = 0; i < WIN_COUNT; ++i)
        FillWindowPixelBuffer(i, 0);
    FillBgTilemapBufferRect(BG_TEXT, 0, 0, 0, 32, 32, PAL_TEXT);
    FillBgTilemapBufferRect(BG_BADGE, 0, 0, 0, 32, 32, PAL_BADGE);
}

static void ShowWin(u8 win)
{
    PutWindowTilemap(win);
    CopyWindowToVram(win, 3);
}

/* パスの選択中の枠。エメラルドの UpdateAreaHighlight と同じく
   **選択中の配置に差し替える**だけ */
static void DrawPassHighlight(u8 sel)
{
    CopyRect(sState->bg2Buf, gBwFpCancelMap[sel == SEL_CANCEL],
             BW_FP_CANCEL_X, BW_FP_CANCEL_Y, BW_FP_CANCEL_W, BW_FP_CANCEL_H);
    CopyRect(sState->bg2Buf, gBwFpAreaMap[sel == SEL_BADGES],
             BW_FP_AREA_X, BW_FP_AREA_Y, BW_FP_AREA_W, BW_FP_AREA_H);
    CopyRect(sState->bg2Buf, gBwFpCardMap[sel == SEL_CARD],
             BW_FP_CARD_X, BW_FP_CARD_Y, BW_FP_AREA_W, BW_FP_AREA_H);
    CopyBgTilemapBufferToVram(BG_ART);
}

/* 暗転中に絵を入れ替えるときのパレットの読み込み。素の LoadPalette は表示用（faded）にも明るい色を書くので、
   続く描き直し（地方名・バッジ・文字で 5 コマほどかかる）の間、描きかけの画面が明るく見えていた
   （2026-10-07 mGBA で 1 コマずつ記録: 読み込みから 5 コマ明るいまま）。
   → 元の色（unfaded）にだけ写し、表示用はすぐ真っ黒にする。明るくするのは描き終えたあとのフェード */
#define BWFP_PLTT_UNFADED ((u16 *)0x0203712C)   /* gPlttBufferUnfaded（src/dns.c と同じ番地）*/
static void LoadPaletteDark(const void *src, u16 offset, u16 size)
{
    CpuCopy16(src, BWFP_PLTT_UNFADED + offset, size);
    BlendPalettes(0xFFFFFFFF, 16, RGB_BLACK);
}

static void LoadPassArt(void)
{
    LoadBgTiles(BG_ART, gBwFpPassTiles, BW_FP_PASS_NTILES * 32, 0);
    LoadPaletteDark(gBwFpPassPal, 0, 8 * 32);
    LoadPaletteDark(gBwFpAreaPal, BW_FP_PAL_AREA * 16, 32);
    CopyToBgTilemapBuffer(BG_ART, gBwFpPassMap, 32 * 20 * 2, 0);
}

static void LoadBoardArt(void)
{
    LoadBgTiles(BG_ART, gBwFpBoardTiles, BW_FP_BOARD_NTILES * 32, 0);
    LoadPaletteDark(gBwFpBoardPal, 0, BW_FP_BOARD_NPALS * 32);
    CopyToBgTilemapBuffer(BG_ART, gBwFpBoardMap, 32 * 20 * 2, 0);
    CopyBgTilemapBufferToVram(BG_ART);
}

static void DrawPass(u8 sel)
{
    u8 r, i;

    ClearText();

    /* ★ 文字の縦位置は実機の写真（240x160）で測って決めた（2026-09-23）。
       字は y+2 から 9 画素。枠の上下の余白が揃うようにしてある */
    Print(WIN_SYMBOLS, gText_BwFp_Symbols, 4, 5, sColorDark);

    /* たいせんきろく（本物）と BP（#5 待ち）*/
    Print(WIN_RECORD, gText_BwFp_Streak, 4, 5, sColorDark);
    PrintNum(WIN_RECORD, GetCurrentBattleTowerStreak(), 5, BWFP_NUM_RIGHT, 5);
    Print(WIN_POINTS, gText_BwFp_MaxStreak, 4, 4, sColorDark);
    PrintNum(WIN_POINTS, GetBattleTowerStreak(MAX_STREAK, 0xFFFF, 0xFFFF, 0xFFFF, 0),
             5, BWFP_NUM_RIGHT, 4);
    Print(WIN_POINTS, gText_BwFp_Bp, 4, 15, sColorDark);
    PrintNum(WIN_POINTS, VarGet(HOENN_VAR_FRONTIER_BP), 5, BWFP_NUM_RIGHT, 15);

    Print(WIN_HINT, gText_BwFp_HintPass, 8, 1, sColorWhite);

    ShowWin(WIN_SYMBOLS);
    ShowWin(WIN_RECORD);
    ShowWin(WIN_POINTS);
    ShowWin(WIN_HINT);

    /* 右上の縮小版（8x8）。持っているものだけ */
    for (r = 0; r < BW_FP_NUM_REGIONS; ++r)
        for (i = 0; i < BW_FP_PER_REGION; ++i)
            if (HasBadge(r * BW_FP_PER_REGION + i))
                FillBgTilemapBufferRect(BG_BADGE,
                    TILE_MINI0 + r * BW_FP_PER_REGION + i,
                    BW_FP_MINI_COL0 + i,
                    BW_FP_MINI_ROW0 + r * BW_FP_MINI_ROW_STEP, 1, 1, PAL_BADGE);

    CopyBgTilemapBufferToVram(BG_TEXT);
    CopyBgTilemapBufferToVram(BG_BADGE);
    DrawPassHighlight(sel);
}

static void DrawBoard(void)
{
    static const u8 *const sRegionNames[BW_FP_NUM_REGIONS] =
    {
        gText_BwFp_Hoenn, gText_BwFp_Johto, gText_BwFp_Sinnoh,
    };
    u8 r, i;

    ClearText();
    Print(WIN_BOARD_TITLE, gText_BwFp_Badges, 8, 1, sColorDark);
    Print(WIN_BOARD_HINT, gText_BwFp_HintBack, 12, 1, sColorWhite);
    ShowWin(WIN_BOARD_TITLE);
    ShowWin(WIN_BOARD_HINT);

    for (r = 0; r < BW_FP_NUM_REGIONS; ++r)
    {
        /* 札（x 3〜44）のまん中に地方名 */
        s32 x = 24 - (s32)GetStringWidth(BWFP_FONT, sRegionNames[r], 0) / 2;

        Print(WIN_REGION0 + r, sRegionNames[r], x < 0 ? 0 : x, 9, sColorDark);
        ShowWin(WIN_REGION0 + r);

        for (i = 0; i < BW_FP_PER_REGION; ++i)
        {
            u8 n = r * BW_FP_PER_REGION + i;

            if (HasBadge(n))
                Put16(n, BW_FP_BOARD_COL0 + i * BW_FP_BOARD_COL_STEP,
                      BW_FP_BOARD_TITLE_ROWS + r * BW_FP_BAND_ROWS
                      + BW_FP_BOARD_BADGE_ROW);
        }
    }
    CopyBgTilemapBufferToVram(BG_TEXT);
    CopyBgTilemapBufferToVram(BG_BADGE);
}

/* ---- 片づけ --------------------------------------------------------- */

static void FreeAll(void)
{
    FreeAllWindowBuffers();
    if (sState != NULL)
    {
        BwFp_UnsetBgTilemapBuffer(BG_BADGE);
        BwFp_UnsetBgTilemapBuffer(BG_ART);
        Free(sState->bg1Buf);
        Free(sState->bg2Buf);
        Free(sState);
        sState = NULL;
    }
}

/* ---- 画面の流れ ----------------------------------------------------- */

static void MainCB2_BwFrontierPass(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void VBlankCB_BwFrontierPass(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

void CB2_BwFrontierPass(void)
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
        /* ★ 前回の入れ物が残っていたら先に片づける（二重に確保しない）*/
        if (sState != NULL)
            FreeAll();
        sState = AllocZeroed(sizeof(struct BwFpState));
        sState->bg1Buf = AllocZeroed(0x800);
        sState->bg2Buf = AllocZeroed(0x800);
        SetBgTilemapBuffer(BG_BADGE, sState->bg1Buf);
        SetBgTilemapBuffer(BG_ART, sState->bg2Buf);
        InitWindows(sWindowTemplates);
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_WIN0H, 0);
        SetGpuReg(REG_OFFSET_WIN0V, 0);
        SetGpuReg(REG_OFFSET_WININ, 0);
        SetGpuReg(REG_OFFSET_WINOUT, 0);
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        SetGpuReg(REG_OFFSET_BLDY, 0);
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
        /* バッジ（BG1）。★ タイル 0 は空白のままにする */
        LoadBgTiles(BG_BADGE, gBwFpBadgeTiles, BW_FP_NUM_BADGES * BW_FP_TILE16,
                    TILE_BADGE0);
        LoadBgTiles(BG_BADGE, gBwFpMiniTiles, BW_FP_NUM_BADGES * 32, TILE_MINI0);
        LoadPalette(gBwFpBadgePal, PAL_BADGE * 16, 32);
        LoadPalette(sTextPal, PAL_TEXT * 16, 32);
        LoadPassArt();
        gMain.state++;
        break;
    case 4:
        DrawPass(SEL_BADGES);
        gMain.state++;
        break;
    case 5:
    {
        u8 taskId = CreateTask(Task_FadeIn, 0);

        gTasks[taskId].data[TD_VIEW] = VIEW_PASS;
        gTasks[taskId].data[TD_SEL] = SEL_BADGES;
        gTasks[taskId].data[TD_NEXT] = NEXT_NONE;
        CreateSymbols(taskId);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP
                                      | DISPCNT_OBJ_ON);
        ShowBg(BG_TEXT);
        ShowBg(BG_BADGE);
        ShowBg(BG_ART);
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
        SetVBlankCallback(VBlankCB_BwFrontierPass);
        SetMainCallback2(MainCB2_BwFrontierPass);
        return;
    }
    }
}

/* ★ 素のカードから帰ってきたとき。**`gMain.state` を 0 に戻してから**
     こちらの CB2 へ渡す（この CB2 は `gMain.state` で組み立てを進めるので、
     途中の値のままだと初期化を飛ばして画面が壊れる）。 */
static void CB2_ReturnFromCard(void)
{
    gMain.state = 0;
    SetMainCallback2(CB2_BwFrontierPass);
}

static void Task_FadeIn(u8 taskId);

/* 描き直したあと、転送が済むまで真っ黒のまま数コマ待ってから明るくする */
static void Task_WaitThenFadeIn(u8 taskId)
{
    if (--gTasks[taskId].data[TD_WAIT] > 0)
        return;
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
    gTasks[taskId].func = Task_FadeIn;
}

static void Task_FadeIn(u8 taskId)
{
    if (!gPaletteFade->active)
        gTasks[taskId].func = Task_Input;
}

static void StartFade(u8 taskId, u8 next)
{
    gTasks[taskId].data[TD_NEXT] = next;
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
    gTasks[taskId].func = Task_FadeOut;
}

static void Task_Input(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (data[TD_VIEW] == VIEW_BOARD)
    {
        if (JOY_NEW(B_BUTTON) || JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            StartFade(taskId, NEXT_PASS);
        }
        return;
    }

    if (JOY_NEW(DPAD_UP))
    {
        PlaySE(SE_SELECT);
        data[TD_SEL] = (data[TD_SEL] + SEL_COUNT - 1) % SEL_COUNT;
        DrawPassHighlight(data[TD_SEL]);
        return;
    }
    if (JOY_NEW(DPAD_DOWN))
    {
        PlaySE(SE_SELECT);
        data[TD_SEL] = (data[TD_SEL] + 1) % SEL_COUNT;
        DrawPassHighlight(data[TD_SEL]);
        return;
    }
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        switch (data[TD_SEL]) {
        case SEL_CARD:   StartFade(taskId, NEXT_CARD); break;
        case SEL_CANCEL: StartFade(taskId, NEXT_EXIT); break;
        default:         StartFade(taskId, NEXT_BOARD); break;
        }
        return;
    }
    if (JOY_NEW(B_BUTTON))
    {
        PlaySE(SE_SELECT);
        StartFade(taskId, NEXT_EXIT);
    }
}

static void Task_FadeOut(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    if (gPaletteFade->active)
        return;

    switch (data[TD_NEXT]) {
    case NEXT_BOARD:
        data[TD_VIEW] = VIEW_BOARD;
        ShowSymbols(taskId, FALSE);
        LoadBoardArt();
        DrawBoard();
        break;
    case NEXT_PASS:
        data[TD_VIEW] = VIEW_PASS;
        LoadPassArt();
        DrawPass(data[TD_SEL]);
        ShowSymbols(taskId, TRUE);
        break;
    case NEXT_CARD:
        /* ★ 素のトレーナーカードへ。戻ってくるとこの画面を作り直す。
           ★★ `gMain.savedCallback` は**触らない** — あれはスタートメニューが
             入れた「野外へ帰る道」で、潰すと B で戻れなくなる。
             素の `ShowPlayerTrainerCard` は行き先を自分の入れ物
             （`sTrainerCardDataPtr->callback2`）に持つので、
             引数で渡すだけでよい。 */
        FreeAll();
        BwFp_ShowPlayerTrainerCard(CB2_ReturnFromCard);
        DestroyTask(taskId);
        return;
    default:
        /* 野外へ帰る（スタートメニューが入れた行き先）*/
        FreeAll();
        SetMainCallback2(gMain.savedCallback);
        DestroyTask(taskId);
        return;
    }
    data[TD_NEXT] = NEXT_NONE;
    /* ★ 2026-10-07 直した（ユーザー「他地方のバッジ表示時に一瞬表示がおかしくなる」）: 絵の読み込みは LoadPaletteDark
         （表示用は真っ黒のまま）。念のためここでも真っ黒にし、文字とタイルの転送が済んでから明るくする */
    BlendPalettes(0xFFFFFFFF, 16, RGB_BLACK);
    data[TD_WAIT] = 2;
    gTasks[taskId].func = Task_WaitThenFadeIn;
}
