#include "defines.h"
#include "map_region.h"
#include "../include/constants/abilities.h"
#include "../include/bg.h"
#include "../include/window.h"
#include "../include/menu.h"
#include "../include/palette.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "../include/decompress.h"
#include "../include/new/util.h"
#include "../include/string_util.h"
#include "../include/sound.h"
#include "../include/link.h"
#include "../include/overworld.h"
#include "../include/item.h"
#include "../include/pokemon.h"
#include "../include/pokemon_icon.h"
#include "../include/trainer_pokemon_sprites.h"
#include "../include/malloc.h"
#include "../include/gpu_regs.h"
#include "../include/party_menu.h"
#include "../include/menu_helpers.h"
#include "../include/scanline_effect.h"
#include "../include/event_data.h"
#include "../include/constants/songs.h"
#include "../include/constants/moves.h"
#include "../include/constants/pokemon.h"
#include "../include/constants/region_map_sections.h"
#include "../include/new/catching.h"
#include "../include/new/frontier.h"
#include "../include/new/damage_calc.h"
#include "../include/new/battle_strings.h"
#include "../include/new/multi.h"
#include "../include/new/pokemon_storage_system.h"
#include "../include/new/dns.h"

/*
    BW 風ステータス画面（つよさをみる）— #19 の 4 つ目

    素材: 参考/github/New-BW-summary-screen（Zake 移植 / CompuMaxx 原作。BPRE 用に画面まるごと C で書き直したもの）を
    CFRU-JP に移植した。設計は docs/research/2026-09-19_bw-summary-screen.md。

    入口（functionrewrites・tools/bwsummary/build_bw_summary.py が書く）:
      ShowPokemonSummaryScreen           BPRJ 08134CE4 → BwShowPokemonSummaryScreen
      ShowSelectMovePokemonSummaryScreen BPRJ 08134F1C → BwShowSelectMovePokemonSummaryScreen
      SetPokemonSummaryScreenMode        BPRJ 081392FC → BwSetPokemonSummaryScreenMode（素は構造体の 0x31B4 を書く）
    素の GetLastViewedMonIndex（081392E4）と GetMoveSlotToReplace（081392F0）は 0203B0E0 / 0203B0E2 を読むだけなので
    同じ場所を使えば触らなくてよい。

    ★ RAM: 素の日本語版のステータス画面の私物 0203B0B4〜0203B0E7 をそのまま使う（他の画面は読まない。実 ROM で確認）。
      書き込み可能な static は使えない（6.13）。
    ★ CFRU / DPE が素の画面に入れていた機能はここに書き直してある:
      拡張表（特性名・技の説明・経験値）/ タイプ（拡張・カモモン・テラスタイプ）/ 親の性別色 /
      個体値ランク・性格の色・ミント（CFRU-JP 独自）/ キョダイマックスの印 / 圧縮した預かりボックス / とおいばしょ /
      メス姿のアイコン（DPE）/ マルチバトルの相方。
*/

/* ---------------- 素の日本語版の番地（BPRJ.ld に無いもの） ---------------- */
#define BwSs_SetHelpContext ((void (*)(u8))(0x0812B9F4 | 1))
#define BwSs_DynPlaceholderReset ((void (*)(void))(0x0813D3D4 | 1))
#define BwSs_DynPlaceholderSet ((void (*)(u8, const u8 *))(0x0813D3F0 | 1))
#define BwSs_DynPlaceholderExpand ((void (*)(u8 *, const u8 *))(0x0813D40C | 1))
#define BwSs_GetMapNameGeneric ((u8 *(*)(u8 *, u16))(0x080C5FEC | 1))
#define BwSs_IsLinkRecvQueueLengthAtLeast3 ((bool32 (*)(void))(0x0800AC68 | 1))
#define BwSs_LinkRecvQueueLengthMoreThan2 ((bool32 (*)(void))(0x08057B00 | 1))
#define BwSs_MenuHelpers_CallLinkSomething ((bool32 (*)(void))(0x080C08FC | 1))
#define BwSs_ShouldIgnoreDeoxysForm ((bool8 (*)(u8, u8))(0x08043B94 | 1))
#define BwSs_IsPokeSpriteNotFlipped ((bool8 (*)(u16))(0x0804383C | 1))
#define BwSs_CreateMonPicSprite ((u16 (*)(u16, u32, u32, u8, s16, s16, u8, u16, u8))(0x0810CC44 | 1))
#define BwSs_FreeBallGfx ((void (*)(u8))(0x0804B71C | 1))
#define BwSs_CB2_ReturnToTradeMenuFromSummary ((MainCallback)(0x0804C6A0 | 1))
#define BwSs_GetHPBarLevel ((u8 (*)(s16, s16))(0x08049EF0 | 1))
#define BwSs_ShouldPlayNormalPokeCry ((bool32 (*)(struct Pokemon *))(0x0803519C | 1))
#define BwSs_CreateMonMarkingSprite ((struct Sprite *(*)(u16, u16, const u16 *))(0x080BFD00 | 1))
#define BWSS_RECEIVED_REMOTE_LINK_PLAYERS (*(u8 *)0x03003FA4)   /* gReceivedRemoteLinkPlayers（BPRE 03003F64 + 0x40。JP の素の画面が 4 回読む） */
#define gBwSsNatureNames ((const u8 *const *)0x0842D1EC)      /* 性格名 25 本（素の日本語版） */

/* CFRU の C（ヘッダに無いもの） */
void CompressedMonToMon(struct CompressedPokemon *compMon, struct Pokemon *dst);
void CreateBoxMonFromCompressedMon(struct BoxPokemon *boxMon, struct CompressedPokemon *compMon);
void CreateCompressedMonFromBoxMon(struct BoxPokemon *boxMon, struct CompressedPokemon *compMon);
s16 sub_808B884(struct CompressedPokemon *boxMons, u8 currIndex, u8 maxIndex, u8 arg3);
void BoxShelf_MarkPtrDirty(const void *ptr);
bool8 TryReplaceSummaryScreenLocationWithFarAwayLocation(struct Pokemon *mon, u8 metLocation);
bool8 SummaryScreen_IsMultiBattlePartner(void);
bool8 CheckIsHmMove(u16 move);
u16 GetMonAbility(const struct Pokemon *mon);
u16 SecondAbility_OfSpecies(u16 species);
u16 SecondAbility_OfMon(struct Pokemon* mon);      /* src/second_ability.c */
bool8 SecondAbility_IsUnlocked(u8 level);
static u16 SecondAbilityOfCurrentMon(void);
extern const struct SpriteTemplate gBallSpriteTemplates[];
extern u8 *gMoveDescriptions[];
extern const u8 *const gAbilityDescriptions[];
extern u8 gMoveNames[][MOVE_NAME_LENGTH + 1];
extern const u8 PSSIconsTiles[];
extern const u8 GigantamaxSummaryScreenIconTiles[];
extern const u16 GigantamaxSummaryScreenIconPal[];
extern const u16 sIVRankings_E_MinusPal[];
extern const u8 sIVRankings_E_MinusTiles[];
extern const u8 sIVRankings_ETiles[];
extern const u8 sIVRankings_E_PlusTiles[];
extern const u8 sIVRankings_D_MinusTiles[];
extern const u8 sIVRankings_DTiles[];
extern const u8 sIVRankings_D_PlusTiles[];
extern const u8 sIVRankings_C_MinusTiles[];
extern const u8 sIVRankings_CTiles[];
extern const u8 sIVRankings_C_PlusTiles[];
extern const u8 sIVRankings_B_MinusTiles[];
extern const u8 sIVRankings_BTiles[];
extern const u8 sIVRankings_B_PlusTiles[];
extern const u8 sIVRankings_A_MinusTiles[];
extern const u8 sIVRankings_ATiles[];
extern const u8 sIVRankings_A_PlusTiles[];
extern const u8 sIVRankings_STiles[];

/* 生成物（src/bw_summary_gfx.c / src/bw_summary_text.c） */
extern const u8 gBwSsTiles[], gBwSsBgPal[], gBwSsMenuPal[], gBwSsMapBg[], gBwSsMapEgg[], gBwSsMapKnownMoves[],
    gBwSsMapMovesInfo[], gBwSsMapMoves[], gBwSsMapMovesSelect[], gBwSsMapInfo[], gBwSsMapSkills[], gBwSsIconStatus[], gBwSsIconStatusPal[],
    gBwSsExpBar[], gBwSsHpBar[], gBwSsHpBarPal0[], gBwSsHpBarPal1[], gBwSsHpBarPal2[], gBwSsCursorPal[],
    gBwSsCursorLeft[], gBwSsCursorRight[], gBwSsShinyPal[], gBwSsShinyGfx[], gBwSsTextPal7[], gBwSsMarkingsPal[],
    gBwSsPal8[], gBwSsDotPal[], gBwSsDotGfx[];
extern const u8 gBwSsText_DexNo[], gBwSsText_Name[], gBwSsText_Type[], gBwSsText_OT[], gBwSsText_IdNo[], gBwSsText_Item[],
    gBwSsText_TrainerMemo[], gBwSsText_Status[], gBwSsText_HP[], gBwSsText_Attack[], gBwSsText_Defense[], gBwSsText_SpAtk[],
    gBwSsText_SpDef[], gBwSsText_Speed[], gBwSsText_ExpPoints[], gBwSsText_ToNextLv[], gBwSsText_Ability[],
    gBwSsText_Ability2[], gBwSsText_Ability2Locked[], gBwSsText_CtrlPageAbility[],
    gBwSsText_Category[], gBwSsText_Power[], gBwSsText_Accuracy[], gBwSsText_Effect[], gBwSsText_PageInfo[],
    gBwSsText_PageSkills[], gBwSsText_PageMoves[], gBwSsText_CtrlPageCancel[], gBwSsText_CtrlCancel[], gBwSsText_CtrlPage[],
    gBwSsText_CtrlPageDetail[], gBwSsText_CtrlPickSwitch[], gBwSsText_CtrlPick[], gBwSsText_CtrlPickDelete[],
    gBwSsText_ItemNone[], gBwSsText_DexNoUnknown[], gBwSsText_OneHyphen[], gBwSsText_TwoHyphens[], gBwSsText_ThreeHyphens[],
    gBwSsText_Slash[], gBwSsText_Male[], gBwSsText_Female[], gBwSsText_Lv[], gBwSsText_PP[], gBwSsText_ATrade[],
    gBwSsText_Somewhere[], gBwSsText_EggNickname[], gBwSsText_Cancel[], gBwSsText_HmCantForget[],
    gBwSsText_MetInATrade[], gBwSsText_MetInATradeBG[], gBwSsText_FatefulMet[], gBwSsText_FatefulMetBG[], gBwSsText_Met[],
    gBwSsText_MetBG[], gBwSsText_ApparentlyMet[], gBwSsText_ApparentlyMetBG[], gBwSsText_Hatched[], gBwSsText_HatchedBG[],
    gBwSsText_FatefulHatched[], gBwSsText_FatefulHatchedBG[], gBwSsText_ApparentlyFatefulHatched[],
    gBwSsText_ApparentlyFatefulHatchedBG[], gBwSsText_EggDayCare[], gBwSsText_EggTravelingMan[], gBwSsText_EggNicePlace[],
    gBwSsText_EggSpa[], gBwSsText_EggTrade[], gBwSsText_EggHatchLongTime[], gBwSsText_EggHatchSomeTime[],
    gBwSsText_EggHatchSoon[], gBwSsText_EggHatchAlmostReady[], gBwSsText_EggFanClub[];
extern bool8 HackGibleEgg_IsClubEgg(const struct Pokemon* mon);   /* src/hack_gible_egg.c */

/* ---------------- 定数 ---------------- */
enum { PSS_MODE_NORMAL, PSS_MODE_UNK1, PSS_MODE_SELECT_MOVE, PSS_MODE_FORGET_MOVE, PSS_MODE_TRADE, PSS_MODE_BOX };
enum { PSS_PAGE_INFO, PSS_PAGE_SKILLS, PSS_PAGE_MOVES, PSS_PAGE_MOVES_INFO, PSS_PAGE_UNK4, PSS_PAGE_MOVE_DELETER };
enum { PSS_STAT_ATK, PSS_STAT_DEF, PSS_STAT_SPA, PSS_STAT_SPD, PSS_STAT_SPE };
enum { BWSS_FADEIN, BWSS_PLAYCRY, BWSS_HANDLEINPUT, BWSS_FLIPPAGES, BWSS_EXIT_FADEOUT, BWSS_EXIT_WAITLINK, BWSS_EXIT_WAITFADE };
enum { AIL_NONE, AIL_PSN, AIL_PRZ, AIL_SLP, AIL_FRZ, AIL_BRN, AIL_PKRS, AIL_FNT };
enum { DARK, WHITE_TITLE, BLUE_SHADOW, WHITE, RED_SHADOW, BLUE, RED, ORANGE, RED_2 };

#define TAG_CURSOR 0x64
#define TAG_STATUS 0x6E
#define TAG_HPBAR 0x78
#define TAG_EXPBAR 0x82
#define TAG_MARKINGS 0x8C
#define TAG_DOT 0x96
#define TAG_SHINY 0xA0
#define TAG_GMAX 0x2715                 /* CFRU dynamax.c と同じ */
#define METLOC_FATEFUL_ENCOUNTER 0xFF
#define METLOC_VALID_MAX 0xFC           /* 地名の表は 253 件（#15）。FD/FE/FF は特別 */
#define HELPCONTEXT_POKEMON_INFO 6
#define HELPCONTEXT_POKEMON_SKILLS 7
#define HELPCONTEXT_POKEMON_MOVES 8
#define BWSS_FONT 2                     /* 日本語の標準フォント */
#define MAX_SPRITES 64
#define HIHALF(n) (((n) & 0xFFFF0000) >> 16)
#define LOHALF(n) ((n) & 0xFFFF)
#define SPLIT_OF(move) (gBattleMoves[move].split)

/* ---------------- RAM（素の日本語版の私物） ---------------- */
struct BwSsXpos { u16 dexNo; u16 curHp; u16 atk; u16 def; u16 spa; u16 spd; u16 spe; u16 exp; u16 toNext; u16 pp[5]; u16 ppMax[5]; u16 unk26; };
struct BwSsSpriteBox { struct Sprite *sprite; u16 tileTag; u16 palTag; u16 unk08; };
struct BwSsBar { struct Sprite *sprites[11]; u16 cordX[11]; u16 tileTag; u16 palTag; };
struct BwSsAnim { u8 index; u8 tileTag; u8 kind; };

struct BwSummaryScreen
{
    u8 window[7];
    u8 ballSpriteId;
    u8 monSpriteId;
    u8 iconSpriteId;
    u8 ballId;
    u8 gmaxSpriteId;
    u8 task;
    u8 pageLocked;
    u8 animCount;
    bool8 isEnemyParty;
    struct
    {
        u8 nickname[POKEMON_NAME_LENGTH + 1];
        u8 speciesName[POKEMON_NAME_LENGTH + 1];
        u8 otName[12];
        u8 otCmp[2][12];
        u8 dexNum[5];
        u8 otId[7];
        u8 heldItem[ITEM_NAME_LENGTH + 4];
        u8 genderSymbol[3];
        u8 level[7];
        u8 hpStr[12];                    /* 2026-10-09 4 けた（"1418/1418"＋終わり）*/
        u8 statStr[5][5];
        u8 ppStr[5][11];
        u8 ppMaxStr[5][11];
        u8 moveName[5][MOVE_NAME_LENGTH + 1];
        u8 power[5][5];
        u8 accuracy[5][5];
        u8 expStr[9];
        u8 toNextStr[9];
        u8 abilityName[20];
    } summary;
    u8 isEgg;
    u8 isBadEgg;
    u8 mode;
    u8 lastIndex;
    u8 curPageIndex;
    u8 isBoxMon;
    u8 monTypes[2];
    u8 pageDir;          /* 1 = 右へ */
    u8 slideState;
    u8 slideStart;
    u8 lockMovesFlag;
    u8 unk3238;
    u8 unk323C;
    u8 unk3240;
    u8 sliding;
    s16 slideX;
    u16 moveType[5];
    u16 moves[5];
    u8 numMoves;
    u8 swapping;
    u8 monStatus;
    u8 mainState;
    u8 initState;
    u8 gfxState;
    u8 spriteState;
    u8 dataState;
    u8 flipState;
    u8 movesState;
    u8 changeMonState;
    struct Pokemon currentMon;
    union { struct Pokemon *mons; struct CompressedPokemon *boxMons; } monList;
    MainCallback savedCallback;
    struct Sprite *markingSprite;
    u8 queuedInput[2];
    u8 showEvIv;         /* ★ 2026-10-10 能力のページで 努力値／個体値 の数を出しているか（SELECT で切り替え・bw-reloaded の EV/IV のページの中身） */
    u8 showAbility2;     /* ★ 2026-10-03 能力のページで 2 つ目の特性を出しているか（A で切り替え・ポケモンを替えたら 0） */
};

#define sBwSs (*(struct BwSummaryScreen **)0x0203B0B4)
#define sXpos (*(struct BwSsXpos **)0x0203B0B8)
#define sCursor(i) (((struct BwSsSpriteBox **)0x0203B0BC)[i])
#define sStatusIcon (*(struct BwSsSpriteBox **)0x0203B0CC)
#define sHpBar (*(struct BwSsBar **)0x0203B0D0)
#define sExpBar (*(struct BwSsBar **)0x0203B0D4)
#define sDot (*(struct BwSsSpriteBox **)0x0203B0D8)
#define sShiny (*(struct BwSsSpriteBox **)0x0203B0DC)
/* sLastViewedMonIndex は CFRU の ram_locs.h が 0x0203B0E0 に定義（素の GetLastViewedMonIndex(081392E4) が読む） */
#define sMoveCursor (*(u8 *)0x0203B0E1)
#define sMoveSwapPos (*(u8 *)0x0203B0E2)             /* 素の GetMoveSlotToReplace(081392F0) が読む */
#define sAnim (*(struct BwSsAnim **)0x0203B0E4)

#define FREE_IF_SET(ptr) do { if ((ptr) != NULL) { Free(ptr); (ptr) = NULL; } } while (0)


/* ---------------- 静的データ ---------------- */
static const struct OamData sOam64x32 = { .shape = SPRITE_SHAPE(64x32), .size = SPRITE_SIZE(64x32) };
static const struct OamData sOam32x8 = { .shape = SPRITE_SHAPE(32x8), .size = SPRITE_SIZE(32x8) };
static const struct OamData sOam8x8 = { .shape = SPRITE_SHAPE(8x8), .size = SPRITE_SIZE(8x8) };
static const struct OamData sOam16x16 = { .shape = SPRITE_SHAPE(16x16), .size = SPRITE_SIZE(16x16), .priority = 0 };

static const union AnimCmd sAnimCursor0[] = { ANIMCMD_FRAME(0, 20), ANIMCMD_JUMP(0) };
static const union AnimCmd sAnimCursor1[] = { ANIMCMD_FRAME(0x20, 20), ANIMCMD_JUMP(0) };
static const union AnimCmd *const sAnimsCursor[] = { sAnimCursor0, sAnimCursor1 };

#define STATUS_ANIM(n, f) static const union AnimCmd sAnimStatus##n[] = { ANIMCMD_FRAME(f, 20), ANIMCMD_JUMP(0) };
STATUS_ANIM(0, 0) STATUS_ANIM(1, 4) STATUS_ANIM(2, 8) STATUS_ANIM(3, 12) STATUS_ANIM(4, 16) STATUS_ANIM(5, 20) STATUS_ANIM(6, 24) STATUS_ANIM(7, 28)
static const union AnimCmd *const sAnimsStatus[] = { sAnimStatus0, sAnimStatus1, sAnimStatus2, sAnimStatus3, sAnimStatus4, sAnimStatus5, sAnimStatus6, sAnimStatus7 };

#define BAR_ANIM(n) static const union AnimCmd sAnimBar##n[] = { ANIMCMD_FRAME(n, 20), ANIMCMD_JUMP(0) };
BAR_ANIM(0) BAR_ANIM(1) BAR_ANIM(2) BAR_ANIM(3) BAR_ANIM(4) BAR_ANIM(5) BAR_ANIM(6) BAR_ANIM(7) BAR_ANIM(8) BAR_ANIM(9) BAR_ANIM(10) BAR_ANIM(11)
static const union AnimCmd *const sAnimsBar[] = { sAnimBar0, sAnimBar1, sAnimBar2, sAnimBar3, sAnimBar4, sAnimBar5, sAnimBar6, sAnimBar7, sAnimBar8, sAnimBar9, sAnimBar10, sAnimBar11 };

static const union AnimCmd sAnimDot0[] = { ANIMCMD_FRAME(0, 20), ANIMCMD_JUMP(0) };
static const union AnimCmd *const sAnimsDot[] = { sAnimDot0 };
static const union AnimCmd sAnimShiny0[] = { ANIMCMD_FRAME(1, 20), ANIMCMD_JUMP(0) };
static const union AnimCmd *const sAnimsShiny[] = { sAnimShiny0 };

static const struct SpriteTemplate sGmaxIconTemplate = {
    .tileTag = TAG_GMAX, .paletteTag = TAG_GMAX, .oam = &sOam16x16, .anims = gDummySpriteAnimTable,
    .images = NULL, .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
};
static const struct CompressedSpriteSheet sGmaxIconSheet = { GigantamaxSummaryScreenIconTiles, (16 * 16) / 2, TAG_GMAX };

static const u8 *const sEggHatchTexts[] = { gBwSsText_EggHatchLongTime, gBwSsText_EggHatchSomeTime, gBwSsText_EggHatchSoon, gBwSsText_EggHatchAlmostReady };
static const u8 *const sEggOriginTexts[] = { gBwSsText_EggDayCare, gBwSsText_EggTrade, gBwSsText_EggTravelingMan, gBwSsText_EggTrade, gBwSsText_EggNicePlace, gBwSsText_EggSpa, gBwSsText_EggTrade };

static const struct BgTemplate sBgTemplates[] = {
    { .bg = 0, .charBaseIndex = 1, .mapBaseIndex = 29, .priority = 0 },   /* 文字 */
    { .bg = 1, .charBaseIndex = 0, .mapBaseIndex = 30, .priority = 2 },   /* ページの格子 */
    { .bg = 2, .charBaseIndex = 0, .mapBaseIndex = 31, .priority = 3 },   /* 背景（流れる） */
    { .bg = 3, .charBaseIndex = 0, .mapBaseIndex = 28, .priority = 1 },   /* 技の枠 */
};

static const struct WindowTemplate sWinTop[] = {
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 13, .height = 2, .paletteNum = 7, .baseBlock = 0x258 },   /* ページ名 */
    { .bg = 0, .tilemapLeft = 15, .tilemapTop = 0, .width = 15, .height = 2, .paletteNum = 7, .baseBlock = 0x272 },  /* 操作（日本語は長いので 15 タイル・2026-10-03 「{DLR}ページ {A} とくせい」が 13 タイルからはみ出した。0x272+30=0x290 で次の窓の手前まで） */
    { .bg = 0, .tilemapLeft = 20, .tilemapTop = 2, .width = 10, .height = 4, .paletteNum = 6, .baseBlock = 0x290 },  /* 名前・レベル（x160〜240。左端にボール）。操作の窓 0x272+26=0x28C の後ろ */
};
static const struct WindowTemplate sWinInfo[] = {
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 2, .width = 21, .height = 10, .paletteNum = 6, .baseBlock = 1 },
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 12, .width = 28, .height = 8, .paletteNum = 6, .baseBlock = 220 },
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 0, .height = 0, .paletteNum = 0, .baseBlock = 0 },
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 0, .height = 0, .paletteNum = 0, .baseBlock = 0 },
};
static const struct WindowTemplate sWinSkills[] = {
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 2, .width = 18, .height = 13, .paletteNum = 6, .baseBlock = 1 },   /* 2026-10-09 17 → 18 タイル（4 けたの HP・8 けたの経験値・1 + 18 × 13 = 235 < 次の窓 292） */
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 0, .height = 0, .paletteNum = 0, .baseBlock = 0 },
    { .bg = 0, .tilemapLeft = 1, .tilemapTop = 15, .width = 29, .height = 5, .paletteNum = 6, .baseBlock = 292 },
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 0, .height = 0, .paletteNum = 0, .baseBlock = 0 },
};
static const struct WindowTemplate sWinMoves[] = {
    { .bg = 0, .tilemapLeft = 5, .tilemapTop = 2, .width = 10, .height = 18, .paletteNum = 6, .baseBlock = 1 },
    { .bg = 0, .tilemapLeft = 15, .tilemapTop = 6, .width = 15, .height = 14, .paletteNum = 6, .baseBlock = 181 },
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 2, .width = 5, .height = 18, .paletteNum = 6, .baseBlock = 452 },
    { .bg = 0, .tilemapLeft = 0, .tilemapTop = 0, .width = 0, .height = 0, .paletteNum = 0, .baseBlock = 0 },
};
static const struct WindowTemplate sWinDummy[] = { { .bg = 0xFF } };

static const u8 sTextColours[][3] = {
    [DARK] = {0, 14, 13}, [WHITE_TITLE] = {0, 1, 2}, [BLUE_SHADOW] = {0, 15, 7}, [WHITE] = {0, 15, 14},
    [RED_SHADOW] = {0, 15, 1}, [BLUE] = {0, 7, 6}, [RED] = {0, 1, 2}, [ORANGE] = {0, 3, 14}, [RED_2] = {0, 1, 14},
};

/* 性格による能力の上下（Atk Def Spd SpAtk SpDef の順・素と同じ） */
static const s8 sNatureStatTable[][5] = {
    {0,0,0,0,0},{1,-1,0,0,0},{1,0,-1,0,0},{1,0,0,-1,0},{1,0,0,0,-1},{-1,1,0,0,0},{0,0,0,0,0},{0,1,-1,0,0},{0,1,0,-1,0},{0,1,0,0,-1},
    {-1,0,1,0,0},{0,-1,1,0,0},{0,0,0,0,0},{0,0,1,-1,0},{0,0,1,0,-1},{-1,0,0,1,0},{0,-1,0,1,0},{0,0,-1,1,0},{0,0,0,0,0},{0,0,0,1,-1},
    {-1,0,0,0,1},{0,-1,0,0,1},{0,0,-1,0,1},{0,0,0,-1,1},{0,0,0,0,0},
};
static const u8 *const sIVRankImages[] = {
    [0 ... 2] = sIVRankings_E_MinusTiles, [3 ... 4] = sIVRankings_ETiles, [5 ... 6] = sIVRankings_E_PlusTiles,
    [7 ... 8] = sIVRankings_D_MinusTiles, [9 ... 10] = sIVRankings_DTiles, [11 ... 12] = sIVRankings_D_PlusTiles,
    [13 ... 14] = sIVRankings_C_MinusTiles, [15 ... 16] = sIVRankings_CTiles, [17 ... 18] = sIVRankings_C_PlusTiles,
    [19 ... 20] = sIVRankings_B_MinusTiles, [21 ... 22] = sIVRankings_BTiles, [23 ... 24] = sIVRankings_B_PlusTiles,
    [25 ... 26] = sIVRankings_A_MinusTiles, [27 ... 28] = sIVRankings_ATiles, [29 ... 30] = sIVRankings_A_PlusTiles,
    [31] = sIVRankings_STiles,
};
static const u8 sMultiOrder[] = { 0, 2, 3, 1, 4, 5 };
static const s8 sBob3[] = { -1, 0, 1 };
static const s8 sBob5[] = { -2, -1, 0, 1, 2 };
static const s8 sBob7[] = { -3, -2, -1, 0, 1, 2, 3 };
static const s8 sBob7b[] = { -5, -3, -1, 0, 1, 3, 5 };
static const s8 sEggShake11[] = { 1, 1, 0, -1, -1, 0, -1, -1, 0, 1, 1 };
static const s8 sEggShake11b[] = { 2, 1, 0, -1, -2, 0, -2, -1, 0, 1, 2 };
static const s8 sEggShake15[] = { 2, 1, 1, 0, -1, -1, -2, 0, -2, -1, -1, 0, 1, 1, 2 };
static const u8 sPPUpMask[] = { 0x03, 0x0C, 0x30, 0xC0 };
static const u16 *const sHpBarPals[] = { (const u16 *)gBwSsHpBarPal0, (const u16 *)gBwSsHpBarPal1, (const u16 *)gBwSsHpBarPal2 };

/* ---------------- 前方宣言 ---------------- */
static void CB2_Init(void);
static void CB2_Main(void);
static void VBlank(void);
static void Task_Main(u8 taskId);
static void Task_FlipPage(u8 taskId);
static void Task_ToMovesInfo(unusedArg u8 taskId);
static void Task_FromMovesInfo(unusedArg u8 taskId);
static void Task_MovesInfoInput(unusedArg u8 taskId);
static void Task_SelectMove(u8 taskId);
static void Task_ChangeMon(u8 taskId);
static void Exit(u8 taskId);
static void PlayMonCry(void);
static void AddWindowsForPage(u8 page);
static void RemoveAllWindows(void);
static void SetHelpContextForPage(void);
static void PrintPageTitle(const u8 *str);
static void PrintControls(const u8 *str);
static void PrintNickLevel(void);
static void PrintWin3(void);
static void PrintWin4(void);
static void PrintWin5(void);
static void DrawTypeIcons(void);
static void BwSs_LoadIconColours(void);
static void blit_move_info_icon_bw(u8 windowId, u8 iconId, u16 x, u16 y);
static void PrintTitleAndControls(u8 page);
static void LoadPageTilemaps(void);
static void BeforeSlide(void);
static void AfterSlide(void);
static void StartSlide(void);
static bool8 SlideDone(void);
static bool8 LoadGfx(void);
static bool8 LoadSprites(void);
static bool8 GatherData(void);
static void GetMonInfo(void);
static void GetMonStats(void);
static void GetAllMoves(void);
static void GetMove(u8 i);
static u8 GetAilment(u32 status);
static void CopyMonFromList(struct Pokemon *mon);
static void SwapMovesParty(void);
static void SwapMovesBox(void);
static void TryChangeMon(unusedArg u8 taskId, s8 delta);
static void LoadShiny(void);
static void LoadDot(void);
static void LoadMarkings(void);
static void LoadCursor(void);
static void LoadStatusIcon(void);
static void LoadHpBar(void);
static void LoadExpBar(void);
static void LoadBall(void);
static void LoadIcon(void);
static void LoadMonPic(void);
static void FreeAllSprites(void);
static void RefreshSprites(void);
static void SetupHpBar(void);
static void SetupExpBar(void);
static void ShowStatusIcon(void);
static void SetInvisMonPic(u8 inv);
static void SetInvisMarkings(u8 inv);
static void SetInvisBall(u8 inv);
static void SetInvisStatus(u8 inv);
static void SetInvisHpBar(u8 inv);
static void SetInvisExpBar(u8 inv);
static void SetInvisCursor(u8 inv);
static void SetInvisIcon(u8 inv);
static void SetInvisDot(u8 inv);
static void SetInvisShiny(u8 inv);
static void SetMonPicAnim(u16 spriteId);
static void UpdateDot(void);
static void UpdateShiny(void);
static void SpriteCB_Cursor(struct Sprite *sprite);
static void SpriteCB_MonBob(struct Sprite *sprite);
static void SpriteCB_EggShake(struct Sprite *sprite);
static void SpriteCB_None(unusedArg struct Sprite *sprite);
static bool32 IsMetLocationValid(u8 place);
static bool32 IsMultiPartner(void);
static bool32 NatureNeedsNoNa(u8 nature);
static bool32 IsFromGenIII(void);
static u8 IsOriginalTrainer(struct Pokemon *mon);
static void PrintTrainerMemo(void);
static void PrintTrainerMemoForeign(void);
static void PrintEggMemo(void);
static void PrintEggInfo(void);
static void PrintMonInfo(void);
static void PrintMonStats(void);
static void PrintMoveNames(void);
static void PrintMoveNameAndPP(u8 i);
static void PrintExpLabels(void);
static void PrintMoveInfo(void);
static void PrintAbility(void);
static void DrawMoveTypeIcons(void);
static bool8 CanForgetSelectedMove(void);
static void ScrollBackground(void);

/* ---------------- 入口 ---------------- */
void HackMomoIce_RefreshParty(void); //[hack] 2026-10-05 ももアイスを持たせた／外したあとの能力（src/hack_momo_ice.c）

void BwShowPokemonSummaryScreen(struct Pokemon *party, u8 cursorPos, u8 lastIdx, MainCallback savedCallback, u8 mode)
{
    HackMomoIce_RefreshParty();
    sBwSs = AllocZeroed(sizeof(struct BwSummaryScreen));
    sXpos = AllocZeroed(sizeof(struct BwSsXpos));
    if (sBwSs == NULL)
    {
        SetMainCallback2(savedCallback);
        return;
    }
    sLastViewedMonIndex = cursorPos;
    sMoveCursor = 0;
    sMoveSwapPos = 0;
    sBwSs->savedCallback = savedCallback;
    sBwSs->monList.mons = party;
    sBwSs->isEnemyParty = (party == gEnemyParty);
    sBwSs->lastIndex = lastIdx;
    sBwSs->mode = mode;
    switch (mode)
    {
    case PSS_MODE_BOX:
        BwSs_SetHelpContext(HELPCONTEXT_POKEMON_INFO);
        sBwSs->curPageIndex = PSS_PAGE_INFO;
        sBwSs->isBoxMon = TRUE;
        sBwSs->lockMovesFlag = FALSE;
        break;
    case PSS_MODE_SELECT_MOVE:
    case PSS_MODE_FORGET_MOVE:
        BwSs_SetHelpContext(HELPCONTEXT_POKEMON_MOVES);
        sBwSs->curPageIndex = PSS_PAGE_MOVES_INFO;
        sBwSs->isBoxMon = FALSE;
        sBwSs->lockMovesFlag = TRUE;
        break;
    default:
        BwSs_SetHelpContext(HELPCONTEXT_POKEMON_INFO);
        sBwSs->curPageIndex = PSS_PAGE_INFO;
        sBwSs->isBoxMon = FALSE;
        sBwSs->lockMovesFlag = FALSE;
        break;
    }
    sBwSs->unk323C = 2;
    sBwSs->unk3240 = 1;
    sBwSs->gmaxSpriteId = MAX_SPRITES;
    CopyMonFromList(&sBwSs->currentMon);
    sBwSs->isEgg = GetMonData(&sBwSs->currentMon, MON_DATA_IS_EGG, NULL);
    sBwSs->isBadEgg = GetMonData(&sBwSs->currentMon, MON_DATA_SANITY_IS_BAD_EGG, NULL);
    if (sBwSs->isBadEgg)
        sBwSs->isEgg = TRUE;
    sBwSs->queuedInput[0] = 0xFF;
    SetMainCallback2(CB2_Init);
}

void BwShowSelectMovePokemonSummaryScreen(struct Pokemon *party, u8 cursorPos, u8 lastIdx, MainCallback savedCallback, u16 newMove)
{
    BwShowPokemonSummaryScreen(party, cursorPos, lastIdx, savedCallback, PSS_MODE_SELECT_MOVE);
    sBwSs->moves[4] = newMove;
}

void BwSetPokemonSummaryScreenMode(u8 mode)
{
    if (sBwSs != NULL)          /* 素は解放済みの入れ物にも書いていた。こちらは Exit で NULL にする */
        sBwSs->mode = mode;
}

/* ---------------- 入力（ページ送り） ---------------- */
static bool32 IsPageTurnLocked(u8 dir)
{
    return sBwSs->pageLocked == TRUE && sBwSs->pageDir != dir;
}

static bool32 CheckPageInput(u8 dir)
{
    if (sBwSs->isEgg)
        return FALSE;
    if (sBwSs->queuedInput[0] != 0xFF && sBwSs->queuedInput[0] == dir)
    {
        sBwSs->queuedInput[0] = 0xFF;
        return TRUE;
    }
    if (IsPageTurnLocked(dir))
        return FALSE;
    if (dir == 1)
    {
        if (JOY_NEW(DPAD_RIGHT))
            return TRUE;
        if (gSaveBlock2->optionsButtonMode == OPTIONS_BUTTON_MODE_LR && JOY_NEW(R_BUTTON))
            return TRUE;
    }
    else
    {
        if (JOY_NEW(DPAD_LEFT))
            return TRUE;
        if (gSaveBlock2->optionsButtonMode == OPTIONS_BUTTON_MODE_LR && JOY_NEW(L_BUTTON))
            return TRUE;
    }
    return FALSE;
}

static void Task_Main(u8 taskId)
{
    switch (sBwSs->mainState)
    {
    case BWSS_FADEIN:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, 0);
        sBwSs->mainState = BWSS_PLAYCRY;
        break;
    case BWSS_PLAYCRY:
        if (!gPaletteFade->active)
        {
            PlayMonCry();
            sBwSs->mainState = BWSS_HANDLEINPUT;
        }
        break;
    case BWSS_HANDLEINPUT:
        if (BwSs_MenuHelpers_CallLinkSomething() == TRUE || BwSs_IsLinkRecvQueueLengthAtLeast3() == TRUE || FuncIsActiveTask(Task_ChangeMon))
            return;
        if (sBwSs->curPageIndex != PSS_PAGE_MOVES_INFO)
        {
            if (CheckPageInput(1) == TRUE)
            {
                if (FuncIsActiveTask(Task_FlipPage))
                {
                    sBwSs->queuedInput[0] = 1;
                }
                else if (sBwSs->curPageIndex < PSS_PAGE_MOVES)
                {
                    PlaySE(SE_SELECT);
                    sBwSs->pageDir = 1;
                    RemoveAllWindows();
                    sBwSs->curPageIndex++;
                    sBwSs->mainState = BWSS_FLIPPAGES;
                }
                return;
            }
            if (CheckPageInput(0) == TRUE)
            {
                if (FuncIsActiveTask(Task_FlipPage))
                {
                    sBwSs->queuedInput[0] = 0;
                }
                else if (sBwSs->curPageIndex > PSS_PAGE_INFO)
                {
                    PlaySE(SE_SELECT);
                    sBwSs->pageDir = 0;
                    RemoveAllWindows();
                    sBwSs->curPageIndex--;
                    sBwSs->mainState = BWSS_FLIPPAGES;
                }
                return;
            }
        }
        if (!FuncIsActiveTask(Task_FlipPage) || FuncIsActiveTask(Task_ChangeMon))
        {
            if (JOY_NEW(DPAD_UP))
            {
                TryChangeMon(taskId, -1);
            }
            else if (JOY_NEW(DPAD_DOWN))
            {
                TryChangeMon(taskId, 1);
            }
            else if (JOY_NEW(SELECT_BUTTON) && sBwSs->curPageIndex == PSS_PAGE_SKILLS && !sBwSs->isEgg)
            {
                PlaySE(SE_SELECT);
                sBwSs->showEvIv ^= 1;
                FillWindowPixelBuffer(sBwSs->window[3], 0);
                PrintMonStats();
                PutWindowTilemap(sBwSs->window[3]);
                CopyWindowToVram(sBwSs->window[3], COPYWIN_GFX);
            }
            else if (JOY_NEW(A_BUTTON))
            {
                if (sBwSs->curPageIndex == PSS_PAGE_INFO)
                {
                    PlaySE(SE_SELECT);
                    sBwSs->mainState = BWSS_EXIT_FADEOUT;
                }
                else if (sBwSs->curPageIndex == PSS_PAGE_SKILLS && SecondAbilityOfCurrentMon() != ABILITY_NONE)
                {
                    PlaySE(SE_SELECT);
                    sBwSs->showAbility2 ^= 1;     /* ★ 2026-10-03 とくせい ⇔ とくせい2 */
                    PrintAbility();
                    PutWindowTilemap(sBwSs->window[5]);
                    CopyWindowToVram(sBwSs->window[5], COPYWIN_GFX);
                }
                else if (sBwSs->curPageIndex == PSS_PAGE_MOVES)
                {
                    PlaySE(SE_SELECT);
                    sBwSs->pageDir = 1;
                    RemoveAllWindows();
                    sBwSs->curPageIndex++;
                    sBwSs->mainState = BWSS_FLIPPAGES;
                }
            }
            else if (JOY_NEW(B_BUTTON))
            {
                sBwSs->mainState = BWSS_EXIT_FADEOUT;
            }
        }
        break;
    case BWSS_FLIPPAGES:
        if (sBwSs->curPageIndex != PSS_PAGE_MOVES_INFO)
        {
            CreateTask(Task_FlipPage, 0);
            sBwSs->mainState = BWSS_HANDLEINPUT;
        }
        else
        {
            gTasks[sBwSs->task].func = Task_ToMovesInfo;
            sBwSs->mainState = BWSS_HANDLEINPUT;
        }
        break;
    case BWSS_EXIT_FADEOUT:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, 0);
        sBwSs->mainState = BWSS_EXIT_WAITLINK;
        break;
    case BWSS_EXIT_WAITLINK:
        SetGpuReg(REG_OFFSET_BLDCNT, 0);
        SetGpuReg(REG_OFFSET_BLDALPHA, 0);
        if (BwSs_LinkRecvQueueLengthMoreThan2() == TRUE || BwSs_IsLinkRecvQueueLengthAtLeast3() == TRUE)
            return;
        sBwSs->mainState = BWSS_EXIT_WAITFADE;
        break;
    default:
        if (!gPaletteFade->active)
            Exit(taskId);
        break;
    }
}

/* 情報 ⇔ 能力 ⇔ 技 のページ送り（横にスライド） */
static void Task_FlipPage(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    switch (data[0])
    {
    case 0:
        BeforeSlide();
        AfterSlide();
        sBwSs->lockMovesFlag = TRUE;
        sBwSs->pageLocked = TRUE;
        AddWindowsForPage(sBwSs->curPageIndex);
        break;
    case 1:
        break;
    case 2:
        /* ★ 2026-10-03 ユーザーの動画「描画が遅いってちらつきがある」: ここで新しいページの背景を出すのに、
           文字（BG0）は case 9 まで前のページのまま残り、7 コマほど「新しい背景に古い文字」が見えた。
           背景を替える時点で文字を隠し、case 9 で新しい文字ができてから出す */
        HideBg(0);
        LoadPageTilemaps();
        PrintTitleAndControls(sBwSs->curPageIndex);
        break;
    case 3:
        CopyWindowToVram(sBwSs->window[0], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[1], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[2], COPYWIN_GFX);
        break;
    case 4:
        break;
    case 5:
        StartSlide();
        sBwSs->sliding = TRUE;
        break;
    case 6:
        if (!SlideDone())
            return;
        break;
    case 7:
        PrintWin3();
        if (sBwSs->curPageIndex != PSS_PAGE_MOVES_INFO)
            PrintWin4();
        PrintWin5();
        DrawTypeIcons();
        break;
    case 8:
        CopyWindowToVram(sBwSs->window[3], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[4], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[5], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[6], COPYWIN_GFX);
        break;
    case 9:
        HideBg(0);
        if (IsDma3ManagerBusyWithBgCopy())
            return;
        CopyBgTilemapBufferToVram(0);
        ShowBg(0);
        break;
    default:
        SetHelpContextForPage();
        if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO)
            gTasks[sBwSs->task].func = Task_MovesInfoInput;
        DestroyTask(taskId);
        data[0] = 0;
        sBwSs->lockMovesFlag = FALSE;
        sBwSs->pageLocked = FALSE;
        return;
    }
    data[0]++;
}

/* 技 → 技の詳細 */
static void Task_ToMovesInfo(unusedArg u8 taskId)
{
    switch (sBwSs->movesState)
    {
    case 0:
        sBwSs->lockMovesFlag = TRUE;
        sBwSs->pageLocked = TRUE;
        AddWindowsForPage(sBwSs->curPageIndex);
        break;
    case 1:
        break;
    case 2:
        BeforeSlide();
        LoadPageTilemaps();
        break;
    case 3:
        PrintPageTitle(gBwSsText_PageMoves);
        if (!(gMain.inBattle || BWSS_RECEIVED_REMOTE_LINK_PLAYERS))
            PrintControls(gBwSsText_CtrlPickSwitch);
        else
            PrintControls(gBwSsText_CtrlPick);
        break;
    case 4:
        CopyWindowToVram(sBwSs->window[0], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[1], COPYWIN_GFX);
        break;
    case 5:
        break;
    case 6:
        PrintWin3();
        PrintWin5();
        CopyWindowToVram(sBwSs->window[3], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[5], COPYWIN_GFX);
        break;
    case 7:
        if (IsDma3ManagerBusyWithBgCopy())
            return;
        CopyBgTilemapBufferToVram(0);
        StartSlide();
        sBwSs->sliding = TRUE;
        break;
    case 8:
        if (!SlideDone())
            return;
        PrintWin4();
        CopyWindowToVram(sBwSs->window[4], COPYWIN_GFX);
        break;
    case 9:
        DrawTypeIcons();
        PrintNickLevel();
        break;
    case 10:
        AfterSlide();
        CopyWindowToVram(sBwSs->window[6], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[2], COPYWIN_GFX);
        break;
    case 11:
        if (IsDma3ManagerBusyWithBgCopy())
            return;
        CopyBgTilemapBufferToVram(0);
        break;
    default:
        SetHelpContextForPage();
        gTasks[sBwSs->task].func = Task_MovesInfoInput;
        sBwSs->movesState = 0;
        sBwSs->lockMovesFlag = FALSE;
        sBwSs->pageLocked = FALSE;
        return;
    }
    sBwSs->movesState++;
}

/* 技の詳細 → 技 */
static void Task_FromMovesInfo(unusedArg u8 taskId)
{
    switch (sBwSs->movesState)
    {
    case 0:
        sBwSs->lockMovesFlag = TRUE;
        sBwSs->pageLocked = TRUE;
        AddWindowsForPage(sBwSs->curPageIndex);
        break;
    case 1:
        break;
    case 2:
        break;
    case 3:
        PrintWin3();
        PrintWin4();
        PrintWin5();
        CopyWindowToVram(sBwSs->window[3], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[4], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[5], COPYWIN_GFX);
        CopyBgTilemapBufferToVram(0);
        break;
    case 4:
        PrintPageTitle(gBwSsText_PageMoves);
        PrintControls(gBwSsText_CtrlPageDetail);
        break;
    case 5:
        CopyWindowToVram(sBwSs->window[0], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[1], COPYWIN_GFX);
        break;
    case 6:
        StartSlide();
        sBwSs->sliding = TRUE;
        BeforeSlide();
        DrawTypeIcons();
        break;
    case 7:
        break;
    case 8:
        if (!SlideDone())
            return;
        PrintNickLevel();
        break;
    case 9:
        CopyWindowToVram(sBwSs->window[6], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[2], COPYWIN_GFX);
        CopyBgTilemapBufferToVram(0);
        break;
    case 10:
        LoadPageTilemaps();
        AfterSlide();
        break;
    default:
        SetHelpContextForPage();
        gTasks[sBwSs->task].func = Task_Main;
        sBwSs->movesState = 0;
        sBwSs->lockMovesFlag = FALSE;
        sBwSs->pageLocked = FALSE;
        return;
    }
    sBwSs->movesState++;
}

/* ---------------- スライド（bg の x） ---------------- */
static void StartSlide(void)
{
    s8 pageDelta = (sBwSs->pageDir == 1) ? -1 : 1;

    if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO)
    {
        sBwSs->slideX = 240;
        return;
    }
    if ((sBwSs->curPageIndex + pageDelta) == PSS_PAGE_MOVES_INFO)
    {
        sBwSs->slideX = 0;
        return;
    }
    if (sBwSs->pageDir == 1)
        sBwSs->slideX = 0;
    if (sBwSs->curPageIndex != PSS_PAGE_SKILLS)
    {
        SetInvisHpBar(1);
        SetInvisExpBar(1);
    }
    else
    {
        SetInvisHpBar(0);
        SetInvisExpBar(0);
    }
}

static void BeforeSlide(void)
{
    u8 newPage = (sBwSs->pageDir == 1) ? sBwSs->curPageIndex - 1 : sBwSs->curPageIndex + 1;

    switch (newPage)
    {
    case PSS_PAGE_INFO:
        SetInvisHpBar(1);
        SetInvisExpBar(1);
        break;
    case PSS_PAGE_SKILLS:
        SetInvisHpBar(0);
        SetInvisExpBar(0);
        break;
    case PSS_PAGE_MOVES:
        if (sBwSs->pageDir == 1)
        {
            SetInvisMonPic(1);
            SetInvisMarkings(1);
            SetInvisBall(1);
            SetInvisStatus(1);
            SetInvisDot(1);
            SetInvisShiny(1);
            SetInvisHpBar(1);
            SetInvisExpBar(1);
        }
        break;
    case PSS_PAGE_MOVES_INFO:
        SetInvisCursor(1);
        SetInvisIcon(1);
        SetInvisStatus(1);
        SetInvisDot(1);
        SetInvisShiny(1);
        SetInvisHpBar(1);
        SetInvisExpBar(1);
        break;
    }
}

static void AfterSlide(void)
{
    u8 newPage = (sBwSs->pageDir == 1) ? sBwSs->curPageIndex - 1 : sBwSs->curPageIndex + 1;

    switch (newPage)
    {
    case PSS_PAGE_INFO:
        SetInvisHpBar(1);
        SetInvisExpBar(1);
        break;
    case PSS_PAGE_MOVES:
        if (sBwSs->pageDir != 0)
        {
            SetInvisCursor(0);
            SetInvisDot(0);
            SetInvisIcon(0);
            SetInvisShiny(0);
        }
        break;
    case PSS_PAGE_MOVES_INFO:
        SetInvisMonPic(0);
        SetInvisMarkings(0);
        SetInvisStatus(0);
        SetInvisBall(0);
        SetInvisDot(0);
        SetInvisShiny(0);
        break;
    }
}

static bool8 SlideDone(void)
{
    s8 pageDelta = (sBwSs->pageDir == 1) ? -1 : 1;

    if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO && sBwSs->slideX <= 0)
    {
        sBwSs->slideX = 0;
        sBwSs->unk3238 ^= 1;
        sBwSs->sliding = FALSE;
        return TRUE;
    }
    if ((sBwSs->curPageIndex + pageDelta) == PSS_PAGE_MOVES_INFO && sBwSs->slideX >= 240)
    {
        sBwSs->slideX = 240;
        sBwSs->unk3238 ^= 1;
        sBwSs->sliding = FALSE;
        return TRUE;
    }
    if (sBwSs->pageDir == 1)
    {
        if (sBwSs->slideX >= 240)
        {
            sBwSs->slideX = 240;
            sBwSs->unk3238 ^= 1;
            sBwSs->sliding = FALSE;
            return TRUE;
        }
    }
    else if (sBwSs->slideX <= 0)
    {
        sBwSs->unk3238 ^= 1;
        sBwSs->slideX = 0;
        sBwSs->sliding = FALSE;
        return TRUE;
    }
    return FALSE;
}

static void SlideIn(void)
{
    if (sBwSs->slideX < 240)
    {
        sBwSs->slideX += 240;
        if (sBwSs->slideX > 240)
            sBwSs->slideX = 240;
    }
}

static void SlideOut(void)
{
    if (sBwSs->slideX >= 240)
    {
        sBwSs->slideX -= 240;
        if (sBwSs->slideX < 0)
            sBwSs->slideX = 0;
    }
}

static void UpdateSlide(void)
{
    if (sBwSs->pageDir == 1)
    {
        if (sBwSs->curPageIndex != PSS_PAGE_MOVES_INFO)
            SlideIn();
        else
            SlideOut();
    }
    else
    {
        if (sBwSs->curPageIndex != PSS_PAGE_MOVES)
            SlideOut();
        else
            SlideIn();
    }
}

/* ---------------- 起動 ---------------- */
static void CB2_Init(void)
{
    switch (sBwSs->initState)
    {
    case 0:
        SetVBlankCallback(NULL);
        SetHBlankCallback(NULL);
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
        DeactivateAllTextPrinters();
        SetGpuReg(REG_OFFSET_BLDCNT, BLDCNT_EFFECT_BLEND | BLDCNT_TGT1_BG1 | BLDCNT_TGT2_BG2);
        SetGpuReg(REG_OFFSET_BLDALPHA, BLDALPHA_BLEND(13, 16));
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON);
        ShowBg(0); ShowBg(1); ShowBg(2); ShowBg(3);
        break;
    case 2:
        ResetSpriteData();
        ResetPaletteFade();
        FreeAllSpritePalettes();
        ScanlineEffect_Stop();
        break;
    case 3:
        if (!LoadGfx())
            return;
        break;
    case 4:
        if (!LoadSprites())
            return;
        break;
    case 5:
        InitWindows(sWinDummy);
        AddWindowsForPage(sBwSs->curPageIndex);
        break;
    case 6:
        if (!GatherData())
            return;
        break;
    case 7:
        PrintWin3();
        break;
    case 8:
        PrintWin4();
        break;
    case 9:
        PrintWin5();
        DrawTypeIcons();
        break;
    case 10:
        LoadPageTilemaps();
        break;
    case 11:
        break;
    case 12:
        BlendPalettes(0xFFFFFFFF, 16, 0);
        PrintTitleAndControls(sBwSs->curPageIndex);
        PutWindowTilemap(sBwSs->window[0]);
        PutWindowTilemap(sBwSs->window[1]);
        PutWindowTilemap(sBwSs->window[2]);
        break;
    case 13:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, 0);
        {
            u8 i;
            for (i = 0; i < 7; i++)
                CopyWindowToVram(sBwSs->window[i], COPYWIN_GFX);
        }
        break;
    case 14:
        CopyBgTilemapBufferToVram(0);
        break;
    case 15:
        if (sBwSs->mode == PSS_MODE_SELECT_MOVE || sBwSs->mode == PSS_MODE_FORGET_MOVE)
        {
            SetInvisIcon(0);
            SetInvisCursor(0);
        }
        else
        {
            SetInvisMonPic(0);
            SetInvisMarkings(0);
            SetInvisBall(0);
        }
        SetInvisHpBar(1);
        SetInvisExpBar(1);
        SetInvisStatus(0);
        SetInvisDot(0);
        SetInvisShiny(0);
        break;
    default:
        SetVBlankCallback(VBlank);
        if (sBwSs->mode == PSS_MODE_SELECT_MOVE || sBwSs->mode == PSS_MODE_FORGET_MOVE)
            sBwSs->task = CreateTask(Task_SelectMove, 0);
        else
            sBwSs->task = CreateTask(Task_Main, 0);
        SetMainCallback2(CB2_Main);
        return;
    }
    sBwSs->initState++;
}

static bool8 LoadGfx(void)
{
    switch (sBwSs->gfxState)
    {
    case 0:
        LoadPalette(gBwSsBgPal, 0, 0x20);
        break;
    case 1:
        LoadPalette(gBwSsMenuPal, 0x60, 0x20);       /* 窓のパレット 6: 文字とタイプの札（BW の配色） */
        BwSs_LoadIconColours();                       /* 空き番号に CFRU の札の色（2026-10-04） */
        LoadPalette(gBwSsTextPal7, 0x70, 0x20);
        break;
    case 2:
        LZ77UnCompVram(gBwSsTiles, (void *)VRAM);
        break;
    case 3:
    case 4:
        LZ77UnCompVram(gBwSsMapBg, (void *)(VRAM + 0xF800));
        break;
    case 5:
        LoadPalette(gBwSsPal8, 0x80, 0x20);
        return TRUE;
    }
    sBwSs->gfxState++;
    return FALSE;
}

static bool8 GatherData(void)
{
    switch (sBwSs->dataState)
    {
    case 0:
        GetMonInfo();
        if (sBwSs->isEgg)
        {
            sBwSs->dataState = 0;
            return TRUE;
        }
        break;
    case 1:
        GetMonStats();
        break;
    case 2:
        GetAllMoves();
        break;
    default:
        sBwSs->dataState = 0;
        return TRUE;
    }
    sBwSs->dataState++;
    return FALSE;
}

static void CB2_Main(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
    ScrollBackground();
}

static void VBlank(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    if (sBwSs->sliding)
        UpdateSlide();
}

static void ScrollBackground(void)
{
    ChangeBgX(2, 80, 1);
    ChangeBgY(2, 80, 1);
}

static void Exit(u8 taskId)
{
    FreeAllSprites();
    FreeAllSpritePalettes();
    if (IsCryPlayingOrClearCrySongs() == TRUE)
        StopCryAndClearCrySongs();
    RemoveAllWindows();
    FreeAllWindowBuffers();
    DestroyTask(taskId);
    SetMainCallback2(sBwSs->savedCallback);
    FREE_IF_SET(sBwSs);
    FREE_IF_SET(sXpos);
}

static void PlayMonCry(void)
{
    if (!GetMonData(&sBwSs->currentMon, MON_DATA_IS_EGG, NULL))
    {
        if (BwSs_ShouldPlayNormalPokeCry(&sBwSs->currentMon) == TRUE)
            PlayCry3(GetMonData(&sBwSs->currentMon, MON_DATA_SPECIES2, NULL), 0, 0);
        else
            PlayCry3(GetMonData(&sBwSs->currentMon, MON_DATA_SPECIES2, NULL), 0, 11);
    }
}

/* ---------------- 窓と文字 ---------------- */
static void AddWindowsForPage(u8 page)
{
    u8 i;

    for (i = 0; i < 3; i++)
        sBwSs->window[i] = AddWindow(&sWinTop[i]);
    for (i = 0; i < 4; i++)
    {
        switch (page)
        {
        case PSS_PAGE_INFO:
            sBwSs->window[i + 3] = AddWindow(&sWinInfo[i]);
            break;
        case PSS_PAGE_MOVES:
        case PSS_PAGE_MOVES_INFO:
            sBwSs->window[i + 3] = AddWindow(&sWinMoves[i]);
            break;
        default:
            sBwSs->window[i + 3] = AddWindow(&sWinSkills[i]);
            break;
        }
    }
}

static void RemoveAllWindows(void)
{
    u8 i;

    for (i = 0; i < 7; i++)
        RemoveWindow(sBwSs->window[i]);
}

static void SetHelpContextForPage(void)
{
    switch (sBwSs->curPageIndex)
    {
    case PSS_PAGE_INFO:
        BwSs_SetHelpContext(HELPCONTEXT_POKEMON_INFO);
        break;
    case PSS_PAGE_SKILLS:
        BwSs_SetHelpContext(HELPCONTEXT_POKEMON_SKILLS);
        break;
    default:
        BwSs_SetHelpContext(HELPCONTEXT_POKEMON_MOVES);
        break;
    }
}

static void Print(u8 win, u8 x, u8 y, u8 colour, const u8 *str)
{
    AddTextPrinterParameterized3(sBwSs->window[win], BWSS_FONT, x, y, sTextColours[colour], TEXT_SPEED_FF, str);
}

static void PrintTight(u8 win, u8 x, u8 y, u8 colour, const u8 *str)
{
    AddTextPrinterParameterized4(sBwSs->window[win], BWSS_FONT, x, y, 0, (u8)-2, sTextColours[colour], TEXT_SPEED_FF, str);
}

/* 2026-10-09 4 けた（ももアイス・Lv200）: 右端 base + field に右寄せ。入らなければ左へずらす（字はふつうの大きさのまま・
   ユーザー「個体値表示欄と重なっているHPは前のほうがよかった」で小さい字はやめた）。のうりょくのページの窓は 18 タイル（144）に広げた */
static void PrintFit(u8 win, u8 base, u8 field, u8 y, u8 colour, const u8 *str)
{
    s32 w = GetStringWidth(BWSS_FONT, str, 0);
    Print(win, (w < base + field) ? base + field - w : 0, y, colour, str);
}

/* 右寄せの分（英字 6 画素の決め打ちではなく実際の幅で） */
static u16 RightAlign(u8 fieldWidth, const u8 *str)
{
    s32 w = GetStringWidth(BWSS_FONT, str, 0);
    return (w >= fieldWidth) ? 0 : fieldWidth - w;
}

static void PrintPageTitle(const u8 *str)
{
    FillWindowPixelBuffer(sBwSs->window[0], 0);
    Print(0, 4, 1, WHITE_TITLE, str);
    PutWindowTilemap(sBwSs->window[0]);
}

static void PrintControls(const u8 *str)
{
    s32 width = GetStringWidth(BWSS_FONT, str, 0);

    FillWindowPixelBuffer(sBwSs->window[1], 0);
    Print(1, (width >= 116) ? 0 : 116 - width, 1, WHITE_TITLE, str);     /* 右端は今までと同じ x236（窓を 2 タイル左へ広げた分だけ基準を足す） */
    PutWindowTilemap(sBwSs->window[1]);
}

static void PrintNickLevel(void)
{
    FillWindowPixelBuffer(sBwSs->window[2], 0);
    if (!sBwSs->isEgg)
    {
        if (sBwSs->curPageIndex != PSS_PAGE_MOVES_INFO)
        {
            Print(2, 8, 14, DARK, sBwSs->summary.level);
            #ifdef TERASTAL_FEATURE
            blit_move_info_icon_bw(sBwSs->window[2], sBwSs->currentMon.teratype + 1, 48, 15);   /* テラスタイプ（画面 208〜240） */
            #endif
        }
        else
        {
            blit_move_info_icon_bw(sBwSs->window[2], sBwSs->monTypes[0] + 1, 6, 16);
            if (sBwSs->monTypes[0] != sBwSs->monTypes[1])
                blit_move_info_icon_bw(sBwSs->window[2], sBwSs->monTypes[1] + 1, 38, 16);
        }
        /* 左端 16 画素はボールのスプライト（実機 1 回目: テラスタイプの札とボールが重なった → ボールを名前の横へ） */
        {
            s32 gx = 18 + GetStringWidth(BWSS_FONT, sBwSs->summary.nickname, 0) + 2;

            if (gx > 72)
                gx = 72;
            Print(2, 18, 2, DARK, sBwSs->summary.nickname);
            if (GetMonGender(&sBwSs->currentMon) == MON_FEMALE)
                Print(2, gx, 2, RED, sBwSs->summary.genderSymbol);
            else
                Print(2, gx, 2, BLUE, sBwSs->summary.genderSymbol);
        }
    }
    PutWindowTilemap(sBwSs->window[2]);
}

static void PrintWin3(void)
{
    FillWindowPixelBuffer(sBwSs->window[3], 0);
    switch (sBwSs->curPageIndex)
    {
    case PSS_PAGE_INFO:
        PrintMonInfo();
        break;
    case PSS_PAGE_SKILLS:
        PrintMonStats();
        break;
    case PSS_PAGE_MOVES:
    case PSS_PAGE_MOVES_INFO:
        PrintMoveNames();
        break;
    }
    PutWindowTilemap(sBwSs->window[3]);
}

static void PrintWin4(void)
{
    FillWindowPixelBuffer(sBwSs->window[4], 0);
    switch (sBwSs->curPageIndex)
    {
    case PSS_PAGE_INFO:
        if (!sBwSs->isEgg)
            PrintEggMemo();          /* 名前は素のまま: たまごでないときのトレーナーメモ */
        else
            PrintEggInfo();
        break;
    case PSS_PAGE_SKILLS:
        PrintExpLabels();
        break;
    case PSS_PAGE_MOVES_INFO:
        PrintMoveInfo();
        break;
    }
    PutWindowTilemap(sBwSs->window[4]);
}

static void PrintWin5(void)
{
    switch (sBwSs->curPageIndex)
    {
    case PSS_PAGE_SKILLS:
        PrintAbility();
        break;
    case PSS_PAGE_MOVES:
    case PSS_PAGE_MOVES_INFO:
        DrawMoveTypeIcons();
        break;
    }
    PutWindowTilemap(sBwSs->window[5]);
}

/* ★ 2026-10-04 ユーザーの画像「BWメニューのタイプアイコンの色がおかしい」（かくとうが桃色・ゴーストが茶色・あくが灰色）:
   タイプの札の絵は CFRU の Type_Icons（色の番号は CFRU の並び・graphics/Other/Type_Icons/Type_Icons.bmp）なのに、
   窓のパレット 6 は BW の配色（expansion の絵の並び）だった。文字が使う番号（1 2 3 6 7 13 14 15）はそのままにして、
   空いている番号（4 5 8 9 10 11 12）に CFRU の色を入れ、札を描くときに色の番号を付け替える（素の blit_move_info_icon の代わり）。
   パレット 6 を使うのは窓だけ（BG の地図は全部パレット 0）なので、空き番号を変えても ほかに響かない。 */
struct BwSsInfoIcon { u8 width; u8 height; u16 offset; };
extern const struct BwSsInfoIcon gMoveMenuInfoIcons[];   /* assembly/data/type_tables.s */
extern const u8 Type_IconsTiles[];                       /* graphics/Other/Type_Icons */

/* CFRU の色の番号 → パレット 6 の番号（1 2 3 7 13 は BW の同じ色に、ほかは空き番号へ） */
static const u8 sBwSsIconColourMap[16] = {0, 1, 8, 3, 4, 5, 6, 7, 9, 12, 13, 11, 2, 10, 14, 15};
/* 空き番号に入れる CFRU の色（RGB555）: [4] d8e030 [8] a45239 [9] a040a0 [10] 507888 [11] b8a038 [12] f6b4f6 */
static const u16 sBwSsIconExtraColours[][2] = {
    {4, RGB(27, 28, 6)}, {8, RGB(20, 10, 7)}, {9, RGB(20, 8, 20)}, {10, RGB(10, 15, 17)},
    {11, RGB(23, 20, 7)}, {12, RGB(30, 22, 30)},
};

static void BwSs_LoadIconColours(void)
{
    u32 i;

    for (i = 0; i < NELEMS(sBwSsIconExtraColours); ++i)
        LoadPalette(&sBwSsIconExtraColours[i][1], 0x60 + sBwSsIconExtraColours[i][0], 2);
}

static void blit_move_info_icon_bw(u8 windowId, u8 iconId, u16 x, u16 y)
{
    u8 buf[8 * 2 * 32];     /* 横 8 タイル x 縦 2 タイルまで（札は 32 x 12） */
    const struct BwSsInfoIcon *icon = &gMoveMenuInfoIcons[iconId];
    u32 tw = (icon->width + 7) / 8, th = (icon->height + 7) / 8;
    u32 tx, ty, i;

    if (tw > 8 || th > 2)
    {
        blit_move_info_icon(windowId, iconId, x, y);
        return;
    }
    for (ty = 0; ty < th; ++ty)
    {
        for (tx = 0; tx < tw; ++tx)
        {
            const u8 *src = Type_IconsTiles + (icon->offset + ty * 16 + tx) * 32;   /* 絵は横 128 画素＝16 タイル */
            u8 *dst = buf + (ty * tw + tx) * 32;

            for (i = 0; i < 32; ++i)
                dst[i] = sBwSsIconColourMap[src[i] & 0xF] | (sBwSsIconColourMap[src[i] >> 4] << 4);
        }
    }
    BlitBitmapRectToWindow(windowId, buf, 0, 0, tw * 8, th * 8, x, y, icon->width, icon->height);
}

static void DrawTypeIcons(void)
{
    if (sBwSs->curPageIndex == PSS_PAGE_INFO && !sBwSs->isEgg)
    {
        blit_move_info_icon_bw(sBwSs->window[3], sBwSs->monTypes[0] + 1, 78, 33);
        if (sBwSs->monTypes[0] != sBwSs->monTypes[1])
            blit_move_info_icon_bw(sBwSs->window[3], sBwSs->monTypes[1] + 1, 110, 33);
    }
}

static void PrintTitleAndControls(u8 page)
{
    switch (page)
    {
    case PSS_PAGE_INFO:
        PrintPageTitle(gBwSsText_PageInfo);
        PrintControls(sBwSs->isEgg ? gBwSsText_CtrlCancel : gBwSsText_CtrlPageCancel);
        break;
    case PSS_PAGE_SKILLS:
        PrintPageTitle(gBwSsText_PageSkills);
        PrintControls(SecondAbilityOfCurrentMon() != ABILITY_NONE ? gBwSsText_CtrlPageAbility : gBwSsText_CtrlPage);
        break;
    case PSS_PAGE_MOVES:
        PrintPageTitle(gBwSsText_PageMoves);
        PrintControls(gBwSsText_CtrlPageDetail);
        break;
    case PSS_PAGE_MOVES_INFO:
        PrintPageTitle(gBwSsText_PageMoves);
        PrintControls(gMain.inBattle ? gBwSsText_CtrlPick : gBwSsText_CtrlPickSwitch);
        break;
    case PSS_PAGE_MOVE_DELETER:
        PrintPageTitle(gBwSsText_PageMoves);
        PrintControls(gBwSsText_CtrlPickDelete);
        break;
    }
    PrintNickLevel();
}

/* 技の枠の地図（BG の 0xE000）。★ 2026-10-03 ユーザー「あたらしい技を覚えるときその技の枠がない」:
   技を覚えるとき（PSS_MODE_SELECT_MOVE）は 5 つ目の枠がある地図（bg_moves_select.bin・LZ 無し）にする */
static void LoadMovesBoxTilemap(void)
{
    if (sBwSs->mode == PSS_MODE_SELECT_MOVE)
        CpuCopy16(gBwSsMapMovesSelect, (void *)(VRAM + 0xE000), 1280);
    else
        LZ77UnCompVram(gBwSsMapMoves, (void *)(VRAM + 0xE000));
}

static void LoadPageTilemaps(void)
{
    switch (sBwSs->curPageIndex)
    {
    case PSS_PAGE_INFO:
        LZ77UnCompVram(sBwSs->isEgg ? gBwSsMapEgg : gBwSsMapInfo, (void *)(VRAM + 0xF000));
        SetInvisHpBar(1);
        SetInvisExpBar(1);
        break;
    case PSS_PAGE_SKILLS:
        LZ77UnCompVram(gBwSsMapSkills, (void *)(VRAM + 0xF000));
        SetInvisHpBar(0);
        SetInvisExpBar(0);
        HideBg(3);
        break;
    case PSS_PAGE_MOVES:
        LZ77UnCompVram(gBwSsMapKnownMoves, (void *)(VRAM + 0xF000));
        LoadMovesBoxTilemap();
        SetInvisHpBar(1);
        SetInvisExpBar(1);
        ShowBg(3);
        break;
    case PSS_PAGE_MOVES_INFO:
        LZ77UnCompVram(gBwSsMapMovesInfo, (void *)(VRAM + 0xF000));
        LoadMovesBoxTilemap();
        SetInvisHpBar(1);
        SetInvisExpBar(1);
        ShowBg(3);
        break;
    }
}

/* ---- 情報ページ ---- */
static void PrintMonInfo(void)
{
    if (!sBwSs->isEgg)
    {
        u32 otId = GetMonData(&sBwSs->currentMon, MON_DATA_OT_ID, NULL);
        u32 pid = GetMonData(&sBwSs->currentMon, MON_DATA_PERSONALITY, NULL);

        Print(3, 80, 20, DARK, sBwSs->summary.speciesName);
        Print(3, 10, 7, WHITE, gBwSsText_DexNo);
        Print(3, 10, 19, WHITE, gBwSsText_Name);
        Print(3, 10, 31, WHITE, gBwSsText_Type);
        Print(3, 10, 43, WHITE, gBwSsText_OT);
        Print(3, 10, 55, WHITE, gBwSsText_IdNo);
        Print(3, 10, 67, WHITE, gBwSsText_Item);
        /* 色違いなら番号を赤く */
        if ((HIHALF(otId) ^ LOHALF(otId) ^ HIHALF(pid) ^ LOHALF(pid)) < 8)
            Print(3, 80 + sXpos->dexNo, 8, RED, sBwSs->summary.dexNum);
        else
            Print(3, 80 + sXpos->dexNo, 8, DARK, sBwSs->summary.dexNum);
        /* 親の名前は親の性別で色分け（CFRU の PokeSum_PrintOTGender と同じ）。
           ★ 2026-10-07 直した: 親の性別は 1 ビット（0 = おとこ・1 = おんな）。前は MON_FEMALE（0xFE）と比べていて、いつも青だった */
        if (GetMonData(&sBwSs->currentMon, MON_DATA_OT_GENDER, NULL) != 0)
            Print(3, 80, 44, RED, sBwSs->summary.otName);
        else
            Print(3, 80, 44, BLUE, sBwSs->summary.otName);
        Print(3, 80, 56, DARK, sBwSs->summary.otId);
        Print(3, 80, 68, DARK, sBwSs->summary.heldItem);
    }
    else
    {
        u8 eggCycles = GetMonData(&sBwSs->currentMon, MON_DATA_FRIENDSHIP, NULL);
        u8 hatchMsg;

        if (eggCycles <= 5)
            hatchMsg = 3;
        else if (eggCycles <= 10)
            hatchMsg = 2;
        else if (eggCycles <= 40)
            hatchMsg = 1;
        else
            hatchMsg = 0;
        if (sBwSs->isBadEgg)
            hatchMsg = 0;
        /* ★ 2026-10-10 ユーザー「たまごの文字がずれる」: たまごの背景（gBwSsMapEgg）の線は画面の y 20・32（濃い）・44…92（濃い）＝12 画素ごと。
           窓 3 は y 16 から。字の上を線にそろえる（前は 4 画素下で、y 32 の線が「タマゴ」の下を通っていた） */
        Print(3, 10, 4, WHITE, gBwSsText_Name);
        Print(3, 10, 17, WHITE, gBwSsText_Status);
        PrintTight(3, 80, 17, DARK, sEggHatchTexts[hatchMsg]);
        Print(3, 80, 4, DARK, sBwSs->summary.speciesName);
    }
}

/* ---- 能力ページ（性格の色・個体値ランクは CFRU-JP 独自） ---- */
static u8 GetNatureForStats(struct Pokemon *mon)
{
    if (mon->natureMint != 0)
        return mon->natureMint - 1;
    return GetNature(mon);
}

/* ★ 2026-10-11 ユーザー「銀の王冠で HP の個体値を鍛えた → 表示が何も変わらない」（patches/0312）:
   すごいとっくん（CFRU の special 0x12F）は個体値を書き換えず mon->hyperTraining のビットを立て、能力の計算（CalculateMonStatsNew）だけが
   それを 31 として読む。この画面は生の個体値を読んでいたので 20 のまま出ていた。計算と同じ読み方にする（ビットの並びは MON_DATA_HP_IV + i と同じ） */
static u8 GetIvForDisplay(struct Pokemon *mon, u8 ivField)
{
    if (mon->hyperTraining & gBitTable[ivField - MON_DATA_HP_IV])
        return 31;
    return GetMonData(mon, ivField, NULL);
}

static void DrawIvRank(u8 iv, u8 x, u8 y)
{
    BlitBitmapRectToWindow(sBwSs->window[3], sIVRankImages[iv & 31], 0, 0, 8, 8, x, y, 16, 8);
}

/* ★ 2026-10-10 bw-reloaded の EV/IV のページの中身を、能力のページの SELECT で（ページを足さない）。「努力値/個体値」・個体値 31 は赤 */
static void PrintEvIvRow(u8 y, u8 evField, u8 ivField, u8 x, u8 width)
{
    u8 text[12];
    u8 iv = GetIvForDisplay(&sBwSs->currentMon, ivField);
    u8 *p = ConvertIntToDecimalStringN(text, GetMonData(&sBwSs->currentMon, evField, NULL), STR_CONV_MODE_LEFT_ALIGN, 3);

    *p++ = 0xBA;   /* / */
    *p = EOS;
    ConvertIntToDecimalStringN(p, iv, STR_CONV_MODE_LEFT_ALIGN, 2);
    PrintFit(3, x, width, y, iv == 31 ? RED : DARK, text);
}

static void PrintMonStats(void)
{
    u8 nature = GetNatureForStats(&sBwSs->currentMon);
    const s8 *mod = sNatureStatTable[nature];
    struct Pokemon *mon = &sBwSs->currentMon;

    Print(3, 44, 0, WHITE, gBwSsText_HP);
    Print(3, 10, 79, WHITE, gBwSsText_ExpPoints);
    Print(3, 10, 91, WHITE, gBwSsText_ToNextLv);
    Print(3, 10, 19, WHITE + mod[0], gBwSsText_Attack);
    Print(3, 10, 31, WHITE + mod[1], gBwSsText_Defense);
    Print(3, 10, 43, WHITE + mod[3], gBwSsText_SpAtk);
    Print(3, 10, 55, WHITE + mod[4], gBwSsText_SpDef);
    Print(3, 10, 67, WHITE + mod[2], gBwSsText_Speed);
    /* ★ 2026-10-09 4 けた・8 けたで窓の右端（136）とランクの字（x120）にかかった → PrintFit（入らなければ字の間を詰める） */
    if (sBwSs->showEvIv)
    {
        PrintEvIvRow(0, MON_DATA_HP_EV, MON_DATA_HP_IV, 68, 74);
        PrintEvIvRow(20, MON_DATA_ATK_EV, MON_DATA_ATK_IV, 90, 50);
        PrintEvIvRow(32, MON_DATA_DEF_EV, MON_DATA_DEF_IV, 90, 50);
        PrintEvIvRow(44, MON_DATA_SPATK_EV, MON_DATA_SPATK_IV, 90, 50);
        PrintEvIvRow(56, MON_DATA_SPDEF_EV, MON_DATA_SPDEF_IV, 90, 50);
        PrintEvIvRow(68, MON_DATA_SPEED_EV, MON_DATA_SPEED_IV, 90, 50);
        PrintFit(3, 69, 72, 80, DARK, sBwSs->summary.expStr);
        PrintFit(3, 69, 72, 92, DARK, sBwSs->summary.toNextStr);
        return;
    }
    PrintFit(3, 68, 74, 0, DARK, sBwSs->summary.hpStr);       /* 右端 142（窓は 144） */
    PrintFit(3, 90, 27, 20, DARK, sBwSs->summary.statStr[PSS_STAT_ATK]);
    PrintFit(3, 90, 27, 32, DARK, sBwSs->summary.statStr[PSS_STAT_DEF]);
    PrintFit(3, 90, 27, 44, DARK, sBwSs->summary.statStr[PSS_STAT_SPA]);
    PrintFit(3, 90, 27, 56, DARK, sBwSs->summary.statStr[PSS_STAT_SPD]);
    PrintFit(3, 90, 27, 68, DARK, sBwSs->summary.statStr[PSS_STAT_SPE]);
    PrintFit(3, 69, 72, 80, DARK, sBwSs->summary.expStr);      /* 右端 141（実機 1 回目: 見出しと重なった・2026-10-09 ユーザー「経験値右にずらしたほうが」） */
    PrintFit(3, 69, 72, 92, DARK, sBwSs->summary.toNextStr);
    DrawIvRank(GetIvForDisplay(mon, MON_DATA_HP_IV), 10, 1);
    DrawIvRank(GetIvForDisplay(mon, MON_DATA_ATK_IV), 120, 21);
    DrawIvRank(GetIvForDisplay(mon, MON_DATA_DEF_IV), 120, 33);
    DrawIvRank(GetIvForDisplay(mon, MON_DATA_SPATK_IV), 120, 45);
    DrawIvRank(GetIvForDisplay(mon, MON_DATA_SPDEF_IV), 120, 57);
    DrawIvRank(GetIvForDisplay(mon, MON_DATA_SPEED_IV), 120, 69);
}

static void PrintExpLabels(void)
{
    /* 素の BW は「けいけんち」「つぎの Lv」の見出しをここに重ねて出していた。左の帯にあるので出さない */
}

/* ★ 2026-10-03 ユーザー「ポケモンのステータス表示画面でボタン押下すると２つ目の特性を表示するように切り替え処理を追加してほしい」:
   2 つ目の特性（src/second_ability.c・場に出たときだけ・Lv 50 から）。無ければ ABILITY_NONE（タマゴ・進化する種族・元の特性と同じ） */
static u16 SecondAbilityOfCurrentMon(void)
{
    u16 ability;

    if (sBwSs->isEgg)
        return ABILITY_NONE;
    ability = SecondAbility_OfMon(&sBwSs->currentMon);
    if (ability == GetMonAbility(&sBwSs->currentMon))
        return ABILITY_NONE;
    return ability;
}

static void PrintAbility(void)
{
    u16 ability = GetMonAbility(&sBwSs->currentMon);
    u16 second = SecondAbilityOfCurrentMon();
    u8 name[20];

    FillWindowPixelBuffer(sBwSs->window[5], 0);
    if (sBwSs->showAbility2 && second != ABILITY_NONE)
    {
        CopyAbilityName(name, second);
        /* 見出しの帯は窓の x4〜54（51 画素）。「とくせい2」は x11 からだと「2」が帯の右へ 3 画素はみ出した（実機 2026-10-03）ので帯の真ん中に */
        s32 w = GetStringWidth(BWSS_FONT, gBwSsText_Ability2, 0);
        Print(5, (w < 51) ? 4 + (51 - w) / 2 : 4, 4, WHITE, gBwSsText_Ability2);
        Print(5, 60, 4, DARK, name);
        if (!SecondAbility_IsUnlocked(GetMonData(&sBwSs->currentMon, MON_DATA_LEVEL, NULL)))
            Print(5, 160, 4, DARK, gBwSsText_Ability2Locked);
        PrintTight(5, 20, 23, DARK, gAbilityDescriptions[second]);
        return;
    }
    Print(5, 11, 4, WHITE, gBwSsText_Ability);
    Print(5, 60, 4, DARK, sBwSs->summary.abilityName);
    PrintTight(5, 20, 23, DARK, gAbilityDescriptions[ability]);
}

/* ---- 技ページ ---- */
#define MOVE_ROW_Y(i) ((i) * 28 + 5)
#define PP_ROW_Y(i) ((i) * 28 + 16)

static void PrintMoveNames(void)
{
    u8 i;

    for (i = 0; i < 4; i++)
        PrintMoveNameAndPP(i);
    if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO)
    {
        if (sBwSs->mode == PSS_MODE_SELECT_MOVE)
            PrintMoveNameAndPP(4);
        else
            Print(3, 3, MOVE_ROW_Y(4), WHITE, gBwSsText_Cancel);
    }
}

static void PrintMoveNameAndPP(u8 i)
{
    u8 colour = WHITE;
    u16 move = sBwSs->moves[i];
    u8 ppBonuses = GetMonData(&sBwSs->currentMon, MON_DATA_PP_BONUSES, NULL);
    u8 maxPP = CalculatePPWithBonus(move, ppBonuses, i);
    u8 curPP = (i == 4) ? maxPP : GetMonData(&sBwSs->currentMon, MON_DATA_PP1 + i, NULL);

    /* 技名は 8 文字まで。窓は 80 画素なので字間 0 で詰める（実機 3 回目: こんげんのはどう が切れた） */
    AddTextPrinterParameterized4(sBwSs->window[3], BWSS_FONT, 1, MOVE_ROW_Y(i) - 2, 0, 0, sTextColours[WHITE], TEXT_SPEED_FF,
                                 sBwSs->summary.moveName[i]);
    if (move == MOVE_NONE || curPP == maxPP)
        colour = WHITE;
    else if (curPP == 0)
        colour = RED_2;
    else if (maxPP == 3 || maxPP == 2 || curPP <= (maxPP / 2))
        colour = ORANGE;
    Print(3, 20, PP_ROW_Y(i), colour, gBwSsText_PP);
    Print(3, 36 + sXpos->pp[i], PP_ROW_Y(i), colour, sBwSs->summary.ppStr[i]);      /* 右端 52（実機 2 回目: 2 桁が / と重なった） */
    if (move != MOVE_NONE)
    {
        Print(3, 54, PP_ROW_Y(i), colour, gBwSsText_Slash);
        Print(3, 62, PP_ROW_Y(i), colour, sBwSs->summary.ppMaxStr[i]);
    }
}

static void PrintMoveInfo(void)
{
    u16 move;

    if (sMoveCursor >= 5)
        return;
    if (sBwSs->mode != PSS_MODE_SELECT_MOVE && sMoveCursor == 4)
        return;
    move = sBwSs->moves[sMoveCursor];
    if (move != MOVE_NONE)
        BlitBitmapToWindow(sBwSs->window[4], PSSIconsTiles + 24 * 8 * SPLIT_OF(move), 88, 3, 24, 15);   /* ぶつり／とくしゅ／へんか */
    Print(4, 91, 18, DARK, sBwSs->summary.power[sMoveCursor]);
    Print(4, 91, 33, DARK, sBwSs->summary.accuracy[sMoveCursor]);
    PrintTight(4, 14, 2, WHITE, gBwSsText_Category);
    PrintTight(4, 14, 17, WHITE, gBwSsText_Power);
    PrintTight(4, 14, 32, WHITE, gBwSsText_Accuracy);
    PrintTight(4, 10, 48, WHITE, gBwSsText_Effect);
    /* 説明は最大 5 行（素の日本語版を数えた）。窓の下端 112 に収めるため 1 行 10 画素（実機 2 回目で 5 行目が切れた） */
    if (move != MOVE_NONE)
        AddTextPrinterParameterized4(sBwSs->window[4], BWSS_FONT, 7, 61, 0, (u8)-3, sTextColours[DARK], TEXT_SPEED_FF,
                                     gMoveDescriptions[move]);   /* CFRU の表は 0 が MOVE_NONE（技番号そのまま） */
    PutWindowTilemap(sBwSs->window[4]);
}

static void DrawMoveTypeIcons(void)
{
    u8 i;

    FillWindowPixelBuffer(sBwSs->window[5], 0);
    for (i = 0; i < 4; i++)
    {
        if (sBwSs->moves[i] == MOVE_NONE)
            continue;
        blit_move_info_icon_bw(sBwSs->window[5], sBwSs->moveType[i] + 1, 8, MOVE_ROW_Y(i) - 2);
    }
    if (sBwSs->mode == PSS_MODE_SELECT_MOVE)
        blit_move_info_icon_bw(sBwSs->window[5], sBwSs->moveType[4] + 1, 8, MOVE_ROW_Y(4) - 2);
}

/* ---- トレーナーメモ ---- */
/* 出会った場所の地名（2026-09-29）。★ 素の GetMapNameGeneric は**今いるマップの地方**で引くので使わない。
   ポケモンの親の版（シンオウで出会ったら 12）で地方を決めて引く（src/map_region.c） */
static void BwSs_GetMetLocationName(u8 *dest, u8 metLocation)
{
    u8 region = MapRegion_FromMetGame(GetMonData(&sBwSs->currentMon, MON_DATA_MET_GAME, NULL));

    StringCopy(dest, MapRegion_GetName(metLocation, region));
}

static bool32 IsMetLocationValid(u8 place)
{
    return place <= METLOC_VALID_MAX;
}

static bool32 NatureNeedsNoNa(u8 nature)
{
    return nature == NATURE_BOLD || nature == NATURE_GENTLE;   /* ずぶとい・おとなしい は「な」を付けない */
}

static bool32 IsFromGenIII(void)
{
    u8 version = GetMonData(&sBwSs->currentMon, MON_DATA_MET_GAME, NULL);

    /* サファイア・ルビー・エメラルド・FR・LG。★ シンオウで出会った印 12 もこのゲーム（2026-09-29・src/map_region.h） */
    return (version >= 1 && version <= 5) || version == MET_GAME_SINNOH || version == MET_GAME_KITAKAMI;
}

static bool32 IsMultiPartner(void)
{
    if (!IsUpdateLinkStateCBActive() && IsMultiBattle() == TRUE && BWSS_RECEIVED_REMOTE_LINK_PLAYERS == 1
        && (sLastViewedMonIndex >= 4 || sLastViewedMonIndex == 1))
        return TRUE;
    return SummaryScreen_IsMultiBattlePartner();
}

static void PrintMemoText(const u8 *withNa, const u8 *noNa, u8 nature)
{
    u8 buf[152];

    BwSs_DynPlaceholderExpand(buf, NatureNeedsNoNa(nature) ? noNa : withNa);
    Print(4, 6, 0, WHITE, gBwSsText_TrainerMemo);
    PrintTight(4, 16, 12, DARK, buf);
}

static void SetMemoPlaceholders(u8 nature, u8 *levelStr, u8 metLevel)
{
    BwSs_DynPlaceholderReset();
    BwSs_DynPlaceholderSet(0, gBwSsNatureNames[nature]);
    ConvertIntToDecimalStringN(levelStr, metLevel == 0 ? 1 : metLevel, STR_CONV_MODE_LEFT_ALIGN, 3);
    BwSs_DynPlaceholderSet(1, levelStr);
}

/* 相手（通信・マルチの相方）のポケモン、または よそのゲーム出身 */
static void PrintTrainerMemoForeign(void)
{
    u8 nature = GetNature(&sBwSs->currentMon);
    u8 levelStr[5];
    u8 mapName[40];
    u8 metLevel = GetMonData(&sBwSs->currentMon, MON_DATA_MET_LEVEL, NULL);
    u8 metLocation = GetMonData(&sBwSs->currentMon, MON_DATA_MET_LOCATION, NULL);

    SetMemoPlaceholders(nature, levelStr, metLevel);
    if (IsMetLocationValid(metLocation))
        BwSs_GetMetLocationName(mapName, metLocation);
    else if (sBwSs->isEnemyParty || IsMultiPartner())
        StringCopy(mapName, gBwSsText_Somewhere);
    else
        StringCopy(mapName, gBwSsText_ATrade);
    BwSs_DynPlaceholderSet(2, mapName);

    if (metLevel == 0)
    {
        if (GetMonData(&sBwSs->currentMon, MON_DATA_FATEFUL_ENCOUNTER, NULL) == 1)
            PrintMemoText(gBwSsText_FatefulHatched, gBwSsText_FatefulHatchedBG, nature);
        else
            PrintMemoText(gBwSsText_Hatched, gBwSsText_HatchedBG, nature);
    }
    else if (metLocation == METLOC_FATEFUL_ENCOUNTER)
        PrintMemoText(gBwSsText_FatefulMet, gBwSsText_FatefulMetBG, nature);
    else
        PrintMemoText(gBwSsText_Met, gBwSsText_MetBG, nature);
}

static void PrintTrainerMemo(void)
{
    u8 nature = GetNature(&sBwSs->currentMon);
    u8 levelStr[5];
    u8 mapName[40];
    u8 metLevel = GetMonData(&sBwSs->currentMon, MON_DATA_MET_LEVEL, NULL);
    u8 metLocation = GetMonData(&sBwSs->currentMon, MON_DATA_MET_LOCATION, NULL);

    SetMemoPlaceholders(nature, levelStr, metLevel);
    /* 通信で来たポケモン（CFRU の TryReplaceSummaryScreenLocationWithFarAwayLocation と同じ判定） */
    if (IsTradedMon(&sBwSs->currentMon) && !SummaryScreen_IsMultiBattlePartner())
        metLocation = 0xFE;
    if (!IsMetLocationValid(metLocation) || !IsFromGenIII())
    {
        if (IsMultiPartner())
        {
            PrintTrainerMemoForeign();
            return;
        }
        if (metLocation == METLOC_FATEFUL_ENCOUNTER)
            PrintMemoText(gBwSsText_FatefulMet, gBwSsText_FatefulMetBG, nature);
        else
            PrintMemoText(gBwSsText_MetInATrade, gBwSsText_MetInATradeBG, nature);
        return;
    }
    BwSs_GetMetLocationName(mapName, metLocation);
    BwSs_DynPlaceholderSet(2, mapName);
    if (metLevel == 0)
    {
        if (GetMonData(&sBwSs->currentMon, MON_DATA_FATEFUL_ENCOUNTER, NULL) == 1)
            PrintMemoText(gBwSsText_ApparentlyFatefulHatched, gBwSsText_ApparentlyFatefulHatchedBG, nature);
        else
            PrintMemoText(gBwSsText_ApparentlyMet, gBwSsText_ApparentlyMetBG, nature);
    }
    else if (metLocation == METLOC_FATEFUL_ENCOUNTER)
        PrintMemoText(gBwSsText_FatefulMet, gBwSsText_FatefulMetBG, nature);
    else
        PrintMemoText(gBwSsText_ApparentlyMet, gBwSsText_ApparentlyMetBG, nature);
}

/* たまごでないときの window[4]（素の BW の名前づけがねじれているので中身で命名） */
static void PrintEggMemo(void)
{
    if (IsOriginalTrainer(&sBwSs->currentMon) == TRUE)
        PrintTrainerMemoForeign();
    else
        PrintTrainerMemo();
}

/* たまごの window[4] */
static void PrintEggInfo(void)
{
    u8 metLocation = GetMonData(&sBwSs->currentMon, MON_DATA_MET_LOCATION, NULL);
    u8 version;
    u8 idx = 0;

    if (metLocation == METLOC_FATEFUL_ENCOUNTER || GetMonData(&sBwSs->currentMon, MON_DATA_FATEFUL_ENCOUNTER, NULL) == 1)
        idx = 4;
    else
    {
        version = GetMonData(&sBwSs->currentMon, MON_DATA_MET_GAME, NULL);
        if (sBwSs->monList.mons != gEnemyParty)
        {
            if (!MapRegion_IsOurMetGame(version))
                idx = 1;
            else if (metLocation == METLOC_SPECIAL_EGG)
                idx = 2;
            if ((idx == 0 || idx == 2) && IsOriginalTrainer(&sBwSs->currentMon) == FALSE)
                idx++;
        }
        else
        {
            if (!MapRegion_IsOurMetGame(version))
            {
                if (metLocation == METLOC_SPECIAL_EGG)
                    idx = 5;
            }
            else if (metLocation == METLOC_SPECIAL_EGG)
                idx = 2;
            if (IsOriginalTrainer(&sBwSs->currentMon) == FALSE)
                idx++;
        }
    }
    if (sBwSs->isBadEgg)
        idx = 0;
    if (!sBwSs->isBadEgg && HackGibleEgg_IsClubEgg(&sBwSs->currentMon))
        PrintTight(4, 16, 12, DARK, gBwSsText_EggFanClub);   /* クチバのだいすきクラブのフカマル（2026-10-05・「たびの おとこから」になっていた） */
    else
        PrintTight(4, 16, 12, DARK, sEggOriginTexts[idx]);
    Print(4, 6, 0, WHITE, gBwSsText_TrainerMemo);
}

/* 主人公（またはリンクの相手）が親か */
static u8 IsOriginalTrainer(struct Pokemon *mon)
{
    u32 trainerId;
    const u8 *a;
    const u8 *b;

    if (sBwSs->monList.mons == gEnemyParty)
    {
        u8 multiplayerId = GetMultiplayerId() ^ 1;
        trainerId = gLinkPlayers[multiplayerId].trainerId & 0xFFFF;
        StringCopy(sBwSs->summary.otCmp[0], gLinkPlayers[multiplayerId].name);
    }
    else
    {
        trainerId = (gSaveBlock2->playerTrainerId[0] | (gSaveBlock2->playerTrainerId[1] << 8)) & 0xFFFF;
        StringCopy(sBwSs->summary.otCmp[0], gSaveBlock2->playerName);
    }
    if (trainerId != (GetMonData(mon, MON_DATA_OT_ID, NULL) & 0xFFFF))
        return FALSE;
    GetMonData(mon, MON_DATA_OT_NAME, sBwSs->summary.otCmp[1]);
    /* 制御コード（FC xx）を飛ばして比べる */
    a = sBwSs->summary.otCmp[0];
    b = sBwSs->summary.otCmp[1];
    while (TRUE)
    {
        while (*a == 0xFC) a += 2;
        while (*b == 0xFC) b += 2;
        if (*a != *b)
            return FALSE;
        if (*a == 0xFF)
            return TRUE;
        a++;
        b++;
    }
}

/* ---------------- ポケモンのデータを文字にする ---------------- */
static void GetMonInfo(void)
{
    struct Pokemon *mon = &sBwSs->currentMon;
    u8 tmp[24];
    u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
    u16 dexNum = SpeciesToPokedexNum(species);
    u16 gender;
    u16 heldItem;
    u32 otId;

    if (dexNum == 0xFFFF)
        StringCopy(sBwSs->summary.dexNum, gBwSsText_DexNoUnknown);
    else
        ConvertIntToDecimalStringN(sBwSs->summary.dexNum, dexNum, STR_CONV_MODE_LEADING_ZEROS, dexNum >= 1000 ? 4 : 3);   /* ★ 2026-10-10 4 けた（1000〜・カゲノナカニ 1026）。入れ物は 5 バイト＝4 けた＋終わり */
    sXpos->dexNo = 0;
    if (sBwSs->isEgg)
    {
        StringCopy(sBwSs->summary.speciesName, gBwSsText_EggNickname);
        return;
    }
    GetSpeciesName(sBwSs->summary.speciesName, species);
    /* ★ 2026-10-10 ユーザー「メガ進化したら岩タイプが追加されなかった」: 種族の表から直に出していたので、特別なガブリアスZ の いわ
       （src/hack_dragon_boulder.c HackDragonBoulder_Type2・戦いの中は付いていた）が出なかった。CFRU の GetMonType（util.c・
       カモモンズも中で見る）で出す */
    sBwSs->monTypes[0] = GetMonType(mon, 0);
    sBwSs->monTypes[1] = GetMonType(mon, 1);
    GetMonNickname(mon, sBwSs->summary.nickname);
    gender = GetMonGender(mon);
    if (gender == MON_FEMALE)
        StringCopy(sBwSs->summary.genderSymbol, gBwSsText_Female);
    else if (gender == MON_MALE)
        StringCopy(sBwSs->summary.genderSymbol, gBwSsText_Male);
    else
        sBwSs->summary.genderSymbol[0] = EOS;
    species = GetMonData(mon, MON_DATA_SPECIES2, NULL);
    if (species == SPECIES_NIDORAN_M || species == SPECIES_NIDORAN_F)
    {
        GetSpeciesName(tmp, species);
        if (StringCompare(sBwSs->summary.nickname, tmp) == 0)
            sBwSs->summary.genderSymbol[0] = EOS;
    }
    GetMonData(mon, MON_DATA_OT_NAME, tmp);
    StringCopyN(sBwSs->summary.otName, tmp, OT_NAME_LENGTH);
    sBwSs->summary.otName[OT_NAME_LENGTH] = EOS;
    otId = GetMonData(mon, MON_DATA_OT_ID, NULL) & 0xFFFF;
    ConvertIntToDecimalStringN(sBwSs->summary.otId, otId, STR_CONV_MODE_LEADING_ZEROS, 5);
    ConvertIntToDecimalStringN(tmp, GetMonData(mon, MON_DATA_LEVEL, NULL), STR_CONV_MODE_LEFT_ALIGN, 3);
    StringCopy(sBwSs->summary.level, gBwSsText_Lv);
    StringAppendN(sBwSs->summary.level, tmp, 4);
    heldItem = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);
    if (heldItem == ITEM_NONE)
        StringCopy(sBwSs->summary.heldItem, gBwSsText_ItemNone);
    else
        CopyItemName(heldItem, sBwSs->summary.heldItem);
}

static void GetStatStr(u8 slot, u32 value)
{
    ConvertIntToDecimalStringN(sBwSs->summary.statStr[slot], value, STR_CONV_MODE_LEFT_ALIGN, 4);   /* 2026-10-09 3 → 4 けた（ももアイス・Lv200 で 1000 をこえる）*/
}

static void GetMonStats(void)
{
    struct Pokemon *mon = &sBwSs->currentMon;
    u8 tmp[20];
    u16 hp;
    u16 species;
    u8 level;
    u32 exp;
    u32 expToNext = 0;
    u16 ability;
    bool8 traded = (sBwSs->savedCallback == BwSs_CB2_ReturnToTradeMenuFromSummary && sBwSs->isEnemyParty == TRUE);

    hp = GetMonData(mon, MON_DATA_HP, NULL);
    ConvertIntToDecimalStringN(sBwSs->summary.hpStr, hp, STR_CONV_MODE_LEFT_ALIGN, 4);
    StringAppend(sBwSs->summary.hpStr, gBwSsText_Slash);
    hp = GetMonData(mon, MON_DATA_MAX_HP, NULL);
    ConvertIntToDecimalStringN(tmp, hp, STR_CONV_MODE_LEFT_ALIGN, 4);
    StringAppend(sBwSs->summary.hpStr, tmp);
    sXpos->curHp = RightAlign(66, sBwSs->summary.hpStr);   /* 2026-10-09 右端を窓（17 タイル＝136）の 134 に（4 けたの "2576/2576" がはみ出した）*/

    GetStatStr(PSS_STAT_ATK, GetMonData(mon, traded ? MON_DATA_ATK2 : MON_DATA_ATK, NULL));
    GetStatStr(PSS_STAT_DEF, GetMonData(mon, traded ? MON_DATA_DEF2 : MON_DATA_DEF, NULL));
    GetStatStr(PSS_STAT_SPA, GetMonData(mon, traded ? MON_DATA_SPATK2 : MON_DATA_SPATK, NULL));
    GetStatStr(PSS_STAT_SPD, GetMonData(mon, traded ? MON_DATA_SPDEF2 : MON_DATA_SPDEF, NULL));
    GetStatStr(PSS_STAT_SPE, GetMonData(mon, traded ? MON_DATA_SPEED2 : MON_DATA_SPEED, NULL));
    sXpos->atk = RightAlign(27, sBwSs->summary.statStr[PSS_STAT_ATK]);
    sXpos->def = RightAlign(27, sBwSs->summary.statStr[PSS_STAT_DEF]);
    sXpos->spa = RightAlign(27, sBwSs->summary.statStr[PSS_STAT_SPA]);
    sXpos->spd = RightAlign(27, sBwSs->summary.statStr[PSS_STAT_SPD]);
    sXpos->spe = RightAlign(27, sBwSs->summary.statStr[PSS_STAT_SPE]);

    exp = GetMonData(mon, MON_DATA_EXP, NULL);
    ConvertIntToDecimalStringN(sBwSs->summary.expStr, exp, STR_CONV_MODE_LEFT_ALIGN, 8);   /* 2026-10-09 Lv200 で 8 けた */
    sXpos->exp = RightAlign(65, sBwSs->summary.expStr);   /* 2026-10-09 8 けた（Lv200）で左の見出しにかかった → 右端 134 */
    level = GetMonData(mon, MON_DATA_LEVEL, NULL);
    if (level < MAX_LEVEL)   /* 2026-10-09 config.h の上限（MAX_MON_LEVEL は 100 のまま）*/
    {
        species = GetMonData(mon, MON_DATA_SPECIES, NULL);
        expToNext = gExperienceTables[gBaseStats[species].growthRate][level + 1] - exp;
    }
    ConvertIntToDecimalStringN(sBwSs->summary.toNextStr, expToNext, STR_CONV_MODE_LEFT_ALIGN, 8);
    sXpos->toNext = RightAlign(65, sBwSs->summary.toNextStr);

    ability = GetMonAbility(mon);
    CopyAbilityName(sBwSs->summary.abilityName, ability);
    sBwSs->showAbility2 = 0;             /* ポケモンを替えたら 1 つ目の特性から */
    sBwSs->showEvIv = 0;

    sBwSs->monStatus = GetAilment(GetMonData(mon, MON_DATA_STATUS, NULL));
    if (sBwSs->monStatus == AIL_NONE && CheckPartyPokerus(mon, 0))
        sBwSs->monStatus = AIL_PKRS;
}

static void GetAllMoves(void)
{
    u8 i;

    for (i = 0; i < 4; i++)
        GetMove(i);
    if (sBwSs->mode == PSS_MODE_SELECT_MOVE)
        GetMove(4);
}

static void GetMove(u8 i)
{
    struct Pokemon *mon = &sBwSs->currentMon;
    u16 move;

    if (i < 4)
        sBwSs->moves[i] = GetMonData(mon, MON_DATA_MOVE1 + i, NULL);
    move = sBwSs->moves[i];
    if (move == MOVE_NONE)
    {
        StringCopy(sBwSs->summary.moveName[i], gBwSsText_OneHyphen);
        StringCopy(sBwSs->summary.ppStr[i], gBwSsText_TwoHyphens);
        StringCopy(sBwSs->summary.power[i], gBwSsText_ThreeHyphens);
        StringCopy(sBwSs->summary.accuracy[i], gBwSsText_ThreeHyphens);
        sXpos->pp[i] = 0xFF;
        sXpos->ppMax[i] = 0xFF;
        return;
    }
    sBwSs->numMoves++;
    sBwSs->moveType[i] = GetSummaryScreenMoveType(move, mon);
    StringCopy(sBwSs->summary.moveName[i], gMoveNames[move]);
    if (i >= 4 && sBwSs->mode == PSS_MODE_SELECT_MOVE)
    {
        ConvertIntToDecimalStringN(sBwSs->summary.ppStr[i], gBattleMoves[move].pp, STR_CONV_MODE_LEFT_ALIGN, 3);
        ConvertIntToDecimalStringN(sBwSs->summary.ppMaxStr[i], gBattleMoves[move].pp, STR_CONV_MODE_LEFT_ALIGN, 3);
    }
    else
    {
        ConvertIntToDecimalStringN(sBwSs->summary.ppStr[i], GetMonData(mon, MON_DATA_PP1 + i, NULL), STR_CONV_MODE_LEFT_ALIGN, 3);
        ConvertIntToDecimalStringN(sBwSs->summary.ppMaxStr[i],
                                   CalculatePPWithBonus(move, GetMonData(mon, MON_DATA_PP_BONUSES, NULL), i),
                                   STR_CONV_MODE_LEFT_ALIGN, 3);
    }
    sXpos->pp[i] = RightAlign(16, sBwSs->summary.ppStr[i]);
    sXpos->ppMax[i] = 0;
    if (gBattleMoves[move].power <= 1)
        StringCopy(sBwSs->summary.power[i], gBwSsText_ThreeHyphens);
    else
        ConvertIntToDecimalStringN(sBwSs->summary.power[i], gBattleMoves[move].power, STR_CONV_MODE_RIGHT_ALIGN, 3);
    if (gBattleMoves[move].accuracy == 0)
        StringCopy(sBwSs->summary.accuracy[i], gBwSsText_ThreeHyphens);
    else
        ConvertIntToDecimalStringN(sBwSs->summary.accuracy[i], gBattleMoves[move].accuracy, STR_CONV_MODE_RIGHT_ALIGN, 3);
}

static u8 GetAilment(u32 status)
{
    if (GetMonData(&sBwSs->currentMon, MON_DATA_HP, NULL) == 0)
        return AIL_FNT;
    if (status & STATUS1_PSN_ANY)
        return AIL_PSN;
    if (status & STATUS1_PARALYSIS)
        return AIL_PRZ;
    if (status & STATUS1_SLEEP)
        return AIL_SLP;
    if (status & STATUS1_FREEZE)
        return AIL_FRZ;
    if (status & STATUS1_BURN)
        return AIL_BRN;
    if (CheckPartyPokerus(&sBwSs->currentMon, 0))
        return AIL_PKRS;
    return AIL_NONE;
}

/* 手持ちは素のまま、預かりボックスは CFRU の圧縮形（58 バイト）から起こす */
static void CopyMonFromList(struct Pokemon *mon)
{
    if (!sBwSs->isBoxMon)
        *mon = sBwSs->monList.mons[sLastViewedMonIndex];
    else
        CompressedMonToMon(&sBwSs->monList.boxMons[sLastViewedMonIndex], mon);
}

static void SwapMovesInBoxMon(struct BoxPokemon *boxMon)
{
    u16 move1 = GetBoxMonData(boxMon, MON_DATA_MOVE1 + sMoveCursor, NULL);
    u16 move2 = GetBoxMonData(boxMon, MON_DATA_MOVE1 + sMoveSwapPos, NULL);
    u8 pp1 = GetBoxMonData(boxMon, MON_DATA_PP1 + sMoveCursor, NULL);
    u8 pp2 = GetBoxMonData(boxMon, MON_DATA_PP1 + sMoveSwapPos, NULL);
    u8 bonuses = GetBoxMonData(boxMon, MON_DATA_PP_BONUSES, NULL);
    u8 b1 = (bonuses & sPPUpMask[sMoveCursor]) >> (sMoveCursor * 2);
    u8 b2 = (bonuses & sPPUpMask[sMoveSwapPos]) >> (sMoveSwapPos * 2);

    bonuses &= ~sPPUpMask[sMoveCursor];
    bonuses &= ~sPPUpMask[sMoveSwapPos];
    bonuses |= (b1 << (sMoveSwapPos * 2)) + (b2 << (sMoveCursor * 2));
    SetBoxMonData(boxMon, MON_DATA_MOVE1 + sMoveCursor, &move2);
    SetBoxMonData(boxMon, MON_DATA_MOVE1 + sMoveSwapPos, &move1);
    SetBoxMonData(boxMon, MON_DATA_PP1 + sMoveCursor, &pp2);
    SetBoxMonData(boxMon, MON_DATA_PP1 + sMoveSwapPos, &pp1);
    SetBoxMonData(boxMon, MON_DATA_PP_BONUSES, &bonuses);
}

static void SwapMovesParty(void)
{
    SwapMovesInBoxMon((struct BoxPokemon *)&sBwSs->monList.mons[sLastViewedMonIndex]);   /* BoxPokemon は Pokemon の先頭 */
}

static void SwapMovesBox(void)
{
    struct BoxPokemon tmp;
    struct CompressedPokemon *comp = &sBwSs->monList.boxMons[sLastViewedMonIndex];

    CreateBoxMonFromCompressedMon(&tmp, comp);
    SwapMovesInBoxMon(&tmp);
    CreateCompressedMonFromBoxMon(&tmp, comp);
    BoxShelf_MarkPtrDirty(comp);   /* 2026-10-06 棚の箱の入れ物を直に書いた → 汚れの印（src/hack_box_shelf.c） */
}

static bool8 CanForgetSelectedMove(void)
{
    u16 move = GetMonData(&sBwSs->currentMon, MON_DATA_MOVE1 + sMoveCursor, NULL);

    if (CheckIsHmMove(move) == TRUE && sBwSs->mode != PSS_MODE_FORGET_MOVE)
        return FALSE;
    return TRUE;
}

/* ---------------- スプライト ---------------- */
static bool8 LoadSprites(void)
{
    switch (sBwSs->spriteState)
    {
    case 0: LoadShiny(); break;
    case 1: LoadDot(); break;
    case 2: LoadMarkings(); break;
    case 3: LoadCursor(); break;
    case 4: LoadStatusIcon(); break;
    case 5: LoadHpBar(); break;
    case 6: LoadExpBar(); break;
    case 7: LoadBall(); break;
    case 8: LoadIcon(); break;
    default:
        LoadMonPic();
        return TRUE;
    }
    sBwSs->spriteState++;
    return FALSE;
}

static struct BwSsSpriteBox *LoadSmallSprite(const u8 *lzGfx, u16 tiles, const u8 *pal, u16 tileTag, u16 palTag,
                                             const struct OamData *oam, const union AnimCmd *const *anims, s16 x, s16 y)
{
    struct BwSsSpriteBox *box = AllocZeroed(sizeof(struct BwSsSpriteBox));
    void *buf = AllocZeroed(0x20 * tiles);
    struct SpriteSheet sheet = { buf, 0x20 * tiles, tileTag };
    struct SpritePalette palette = { (const u16 *)pal, palTag };
    struct SpriteTemplate template = {
        .tileTag = tileTag, .paletteTag = palTag, .oam = oam, .anims = anims, .images = NULL,
        .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
    };

    LZ77UnCompWram(lzGfx, buf);
    LoadSpriteSheet(&sheet);
    LoadSpritePalette(&palette);
    box->sprite = &gSprites[CreateSprite(&template, x, y, 0)];
    box->tileTag = tileTag;
    box->palTag = palTag;
    FREE_IF_SET(buf);
    return box;
}

static void FreeSpriteBox(struct BwSsSpriteBox *box)
{
    if (box->sprite != NULL)
        DestroySpriteAndFreeResources(box->sprite);
    Free(box);
}

/* ---- 技えらびのカーソル（左右 2 枚 × 2 組） ---- */
static void LoadCursor(void)
{
    u8 i;
    void *bufs[2];

    bufs[0] = AllocZeroed(0x20 * 64);
    bufs[1] = AllocZeroed(0x20 * 64);
    LZ77UnCompWram(gBwSsCursorLeft, bufs[0]);
    LZ77UnCompWram(gBwSsCursorRight, bufs[1]);
    for (i = 0; i < 4; i++)
    {
        struct SpriteSheet sheet = { bufs[i % 2], 0x20 * 64, TAG_CURSOR + i };
        struct SpritePalette palette = { (const u16 *)gBwSsCursorPal, TAG_CURSOR };
        struct SpriteTemplate template = {
            .tileTag = TAG_CURSOR + i, .paletteTag = TAG_CURSOR, .oam = &sOam64x32, .anims = sAnimsCursor,
            .images = NULL, .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCB_Cursor,
        };
        u8 spriteId;

        sCursor(i) = AllocZeroed(sizeof(struct BwSsSpriteBox));
        LoadSpriteSheet(&sheet);
        LoadSpritePalette(&palette);
        spriteId = CreateSprite(&template, 64 * (i % 2) + 32, sMoveCursor * 28 + 34, i % 2);
        sCursor(i)->sprite = &gSprites[spriteId];
        sCursor(i)->tileTag = i;
        sCursor(i)->palTag = TAG_CURSOR + i;
        sCursor(i)->sprite->subpriority = i;
        if (i > 1)
            StartSpriteAnim(sCursor(i)->sprite, 1);
    }
    SetInvisCursor(1);
    FREE_IF_SET(bufs[0]);
    FREE_IF_SET(bufs[1]);
}

static void SetInvisCursor(u8 inv)
{
    u8 i;

    for (i = 0; i < 4; i++)
        sCursor(i)->sprite->invisible = inv;
}

static void SpriteCB_Cursor(struct Sprite *sprite)
{
    u8 i;

    for (i = 0; i < 4; i++)
    {
        if (sBwSs->swapping == TRUE && i > 1)
            continue;
        sCursor(i)->sprite->pos1.y = sMoveCursor * 28 + 34;
    }
    if (sBwSs->swapping != TRUE)
    {
        if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO)
        {
            sCursor(0)->sprite->invisible = FALSE;
            sCursor(1)->sprite->invisible = FALSE;
        }
        return;
    }
    for (i = 0; i < 2; i++)
    {
        sprite = sCursor(i)->sprite;
        sprite->data[0]++;
        if (sprite->data[0] > 60)
        {
            sprite->invisible ^= 1;
            sprite->data[0] = 0;
        }
    }
}

static void FreeCursor(void)
{
    u8 i;

    for (i = 0; i < 4; i++)
    {
        FreeSpriteBox(sCursor(i));
        sCursor(i) = NULL;
    }
}

/* ---- 状態異常の印 ---- */
static void LoadStatusIcon(void)
{
    sStatusIcon = LoadSmallSprite(gBwSsIconStatus, 32, gBwSsIconStatusPal, TAG_STATUS, TAG_STATUS, &sOam32x8, sAnimsStatus, 0, 0);
    SetInvisStatus(1);
    ShowStatusIcon();
}

static void ShowStatusIcon(void)
{
    sBwSs->monStatus = GetAilment(GetMonData(&sBwSs->currentMon, MON_DATA_STATUS, NULL));
    if (sBwSs->monStatus == AIL_NONE)
    {
        SetInvisStatus(1);
        return;
    }
    StartSpriteAnim(sStatusIcon->sprite, sBwSs->monStatus - 1);
    SetInvisStatus(0);
}

static void SetInvisStatus(u8 inv)
{
    struct Sprite *s = sStatusIcon->sprite;

    s->invisible = (sBwSs->monStatus == AIL_NONE || sBwSs->isEgg) ? TRUE : inv;
    /* ★ 2026-10-04 ユーザー「ひんしのアイコンがずっと表示されてしまう」: 技の詳しい説明のページでは x16・y45 が 2 つ目の技の欄に
       重なっていた（元の BW 風の画面の置き場所のまま・こちらはそこに技の一覧がある）→ このページでは出さない */
    if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO)
        s->invisible = TRUE;
    if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO)
    {
        s->pos1.x = 16;
        s->pos1.y = 45;
    }
    else
    {
        s->pos1.x = 179;   /* 「Lv50」の真下（左端を Lv の字 x168 にそろえる・絵の不透明な所は 32 画素の 5〜26 列） */
        s->pos1.y = 50;    /* 前は x214・y38 でテラスタイプの札（x208〜240）と重なった（ユーザー 2026-10-04） */
    }
}

/* ---- HP バー（8x8 を 9 枚） ---- */
static struct BwSsBar *LoadBar(const u8 *lzGfx, u16 tileTag, u16 palTag, u8 count, s16 x0, s16 y)
{
    struct BwSsBar *bar = AllocZeroed(sizeof(struct BwSsBar));
    void *buf = AllocZeroed(0x20 * 12);
    struct SpriteSheet sheet = { buf, 0x20 * 12, tileTag };
    u8 i;

    LZ77UnCompWram(lzGfx, buf);
    LoadSpriteSheet(&sheet);
    for (i = 0; i < count; i++)
    {
        struct SpriteTemplate template = {
            .tileTag = tileTag, .paletteTag = palTag, .oam = &sOam8x8, .anims = sAnimsBar, .images = NULL,
            .affineAnims = gDummySpriteAffineAnimTable, .callback = SpriteCallbackDummy,
        };
        bar->cordX[i] = x0 + i * 8;
        bar->sprites[i] = &gSprites[CreateSprite(&template, bar->cordX[i], y, 0)];
        bar->sprites[i]->oam.priority = 2;
    }
    bar->tileTag = tileTag;
    bar->palTag = palTag;
    FREE_IF_SET(buf);
    return bar;
}

static void FreeBar(struct BwSsBar *bar, u8 count)
{
    u8 i;

    for (i = 0; i < count; i++)
        if (bar->sprites[i] != NULL)
            DestroySpriteAndFreeResources(bar->sprites[i]);
    Free(bar);
}

static void LoadHpBar(void)
{
    struct SpritePalette p0 = { sHpBarPals[0], TAG_HPBAR };
    struct SpritePalette p1 = { sHpBarPals[1], TAG_HPBAR + 1 };
    struct SpritePalette p2 = { sHpBarPals[2], TAG_HPBAR + 2 };

    LoadSpritePalette(&p0);
    LoadSpritePalette(&p1);
    LoadSpritePalette(&p2);
    sHpBar = LoadBar(gBwSsHpBar, TAG_HPBAR, TAG_HPBAR, 9, 81, 33);
    SetupHpBar();
    SetInvisHpBar(1);
}

static void SetupHpBar(void)
{
    u8 whole = 0;
    u8 i;
    u8 palOffset;
    u32 curHp, maxHp;
    u32 v0, v1;               /* 64 ビット割り算は libgcc が無いので使わない */

    if (sBwSs->isEgg)
        return;
    curHp = GetMonData(&sBwSs->currentMon, MON_DATA_HP, NULL);
    maxHp = GetMonData(&sBwSs->currentMon, MON_DATA_MAX_HP, NULL);
    switch (BwSs_GetHPBarLevel(curHp, maxHp))
    {
    case 2: palOffset = 1; break;
    case 1: palOffset = 2; break;
    default: palOffset = 0; break;
    }
    for (i = 0; i < 9; i++)
        sHpBar->sprites[i]->oam.paletteNum = IndexOfSpritePaletteTag(TAG_HPBAR) + palOffset;
    if (curHp == maxHp)
    {
        for (i = 2; i < 8; i++)
            StartSpriteAnim(sHpBar->sprites[i], 8);
    }
    else
    {
        v0 = (maxHp << 2) / 6;
        v1 = (curHp << 2);
        while (v1 > v0)
        {
            v1 -= v0;
            whole++;
        }
        whole += 2;
        for (i = 2; i < whole; i++)
            StartSpriteAnim(sHpBar->sprites[i], 8);
        StartSpriteAnim(sHpBar->sprites[whole], (v1 * 6) / v0);
        for (i = whole + 1; i < 8; i++)
            StartSpriteAnim(sHpBar->sprites[i], 0);
    }
    StartSpriteAnim(sHpBar->sprites[0], 9);
    StartSpriteAnim(sHpBar->sprites[1], 10);
    StartSpriteAnim(sHpBar->sprites[8], 11);
}

static void SetInvisHpBar(u8 inv)
{
    u8 i;

    for (i = 0; i < 9; i++)
        sHpBar->sprites[i]->invisible = inv;
}

/* ---- 経験値バー（8x8 を 11 枚） ---- */
static void LoadExpBar(void)
{
    struct SpritePalette p = { sHpBarPals[0], TAG_EXPBAR };

    LoadSpritePalette(&p);
    sExpBar = LoadBar(gBwSsExpBar, TAG_EXPBAR, TAG_EXPBAR, 11, 156, 130);
    SetupExpBar();
    SetInvisExpBar(1);
}

static void SetupExpBar(void)
{
    u8 whole = 0;
    u8 i;
    u8 level;
    u16 species;
    u32 exp, total, cur;
    u32 v0, v1;

    if (sBwSs->isEgg)
        return;
    exp = GetMonData(&sBwSs->currentMon, MON_DATA_EXP, NULL);
    level = GetMonData(&sBwSs->currentMon, MON_DATA_LEVEL, NULL);
    species = GetMonData(&sBwSs->currentMon, MON_DATA_SPECIES, NULL);
    if (level < MAX_LEVEL)   /* 2026-10-09 config.h の上限（MAX_MON_LEVEL は 100 のまま）*/
    {
        const u32 *table = gExperienceTables[gBaseStats[species].growthRate];

        total = table[level + 1] - table[level];
        cur = exp - table[level];
        v0 = (total << 2) / 8;
        v1 = (cur << 2);
        while (v1 > v0)
        {
            v1 -= v0;
            whole++;
        }
        whole += 2;
        for (i = 2; i < whole; i++)
            StartSpriteAnim(sExpBar->sprites[i], 8);
        if (whole >= 10)
        {
            if (total != cur)
                StartSpriteAnim(sExpBar->sprites[9], 7);
        }
        else
        {
            StartSpriteAnim(sExpBar->sprites[whole], (v1 * 8) / v0);
            for (i = whole + 1; i < 10; i++)
                StartSpriteAnim(sExpBar->sprites[i], 0);
        }
    }
    else
    {
        for (i = 2; i < 10; i++)
            StartSpriteAnim(sExpBar->sprites[i], 0);
    }
    StartSpriteAnim(sExpBar->sprites[0], 9);
    StartSpriteAnim(sExpBar->sprites[1], 10);
    StartSpriteAnim(sExpBar->sprites[10], 11);
}

static void SetInvisExpBar(u8 inv)
{
    u8 i;

    for (i = 0; i < 11; i++)
        sExpBar->sprites[i]->invisible = inv;
}

/* ---- ポケルスの跡（点）と色違いの印 ---- */
static void LoadDot(void)
{
    sDot = LoadSmallSprite(gBwSsDotGfx, 1, gBwSsDotPal, TAG_DOT, TAG_DOT, &sOam8x8, sAnimsDot, 114, 92);
    SetInvisDot(1);
    UpdateDot();
}

static bool8 HasHadPokerus(void)
{
    return !CheckPartyPokerus(&sBwSs->currentMon, 0) && CheckPartyHasHadPokerus(&sBwSs->currentMon, 0);
}

static void UpdateDot(void)
{
    SetInvisDot(HasHadPokerus() ? 0 : 1);
}

static void SetInvisDot(u8 inv)
{
    struct Sprite *s = sDot->sprite;

    s->invisible = HasHadPokerus() ? inv : TRUE;
    if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO)
    {
        s->invisible = TRUE;
        s->pos1.x = 16;
        s->pos1.y = 44;
    }
    else
    {
        s->pos1.x = 114;
        s->pos1.y = 92;
    }
}

static void LoadShiny(void)
{
    sShiny = LoadSmallSprite(gBwSsShinyGfx, 2, gBwSsShinyPal, TAG_SHINY, TAG_SHINY, &sOam8x8, sAnimsShiny, 166, 50);
    SetInvisShiny(1);
    UpdateShiny();
}

static void UpdateShiny(void)
{
    SetInvisShiny((IsMonShiny(&sBwSs->currentMon) == TRUE && !sBwSs->isEgg) ? 0 : 1);
}

static void SetInvisShiny(u8 inv)
{
    struct Sprite *s = sShiny->sprite;

    s->invisible = (IsMonShiny(&sBwSs->currentMon) == TRUE && !sBwSs->isEgg) ? inv : TRUE;
    if (sBwSs->curPageIndex == PSS_PAGE_MOVES_INFO)
    {
        s->pos1.x = 126;
        s->pos1.y = 20;
    }
    else
    {
        s->pos1.x = 166;
        s->pos1.y = 50;
    }
}

/* ---- マーキング ---- */
static void LoadMarkings(void)
{
    u32 markings = GetMonData(&sBwSs->currentMon, MON_DATA_MARKINGS, NULL);

    sBwSs->markingSprite = BwSs_CreateMonMarkingSprite(TAG_MARKINGS, TAG_MARKINGS, (const u16 *)gBwSsMarkingsPal);
    if (sBwSs->markingSprite != NULL)
    {
        StartSpriteAnim(sBwSs->markingSprite, markings);
        sBwSs->markingSprite->pos1.x = 208;
        sBwSs->markingSprite->pos1.y = 114;
    }
    SetInvisMarkings(1);
}

static void SetInvisMarkings(u8 inv)
{
    u32 markings = GetMonData(&sBwSs->currentMon, MON_DATA_MARKINGS, NULL);

    if (sBwSs->markingSprite == NULL)
        return;
    sBwSs->markingSprite->invisible = (markings == 0) ? TRUE : inv;
}

static void UpdateMarkings(void)
{
    if (sBwSs->markingSprite != NULL)
        StartSpriteAnim(sBwSs->markingSprite, GetMonData(&sBwSs->currentMon, MON_DATA_MARKINGS, NULL));
    SetInvisMarkings(0);
}

/* ---- 捕まえたボール（＋キョダイマックスの印。CFRU の CreateSummaryScreenGigantamaxIcon と同じ置き方） ---- */
/* ★ 2026-10-09 ユーザーの画像「巨大ダイマックスアイコンがステータスの全部のページで表示される」:
   印はボールの 32 画素左（x136）＝左の欄の中で、のうりょくのページでは HP の数字に重なった。
   ボールと同じく全ページに出していたので、1 ページ目（ポケモンじょうほう・図鑑番号の行の右の空き）だけにする。
   ページの出し入れは何か所もあるので、毎フレーム ボールが見えていて 1 ページ目のときだけ見せる */
static void SpriteCB_GmaxIcon(struct Sprite *sprite)
{
    sprite->invisible = gSprites[sBwSs->ballSpriteId].invisible || sBwSs->curPageIndex != PSS_PAGE_INFO;
}

static void LoadBall(void)
{
    /* ★★ **CFRU は `MON_DATA_POKEBALL` に「道具の番号」ではなく「ボールの種類」を入れている。**
       素 FRLG の要約画面は `ItemIdToBallId(GetMonData(...))` と変換してから使うが、
       CFRU の `src/catching.c` は `ballType = ItemId_GetType(gLastUsedItem)` を**そのまま**
       しまう（87 / 177 / 234 行）。`src/build_pokemon.c` も `BALL_TYPE_POKE_BALL` を直接入れる。
       変換すると**二重変換**になって別のボールが出る（実機 2026-09-20）。そのまま使う。
       ★ 表の外を引かない見張りも置く（6.23）。`gBallSpriteTemplates` は 0〜27 の 28 件。 */
    u16 ballType = sBwSs->isEgg ? BALL_TYPE_POKE_BALL
                                : GetMonData(&sBwSs->currentMon, MON_DATA_POKEBALL, NULL);
    struct Sprite *ball;

    if (ballType > BALL_TYPE_DYNAMAX_BALL)
        ballType = BALL_TYPE_POKE_BALL;
    sBwSs->ballId = ballType;
    LoadBallGfx(sBwSs->ballId);
    sBwSs->ballSpriteId = CreateSprite(&gBallSpriteTemplates[sBwSs->ballId], 168, 24, 0);   /* 名前の左（実機 1 回目でテラスタイプの札と重なった） */
    ball = &gSprites[sBwSs->ballSpriteId];
    ball->callback = SpriteCallbackDummy;
    ball->oam.priority = 0;
    sBwSs->gmaxSpriteId = MAX_SPRITES;
    if (!sBwSs->isEgg && sBwSs->currentMon.gigantamax)
    {
        LoadCompressedSpriteSheetUsingHeap(&sGmaxIconSheet);
        LoadPalette(GigantamaxSummaryScreenIconPal, (15 * 16) + 0x100, 32);
        sBwSs->gmaxSpriteId = CreateSprite(&sGmaxIconTemplate, ball->pos1.x - 32, ball->pos1.y, 0);
        if (sBwSs->gmaxSpriteId < MAX_SPRITES)
        {
            gSprites[sBwSs->gmaxSpriteId].oam.paletteNum = 15;
            gSprites[sBwSs->gmaxSpriteId].callback = SpriteCB_GmaxIcon;
        }
    }
    SetInvisBall(1);
}

static void SetInvisBall(u8 inv)
{
    gSprites[sBwSs->ballSpriteId].invisible = inv;
    if (sBwSs->gmaxSpriteId < MAX_SPRITES)
        gSprites[sBwSs->gmaxSpriteId].invisible = inv;
}

static void FreeBall(void)
{
    DestroySpriteAndFreeResources(&gSprites[sBwSs->ballSpriteId]);
    BwSs_FreeBallGfx(sBwSs->ballId);
    if (sBwSs->gmaxSpriteId < MAX_SPRITES)
        DestroySpriteAndFreeResources(&gSprites[sBwSs->gmaxSpriteId]);
    sBwSs->gmaxSpriteId = MAX_SPRITES;
}

/* ---- アイコン（技えらびの画面用。DPE のメス姿を通す） ---- */
static void LoadIcon(void)
{
    u16 species = GetMonData(&sBwSs->currentMon, MON_DATA_SPECIES2, NULL);
    u32 personality = GetMonData(&sBwSs->currentMon, MON_DATA_PERSONALITY, NULL);
    bool8 handleDeoxys = TRUE;

    /* ★ メス姿（DPE の TryGetFemaleGenderedSpecies）は DPE 側の関数で CFRU から呼べない。
       アイコンの絵は DPE が CreateMonIcon の中（GetIconSpecies 8096988）で差し替えるが、パレットは種族のもの */
    SafeLoadMonIconPalette(GetIconSpecies(species, personality)); /* 萌えもんのアイコンは自分の色の組（2026-10-08・DPE の src/hack_moemon.c） */
    if (sBwSs->savedCallback == BwSs_CB2_ReturnToTradeMenuFromSummary)
        handleDeoxys = !sBwSs->isEnemyParty;
    else if (BwSs_ShouldIgnoreDeoxysForm(3, sLastViewedMonIndex))
        handleDeoxys = FALSE;
    sBwSs->iconSpriteId = CreateMonIcon(species, SpriteCallbackDummy, 140, 28, 0, personality, handleDeoxys);
    gSprites[sBwSs->iconSpriteId].hFlip = BwSs_IsPokeSpriteNotFlipped(species) ? TRUE : FALSE;
    SetInvisIcon(1);
}

static void SetInvisIcon(u8 inv)
{
    gSprites[sBwSs->iconSpriteId].invisible = inv;
}

static void FreeIcon(void)
{
    u16 species = GetMonData(&sBwSs->currentMon, MON_DATA_SPECIES2, NULL);

    SafeFreeMonIconPalette(species);
    DestroyMonIcon(&gSprites[sBwSs->iconSpriteId]);
}

/* ---- ポケモンの絵（右上）。HP に応じて上下に揺れる。たまごは左右に ---- */
static void LoadMonPic(void)
{
    u16 species = GetMonData(&sBwSs->currentMon, MON_DATA_SPECIES2, NULL);
    u32 personality = GetMonData(&sBwSs->currentMon, MON_DATA_PERSONALITY, NULL);
    u32 otId = GetMonData(&sBwSs->currentMon, MON_DATA_OT_ID, NULL);
    u16 spriteId;
    bool8 ignoreDeoxys;

    sAnim = AllocZeroed(sizeof(struct BwSsAnim));
    if (sBwSs->savedCallback == BwSs_CB2_ReturnToTradeMenuFromSummary)
        ignoreDeoxys = sBwSs->isEnemyParty;
    else
        ignoreDeoxys = BwSs_ShouldIgnoreDeoxysForm(3, sLastViewedMonIndex);
    if (ignoreDeoxys)
        spriteId = BwSs_CreateMonPicSprite(species, otId, personality, 1, 204, 78, 12, 0xFFFF, 1);
    else
        spriteId = CreateMonPicSprite_HandleDeoxys(species, otId, personality, 1, 204, 78, 12, 0xFFFF);
    FreeSpriteOamMatrix(&gSprites[spriteId]);
    sBwSs->monSpriteId = spriteId;
    SetInvisMonPic(1);
    SetMonPicAnim(spriteId);
}

static void SetMonPicAnim(u16 spriteId)
{
    u16 curHp, maxHp;

    sBwSs->animCount = 0;
    if (sBwSs->isEgg == TRUE)
    {
        u8 friendship = GetMonData(&sBwSs->currentMon, MON_DATA_FRIENDSHIP, NULL);

        if (friendship <= 5)
            sAnim->kind = 2;
        else if (friendship <= 10)
            sAnim->kind = 1;
        else if (friendship <= 40)
            sAnim->kind = 0;
        gSprites[spriteId].callback = SpriteCB_EggShake;
        return;
    }
    if (sBwSs->monStatus != AIL_NONE && sBwSs->monStatus != AIL_PKRS)
    {
        if (sBwSs->monStatus != AIL_FNT)
            gSprites[spriteId].callback = SpriteCB_None;
        return;
    }
    curHp = GetMonData(&sBwSs->currentMon, MON_DATA_HP, NULL);
    maxHp = GetMonData(&sBwSs->currentMon, MON_DATA_MAX_HP, NULL);
    if (curHp == maxHp)
        sAnim->kind = 3;
    else if (curHp * 5 >= maxHp * 4)
        sAnim->kind = 2;
    else if (curHp * 5 >= maxHp * 3)
        sAnim->kind = 1;
    else
        sAnim->kind = 0;
    gSprites[spriteId].callback = SpriteCB_MonBob;
}

static void SpriteCB_MonBob(struct Sprite *sprite)
{
    const s8 *table;
    u8 n;

    if (sBwSs->animCount >= 2)
        return;
    if (sAnim->tileTag++ < 2)
        return;
    switch (sAnim->kind)
    {
    case 0: table = sBob3; n = NELEMS(sBob3); break;
    case 1: table = sBob5; n = NELEMS(sBob5); break;
    case 2: table = sBob7; n = NELEMS(sBob7); break;
    default: table = sBob7b; n = NELEMS(sBob7b); break;
    }
    sprite->pos1.y += table[sAnim->index++];
    if (sAnim->index >= n)
    {
        sAnim->index = 0;
        sBwSs->animCount++;
    }
    sAnim->tileTag = 0;
}

static void SpriteCB_EggShake(struct Sprite *sprite)
{
    const s8 *table;
    u8 n;
    u8 wait;

    if (sBwSs->animCount >= 2)
        return;
    switch (sAnim->kind)
    {
    case 1: table = sEggShake11b; n = NELEMS(sEggShake11b); wait = 90; break;
    case 2: table = sEggShake15; n = NELEMS(sEggShake15); wait = 60; break;
    default: table = sEggShake11; n = NELEMS(sEggShake11); wait = 120; break;
    }
    if (sAnim->tileTag++ >= wait)
    {
        sprite->pos1.x += table[sAnim->index];
        if (++sAnim->index >= n)
        {
            sAnim->index = 0;
            sAnim->tileTag = 0;
            sBwSs->animCount++;
        }
    }
}

static void SpriteCB_None(unusedArg struct Sprite *sprite)
{
}

static void SetInvisMonPic(u8 inv)
{
    gSprites[sBwSs->monSpriteId].invisible = inv;
}

static void FreeMonPic(void)
{
    FreeAndDestroyMonPicSprite(sBwSs->monSpriteId);
    FREE_IF_SET(sAnim);
}

/* ---- まとめて ---- */
static void FreeAllSprites(void)
{
    FreeCursor();
    FreeBar(sHpBar, 9);
    sHpBar = NULL;
    FreeBar(sExpBar, 11);
    sExpBar = NULL;
    FreeMonPic();
    FreeIcon();
    FreeBall();
    if (sBwSs->markingSprite != NULL)
        DestroySpriteAndFreeResources(sBwSs->markingSprite);
    FreeSpriteBox(sStatusIcon);
    sStatusIcon = NULL;
    FreeSpriteBox(sDot);
    sDot = NULL;
    FreeSpriteBox(sShiny);
    sShiny = NULL;
    ResetSpriteData();
}

/* ポケモンを切り替えたあと、絵・ボール・アイコン・バーを作り直す */
static void RefreshSprites(void)
{
    LoadBall();
    SetInvisBall(0);
    LoadIcon();
    LoadMonPic();
    SetInvisMonPic(0);
    SetupHpBar();
    SetupExpBar();
    UpdateMarkings();
    ShowStatusIcon();
    UpdateDot();
    UpdateShiny();
}

/* ---------------- 技の詳細ページ（技の入れ替え） ---------------- */
static void MoveCursorUp(u8 nextState)
{
    u8 i;

    if (sMoveCursor > 0)
    {
        sBwSs->flipState = nextState;
        PlaySE(SE_SELECT);
        for (i = sMoveCursor; i > 0; i--)
        {
            if (sBwSs->moves[i - 1] != MOVE_NONE)
            {
                sMoveCursor = i - 1;
                return;
            }
        }
    }
    else
    {
        sMoveCursor = 4;
        sBwSs->flipState = nextState;
        PlaySE(SE_SELECT);
        if (sBwSs->swapping == TRUE)
        {
            for (i = sMoveCursor; i > 0; i--)
            {
                if (sBwSs->moves[i - 1] != MOVE_NONE)
                {
                    sMoveCursor = i - 1;
                    return;
                }
            }
        }
    }
}

static void MoveCursorDown(u8 nextState)
{
    u8 i;

    if (sMoveCursor < 4)
    {
        u8 limit = 4;

        sBwSs->flipState = nextState;
        if (sBwSs->swapping == TRUE)
        {
            if (sMoveCursor == 3)
            {
                sMoveCursor = 0;
                PlaySE(SE_SELECT);
                return;
            }
            limit--;
        }
        for (i = sMoveCursor; i < limit; i++)
        {
            if (sBwSs->moves[i + 1] != MOVE_NONE)
            {
                PlaySE(SE_SELECT);
                sMoveCursor = i + 1;
                return;
            }
        }
        PlaySE(SE_SELECT);
        sMoveCursor = sBwSs->swapping ? 0 : i;
    }
    else if (sMoveCursor == 4)
    {
        sMoveCursor = 0;
        sBwSs->flipState = nextState;
        PlaySE(SE_SELECT);
    }
}

static void Task_MovesInfoInput(unusedArg u8 taskId)
{
    switch (sBwSs->flipState)
    {
    case 0:
        if (BwSs_MenuHelpers_CallLinkSomething() == TRUE || BwSs_IsLinkRecvQueueLengthAtLeast3() == TRUE)
            return;
        if (JOY_NEW(DPAD_UP))
        {
            MoveCursorUp(2);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            MoveCursorDown(2);
        }
        else if (JOY_NEW(A_BUTTON))
        {
            PlaySE(SE_SELECT);
            if (sMoveCursor == 4)
            {
                /* やめる */
                sMoveCursor = 0;
                sMoveSwapPos = 0;
                sBwSs->swapping = FALSE;
                SetInvisCursor(1);
                sBwSs->pageDir = 0;
                RemoveAllWindows();
                sBwSs->curPageIndex--;
                sBwSs->flipState = 1;
                return;
            }
            if (sBwSs->swapping != TRUE)
            {
                if (sBwSs->isEnemyParty == FALSE && gMain.inBattle == 0 && BWSS_RECEIVED_REMOTE_LINK_PLAYERS == 0)
                {
                    sMoveSwapPos = sMoveCursor;
                    sBwSs->swapping = TRUE;
                }
                return;
            }
            sBwSs->swapping = FALSE;
            if (sMoveCursor == sMoveSwapPos)
                return;
            if (sBwSs->isBoxMon == 0)
                SwapMovesParty();
            else
                SwapMovesBox();
            CopyMonFromList(&sBwSs->currentMon);
            GetAllMoves();
            sBwSs->flipState = 2;
        }
        else if (JOY_NEW(B_BUTTON))
        {
            if (sBwSs->swapping == TRUE)
            {
                sMoveSwapPos = sMoveCursor;
                sBwSs->swapping = FALSE;
                return;
            }
            if (sMoveCursor == 4)
            {
                sMoveCursor = 0;
                sMoveSwapPos = 0;
            }
            SetInvisCursor(1);
            sBwSs->pageDir = 0;
            RemoveAllWindows();
            sBwSs->curPageIndex--;
            sBwSs->flipState = 1;
        }
        break;
    case 1:
        gTasks[sBwSs->task].func = Task_FromMovesInfo;
        sBwSs->flipState = 0;
        break;
    case 2:
        PrintWin3();
        PrintWin4();
        PrintWin5();
        sBwSs->flipState = 3;
        break;
    case 3:
        if (BwSs_MenuHelpers_CallLinkSomething() == TRUE || BwSs_IsLinkRecvQueueLengthAtLeast3() == TRUE)
            return;
        CopyWindowToVram(sBwSs->window[3], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[4], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[5], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[6], COPYWIN_GFX);
        CopyBgTilemapBufferToVram(0);
        sBwSs->flipState = 0;
        break;
    }
}

/* ---------------- 技を選ぶ／忘れる画面（わざマシン・レベルアップ） ---------------- */
static void Task_SelectMove(u8 taskId)
{
    switch (sBwSs->flipState)
    {
    case 0:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, 0);
        sBwSs->flipState++;
        break;
    case 1:
        if (!gPaletteFade->active)
        {
            PlayMonCry();
            sBwSs->flipState++;
        }
        break;
    case 2:
        if (JOY_NEW(DPAD_UP))
        {
            MoveCursorUp(3);
        }
        else if (JOY_NEW(DPAD_DOWN))
        {
            MoveCursorDown(3);
        }
        else if (JOY_NEW(A_BUTTON))
        {
            if (CanForgetSelectedMove() == TRUE || sMoveCursor == 4)
            {
                PlaySE(SE_SELECT);
                sMoveSwapPos = sMoveCursor;
                Var8005 = sMoveSwapPos;
                sBwSs->flipState = 6;
            }
            else
            {
                PlaySE(SE_ERROR);
                sBwSs->flipState = 5;
            }
        }
        else if (JOY_NEW(B_BUTTON))
        {
            sMoveSwapPos = 4;
            Var8005 = sMoveSwapPos;
            sBwSs->flipState = 6;
        }
        break;
    case 3:
        PrintWin3();
        PrintWin4();
        PrintWin5();
        sBwSs->flipState = 4;
        break;
    case 4:
        if (BwSs_MenuHelpers_CallLinkSomething() == TRUE || BwSs_IsLinkRecvQueueLengthAtLeast3() == TRUE)
            return;
        CopyWindowToVram(sBwSs->window[3], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[4], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[5], COPYWIN_GFX);
        CopyWindowToVram(sBwSs->window[6], COPYWIN_GFX);
        CopyBgTilemapBufferToVram(0);
        sBwSs->flipState = 2;
        break;
    case 5:
        FillWindowPixelBuffer(sBwSs->window[4], 0);
        AddTextPrinterParameterized4(sBwSs->window[4], BWSS_FONT, 7, 42, 0, 0, sTextColours[DARK], TEXT_SPEED_FF, gBwSsText_HmCantForget);
        CopyWindowToVram(sBwSs->window[4], COPYWIN_GFX);
        CopyBgTilemapBufferToVram(0);
        sBwSs->flipState = 2;
        break;
    case 6:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, 0);
        sBwSs->flipState++;
        break;
    default:
        if (!gPaletteFade->active)
            Exit(taskId);
        break;
    }
}

/* ---------------- 上下でポケモンを切り替える ---------------- */
static s8 NextPartyMon(s8 delta)
{
    struct Pokemon *mons = sBwSs->monList.mons;
    s8 step = 0;

    if (sBwSs->curPageIndex == PSS_PAGE_INFO)
    {
        if (delta == -1 && sLastViewedMonIndex == 0)
            return -1;
        if (delta == 1 && sLastViewedMonIndex >= sBwSs->lastIndex)
            return -1;
        return sLastViewedMonIndex + delta;
    }
    while (TRUE)
    {
        step += delta;
        if ((s8)sLastViewedMonIndex + step < 0 || sLastViewedMonIndex + step > sBwSs->lastIndex)
            return -1;
        if (GetMonData(&mons[sLastViewedMonIndex + step], MON_DATA_IS_EGG, NULL) == 0)
            return sLastViewedMonIndex + step;
    }
}

static bool8 IsMultiMonSelectable(struct Pokemon *mon)
{
    if (GetMonData(mon, MON_DATA_SPECIES, NULL) != SPECIES_NONE
        && (sBwSs->curPageIndex != PSS_PAGE_INFO || GetMonData(mon, MON_DATA_IS_EGG, NULL) == 0))
        return TRUE;
    return FALSE;
}

static s8 NextMultiMon(s8 delta)
{
    u8 pos = 0;
    u8 i;

    for (i = 0; i < 6; i++)
    {
        if (sMultiOrder[i] == sLastViewedMonIndex)
        {
            pos = i;
            break;
        }
    }
    if ((delta == -1 && pos == 0) || (delta == 1 && pos == 5))
        return -1;
    if (delta == 1)
    {
        while (++pos < 6)
            if (IsMultiMonSelectable(&gPlayerParty[sMultiOrder[pos]]))
                return sMultiOrder[pos];
        return -1;
    }
    while (pos > 0)
    {
        pos--;
        if (IsMultiMonSelectable(&gPlayerParty[sMultiOrder[pos]]))
            return sMultiOrder[pos];
    }
    return -1;
}

static void TryChangeMon(unusedArg u8 taskId, s8 delta)
{
    s8 next;

    if (sBwSs->isBoxMon == TRUE)
    {
        u8 flags;

        if (sBwSs->curPageIndex != PSS_PAGE_INFO)
            flags = (delta == 1) ? 0 : 2;
        else
            flags = (delta == 1) ? 1 : 3;
        next = sub_808B884(sBwSs->monList.boxMons, sLastViewedMonIndex, sBwSs->lastIndex, flags);
    }
    else if (IsUpdateLinkStateCBActive() == FALSE && BWSS_RECEIVED_REMOTE_LINK_PLAYERS == 1 && IsMultiBattle() == TRUE)
    {
        next = NextMultiMon(delta);
    }
    else
    {
        next = NextPartyMon(delta);
    }
    if (next == -1)
        return;
    sLastViewedMonIndex = next;
    CreateTask(Task_ChangeMon, 0);
    sBwSs->changeMonState = 0;
}

static void Task_ChangeMon(u8 taskId)
{
    switch (sBwSs->changeMonState)
    {
    case 0:
        StopCryAndClearCrySongs();
        sMoveCursor = 0;
        sMoveSwapPos = 0;
        break;
    case 1:
        FreeMonPic();
        FreeIcon();
        FreeBall();
        break;
    case 2:
        CopyMonFromList(&sBwSs->currentMon);
        sBwSs->isEgg = GetMonData(&sBwSs->currentMon, MON_DATA_IS_EGG, NULL);
        sBwSs->isBadEgg = GetMonData(&sBwSs->currentMon, MON_DATA_SANITY_IS_BAD_EGG, NULL);
        if (sBwSs->isBadEgg == TRUE)
            sBwSs->isEgg = TRUE;
        break;
    case 3:
    case 4:
        break;
    case 5:
        sBwSs->numMoves = 0;
        GetMonInfo();
        break;
    case 6:
        if (!sBwSs->isEgg)
            GetMonStats();
        break;
    case 7:
        if (!sBwSs->isEgg)
            GetAllMoves();
        break;
    case 8:
        PrintWin3();
        PrintWin4();
        PrintWin5();
        break;
    case 9:
        DrawTypeIcons();
        LoadPageTilemaps();
        PrintTitleAndControls(sBwSs->curPageIndex);
        break;
    case 10:
        {
            u8 i;
            for (i = 0; i < 7; i++)
                CopyWindowToVram(sBwSs->window[i], COPYWIN_GFX);
        }
        CopyBgTilemapBufferToVram(0);
        break;
    case 11:
        if (BwSs_LinkRecvQueueLengthMoreThan2() || BwSs_IsLinkRecvQueueLengthAtLeast3())
            return;
        RefreshSprites();
        PlayMonCry();
        break;
    default:
        sBwSs->changeMonState = 0;
        DestroyTask(taskId);
        return;
    }
    sBwSs->changeMonState++;
}
