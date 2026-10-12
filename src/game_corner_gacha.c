#include "defines.h"
#include "../include/bg.h"
#include "../include/coins.h"
#include "../include/decompress.h"
#include "../include/event_data.h"
#include "../include/gpu_regs.h"
#include "../include/international_string_util.h"
#include "../include/main.h"
#include "../include/malloc.h"
#include "../include/menu.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/pokedex.h"
#include "../include/pokemon.h"
#include "../include/random.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/trainer_pokemon_sprites.h"
#include "../include/window.h"
#include "../include/constants/flags.h"
#include "../include/constants/rgb.h"
#include "../include/constants/songs.h"
#include "../include/constants/species.h"
#include "../include/new/dns.h"

//Game Corner music on start, and restore the map music on exit.
//(PlayBGM does not update the saved map music, so it is written by hand.)
#define GAME_CORNER_MUSIC 0x111 //MUS_GAME_CORNER
static void RestoreMapMusicAfterGame(void)
{
    u16 mapMusic = GetCurrLocationDefaultMusic();
    *((vu16*) 0x3000FC0) = mapMusic; //sCurrentMapMusic
    *((vu16*) 0x3000FC2) = 0;        //sNextMapMusic
    *((vu8*)  0x3000FC4) = 0;        //sMapMusicState
    PlayBGM(mapMusic);
}

/*
game_corner_gacha.c
	Gacha (ported from SoulGold / pokeemerald-expansion game corner)
	Script usage:  setvar 0x8004 <1=Basic 2=Great 3=Ultra 4=Master> ; callasm StartGacha ; waitstate
	Result:        LASTRESULT = 0 (added to party) / 1 (sent to PC) / 2 (cancelled with B, nothing received)
	               Var 0x8005 = species that was received
	The player's coins are used directly (GetCoins / TakeCoins).
*/

//Declared in new/Vanilla_functions.h, which conflicts with gpu_regs.h, so declare here
void __attribute__((long_call)) ResetBgsAndClearDma3BusyFlags(u8 bg);
void __attribute__((long_call)) ResetVramOamAndBgCntRegs(void);
void __attribute__((long_call)) DoScheduledBgTilemapCopiesToVram(void);
void __attribute__((long_call)) DeactivateAllTextPrinters(void);
u8 GiveMonToPlayer(struct Pokemon *mon); //src/catching.c

#define gSpecialVar_0x8004 Var8004
#define gSpecialVar_Result gSpecialVar_LastResult
#define FONT_GACHA 1 //Font used for the texts (change if the text looks wrong)
#define RemoveCoins TakeCoins
#define BG_PLTT_ID(n) ((n) * 16)
#define COPYWIN_FULL COPYWIN_BOTH
#define PLTT_SIZE_4BPP 32
#ifndef PALETTES_ALL
#define PALETTES_ALL 0xFFFFFFFF
#endif
#define MAX_COIN_DIGITS 4
#define WINDOW_NONE 0xFF
#define GACHA_RESULT_SPECIES_VAR 0x8005

//Sound effects (BPRJ equivalents of the Emerald ones)
#define SE_FAILURE SE_ERROR
#define SE_SHOP 88 //SoulGold SE_SHOP (SE_REGI) = ROM song 88
#define SE_VEND 99 //SoulGold SE_VEND (SE_JIHANKI) = ROM song 99
#define SE_BALL_BOUNCE_1 49 //SoulGold SE_BALL_BOUNCE_1 (SE_KON) = ROM song 49
#define SE_BALL_BOUNCE_2 SE_POKE_BALL_BOUNCE_2
#define SE_BALL_BOUNCE_3 SE_POKE_BALL_BOUNCE_3
#define SE_BALL_BOUNCE_4 SE_POKE_BALL_BOUNCE_4
#define MUS_EVOLVED MUS_FANFA1

extern const u16 gGachaTextPal[];
extern const u8 gText_Gacha_NewOdds[];
extern const u8 gText_Gacha_YouGot[];

enum
{
    GACHA_STATE_INIT,
    GACHA_STATE_PROCESS_INPUT,
    GACHA_STATE_START_EXIT,
    GACHA_STATE_EXIT,
    STATE_INIT_A,
    STATE_TIMER_1,
    STATE_TWIST,
    STATE_TIMER_2,
    STATE_INIT_GIVE,
    STATE_SHAKE_1,
    STATE_TIMER_3,
    STATE_INIT_SHAKE_2,
    STATE_SHAKE_2,
    STATE_TIMER_4,
    STATE_INIT_SHAKE_3,
    STATE_TIMER_5,
    STATE_GIVE,
    STATE_FADE,
    STATE_POKEBALL_INIT,
    STATE_POKEBALL_PROCESS,
    STATE_POKEBALL_ARRIVE,
    STATE_FADE_POKEBALL_TO_NORMAL,
    STATE_POKEBALL_ARRIVE_WAIT,
    STATE_SHOW_NEW_MON,
    STATE_NEW_MON_MSG,
    NEW_1,
    NEW_2,
    STATE_SET_EXIT,
};

enum {
    SPR_CREDIT_DIG_1,
    SPR_CREDIT_DIG_10,
    SPR_CREDIT_DIG_100,
    SPR_CREDIT_DIG_1000,
};

enum {
    GACHA_BASIC = 1,
    GACHA_GREAT,
    GACHA_ULTRA,
    GACHA_MASTER,
};

enum {
    RARITY_COMMON,
    RARITY_UNCOMMON,
    RARITY_RARE,
    RARITY_ULTRA_RARE,
};

enum {
    SPR_PLAYER_DIG_1,
    SPR_PLAYER_DIG_10,
    SPR_PLAYER_DIG_100,
    SPR_PLAYER_DIG_1000,
};

#define RARITY_COMMON_ODDS 40
#define RARITY_UNCOMMON_ODDS 35
#define RARITY_RARE_ODDS 20
#define RARITY_ULTRA_RARE_ODDS 5

#define GACHA_BASIC_MIN_WAGER 50
#define GACHA_GREAT_MIN_WAGER 250
#define GACHA_ULTRA_MIN_WAGER 1000
#define GACHA_MASTER_MIN_WAGER 3000
#define GACHA_GUARANTEED_NEW_WAGER 4999

#define SPR_CREDIT_DIGITS SPR_CREDIT_DIG_1
#define SPR_PLAYER_DIGITS SPR_PLAYER_DIG_1

#define MAX_SPRITES_CREDIT 4
#define MAX_SPRITES_PLAYER 4

struct Gacha {
    u8 state;
    u8 GachaId;
    u8 KnobSpriteId;
    u8 DigitalTextSpriteId;
    u8 LotteryJPNspriteId;
    u8 CreditSpriteIds[MAX_SPRITES_CREDIT];
    u8 PlayerSpriteIds[MAX_SPRITES_PLAYER];
    u8 CreditMenu1Id;
    u8 CreditMenu2Id;
    u8 PokemonOneSpriteId;
    u8 PokemonTwoSpriteId;
    u8 PokemonThreeSpriteId;
    u8 newMonOdds;
    u8 ArrowsSpriteId;
    u8 CTAspriteId;
    u8 cursorPosition;
    bool8 canBetWager;
    u8 Rarity; // 0 = Common, 1 = Uncommon, 2 = Rare, 3 = Ultra Rare
    u8 ownedCommon;
    u8 ownedUncommon;
    u8 ownedRare;
    u8 ownedUltraRare;
    u8 bouncingPokeballSpriteId;
    u8 timer;
    u16 monSpriteId;
    u16 CalculatedSpecies;
    u16 wager;
    u32 waitTimer;
    u8 canGiveMon;
    u8 monShown;
    u8 bgReady;
    struct Pokemon mon;
};    


static const s8 sTradeBallVerticalVelocityTable[] =
{
     0,  0,  1,  0,  1,  0,  1,  1,  1,
     1,  2,  2,  2,  2,  3,  3,  3,  3,
     4,  4,  4,  4, -4, -4, -4, -3, -3,
    -3, -3, -2, -2, -2, -2, -1, -1, -1,
    -1,  0, -1,  0, -1,  0,  0,  0,  0,
     0,  1,  0,  1,  0,  1,  1,  1,  1,
     2,  2,  2,  2,  3,  3,  3,  3,  4,
     4,  4,  4, -4, -3, -3, -2, -2, -1,
    -1, -1,  0, -1,  0,  0,  0,  0,  0,
     0,  1,  0,  1,  1,  1,  2,  2,  3,
     3,  4, -4, -3, -2, -1, -1, -1,  0,
     0,  0,  0,  1,  0,  1,  1,  2,  3
};

#define sGacha (*((struct Gacha **) 0x2023B44)) //Fixed RAM (CFRU cannot place writable variables in RAM): reuses gBattleMons, free outside battle
#define sGachaTilemap (*((u16 **) 0x2023B48)) //Fixed RAM (CFRU cannot place writable variables in RAM): reuses gBattleMons, free outside battle
#define sTextWindowId (*((u8*) 0x2023B4C)) //Fixed RAM (CFRU cannot place writable variables in RAM): reuses gBattleMons, free outside battle

static void FadeToGachaScreen(u8 taskId);
static void ShowFinalMessage(void);

//CFRU Pokedex (national dex flags, up to NATIONAL_DEX_COUNT) is used for owned checks / registration.
extern void SetMonPokedexFlags(struct Pokemon* mon);

static bool32 GachaHasCaught(u16 species)
{
    u16 nationalNum;

    if (species == SPECIES_NONE)
        return FALSE;
    nationalNum = SpeciesToNationalPokedexNum(species);
    if (nationalNum == 0)
        return FALSE;
    return GetSetPokedexFlag(nationalNum, FLAG_GET_CAUGHT) != 0;
}

static void ResetMessage(void);
static void ShowMessage(void);
static void InitGachaScreen(void);
static void GachaVBlankCallback(void);
static void SpriteCB_BouncingPokeball(struct Sprite *);
static void SpriteCB_BouncingPokeballArrive(struct Sprite *);


static void SpriteCB_Null(struct Sprite *sprite)
{
}

// BG Images/Tilemaps

// Main, no shake
extern const u8 gGacha_Gacha_BG_Main[5632];
extern const u8 gGacha_Gacha_BG_Main_Tilemap[2048];
extern const u8 gGacha_Gacha_BG_Left[5632];
extern const u8 gGacha_Gacha_BG_Left_Tilemap[2048];
extern const u8 gGacha_Gacha_BG_Right[5632];
extern const u8 gGacha_Gacha_BG_Right_Tilemap[2048];
extern const u8 gGacha_Gacha_BG_Red[1024];
extern const u8 gGacha_Gacha_BG_Red_Tilemap[2048];

// Basic
extern const u16 gGacha_Gacha_BG_Basic_Pal[16];
extern const u16 gGacha_Gacha_BG_Great_Pal[16];
extern const u16 gGacha_Gacha_BG_Ultra_Pal[16];
extern const u16 gGacha_Gacha_BG_Master_Pal[16];
extern const u16 gGacha_Gacha_BG_Red_Pal[16];
extern const u8 gGacha_Gacha_Knob[2048];

extern const u16 gGacha_Gacha_Knob_Pal[16];
extern const u16 gGacha_Gacha_Digital_Text_Pal[16];
extern const u16 gGacha_Gacha_Lottery_Pal[16];
extern const u16 gGacha_Gacha_press_a_Pal[16];
extern const u8 gGacha_Gacha_Digital_Text[4096];
extern const u8 gGacha_Gacha_Lottery_JPN[2048];

extern const u8 gGacha_gCredits_Gfx[640];
extern const u16 gGacha_sCredit_Pal[16];
extern const u8 gGacha_gPlayer_Gfx[640];
extern const u16 gGacha_sPlayer_Pal[16];

// Images

extern const u8 gGacha_Gacha_Menu_1[2048];
extern const u8 gGacha_Gacha_Menu_2[4096];

// Basic
extern const u16 gGacha_Gacha_Menu_Basic_Pal[16];
extern const u16 gGacha_Gacha_Menu_Great_Pal[16];
extern const u16 gGacha_Gacha_Menu_Ultra_Pal[16];
extern const u16 gGacha_Gacha_Menu_Master_Pal[16];
extern const u16 gGacha_Gacha_Menu2_Basic_Pal[16];
extern const u16 gGacha_Gacha_Menu2_Great_Pal[16];
extern const u16 gGacha_Gacha_Menu2_Ultra_Pal[16];
extern const u16 gGacha_Gacha_Menu2_Master_Pal[16];

// Belossom
extern const u8 gGacha_BelossomGFX[2048];
extern const u16 gGacha_BelossomPAL[16];
extern const u8 gGacha_PhanpyGFX[2048];
extern const u16 gGacha_PhanpyPal[16];
extern const u8 gGacha_TeddiursaGFX[2048];
extern const u16 gGacha_TeddiursaPAL[16];
extern const u8 gGacha_ElekidGFX[2048];
extern const u16 gGacha_ElekidPAL[16];
extern const u8 gGacha_HoppipGFX[2048];
extern const u16 gGacha_HoppipPAL[16];

extern const u8 gGacha_Gacha_Arrows_GFX[512];

extern const u8 gGacha_Gacha_Press_A_GFX[3072];
extern const u16 gGacha_sPokeball_Pal[16];
extern const u8 gGacha_sPokeball_Gfx[1536];
#define GACHA_BG_BASE 1
#define GACHA_MENUS 2

static const struct BgTemplate sGachaBGtemplates[] = {
    {
       .bg = GACHA_BG_BASE,
       .charBaseIndex = 2,
       .mapBaseIndex = 31,
       .screenSize = 0,
       .paletteMode = 0,
       .priority = 3,
       .baseTile = 0
   },
   {
        .bg = GACHA_MENUS,
        .charBaseIndex = 0,
        .mapBaseIndex = 0x17,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    }
};

static const struct WindowTemplate sGachaWinTemplates[] = {
    {
        .bg = GACHA_MENUS,
        .tilemapLeft = 16,
        .tilemapTop = 9,
        .width = 14,
        .height = 2,
        .paletteNum = 0xF,
        .baseBlock = 0x194,
    },
    DUMMY_WIN_TEMPLATE,
};

static const struct WindowTemplate sWinTemplates_EggHatch[] =
{
    {
        .bg = 0,
        .tilemapLeft = 2,
        .tilemapTop = 15,
        .width = 26,
        .height = 4,
        .paletteNum = 0,
        .baseBlock = 64
    },
    DUMMY_WIN_TEMPLATE
};

static const struct WindowTemplate sYesNoWinTemplate =
{
    .bg = 0,
    .tilemapLeft = 21,
    .tilemapTop = 9,
    .width = 5,
    .height = 4,
    .paletteNum = 15,
    .baseBlock = 424
};

#define BG_MIDDLE_GFX 1
#define BG_LEFT_GFX 2
#define BG_RIGHT_GFX 3
#define KNOB_GFX 4
#define DIGITAL_TEXT_GFX 5
#define LOTTERY_JPN_GFX 6
#define GFXTAG_CREDIT_DIGIT 7
#define GFXTAG_PLAYER_DIGIT 8
#define GFXTAG_MENU_1 9
#define GFXTAG_MENU_2 10
#define GFXTAG_MASCOT 11
#define GFXTAG_ARROWS 12
#define GFXTAG_PRESS_A 13

#define GFXTAG_POKEBALL        5557

#define PALTAG_KNOB 1
#define DIGITAL_TEXT_PAL 2
#define LOTTERY_JPN_PAL 3
#define PALTAG_INTERFACE 4
#define PALTAG_INTERFACEPLAYER 5
#define PALTAG_MENU_ID 6

#define PALTAG_MASCOT 7
#define PALTAG_ARROWS 8
#define PALTAG_PRESS_A 9

#define PALTAG_POKEBALL  5558

static const struct SpritePalette sSpritePalettesBasic[] =
{
    { .data = gGacha_Gacha_press_a_Pal,       .tag = PALTAG_PRESS_A },
    { .data = gGacha_Gacha_Knob_Pal,          .tag = PALTAG_KNOB },
    { .data = gGacha_Gacha_Digital_Text_Pal,  .tag = DIGITAL_TEXT_PAL },
    { .data = gGacha_sCredit_Pal,             .tag = PALTAG_INTERFACE },
    { .data = gGacha_sPlayer_Pal,             .tag = PALTAG_INTERFACEPLAYER },
    { .data = gGacha_Gacha_Lottery_Pal,       .tag = LOTTERY_JPN_PAL },
    { .data = gGacha_Gacha_Menu_Basic_Pal,    .tag = PALTAG_MENU_ID },
    { .data = gGacha_HoppipPAL,               .tag = PALTAG_MASCOT },
    { .data = gGacha_sCredit_Pal,             .tag = PALTAG_ARROWS },
    {}
};

static const struct SpritePalette sSpritePalettesGreat[] =
{
    { .data = gGacha_Gacha_press_a_Pal,       .tag = PALTAG_PRESS_A },
    { .data = gGacha_Gacha_Knob_Pal,          .tag = PALTAG_KNOB },
    { .data = gGacha_Gacha_Digital_Text_Pal,  .tag = DIGITAL_TEXT_PAL },
    { .data = gGacha_sCredit_Pal,             .tag = PALTAG_INTERFACE },
    { .data = gGacha_sPlayer_Pal,             .tag = PALTAG_INTERFACEPLAYER },
    { .data = gGacha_Gacha_Lottery_Pal,       .tag = LOTTERY_JPN_PAL },
    { .data = gGacha_Gacha_Menu_Great_Pal,    .tag = PALTAG_MENU_ID },
    { .data = gGacha_PhanpyPal,               .tag = PALTAG_MASCOT },
    { .data = gGacha_sCredit_Pal,             .tag = PALTAG_ARROWS },
    {}
};

static const struct SpritePalette sSpritePalettesUltra[] =
{
    { .data = gGacha_Gacha_press_a_Pal,       .tag = PALTAG_PRESS_A },
    { .data = gGacha_Gacha_Knob_Pal,          .tag = PALTAG_KNOB },
    { .data = gGacha_Gacha_Digital_Text_Pal,  .tag = DIGITAL_TEXT_PAL },
    { .data = gGacha_sCredit_Pal,             .tag = PALTAG_INTERFACE },
    { .data = gGacha_sPlayer_Pal,             .tag = PALTAG_INTERFACEPLAYER },
    { .data = gGacha_Gacha_Lottery_Pal,       .tag = LOTTERY_JPN_PAL },
    { .data = gGacha_Gacha_Menu_Ultra_Pal,    .tag = PALTAG_MENU_ID },
    { .data = gGacha_TeddiursaPAL,            .tag = PALTAG_MASCOT },
    { .data = gGacha_sCredit_Pal,             .tag = PALTAG_ARROWS },
    {}
};

static const struct SpritePalette sSpritePalettesMaster[] =
{
    { .data = gGacha_Gacha_press_a_Pal,       .tag = PALTAG_PRESS_A },
    { .data = gGacha_Gacha_Knob_Pal,          .tag = PALTAG_KNOB },
    { .data = gGacha_Gacha_Digital_Text_Pal,  .tag = DIGITAL_TEXT_PAL },
    { .data = gGacha_sCredit_Pal,             .tag = PALTAG_INTERFACE },
    { .data = gGacha_sPlayer_Pal,             .tag = PALTAG_INTERFACEPLAYER },
    { .data = gGacha_Gacha_Lottery_Pal,       .tag = LOTTERY_JPN_PAL },
    { .data = gGacha_Gacha_Menu_Master_Pal,   .tag = PALTAG_MENU_ID },
    { .data = gGacha_BelossomPAL,             .tag = PALTAG_MASCOT },
    { .data = gGacha_sCredit_Pal,             .tag = PALTAG_ARROWS },
    {}
};

static const struct SpriteSheet sSpriteSheet_Press_A =
{
    .data = gGacha_Gacha_Press_A_GFX,
    .size = 0xC00,
    .tag = GFXTAG_PRESS_A,
};

static const struct SpriteSheet sSpriteSheet_Arrows =
{
    .data = gGacha_Gacha_Arrows_GFX,
    .size = 0x200,
    .tag = GFXTAG_ARROWS,
};

static const struct SpriteSheet sSpriteSheet_Hoppip =
{
    .data = gGacha_HoppipGFX,
    .size = 0x800,
    .tag = GFXTAG_MASCOT,
};

static const struct SpriteSheet sSpriteSheet_Elekid =
{
    .data = gGacha_ElekidGFX,
    .size = 0x800,
    .tag = GFXTAG_MASCOT,
};

static const struct SpriteSheet sSpriteSheet_Teddiursa =
{
    .data = gGacha_TeddiursaGFX,
    .size = 0x800,
    .tag = GFXTAG_MASCOT,
};

static const struct SpriteSheet sSpriteSheet_Phanpy =
{
    .data = gGacha_PhanpyGFX,
    .size = 0x800,
    .tag = GFXTAG_MASCOT,
};

static const struct SpriteSheet sSpriteSheet_Belossom =
{
    .data = gGacha_BelossomGFX,
    .size = 0x800,
    .tag = GFXTAG_MASCOT,
};

static const struct SpriteSheet sSpriteSheet_Menu_1 =
{
    .data = gGacha_Gacha_Menu_1,
    .size = 0x800,
    .tag = GFXTAG_MENU_1,
};

static const struct SpriteSheet sSpriteSheet_Menu_2 =
{
    .data = gGacha_Gacha_Menu_2,
    .size = 0x1000,
    .tag = GFXTAG_MENU_2,
};

static const struct SpriteSheet sSpriteSheets_Interface[] =
{
    {
        .data = gGacha_gCredits_Gfx,
        .size = 0x280,
        .tag = GFXTAG_CREDIT_DIGIT,
    },
    {}
};

static const struct SpriteSheet sSpriteSheets_PlayerInterface[] =
{
    {
        .data = gGacha_gPlayer_Gfx,
        .size = 0x280,
        .tag = GFXTAG_PLAYER_DIGIT
    },
    {}
};

static const struct SpriteSheet sSpriteSheet_Lottery_JPN =
{
    .data = gGacha_Gacha_Lottery_JPN,
    .size = 0x800,
    .tag = LOTTERY_JPN_GFX,
};

static const struct SpriteSheet sSpriteSheet_Digital_Text =
{
    .data = gGacha_Gacha_Digital_Text,
    .size = 0x1000,
    .tag = DIGITAL_TEXT_GFX,
};

static const struct SpriteSheet sSpriteSheet_Knob =
{
    .data = gGacha_Gacha_Knob,
    .size = 0x800,
    .tag = KNOB_GFX,
};

static const struct OamData sOamData_Press_A =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Arrows =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(8x32),
    .size = SPRITE_SIZE(8x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Hoppip =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Elekid =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Teddiursa =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Phanpy =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Belossom =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Menu =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Menu_2 =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Lottery_JPN =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Digital_Text =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_Knob =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOam_CreditDigit =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(8x16),
    .size = SPRITE_SIZE(8x16),
    .priority = 2,
};

static const struct OamData sOamData_Pokeball =
{
    .affineMode = ST_OAM_AFFINE_NORMAL,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16)
};

static const union AnimCmd sAnim_Pokeball_SpinOnce[] =
{
    ANIMCMD_FRAME( 0, 3),
    ANIMCMD_FRAME( 4, 3),
    ANIMCMD_FRAME( 8, 3),
    ANIMCMD_FRAME(12, 3),
    ANIMCMD_FRAME(16, 3),
    ANIMCMD_FRAME(20, 3),
    ANIMCMD_FRAME(24, 3),
    ANIMCMD_FRAME(28, 3),
    ANIMCMD_FRAME(32, 3),
    ANIMCMD_FRAME(36, 3),
    ANIMCMD_FRAME(40, 3),
    ANIMCMD_FRAME(44, 3),
    ANIMCMD_LOOP(1),
    ANIMCMD_FRAME( 0, 3),
    ANIMCMD_END
};

static const union AnimCmd sAnim_Pokeball_SpinTwice[] =
{
    ANIMCMD_FRAME( 0, 3),
    ANIMCMD_FRAME( 4, 3),
    ANIMCMD_FRAME( 8, 3),
    ANIMCMD_FRAME(12, 3),
    ANIMCMD_FRAME(16, 3),
    ANIMCMD_FRAME(20, 3),
    ANIMCMD_FRAME(24, 3),
    ANIMCMD_FRAME(28, 3),
    ANIMCMD_FRAME(32, 3),
    ANIMCMD_FRAME(36, 3),
    ANIMCMD_FRAME(40, 3),
    ANIMCMD_FRAME(44, 3),
    ANIMCMD_LOOP(2),
    ANIMCMD_FRAME( 0, 3),
    ANIMCMD_END
};

static const union AnimCmd *const sAnims_Pokeball[] =
{
    sAnim_Pokeball_SpinOnce,
    sAnim_Pokeball_SpinTwice
};

static const union AffineAnimCmd sAffineAnim_Pokeball_Normal[] =
{
    AFFINEANIMCMD_FRAME(0, 0, 0, 1),
    AFFINEANIMCMD_END
};

static const union AffineAnimCmd sAffineAnim_Pokeball_Squish[] =
{
    AFFINEANIMCMD_FRAME(-8, 0, 0, 20),
    AFFINEANIMCMD_END
};

static const union AffineAnimCmd sAffineAnim_Pokeball_Unsquish[] =
{
    AFFINEANIMCMD_FRAME(0x60, 0x100, 0,  0),
    AFFINEANIMCMD_FRAME(   0,     0, 0,  5),
    AFFINEANIMCMD_FRAME(   8,     0, 0, 20),
    AFFINEANIMCMD_END
};

static const union AffineAnimCmd *const sAffineAnims_Pokeball[] =
{
    sAffineAnim_Pokeball_Normal,
    sAffineAnim_Pokeball_Squish,
    sAffineAnim_Pokeball_Unsquish
};

static const struct SpriteSheet sPokeBallSpriteSheet =
{
    .data = gGacha_sPokeball_Gfx,
    .size = sizeof(gGacha_sPokeball_Gfx),
    .tag = GFXTAG_POKEBALL
};

static const struct SpritePalette sPokeBallSpritePalette =
{
    .data = gGacha_sPokeball_Pal,
    .tag = PALTAG_POKEBALL
};

static const struct SpriteTemplate sSpriteTemplate_Pokeball =
{
    .tileTag = GFXTAG_POKEBALL,
    .paletteTag = PALTAG_POKEBALL,
    .oam = &sOamData_Pokeball,
    .anims = sAnims_Pokeball,
    .images = NULL,
    .affineAnims = sAffineAnims_Pokeball,
    .callback = SpriteCB_BouncingPokeball
};

static const union AnimCmd sPressAAnimCmd_1[] = 
{
    ANIMCMD_FRAME(32, 10),
    ANIMCMD_FRAME(64, 10),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd sPressAAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 10),
    ANIMCMD_FRAME(0, 10),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sPressAAnimCmds[] = {
    sPressAAnimCmd_0, // Gray
    sPressAAnimCmd_1, // Highlight
};

static const struct SpriteTemplate sSpriteTemplate_Press_A =
{
    .tileTag = GFXTAG_PRESS_A,
    .paletteTag = PALTAG_PRESS_A,
    .oam = &sOamData_Press_A,
    .anims = sPressAAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const union AnimCmd sArrowAnimCmd_1[] = 
{
    ANIMCMD_FRAME(8, 20),
    ANIMCMD_FRAME(12, 20),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd sArrowAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_FRAME(4, 20),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sArrowsAnimCmds[] = {
    sArrowAnimCmd_0, // Up and Down
    sArrowAnimCmd_1, // Up
};

static const struct SpriteTemplate sSpriteTemplate_Arrows =
{
    .tileTag = GFXTAG_ARROWS,
    .paletteTag = PALTAG_ARROWS,
    .oam = &sOamData_Arrows,
    .anims = sArrowsAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const union AnimCmd sMenu2AnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 10),
    ANIMCMD_FRAME(64, 10),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sMenu2AnimCmds[] = {
    sMenu2AnimCmd_0,  // Looping animation
};

static const union AnimCmd sHoppipAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(48, 15),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sHoppipAnimCmds[] = {
    sHoppipAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Hoppip =
{
    .tileTag = GFXTAG_MASCOT,
    .paletteTag = PALTAG_MASCOT,
    .oam = &sOamData_Hoppip,
    .anims = sHoppipAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const union AnimCmd sElekidAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(32, 15),
    //ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(48, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sElekidAnimCmds[] = {
    sElekidAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Elekid =
{
    .tileTag = GFXTAG_MASCOT,
    .paletteTag = PALTAG_MASCOT,
    .oam = &sOamData_Elekid,
    .anims = sElekidAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const union AnimCmd sTeddiursaAnimCmd_0[] = 
{
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(48, 30),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sTeddiursaAnimCmds[] = {
    sTeddiursaAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Teddiursa =
{
    .tileTag = GFXTAG_MASCOT,
    .paletteTag = PALTAG_MASCOT,
    .oam = &sOamData_Teddiursa,
    .anims = sTeddiursaAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const union AnimCmd sPhanpyAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(48, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(48, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sPhanpyAnimCmds[] = {
    sPhanpyAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Phanpy =
{
    .tileTag = GFXTAG_MASCOT,
    .paletteTag = PALTAG_MASCOT,
    .oam = &sOamData_Phanpy,
    .anims = sPhanpyAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const union AnimCmd sBelossomAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(32, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(16, 15),
    ANIMCMD_FRAME(0, 15),
    ANIMCMD_FRAME(48, 30),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sBelossomAnimCmds[] = {
    sBelossomAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Belossom =
{
    .tileTag = GFXTAG_MASCOT,
    .paletteTag = PALTAG_MASCOT,
    .oam = &sOamData_Belossom,
    .anims = sBelossomAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Menu_1_Master =
{
    .tileTag = GFXTAG_MENU_1,
    .paletteTag = PALTAG_MENU_ID,
    .oam = &sOamData_Menu,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Menu_2_Master =
{
    .tileTag = GFXTAG_MENU_2,
    .paletteTag = PALTAG_MENU_ID,
    .oam = &sOamData_Menu_2,
    .anims = sMenu2AnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Menu_1_Ultra =
{
    .tileTag = GFXTAG_MENU_1,
    .paletteTag = PALTAG_MENU_ID,
    .oam = &sOamData_Menu,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Menu_2_Ultra =
{
    .tileTag = GFXTAG_MENU_2,
    .paletteTag = PALTAG_MENU_ID,
    .oam = &sOamData_Menu_2,
    .anims = sMenu2AnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Menu_1_Great =
{
    .tileTag = GFXTAG_MENU_1,
    .paletteTag = PALTAG_MENU_ID,
    .oam = &sOamData_Menu,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Menu_2_Great =
{
    .tileTag = GFXTAG_MENU_2,
    .paletteTag = PALTAG_MENU_ID,
    .oam = &sOamData_Menu_2,
    .anims = sMenu2AnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Menu_1_Basic =
{
    .tileTag = GFXTAG_MENU_1,
    .paletteTag = PALTAG_MENU_ID,
    .oam = &sOamData_Menu,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Menu_2_Basic =
{
    .tileTag = GFXTAG_MENU_2,
    .paletteTag = PALTAG_MENU_ID,
    .oam = &sOamData_Menu_2,
    .anims = sMenu2AnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_CreditDigit =
{
    .tileTag = GFXTAG_CREDIT_DIGIT,
    .paletteTag = PALTAG_INTERFACE,
    .oam = &sOam_CreditDigit,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct SpriteTemplate sSpriteTemplate_PlayerDigit =
{
    .tileTag = GFXTAG_PLAYER_DIGIT,
    .paletteTag = PALTAG_INTERFACEPLAYER,
    .oam = &sOam_CreditDigit,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const union AnimCmd sDigitalTextAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 30),
    ANIMCMD_FRAME(32, 30),
    ANIMCMD_FRAME(64, 30),
    ANIMCMD_FRAME(96, 30),
    ANIMCMD_JUMP(0)
};

static const union AnimCmd *const sDigitalTextAnimCmds[] = {
    sDigitalTextAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Digital_Text =
{
    .tileTag = DIGITAL_TEXT_GFX,
    .paletteTag = DIGITAL_TEXT_PAL,
    .oam = &sOamData_Digital_Text,
    .anims = sDigitalTextAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Lottery_JPN =
{
    .tileTag = LOTTERY_JPN_GFX,
    .paletteTag = LOTTERY_JPN_PAL,
    .oam = &sOamData_Lottery_JPN,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const union AnimCmd sKnobAnimCmd_1[] = 
{
    ANIMCMD_FRAME(0, 5),
    ANIMCMD_FRAME(16, 5),
    ANIMCMD_FRAME(32, 20),
    ANIMCMD_FRAME(16, 5),
    ANIMCMD_FRAME(0, 5),
    ANIMCMD_END
};

static const union AnimCmd sKnobAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_END
};

static const union AnimCmd *const sKnobAnimCmds[] = {
    sKnobAnimCmd_0, // Still
    sKnobAnimCmd_1, // Rotate
};

static const struct SpriteTemplate sSpriteTemplate_Knob =
{
    .tileTag = KNOB_GFX,
    .paletteTag = PALTAG_KNOB,
    .oam = &sOamData_Knob,
    .anims = sKnobAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

void StartGacha(void)
{
    sTextWindowId = WINDOW_NONE;
    ScriptContext1_Stop();
    sGacha = AllocZeroed(sizeof(struct Gacha));
    sGachaTilemap = NULL; //固定RAMに前回の値が残っているため必ず初期化する
    gSpecialVar_Result = MON_CANT_GIVE;
    VarSet(GACHA_RESULT_SPECIES_VAR, 0);
    CreateTask(FadeToGachaScreen, 0);
}

static void SpriteCB_BouncingPokeball(struct Sprite *sprite)
{
    sprite->pos1.y += sprite->data[0] / 10;
    sprite->data[5] += sprite->data[1];
    sprite->pos1.x = sprite->data[5] / 10;
    if (sprite->pos1.y > 0x4c)
    {
        sprite->pos1.y = 0x4c;
        sprite->data[0] = -(sprite->data[0] * sprite->data[2]) / 100;
        sprite->data[3] ++;
    }
    if (sprite->pos1.x == 0x78)
        sprite->data[1] = 0;
    sprite->data[0] += sprite->data[4];
    if (sprite->data[3] == 4)
    {
        sprite->data[7] = 1;
        sprite->callback = SpriteCallbackDummy;
    }
}

static void SpriteCB_BouncingPokeballArrive(struct Sprite *sprite)
{
    if (sprite->data[2] == 0)
    {
        if ((sprite->pos1.y += 4) > sprite->data[3])
        {
            sprite->data[2] ++;
            sprite->data[0] = 0x16;
            PlaySE(SE_BALL_BOUNCE_1);
        }
    }
    else
    {
        if (sprite->data[0] == 0x42)
            PlaySE(SE_BALL_BOUNCE_2);
        if (sprite->data[0] == 0x5c)
            PlaySE(SE_BALL_BOUNCE_3);
        if (sprite->data[0] == 0x6b)
            PlaySE(SE_BALL_BOUNCE_4);
        sprite->pos2.y += sTradeBallVerticalVelocityTable[sprite->data[0]];
        if (++sprite->data[0] == 0x6c)
            sprite->callback = SpriteCallbackDummy;
    }
}

static void FadeToGachaScreen(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].data[0]++;
        break;
    case 1:
        if (!gPaletteFade->active)
        {
            SetMainCallback2(InitGachaScreen);
            DestroyTask(taskId);
        }
        break;
    }
}

static void LoadGachaBgPalette(void)
{
    switch (sGacha->GachaId)
    {
    default:
    case GACHA_BASIC:
        LoadPalette(gGacha_Gacha_BG_Basic_Pal, 0, PLTT_SIZE_4BPP);
        break;
    case GACHA_GREAT:
        LoadPalette(gGacha_Gacha_BG_Great_Pal, 0, PLTT_SIZE_4BPP);
        break;
    case GACHA_ULTRA:
        LoadPalette(gGacha_Gacha_BG_Ultra_Pal, 0, PLTT_SIZE_4BPP);
        break;
    case GACHA_MASTER:
        LoadPalette(gGacha_Gacha_BG_Master_Pal, 0, PLTT_SIZE_4BPP);
        break;
    }
}

static void LoadGachaBgImage(const u8 *gfx, u32 gfxSize, const u8 *tilemap, u32 tilemapSize)
{
    if (!sGacha->bgReady)
    {
        InitBgsFromTemplates(0, sGachaBGtemplates, ARRAY_COUNT(sGachaBGtemplates));
        if (sGachaTilemap != NULL)
            Free(sGachaTilemap);
        sGachaTilemap = AllocZeroed(BG_SCREEN_SIZE);
        SetBgTilemapBuffer(GACHA_BG_BASE, sGachaTilemap);
        ResetPaletteFade();
        sGacha->bgReady = TRUE;
    }
    if (gfxSize > 0x3800) //tiles must not reach the menu BG tilemap (map base 0x17 = 0xB800)
        gfxSize = 0x3800;
    LoadBgTiles(GACHA_BG_BASE, gfx, gfxSize, 0);
    CopyToBgTilemapBuffer(GACHA_BG_BASE, tilemap, tilemapSize, 0);
    CopyBgTilemapBufferToVram(GACHA_BG_BASE);
}

static void BGSetup(void)
{
    LoadGachaBgImage(gGacha_Gacha_BG_Main, sizeof(gGacha_Gacha_BG_Main), gGacha_Gacha_BG_Main_Tilemap, sizeof(gGacha_Gacha_BG_Main_Tilemap));
    LoadGachaBgPalette();
}

static void BGRed(void)
{
    LoadGachaBgImage(gGacha_Gacha_BG_Red, sizeof(gGacha_Gacha_BG_Red), gGacha_Gacha_BG_Red_Tilemap, sizeof(gGacha_Gacha_BG_Red_Tilemap));
    LoadPalette(gGacha_Gacha_BG_Red_Pal, 0, PLTT_SIZE_4BPP);
}

static void Shake1(void)
{
    LoadGachaBgImage(gGacha_Gacha_BG_Left, sizeof(gGacha_Gacha_BG_Left), gGacha_Gacha_BG_Left_Tilemap, sizeof(gGacha_Gacha_BG_Left_Tilemap));
    LoadGachaBgPalette();
}

static void Shake2(void)
{
    LoadGachaBgImage(gGacha_Gacha_BG_Right, sizeof(gGacha_Gacha_BG_Right), gGacha_Gacha_BG_Right_Tilemap, sizeof(gGacha_Gacha_BG_Right_Tilemap));
    LoadGachaBgPalette();
}




static void GachaVBlankCallback(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
    DoScheduledBgTilemapCopiesToVram();
}

static void GachaMainCallback(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    UpdatePaletteFade();
}

static void SetCreditDigits(u32 numRaw)
{
    u16 num = (numRaw > 9999) ? 9999 : numRaw; //display only: the digit sprites hold 4 digits
    u8 i;
    u16 d = 1000;

    for (i = 0; i < 4; i++)
    {
        u8 digit = num / d;

        gSprites[sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].invisible = FALSE;

        gSprites[sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].oam.tileNum =
            gSprites[sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].sheetTileStart + (digit * 2);

        gSprites[sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].oam.priority = 0;

        num = num % d;
        d = d / 10;
    }

    for (i = 0; i < 4; i++) {
        if (gSprites[sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].invisible == FALSE) {

        } else {
            gSprites[sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].invisible = FALSE;
        }
    }

    BuildOamBuffer();
}

static void SetPlayerDigits(u16 num)
{
    u8 i;
    u16 d = 1000;  // Start with the thousands place

    for (i = 0; i < 4; i++)  // Always show 4 digits
    {
        u8 digit = num / d;

        // Show the digit (all digits are visible)
        gSprites[sGacha->PlayerSpriteIds[i + SPR_PLAYER_DIGITS]].invisible = FALSE;

        // If it's a smaller number, show 0 for the higher place values
        if (i == 0 && num < 1000) {
            digit = 0;  // Force 0 for the thousands place if the number is less than 1000
        }

        // Set the tileNum based on the current digit
        gSprites[sGacha->PlayerSpriteIds[i + SPR_PLAYER_DIGITS]].oam.tileNum =
            gSprites[sGacha->PlayerSpriteIds[i + SPR_PLAYER_DIGITS]].sheetTileStart + (digit * 2);

        gSprites[sGacha->PlayerSpriteIds[i + SPR_PLAYER_DIGITS]].oam.priority = 0;

        // Reduce num for the next digit
        num = num % d;
        d = d / 10;
    }

    BuildOamBuffer();
}

static void CreateCreditSprites(void)
{
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sSpriteSheets_Interface) - 1; i++)  
    {
        LoadSpriteSheet(&sSpriteSheets_Interface[i]);
    }

    for (i = 0; i < MAX_COIN_DIGITS; i++)
    {
        if (i == 0)
        {
            sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS] = CreateSprite(&sSpriteTemplate_CreditDigit, 207, 140, 2);
            gSprites[sGacha->PlayerSpriteIds[i + SPR_CREDIT_DIGITS]].oam.priority = 0;
        }
        if (i == 1)
        {
            sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS] = CreateSprite(&sSpriteTemplate_CreditDigit, 8 + 207, 140, 2);
            gSprites[sGacha->PlayerSpriteIds[i + SPR_CREDIT_DIGITS]].oam.priority = 0;
        }
        if (i == 2)
        {
            sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS] = CreateSprite(&sSpriteTemplate_CreditDigit, 16 + 207, 140, 2);
            gSprites[sGacha->PlayerSpriteIds[i + SPR_CREDIT_DIGITS]].oam.priority = 0;
        }
        if (i == 3)
        {
            sGacha->CreditSpriteIds[i + SPR_CREDIT_DIGITS] = CreateSprite(&sSpriteTemplate_CreditDigit, 24 + 207, 140, 2);
            gSprites[sGacha->PlayerSpriteIds[i + SPR_CREDIT_DIGITS]].oam.priority = 0;
        }
    }
}

static void CreatePlayerSprites(void)
{
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sSpriteSheets_PlayerInterface) - 1; i++)  
    {
        LoadSpriteSheet(&sSpriteSheets_PlayerInterface[i]);
    }

    for (i = 0; i < 4; i++)
    {
        sGacha->PlayerSpriteIds[i + SPR_PLAYER_DIGITS] = CreateSprite(&sSpriteTemplate_PlayerDigit, i * 8 + 207, 118, 2);
        gSprites[sGacha->PlayerSpriteIds[i + SPR_PLAYER_DIGITS]].oam.priority = 0;
    }
}

static void CreateCTA(void)
{
    LoadSpriteSheet(&sSpriteSheet_Press_A);
    sGacha->CTAspriteId = CreateSprite(&sSpriteTemplate_Press_A, 152, 116, 0);
    gSprites[sGacha->CTAspriteId].animNum = 0; // Off
}

static void CreateArrows(void)
{
    LoadSpriteSheet(&sSpriteSheet_Arrows);
    sGacha->ArrowsSpriteId = CreateSprite(&sSpriteTemplate_Arrows, 207 + 24, 120, 0);
    gSprites[sGacha->ArrowsSpriteId].animNum = 1; // Only Up
}

static void CreateLotteryJPN(void)
{
    LoadSpriteSheet(&sSpriteSheet_Lottery_JPN);
    sGacha->LotteryJPNspriteId = CreateSprite(&sSpriteTemplate_Lottery_JPN, 176, 32, 0);
}

static void CreateHoppip(void)
{
    s16 x = 142;
    s16 y = 56;
    s16 x2 = x + 34;
    s16 x3 = x + 68;

    LoadSpriteSheet(&sSpriteSheet_Hoppip);
    sGacha->PokemonOneSpriteId = CreateSprite(&sSpriteTemplate_Hoppip, x, y, 0);
    sGacha->PokemonTwoSpriteId = CreateSprite(&sSpriteTemplate_Hoppip, x2, y, 0);    
    sGacha->PokemonThreeSpriteId = CreateSprite(&sSpriteTemplate_Hoppip, x3, y, 0);    
}

static UNUSED void CreateElekid(void)
{
    s16 x = 142;
    s16 y = 56 + 2;
    s16 x2 = x + 34;
    s16 x3 = x + 68;

    LoadSpriteSheet(&sSpriteSheet_Elekid);
    sGacha->PokemonOneSpriteId = CreateSprite(&sSpriteTemplate_Elekid, x, y, 0);
    sGacha->PokemonTwoSpriteId = CreateSprite(&sSpriteTemplate_Elekid, x2, y, 0);
    sGacha->PokemonThreeSpriteId = CreateSprite(&sSpriteTemplate_Elekid, x3, y, 0);    
}

static void CreateTeddiursa(void)
{
    s16 x = 142;
    s16 y = 56;
    s16 x2 = x + 34;
    s16 x3 = x + 68;

    LoadSpriteSheet(&sSpriteSheet_Teddiursa);
    sGacha->PokemonOneSpriteId = CreateSprite(&sSpriteTemplate_Teddiursa, x, y, 0);
    sGacha->PokemonTwoSpriteId = CreateSprite(&sSpriteTemplate_Teddiursa, x2, y, 0);    
    sGacha->PokemonThreeSpriteId = CreateSprite(&sSpriteTemplate_Teddiursa, x3, y, 0);
}

static void CreatePhanpy(void)
{
    s16 x = 142;
    s16 y = 56;
    s16 x2 = x + 34;
    s16 x3 = x + 68;

    LoadSpriteSheet(&sSpriteSheet_Phanpy);
    sGacha->PokemonOneSpriteId = CreateSprite(&sSpriteTemplate_Phanpy, x, y, 0);
    sGacha->PokemonTwoSpriteId = CreateSprite(&sSpriteTemplate_Phanpy, x2, y, 0);
    sGacha->PokemonThreeSpriteId = CreateSprite(&sSpriteTemplate_Phanpy, x3, y, 0);
}

static void CreateBelossom(void)
{
    s16 x = 142;
    s16 y = 56;
    s16 x2 = x + 34;
    s16 x3 = x + 68;

    LoadSpriteSheet(&sSpriteSheet_Belossom);
    sGacha->PokemonOneSpriteId = CreateSprite(&sSpriteTemplate_Belossom, x, y, 0);
    sGacha->PokemonTwoSpriteId = CreateSprite(&sSpriteTemplate_Belossom, x2, y, 0);    
    sGacha->PokemonThreeSpriteId = CreateSprite(&sSpriteTemplate_Belossom, x3, y, 0);

}

static void CreateDigitalText(void)
{
    LoadSpriteSheet(&sSpriteSheet_Digital_Text);
    sGacha->DigitalTextSpriteId = CreateSprite(&sSpriteTemplate_Digital_Text, 64, 25, 0);
}

static void CreateCreditMenu(void)
{
    s16 x = 144;
    s16 y = 128;
    u8 priority = 1;

    LoadSpriteSheet(&sSpriteSheet_Menu_1);

    switch (sGacha->GachaId)
    {
    default:
    case GACHA_BASIC:
        sGacha->CreditMenu1Id = CreateSprite(&sSpriteTemplate_Menu_1_Basic, x, y, priority);
        break;
    case GACHA_GREAT:
        sGacha->CreditMenu1Id = CreateSprite(&sSpriteTemplate_Menu_1_Great, x, y, priority);
        break;
    case GACHA_ULTRA:
        sGacha->CreditMenu1Id = CreateSprite(&sSpriteTemplate_Menu_1_Ultra, x, y, priority);
        break;
    case GACHA_MASTER:
        sGacha->CreditMenu1Id = CreateSprite(&sSpriteTemplate_Menu_1_Master, x, y, priority);
        break;
    }
    gSprites[sGacha->CreditMenu1Id].oam.priority = 1;
}

static void CreatePlayerMenu(void)
{
    s16 x = 144;
    s16 y = 128;
    s16 x2 = x + 64;
    u8 priority = 1;

    LoadSpriteSheet(&sSpriteSheet_Menu_2);

    switch (sGacha->GachaId)
    {
    default:
    case GACHA_BASIC:
        sGacha->CreditMenu2Id = CreateSprite(&sSpriteTemplate_Menu_2_Basic, x2, y, priority);
        break;
    case GACHA_GREAT:
        sGacha->CreditMenu2Id = CreateSprite(&sSpriteTemplate_Menu_2_Great, x2, y, priority);
        break;
    case GACHA_ULTRA:
        sGacha->CreditMenu2Id = CreateSprite(&sSpriteTemplate_Menu_2_Ultra, x2, y, priority);
        break;
    case GACHA_MASTER:
        sGacha->CreditMenu2Id = CreateSprite(&sSpriteTemplate_Menu_2_Master, x2, y, priority);
        break;
    }
    gSprites[sGacha->CreditMenu2Id].oam.priority = 1;
}

static void CreateKnob(void)
{
    LoadSpriteSheet(&sSpriteSheet_Knob);
    sGacha->KnobSpriteId = CreateSprite(&sSpriteTemplate_Knob, 76, 128, 0);
    gSprites[sGacha->KnobSpriteId].animNum = 0; // No Rotation
}

static const u16 sGachaBasicSpeciesCommon[] = {
    SPECIES_AZURILL,
    SPECIES_CATERPIE,
    SPECIES_IGGLYBUFF,
    SPECIES_WOOPER,
    SPECIES_MAGIKARP,
    SPECIES_SENTRET,
    SPECIES_CLEFFA,
    SPECIES_LOTAD,
    SPECIES_SEEDOT,
    SPECIES_ZIGZAGOON,
    SPECIES_ZUBAT,
    SPECIES_SPINARAK,
    SPECIES_HOPPIP,
    SPECIES_PIDGEY,
    SPECIES_RATTATA,
    SPECIES_HOOTHOOT,
    SPECIES_WINGULL,
    SPECIES_NIDORAN_M,
    SPECIES_NIDORAN_F,
    SPECIES_SHROOMISH,
    SPECIES_POLIWAG,
    SPECIES_BELLSPROUT,
    SPECIES_ODDISH,
    SPECIES_FEEBAS
};

static const u16 sGachaBasicSpeciesUncommon[] = {
    SPECIES_METAPOD,
    SPECIES_KAKUNA,
    SPECIES_PICHU,
    SPECIES_MAKUHITA,
    SPECIES_MARILL,
    SPECIES_SWINUB,
    SPECIES_DIGLETT,
    SPECIES_MAREEP,
    SPECIES_MEDITITE,
    SPECIES_EKANS,
    SPECIES_BARBOACH,
    SPECIES_HORSEA,
    SPECIES_SANDSHREW,
    SPECIES_GEODUDE,
    SPECIES_MANKEY,
    SPECIES_MACHOP,
    SPECIES_SHELLDER,
    SPECIES_GRIMER,
    SPECIES_VOLTORB,
    SPECIES_NATU,
    SPECIES_NINCADA,
};

static const u16 sGachaBasicSpeciesRare[] = {
    SPECIES_RALTS,
    SPECIES_TYROGUE,
    SPECIES_MEOWTH,
    SPECIES_TRAPINCH,
    SPECIES_SPHEAL,
    SPECIES_VULPIX,
    SPECIES_REMORAID,
    SPECIES_ABRA,
    SPECIES_ELEKID,
    SPECIES_MAGBY,
    SPECIES_CORSOLA
};

static const u16 sGachaBasicSpeciesUltraRare[] = {
    SPECIES_TOGEPI,
    SPECIES_CHARMANDER,
    SPECIES_CYNDAQUIL,
    SPECIES_TREECKO,
    SPECIES_TORCHIC,
    SPECIES_MUDKIP,
    SPECIES_SQUIRTLE,
    SPECIES_TOTODILE,
    SPECIES_BULBASAUR,
    SPECIES_CHIKORITA,
};

static const u16 sGachaGreatSpeciesCommon[] = {
    SPECIES_JIGGLYPUFF,
    SPECIES_CLEFAIRY,
    SPECIES_KIRLIA,
    SPECIES_ELECTRIKE,
    SPECIES_SHUPPET,
    SPECIES_DUSKULL,
    SPECIES_SHELLDER,
    SPECIES_MACHOP,
    SPECIES_MANKEY,
    SPECIES_MAREEP,
    SPECIES_REMORAID,
    SPECIES_CARVANHA,
    SPECIES_NUMEL,
    SPECIES_SWABLU,
    SPECIES_SLOWPOKE,
    SPECIES_PIKACHU,
    SPECIES_GRIMER,
    SPECIES_EXEGGCUTE,
    SPECIES_VOLTORB,
    SPECIES_NATU,
    SPECIES_PHANPY,
    SPECIES_ARON,
    SPECIES_TENTACOOL,
    SPECIES_KOFFING,
    SPECIES_SKIPLOOM,
    SPECIES_LOMBRE,
    SPECIES_NUZLEAF,
    SPECIES_RHYHORN,
    SPECIES_CLAMPERL,
    SPECIES_PIDGEOTTO,
    SPECIES_ELEKID,
    SPECIES_NIDORINA,
    SPECIES_NIDORINO,
    SPECIES_MAGBY,
    SPECIES_POLIWHIRL,
    SPECIES_ONIX,
    SPECIES_GRAVELER,
    SPECIES_GLOOM,
    SPECIES_PONYTA,
    SPECIES_FURRET,
    SPECIES_LINOONE,
    SPECIES_CHIMECHO,
    SPECIES_QUAGSIRE,
    SPECIES_PELIPPER,
    SPECIES_PERSIAN,
    SPECIES_SEADRA,
    SPECIES_NOCTOWL,
    SPECIES_SANDSLASH,
    SPECIES_GOLBAT,
    SPECIES_TYROGUE,
    SPECIES_TORKOAL,
    SPECIES_ELECTRODE
};

static const u16 sGachaGreatSpeciesUncommon[] = {
    SPECIES_FEEBAS,
    SPECIES_BALTOY,
    SPECIES_SNORUNT,
    SPECIES_GASTLY,
    SPECIES_ABRA,
    SPECIES_CUBONE,
    SPECIES_MAGNEMITE,
    SPECIES_SEEL,
    SPECIES_CHINCHOU,
    SPECIES_TEDDIURSA,
    SPECIES_HOUNDOUR,
    SPECIES_CACNEA,
    SPECIES_GROWLITHE,
    SPECIES_FLAAFFY,
    SPECIES_CORSOLA,
    SPECIES_WEEPINBELL,
    SPECIES_ARIADOS,
    SPECIES_BUTTERFREE,
    SPECIES_BEEDRILL,
    SPECIES_ROSELIA,
    SPECIES_MACHOKE,
    SPECIES_MURKROW,
    SPECIES_RATICATE,
    SPECIES_CASTFORM,
    SPECIES_GLIGAR,
    SPECIES_QWILFISH,
    SPECIES_TANGELA,
    SPECIES_PILOSWINE,
    SPECIES_PRIMEAPE,
    SPECIES_BRELOOM,
    SPECIES_TROPIUS,
    SPECIES_STANTLER,
    SPECIES_XATU,
    SPECIES_HARIYAMA,
};

static const u16 sGachaGreatSpeciesRare[] = {
    SPECIES_DRATINI,
    SPECIES_LARVITAR,
    SPECIES_BAGON,
    SPECIES_TOGEPI,
    SPECIES_CHARMANDER,
    SPECIES_CYNDAQUIL,
    SPECIES_TREECKO,
    SPECIES_TORCHIC,
    SPECIES_MUDKIP,
    SPECIES_SQUIRTLE,
    SPECIES_TOTODILE,
    SPECIES_BULBASAUR,
    SPECIES_CHIKORITA,
    SPECIES_STARYU,
    SPECIES_VIBRAVA,
    SPECIES_FARFETCHD,
    SPECIES_AIPOM,
    SPECIES_NOSEPASS,
    SPECIES_SABLEYE,
    SPECIES_MAWILE,
    SPECIES_YANMA,
    SPECIES_KADABRA,
    SPECIES_DUGTRIO,
    SPECIES_HAUNTER,
    SPECIES_SUDOWOODO,
    SPECIES_MEDICHAM,
    SPECIES_SEALEO,
    SPECIES_DUNSPARCE,
    SPECIES_SNEASEL,
    SPECIES_MANTINE,
    SPECIES_SKARMORY,
    SPECIES_OCTILLERY,
    SPECIES_RELICANTH,
    SPECIES_MILTANK,
    SPECIES_SCYTHER,
    SPECIES_PINSIR,
};

static const u16 sGachaGreatSpeciesUltraRare[] = {
    SPECIES_WYNAUT,
    SPECIES_DELIBIRD,
    SPECIES_PORYGON,
    SPECIES_IVYSAUR,
    SPECIES_CHARMELEON,
    SPECIES_WARTORTLE,
    SPECIES_BAYLEEF,
    SPECIES_QUILAVA,
    SPECIES_CROCONAW,
    SPECIES_GROVYLE,
    SPECIES_COMBUSKEN,
    SPECIES_MARSHTOMP,
    SPECIES_PUPITAR,
    SPECIES_DRAGONAIR,
    SPECIES_SHELGON,
    SPECIES_METANG,
    SPECIES_MR_MIME,
    SPECIES_HERACROSS
};

static const u16 sGachaUltraSpeciesCommon[] = {
    SPECIES_EXEGGCUTE,
    SPECIES_MAGNEMITE,
    SPECIES_TEDDIURSA,
    SPECIES_ARIADOS,
    SPECIES_DUGTRIO,
    SPECIES_SUDOWOODO,
    SPECIES_MEDICHAM,
    SPECIES_SEALEO,
    SPECIES_LINOONE,
    SPECIES_CASTFORM,
    SPECIES_CHIMECHO,
    SPECIES_PELIPPER,
    SPECIES_LAIRON,
    SPECIES_WIGGLYTUFF,
    SPECIES_ARBOK,
    SPECIES_SANDSLASH,
    SPECIES_NINJASK,
    SPECIES_LANTURN,
    SPECIES_JUMPLUFF,
    SPECIES_BRELOOM,
    SPECIES_SHARPEDO,
    SPECIES_CAMERUPT,
    SPECIES_XATU,
    SPECIES_TORKOAL,
    SPECIES_HARIYAMA,
    SPECIES_PIDGEOT,
    SPECIES_CLEFABLE,
    SPECIES_RAICHU,
    SPECIES_RHYDON,
    SPECIES_VILEPLUME,
    SPECIES_VICTREEBEL,
    SPECIES_BELLOSSOM,
    SPECIES_MILTANK,
    SPECIES_GOLEM,
    SPECIES_RAPIDASH,
    SPECIES_NIDOQUEEN,
    SPECIES_NIDOKING,
    SPECIES_NINETALES,
    SPECIES_MACHAMP,
    SPECIES_POLIWRATH,
    SPECIES_TENTACRUEL,
    SPECIES_EXEGGUTOR,
    SPECIES_CLOYSTER
};

static const u16 sGachaUltraSpeciesUncommon[] = {
    SPECIES_DELIBIRD,
    SPECIES_LICKITUNG,
    SPECIES_YANMA,
    SPECIES_PORYGON,
    SPECIES_TOGETIC,
    SPECIES_AZUMARILL,
    SPECIES_MAROWAK,
    SPECIES_HITMONLEE,
    SPECIES_HITMONCHAN,
    SPECIES_HITMONTOP,
    SPECIES_BANETTE,
    SPECIES_DUSCLOPS,
    SPECIES_MR_MIME,
    SPECIES_TROPIUS,
    SPECIES_MAGNETON,
    SPECIES_MANTINE,
    SPECIES_SKARMORY,
    SPECIES_WHISCASH,
    SPECIES_DEWGONG,
    SPECIES_MANECTRIC,
    SPECIES_OCTILLERY,
    SPECIES_GLALIE,
    SPECIES_SLOWBRO,
    SPECIES_WEEZING,
    SPECIES_ELECTABUZZ,
    SPECIES_SLOWKING,
    SPECIES_MAGMAR,
    SPECIES_MUK,
    SPECIES_SCYTHER,
    SPECIES_PINSIR,
    SPECIES_URSARING,
    SPECIES_HOUNDOOM,
    SPECIES_CLAYDOL,
    SPECIES_AMPHAROS,
    SPECIES_GARDEVOIR,
    SPECIES_ABSOL,
    SPECIES_CACTURNE,
    SPECIES_LUDICOLO,
    SPECIES_SHIFTRY,
    SPECIES_POLITOED,
    SPECIES_SCIZOR,
    SPECIES_HERACROSS,
    SPECIES_STEELIX,
    SPECIES_ALTARIA,
    SPECIES_RELICANTH,
    SPECIES_HUNTAIL,
    SPECIES_GOREBYSS
};

static const u16 sGachaUltraSpeciesRare[] = {
    SPECIES_DITTO,
    SPECIES_EEVEE,
    SPECIES_KABUTO,
    SPECIES_LILEEP,
    SPECIES_WOBBUFFET,
    SPECIES_PUPITAR,
    SPECIES_DUNSPARCE,
    SPECIES_DRAGONAIR,
    SPECIES_SHELGON,
    SPECIES_METANG,
    SPECIES_MISDREAVUS,
    SPECIES_KANGASKHAN,
    SPECIES_TAUROS,
    SPECIES_ALAKAZAM,
    SPECIES_GENGAR,
    SPECIES_STARMIE,
    SPECIES_FLYGON,
    SPECIES_VAPOREON,
    SPECIES_JOLTEON,
    SPECIES_FLAREON,
    SPECIES_AGGRON,
    SPECIES_WALREIN,
    SPECIES_CROBAT,
    SPECIES_GYARADOS,
    SPECIES_KINGDRA,
    SPECIES_MILOTIC
};

static const u16 sGachaUltraSpeciesUltraRare[] = {
    SPECIES_SHEDINJA,
    SPECIES_CHANSEY,
    SPECIES_KABUTOPS,
    SPECIES_CRADILY,
    SPECIES_AERODACTYL,
    SPECIES_PORYGON2,
    SPECIES_VENUSAUR,
    SPECIES_MEGANIUM,
    SPECIES_ESPEON,
    SPECIES_UMBREON,
    SPECIES_BLASTOISE,
    SPECIES_FERALIGATR,
    SPECIES_SCEPTILE,
    SPECIES_BLAZIKEN,
    SPECIES_SWAMPERT,
    SPECIES_CHARIZARD,
    SPECIES_TYPHLOSION,
    SPECIES_LAPRAS,
    SPECIES_SNORLAX,
    SPECIES_ARCANINE,
    SPECIES_DRAGONITE,
    SPECIES_TYRANITAR,
    SPECIES_SALAMENCE,
    SPECIES_METAGROSS,
};

static const u16 sGachaMasterSpeciesCommon[] = {
    SPECIES_BULBASAUR,
    SPECIES_ROWLET,
    SPECIES_SQUIRTLE,
    SPECIES_SCORBUNNY,
    SPECIES_QUAXLY,
    SPECIES_GROOKEY,
    SPECIES_TREECKO,
    SPECIES_SOBBLE,
    SPECIES_MUDKIP
};

static const u16 sGachaMasterSpeciesUncommon[] = {
    SPECIES_TURTWIG,
    SPECIES_CHIMCHAR,
    SPECIES_PIPLUP,
    SPECIES_SNIVY,
    SPECIES_TEPIG,
    SPECIES_OSHAWOTT,
    SPECIES_CHARMANDER,
    SPECIES_LITTEN,
    SPECIES_FUECOCO,
};

static const u16 sGachaMasterSpeciesRare[] = {
    SPECIES_CHESPIN,
    SPECIES_FENNEKIN,
    SPECIES_FROAKIE,
    SPECIES_CHIKORITA,
    SPECIES_CYNDAQUIL,
    SPECIES_TOTODILE,
    SPECIES_SPRIGATITO,
    SPECIES_TORCHIC,
    SPECIES_POPPLIO,
};

static const u16 sGachaMasterSpeciesUltraRare[] = {
    SPECIES_PIKACHU,
    SPECIES_EEVEE
};

static void DrawGachaWindowText(const u8 *str, u8 width, u8 height, u8 textY)
{
    const u8 colors[] = {TEXT_COLOR_WHITE, TEXT_COLOR_DARK_GREY, TEXT_COLOR_LIGHT_GREY};
    u16 w = width * 8;
    u16 h = height * 8;

    FillWindowPixelBuffer(sTextWindowId, PIXEL_FILL(1));
    AddTextPrinterParameterized4(sTextWindowId, FONT_GACHA, 6, textY, 0, 0, colors, TEXT_SPEED_FF, str);
    //日本語フォントは1文字が16px高の背景を塗るので、枠線は文字の後に描く
    FillWindowPixelRect(sTextWindowId, PIXEL_FILL(2), 0, 0, w, 1);
    FillWindowPixelRect(sTextWindowId, PIXEL_FILL(2), 0, h - 1, w, 1);
    FillWindowPixelRect(sTextWindowId, PIXEL_FILL(2), 0, 0, 1, h);
    FillWindowPixelRect(sTextWindowId, PIXEL_FILL(2), w - 1, 0, 1, h);
    PutWindowTilemap(sTextWindowId);
    CopyWindowToVram(sTextWindowId, COPYWIN_FULL);
}

//日本語フォントには「％」の文字が無いので、ピクセルで描画する
static void DrawGachaPercentSign(s32 textWidth)
{
    static const u8 sPercentRows[7] = {0x61, 0x62, 0x04, 0x08, 0x10, 0x23, 0x43}; //左端がbit6
    u16 x0 = 6 + textWidth + 1;
    u8 row, col;

    for (row = 0; row < 7; row++)
    {
        for (col = 0; col < 7; col++)
        {
            if (sPercentRows[row] & (0x40 >> col))
                FillWindowPixelRect(sTextWindowId, PIXEL_FILL(2), x0 + col, 5 + row, 1, 1);
        }
    }
    CopyWindowToVram(sTextWindowId, COPYWIN_FULL);
}

static void ShowMessage(void)
{
    struct WindowTemplate template;

    ResetMessage();
    template = SetWindowTemplateFields(GACHA_MENUS, 16, 10, 14, 2, 15, 0x194);
    sTextWindowId = AddWindow(&template);
    ConvertIntToDecimalStringN(gStringVar1, sGacha->newMonOdds, STR_CONV_MODE_LEADING_ZEROS, 3);
    StringExpandPlaceholders(gStringVar4, gText_Gacha_NewOdds);
    DrawGachaWindowText(gStringVar4, 14, 2, 2);
    DrawGachaPercentSign(GetStringWidth(FONT_GACHA, gStringVar4, 0));
}

static void ResetMessage(void)
{
    if (sTextWindowId != WINDOW_NONE)
    {
        FillWindowPixelBuffer(sTextWindowId, PIXEL_FILL(0));
        ClearWindowTilemap(sTextWindowId);
        CopyWindowToVram(sTextWindowId, COPYWIN_FULL);
        RemoveWindow(sTextWindowId);
        sTextWindowId = WINDOW_NONE;
    }
}

static void StartExitGacha(void)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    sGacha->state = GACHA_STATE_EXIT;
}

static void StartTradeScreen(void)
{
    BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    sGacha->state = STATE_FADE;
}

static u16 GetMaxAvailableGachaRaritySpecies(u32 gachaId, u32 rarity)
{
    // Get the number of available Pokémon based on rarity
    switch (gachaId)
    {
    default:
    case GACHA_BASIC:
        switch (rarity)
        {
        default:
        case RARITY_COMMON:
            return ARRAY_COUNT(sGachaBasicSpeciesCommon);
        case RARITY_UNCOMMON:
            return ARRAY_COUNT(sGachaBasicSpeciesUncommon);
        case RARITY_RARE:
            return ARRAY_COUNT(sGachaBasicSpeciesRare);
        case RARITY_ULTRA_RARE:
            return ARRAY_COUNT(sGachaBasicSpeciesUltraRare);
        }
    case GACHA_GREAT:
        switch (rarity)
        {
        default:
        case RARITY_COMMON:
            return ARRAY_COUNT(sGachaGreatSpeciesCommon);
        case RARITY_UNCOMMON:
            return ARRAY_COUNT(sGachaGreatSpeciesUncommon);
        case RARITY_RARE:
            return ARRAY_COUNT(sGachaGreatSpeciesRare);
        case RARITY_ULTRA_RARE:
            return ARRAY_COUNT(sGachaGreatSpeciesUltraRare);
        }
    case GACHA_ULTRA:
        switch (rarity)
        {
        default:
        case RARITY_COMMON:
            return ARRAY_COUNT(sGachaUltraSpeciesCommon);
        case RARITY_UNCOMMON:
            return ARRAY_COUNT(sGachaUltraSpeciesUncommon);
        case RARITY_RARE:
            return ARRAY_COUNT(sGachaUltraSpeciesRare);
        case RARITY_ULTRA_RARE:
            return ARRAY_COUNT(sGachaUltraSpeciesUltraRare);
        }
    case GACHA_MASTER:
        switch (rarity)
        {
        default:
        case RARITY_COMMON:
            return ARRAY_COUNT(sGachaMasterSpeciesCommon);
        case RARITY_UNCOMMON:
            return ARRAY_COUNT(sGachaMasterSpeciesUncommon);
        case RARITY_RARE:
            return ARRAY_COUNT(sGachaMasterSpeciesRare);
        case RARITY_ULTRA_RARE:
            return ARRAY_COUNT(sGachaMasterSpeciesUltraRare);
        }
    }
    return 0; // failsafe
}

static inline u16 GetGachaBasicSpecies(u16 randNum)
{
    u16 totalMax;

    // Use the pre-defined totalMax values based on the rarity
    totalMax = GetMaxAvailableGachaRaritySpecies(GACHA_BASIC, sGacha->Rarity);

    // Check if the provided Number is valid
    if (randNum >= totalMax)
        return -1;  // Return -1 if the Number is out of range for the list

    // Now, search for the Pokémon based on its customNumber
    switch (sGacha->Rarity)
    {
    default:
    case RARITY_COMMON:
        return sGachaBasicSpeciesCommon[randNum];
    case RARITY_UNCOMMON:
        return sGachaBasicSpeciesUncommon[randNum];
    case RARITY_RARE:
        return sGachaBasicSpeciesRare[randNum];
    case RARITY_ULTRA_RARE:
        return sGachaBasicSpeciesUltraRare[randNum];
    }

    return -1; // Return -1 if customNumber is not found
}

static inline u16 GetGachaGreatSpecies(u16 randNum)
{
    u16 totalMax = 0;

    // Determine the totalMax based on rarity
    totalMax = GetMaxAvailableGachaRaritySpecies(GACHA_GREAT, sGacha->Rarity);

    // Check if the provided Number is within the range
    if (randNum >= totalMax)
        return -1;  // Return -1 if out of range

    // Loop through the correct array based on rarity
    switch (sGacha->Rarity)
    {
    default:
    case RARITY_COMMON:
        return sGachaGreatSpeciesCommon[randNum];
    case RARITY_UNCOMMON:
        return sGachaGreatSpeciesUncommon[randNum];
    case RARITY_RARE:
        return sGachaGreatSpeciesRare[randNum];
    case RARITY_ULTRA_RARE:
        return sGachaGreatSpeciesUltraRare[randNum];
    }

    return -1; // Return -1 if customNumber is not found
}

static inline u16 GetGachaUltraSpecies(u16 randNum)
{
    u16 totalMax = 0;

    // Determine the totalMax based on rarity
    totalMax = GetMaxAvailableGachaRaritySpecies(GACHA_ULTRA, sGacha->Rarity);

    // Check if the provided Number is within the range
    if (randNum >= totalMax)
        return -1;  // Return -1 if out of range

    // Loop through the correct array based on rarity
    switch (sGacha->Rarity)
    {
    default:
    case RARITY_COMMON:
        return sGachaUltraSpeciesCommon[randNum];
    case RARITY_UNCOMMON:
        return sGachaUltraSpeciesUncommon[randNum];
    case RARITY_RARE:
        return sGachaUltraSpeciesRare[randNum];
    case RARITY_ULTRA_RARE:
        return sGachaUltraSpeciesUltraRare[randNum];
    }

    return -1; // Return -1 if customNumber is not found
}

static inline u16 GetGachaMasterSpecies(u16 randNum)
{
    u16 totalMax = 0;

    totalMax = GetMaxAvailableGachaRaritySpecies(GACHA_MASTER, sGacha->Rarity);

    // Check if the provided Number is within the range
    if (randNum >= totalMax)
        return -1;  // Return -1 if out of range

    switch (sGacha->Rarity)
    {
    default:
    case RARITY_COMMON:
        return sGachaMasterSpeciesCommon[randNum];
    case RARITY_UNCOMMON:
        return sGachaMasterSpeciesUncommon[randNum];
    case RARITY_RARE:
        return sGachaMasterSpeciesRare[randNum];
    case RARITY_ULTRA_RARE:
        return sGachaMasterSpeciesUltraRare[randNum];
    }

    return -1; // Return -1 if customNumber is not found
}

static u16 GetGachaMon(u16 randNum)
{
    u32 species;

    switch (sGacha->GachaId)
    {
    default:
    case GACHA_BASIC:
        species = GetGachaBasicSpecies(randNum);
        break;
    case GACHA_GREAT:
        species = GetGachaGreatSpecies(randNum);
        break;
    case GACHA_ULTRA:
        species = GetGachaUltraSpecies(randNum);
        break;
    case GACHA_MASTER:
        species = GetGachaMasterSpecies(randNum);
        break;
    }

    //★ 2026-10-11 ユーザー「キンセツのガチャで？が出ます（2 回に 1 回）」: 元は species >= SPECIES_EGG（0x19C）で NONE にしていたが、
    //  DPE では 4 世代以降・別の姿の種族は SPECIES_EGG より後ろの番号なので、表の半分ほどが NONE（名前「？」・絵なし）になっていた。
    //  タマゴと表の外（NUM_SPECIES 以上）だけを NONE にする
    if (species == SPECIES_EGG || species >= NUM_SPECIES)
        return SPECIES_NONE;
    return species;
}

static inline bool32 CheckIfOwned(u16 species)
{
    return GachaHasCaught(species);
}

static u16 GetGachaOwnedCount(u32 rarity)
{
    switch (rarity)
    {
    default:
    case RARITY_COMMON:
        return sGacha->ownedCommon;
    case RARITY_UNCOMMON:
        return sGacha->ownedUncommon;
    case RARITY_RARE:
        return sGacha->ownedRare;
    case RARITY_ULTRA_RARE:
        return sGacha->ownedUltraRare;
    }
}

static u8 GetGachaRarityOdds(u32 rarity)
{
    switch (rarity)
    {
    default:
    case RARITY_COMMON:
        return RARITY_COMMON_ODDS;
    case RARITY_UNCOMMON:
        return RARITY_UNCOMMON_ODDS;
    case RARITY_RARE:
        return RARITY_RARE_ODDS;
    case RARITY_ULTRA_RARE:
        return RARITY_ULTRA_RARE_ODDS;
    }
}

static void GetPokemonOwned(void)
{
    u16 species;
    int i;

    sGacha->ownedCommon = 0;
    sGacha->ownedUncommon = 0;
    sGacha->ownedRare = 0;
    sGacha->ownedUltraRare = 0;

    switch (sGacha->GachaId)
    {
    default:
    case GACHA_BASIC:
        for (i = 0; i < ARRAY_COUNT(sGachaBasicSpeciesCommon); i++)
        {
            species = sGachaBasicSpeciesCommon[i];
            sGacha->ownedCommon += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaBasicSpeciesUncommon); i++)
        {
            species = sGachaBasicSpeciesUncommon[i];
            sGacha->ownedUncommon += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaBasicSpeciesRare); i++)
        {
            species = sGachaBasicSpeciesRare[i];
            sGacha->ownedRare += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaBasicSpeciesUltraRare); i++)
        {
            species = sGachaBasicSpeciesUltraRare[i];
            sGacha->ownedUltraRare += GachaHasCaught(species);
        }
        break;
    case GACHA_GREAT:
        for (i = 0; i < ARRAY_COUNT(sGachaGreatSpeciesCommon); i++)
        {
            species = sGachaGreatSpeciesCommon[i];
            sGacha->ownedCommon += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaGreatSpeciesUncommon); i++)
        {
            species = sGachaGreatSpeciesUncommon[i];
            sGacha->ownedUncommon += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaGreatSpeciesRare); i++)
        {
            species = sGachaGreatSpeciesRare[i];
            sGacha->ownedRare += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaGreatSpeciesUltraRare); i++)
        {
            species = sGachaGreatSpeciesUltraRare[i];
            sGacha->ownedUltraRare += GachaHasCaught(species);
        }
        break;
    case GACHA_ULTRA:
        for (i = 0; i < ARRAY_COUNT(sGachaUltraSpeciesCommon); i++)
        {
            species = sGachaUltraSpeciesCommon[i];
            sGacha->ownedCommon += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaUltraSpeciesUncommon); i++)
        {
            species = sGachaUltraSpeciesUncommon[i];
            sGacha->ownedUncommon += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaUltraSpeciesRare); i++)
        {
            species = sGachaUltraSpeciesRare[i];
            sGacha->ownedRare += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaUltraSpeciesUltraRare); i++)
        {
            species = sGachaUltraSpeciesUltraRare[i];
            sGacha->ownedUltraRare += GachaHasCaught(species);
        }
        break;
    case GACHA_MASTER:
        for (i = 0; i < ARRAY_COUNT(sGachaMasterSpeciesCommon); i++)
        {
            species = sGachaMasterSpeciesCommon[i];
            sGacha->ownedCommon += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaMasterSpeciesUncommon); i++)
        {
            species = sGachaMasterSpeciesUncommon[i];
            sGacha->ownedUncommon += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaMasterSpeciesRare); i++)
        {
            species = sGachaMasterSpeciesRare[i];
            sGacha->ownedRare += GachaHasCaught(species);
        }
        for (i = 0; i < ARRAY_COUNT(sGachaMasterSpeciesUltraRare); i++)
        {
            species = sGachaMasterSpeciesUltraRare[i];
            sGacha->ownedUltraRare += GachaHasCaught(species);
        }
        break;
    }
}

u8 CalculateChanceForCategory(u16 owned, u16 available, u8 baseChance, u16 wager)
{
    u8 newChance;
    u8 ownedPercentage;
    u8 wagerMultiplier;
    u8 boostedChance;
    u16 minWager;

    // Determine minimum wager based on sGacha->GachaId
    switch (sGacha->GachaId)
    {
    default:
    case GACHA_BASIC:
        minWager = GACHA_BASIC_MIN_WAGER;
        break;
    case GACHA_GREAT:
        minWager = GACHA_GREAT_MIN_WAGER;
        break;
    case GACHA_ULTRA:
        minWager = GACHA_ULTRA_MIN_WAGER;
        break;
    case GACHA_MASTER:
        minWager = GACHA_MASTER_MIN_WAGER;
        break;
    }

    // If available Pokémon is 0, there is no chance
    if (available == 0)
        return 0;

    // Calculate the reduction in chance based on the proportion of owned Pokémon
    ownedPercentage = (owned * 100) / available;
    newChance = baseChance * (100 - ownedPercentage) / 100;

    // Ensure the wager is within the valid range
    if (wager >= minWager && owned < available)
    {
        // Normalize wager to a 0-100 range where [minWager - 9999] -> [0 - 100]
        wagerMultiplier = ((wager - minWager) * 100) / (9999 - minWager);

        // Boost the chance based on the wager multiplier, but cap it by baseChance
        boostedChance = newChance + ((baseChance - newChance) * wagerMultiplier) / 100;

        // Ensure the chance doesn't exceed baseChance
        newChance = (boostedChance > baseChance) ? baseChance : boostedChance;
    }

    return newChance;
}

static bool32 ChooseGachaRarity(bool32 chooseUnowned)
{
    u32 rarity;
    u16 eligibleOdds = 0;
    u16 randomValue;

    for (rarity = RARITY_COMMON; rarity <= RARITY_ULTRA_RARE; rarity++)
    {
        u16 totalMax = GetMaxAvailableGachaRaritySpecies(sGacha->GachaId, rarity);
        u16 totalOwned = GetGachaOwnedCount(rarity);

        if ((chooseUnowned && totalOwned < totalMax) || (!chooseUnowned && totalOwned > 0))
            eligibleOdds += GetGachaRarityOdds(rarity);
    }

    if (eligibleOdds == 0)
        return FALSE;

    randomValue = Random() % eligibleOdds;
    for (rarity = RARITY_COMMON; rarity <= RARITY_ULTRA_RARE; rarity++)
    {
        u16 totalMax = GetMaxAvailableGachaRaritySpecies(sGacha->GachaId, rarity);
        u16 totalOwned = GetGachaOwnedCount(rarity);
        u16 rarityOdds;

        if ((chooseUnowned && totalOwned >= totalMax) || (!chooseUnowned && totalOwned == 0))
            continue;

        rarityOdds = GetGachaRarityOdds(rarity);
        if (randomValue < rarityOdds)
        {
            sGacha->Rarity = rarity;
            return TRUE;
        }
        randomValue -= rarityOdds;
    }

    return FALSE;
}

//★ 2026-10-11 表の先頭が NONE（番号の外）でも「？」のポケモンを渡さない＝表の中で最初に有効な種族
static u16 GetFirstValidGachaMon(void)
{
    u16 i;
    u16 totalMax = GetMaxAvailableGachaRaritySpecies(sGacha->GachaId, sGacha->Rarity);

    for (i = 0; i < totalMax; i++)
    {
        u16 species = GetGachaMon(i);
        if (species != SPECIES_NONE)
            return species;
    }
    return SPECIES_MAGIKARP;
}

static u16 ChooseGachaSpecies(bool32 chooseUnowned)
{
    u16 i;
    u16 totalMax;
    u16 totalOwned;
    u16 eligibleCount;
    u16 target;

    totalMax = GetMaxAvailableGachaRaritySpecies(sGacha->GachaId, sGacha->Rarity);
    totalOwned = GetGachaOwnedCount(sGacha->Rarity);
    eligibleCount = chooseUnowned ? totalMax - totalOwned : totalOwned;
    if (eligibleCount == 0)
        return GetFirstValidGachaMon();
    target = Random() % eligibleCount;

    for (i = 0; i < totalMax; i++)
    {
        u16 species = GetGachaMon(i);

        if (species != SPECIES_NONE && CheckIfOwned(species) != chooseUnowned)
        {
            if (target == 0)
                return species;
            target--;
        }
    }

    return GetFirstValidGachaMon();
}

static void DeterminePokemonRarityAndNewStatus(void)
{
    bool32 chooseUnowned = (Random() % 100) < sGacha->newMonOdds;

    if (!ChooseGachaRarity(chooseUnowned))
    {
        chooseUnowned = !chooseUnowned;
        ChooseGachaRarity(chooseUnowned);
    }

    sGacha->CalculatedSpecies = ChooseGachaSpecies(chooseUnowned);
}

static void CalculatePullOdds(void)
{
    u16 totalCommonAvailable;
    u16 totalUncommonAvailable;
    u16 totalRareAvailable;
    u16 totalUltraRareAvailable;
    u16 wager;
    u8 totalChance;
    bool32 hasUnownedSpecies;

    totalCommonAvailable = GetMaxAvailableGachaRaritySpecies(sGacha->GachaId, RARITY_COMMON);
    totalUncommonAvailable = GetMaxAvailableGachaRaritySpecies(sGacha->GachaId, RARITY_UNCOMMON);
    totalRareAvailable = GetMaxAvailableGachaRaritySpecies(sGacha->GachaId, RARITY_RARE);
    totalUltraRareAvailable = GetMaxAvailableGachaRaritySpecies(sGacha->GachaId, RARITY_ULTRA_RARE);

    wager = sGacha->wager;  // Player's wager (0-9999)
    hasUnownedSpecies = sGacha->ownedCommon < totalCommonAvailable
                     || sGacha->ownedUncommon < totalUncommonAvailable
                     || sGacha->ownedRare < totalRareAvailable
                     || sGacha->ownedUltraRare < totalUltraRareAvailable;

    if (wager >= GACHA_GUARANTEED_NEW_WAGER && hasUnownedSpecies)
    {
        sGacha->newMonOdds = 100;
        return;
    }

    // Add up the chances from each rarity
    totalChance = CalculateChanceForCategory(sGacha->ownedCommon, totalCommonAvailable, RARITY_COMMON_ODDS, wager);
    totalChance += CalculateChanceForCategory(sGacha->ownedUncommon, totalUncommonAvailable, RARITY_UNCOMMON_ODDS, wager);
    totalChance += CalculateChanceForCategory(sGacha->ownedRare, totalRareAvailable, RARITY_RARE_ODDS, wager);
    totalChance += CalculateChanceForCategory(sGacha->ownedUltraRare, totalUltraRareAvailable, RARITY_ULTRA_RARE_ODDS, wager);

    if (totalChance <= 100)
        sGacha->newMonOdds = totalChance;
    else
        sGacha->newMonOdds = 100;
}

static void AButton(void)
{
    if (sGacha->canBetWager)
    {
        sGacha->state = STATE_INIT_A;
    }
    else
    {
        PlaySE(SE_FAILURE);
    }
}

static void UpdateCursorPosition(s16 x)
{
    // Update cursor position based on X coordinate
    if (x == 231)
        sGacha->cursorPosition = 3; // Ones
    else if (x == 223)
        sGacha->cursorPosition = 2; // Tens
    else if (x == 215)
        sGacha->cursorPosition = 1; // Hundreds
    else
        sGacha->cursorPosition = 0; // Thousands
}

static void UpdateWagerDigit(int direction)
{
    u8 place;
    u16 oldWager;
    u8 wagerDigits[4];
    u16 newWager;
    u16 d;
    int i;
    u16 maxWager;
    u16 minWager;

    place = sGacha->cursorPosition;
    d = 1000;
    oldWager = sGacha->wager;
    
    for (i = 0; i < 4; i++)
    {
        if (oldWager >= d)
            wagerDigits[i] = oldWager / d;
        else
            wagerDigits[i] = 0;

        oldWager = oldWager % d;
        d = d / 10;
    }
    maxWager = GetCoins();  // Maximum wager is the current coins
    
    // wagerDigits[0] = Thousands place
    // wagerDigits[1] = Hundreds place
    // wagerDigits[2] = Tens place
    // wagerDigits[3] = Ones place

    if (direction == 0) // Up
    {
        if (wagerDigits[place] == 9)
        {
            // Set the current digit to 0
            wagerDigits[place] = 0;
            if (place > 0)
                place--;  // Move to the next digit on the left
        }

        // Otherwise, simply increase the digit by 1
        wagerDigits[place]++;
        PlaySE(SE_SELECT);
                
        // Ensure the new wager doesn't exceed max available coins
        newWager = (wagerDigits[0] * 1000) + (wagerDigits[1] * 100) + (wagerDigits[2] * 10) + wagerDigits[3];
        if (newWager > maxWager) // If the new wager exceeds available coins, revert back
            newWager = maxWager;
        // Update the wager if it's within the limit
        sGacha->wager = newWager;
    }
    else if (direction == 2) // Down
    {
        if (wagerDigits[place] > 0)
        {
            // Decrease the digit by 1
            wagerDigits[place]--;
            sGacha->wager = (wagerDigits[0] * 1000) + (wagerDigits[1] * 100) + (wagerDigits[2] * 10) + wagerDigits[3];
            PlaySE(SE_SELECT);
        }
    }

    // Update sprite animation based on the new value
    gSprites[sGacha->ArrowsSpriteId].animNum = (wagerDigits[place] == 0) ? 1 : 0;
    SetPlayerDigits(sGacha->wager);  // Update the displayed wager

    switch (sGacha->GachaId)
    {
    default:
    case GACHA_BASIC:
        minWager = GACHA_BASIC_MIN_WAGER;
        break;
    case GACHA_GREAT:
        minWager = GACHA_GREAT_MIN_WAGER;
        break;
    case GACHA_ULTRA:
        minWager = GACHA_ULTRA_MIN_WAGER;
        break;
    case GACHA_MASTER:
        minWager = GACHA_MASTER_MIN_WAGER;
        break;
    }

    if (sGacha->wager >= minWager && sGacha->canGiveMon)
    {
        ResetMessage();
        CalculatePullOdds();
        sGacha->canBetWager = TRUE;
        gSprites[sGacha->CTAspriteId].animNum = 1; // On
        ShowMessage();
    }
    else
    {
        ResetMessage();
        //CalculatePullOdds();
        sGacha->newMonOdds = 0;
        sGacha->canBetWager = FALSE;        
        gSprites[sGacha->CTAspriteId].animNum = 0; // Off
        ShowMessage();
    }
}

static void MoveCursor(int direction)
{
    struct Sprite *cursorSprite = &gSprites[sGacha->ArrowsSpriteId];
    int curX = cursorSprite->pos1.x;
    int destX = curX;
    
    // Move cursor left or right (X axis)
    if (direction == 1 || direction == 3)// Right or Left
    {
        if (direction == 1 && curX < 231)
        {
            destX = curX + 8;
            PlaySE(SE_SELECT);
        }
        else if (direction == 3 && curX > 207)
        {
            destX = curX - 8;
            PlaySE(SE_SELECT);
        }
        
        cursorSprite->pos1.x = destX;
        UpdateCursorPosition(gSprites[sGacha->ArrowsSpriteId].pos1.x);  // Update cursor position based on X coordinate
    }
    
    // Move cursor up or down (change wager digit)
    else if (direction == 0 || direction == 2) // Up or Down
    {
        UpdateWagerDigit(direction); // Update the corresponding digit
    }
}

static void ExitGacha(u8 taskId)
{
    if (!gPaletteFade->active)
    {
        if (sGacha->monShown)
            FreeAndDestroyMonPicSprite(sGacha->monSpriteId);
        ResetMessage();
        FREE_AND_SET_NULL(sGachaTilemap);
        FreeAllWindowBuffers();
        DestroyTask(taskId);
        RestoreMapMusicAfterGame();
        SetMainCallback2(CB2_ReturnToFieldContinueScriptPlayMapMusic);
        FREE_AND_SET_NULL(sGacha);
    }
}

static void HandleInput(void)
{
    if (JOY_NEW(A_BUTTON))
    {
        AButton();
    }
    else if (JOY_NEW(B_BUTTON))
    {
        gSpecialVar_Result = MON_CANT_GIVE;
        sGacha->state = GACHA_STATE_START_EXIT;
    }
    else if (JOY_NEW(DPAD_UP))
    {
        MoveCursor(0);
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        MoveCursor(1);
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        MoveCursor(2);
    }
    else if (JOY_NEW(DPAD_LEFT))
    {
        MoveCursor(3);
    }
}

static void RemoveGarbage(void)
{
    DestroySpriteAndFreeResources(&gSprites[sGacha->CreditSpriteIds[0]]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->CreditSpriteIds[1]]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->CreditSpriteIds[2]]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->CreditSpriteIds[3]]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->PlayerSpriteIds[0]]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->PlayerSpriteIds[1]]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->PlayerSpriteIds[2]]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->PlayerSpriteIds[3]]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->KnobSpriteId]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->DigitalTextSpriteId]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->LotteryJPNspriteId]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->CreditMenu1Id]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->CreditMenu2Id]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->PokemonOneSpriteId]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->PokemonTwoSpriteId]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->PokemonThreeSpriteId]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->ArrowsSpriteId]);
    DestroySpriteAndFreeResources(&gSprites[sGacha->CTAspriteId]);
    ResetMessage();
    SetGpuReg(REG_OFFSET_BLDCNT, 0);
    gPaletteFade->bufferTransferDisabled = TRUE;
    gPaletteFade->bufferTransferDisabled = FALSE;
    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(GachaVBlankCallback);
}

static void ShowFinalMessage(void)
{
    struct WindowTemplate template;

    ResetMessage();
    template = SetWindowTemplateFields(GACHA_BG_BASE, 2, 15, 26, 4, 15, 0x194);
    sTextWindowId = AddWindow(&template);
    GetSpeciesName(gStringVar1, sGacha->CalculatedSpecies);
    StringExpandPlaceholders(gStringVar4, gText_Gacha_YouGot);
    DrawGachaWindowText(gStringVar4, 26, 4, 2);
}

static u8 GetSpeciesGachaLevel(void)
{
    u32 level, minLevel, addedLevelRange, i;
    static const u32 sLevelGachaFlagMap[][3] =
    {
        {FLAG_BADGE01_GET, 5, 6},
        {FLAG_BADGE02_GET, 7, 5},
        {FLAG_BADGE03_GET, 13, 7},
        {FLAG_BADGE04_GET, 18, 5},
        {FLAG_BADGE05_GET, 19, 9},
        {FLAG_BADGE06_GET, 21, 9},
        {FLAG_BADGE07_GET, 28, 8},
        {FLAG_BADGE08_GET, 36, 14},
        {FLAG_SYS_GAME_CLEAR, 40, 29},
    };

    minLevel = 2;
    addedLevelRange = 4;

    for (i = 0; i < ARRAY_COUNT(sLevelGachaFlagMap); i++)
    {
        if (FlagGet(sLevelGachaFlagMap[i][0]))
        {
            minLevel = sLevelGachaFlagMap[i][1];
            addedLevelRange = sLevelGachaFlagMap[i][2];
        }
    }

    addedLevelRange += 1;

    level = (Random() % addedLevelRange) + minLevel;

    if (level > 100)
        return 100;

    return level;
}

static void GachaMain(u8 taskId)
{
    switch (sGacha->state)
    {
    case GACHA_STATE_INIT:
        if (!gPaletteFade->active) {
            sGacha->state = GACHA_STATE_PROCESS_INPUT;
        }
        break;
    case GACHA_STATE_PROCESS_INPUT:
        HandleInput();
        break;
    case GACHA_STATE_START_EXIT:
        StartExitGacha();
        break;
    case GACHA_STATE_EXIT:
        ExitGacha(taskId);
        break;
    case STATE_INIT_A: // Initial state
        DeterminePokemonRarityAndNewStatus();
        PlaySE(SE_SHOP);
        RemoveCoins(sGacha->wager);
        sGacha->wager = 0;
        ResetMessage();
        gSprites[sGacha->CTAspriteId].animNum = 0;
        gSprites[sGacha->ArrowsSpriteId].invisible = TRUE;
        SetCreditDigits(GetCoins());
        SetPlayerDigits(sGacha->wager);
        sGacha->waitTimer = 30;  // Set the timer
        sGacha->state = STATE_TIMER_1;  // Move to next state
        break;
    case STATE_TIMER_1: // Waiting for timer to expire
        if (sGacha->waitTimer > 0)
            sGacha->waitTimer--;  // Decrease timer
        else
            sGacha->state = STATE_TWIST;  // Transition to next state when the timer is done
        break;
    case STATE_TWIST: // After timer expires, proceed with animation
        PlaySE(SE_VEND);
        gSprites[sGacha->KnobSpriteId].animNum = 1;
        sGacha->state = STATE_TIMER_2;  // Move to the next state after animation starts
        break;
    case STATE_TIMER_2: // Handle the next part of the delay or action
        // (You can add another waiting period if needed)
        sGacha->waitTimer = 50;  // Set the next timer
        sGacha->state = STATE_INIT_GIVE;  // Move to next state
        break;
    case STATE_INIT_GIVE: // Final state
        if (sGacha->waitTimer > 0)
            sGacha->waitTimer--;  // Decrease timer
        else
            sGacha->state = STATE_SHAKE_1;  // Final action after timer
        break;
    case STATE_SHAKE_1: // After timer expires, proceed with animation
        PlaySE(SE_BREAKABLE_DOOR);
        Shake1();
        sGacha->state = STATE_TIMER_3;  // Move to the next state after animation starts
        break;
    case STATE_TIMER_3: // Handle the next part of the delay or action
        // (You can add another waiting period if needed)
        sGacha->waitTimer = 3;  // Set the next timer
        sGacha->state = STATE_INIT_SHAKE_2;  // Move to next state
        break;
    case STATE_INIT_SHAKE_2: // Final state
        if (sGacha->waitTimer > 0)
            sGacha->waitTimer--;  // Decrease timer
        else
            sGacha->state = STATE_SHAKE_2;  // Final action after timer
        break;    
    case STATE_SHAKE_2: // After timer expires, proceed with animation
        //PlaySE(SE_BREAKABLE_DOOR);
        Shake2();
        sGacha->state = STATE_TIMER_4;  // Move to the next state after animation starts
        break;
    case STATE_TIMER_4: // Handle the next part of the delay or action
        // (You can add another waiting period if needed)
        sGacha->waitTimer = 3;  // Set the next timer
        sGacha->state = STATE_INIT_SHAKE_3;  // Move to next state
        break;
    case STATE_INIT_SHAKE_3: // Final state
        if (sGacha->waitTimer > 0)
        {
            sGacha->waitTimer--;  // Decrease timer
        }
        else 
        {
            BGSetup();
            sGacha->waitTimer = 20;
            sGacha->state = STATE_TIMER_5;  // Final action after timer
        }
        break;
    case STATE_TIMER_5: // After timer expires, proceed with animation
        if (sGacha->waitTimer > 0)
            sGacha->waitTimer--;  // Decrease timer
        else
            sGacha->state = STATE_GIVE;  // Move to the next state after animation starts
        break;
    case STATE_GIVE:
        StartTradeScreen();
        break;
    case STATE_FADE:
        if (!gPaletteFade->active)
        {
            BGRed();
            sGacha->state = STATE_POKEBALL_INIT;
        }
        break;
    case STATE_POKEBALL_INIT:
        RemoveGarbage();
        sGacha->state++;
        break;    
    case STATE_POKEBALL_PROCESS:
        if (!gPaletteFade->active)
            sGacha->state = STATE_POKEBALL_ARRIVE;
        break;
    case STATE_POKEBALL_ARRIVE:    
        LoadSpriteSheet(&sPokeBallSpriteSheet);
        LoadSpritePalette(&sPokeBallSpritePalette);
        sGacha->bouncingPokeballSpriteId = CreateSprite(&sSpriteTemplate_Pokeball, 120, -8, 0);
        gSprites[sGacha->bouncingPokeballSpriteId].data[3] = 74;
        gSprites[sGacha->bouncingPokeballSpriteId].callback = SpriteCB_BouncingPokeballArrive;
        StartSpriteAnim(&gSprites[sGacha->bouncingPokeballSpriteId], 1);
        StartSpriteAffineAnim(&gSprites[sGacha->bouncingPokeballSpriteId], 2);
        BlendPalettes(1 << (16 + gSprites[sGacha->bouncingPokeballSpriteId].oam.paletteNum), 16, RGB_WHITE);
        sGacha->state++;
        sGacha->timer = 0;
        break;
    case STATE_FADE_POKEBALL_TO_NORMAL:
        BeginNormalPaletteFade(1 << (16 + gSprites[sGacha->bouncingPokeballSpriteId].oam.paletteNum), 1, 16, 0, RGB_WHITE);
        sGacha->state++;
        break;
    case STATE_POKEBALL_ARRIVE_WAIT:
        if (gSprites[sGacha->bouncingPokeballSpriteId].callback == SpriteCallbackDummy)
        {
            u32 personality;
            u32 otId;

            ZeroMonData(&sGacha->mon);
            CreateMon(&sGacha->mon, sGacha->CalculatedSpecies, GetSpeciesGachaLevel(), 32, FALSE, 0, OT_ID_PLAYER_ID, 0);
            gSpecialVar_Result = GiveMonToPlayer(&sGacha->mon);
            VarSet(GACHA_RESULT_SPECIES_VAR, sGacha->CalculatedSpecies);
            SetMonPokedexFlags(&sGacha->mon);
            personality = GetMonData(&sGacha->mon, MON_DATA_PERSONALITY, NULL);
            otId = GetMonData(&sGacha->mon, MON_DATA_OT_ID, NULL);
            sGacha->monSpriteId = CreateMonPicSprite_HandleDeoxys(sGacha->CalculatedSpecies, otId, personality, TRUE, 120, 60, 0, 0x7A10); //[hack] 2026-10-10 前は OBJ パレット 14 番の決め打ち（tag 0xFFFF）で、画面のほかの絵のパレットとぶつかって色がおかしかった（ユーザー「ガチャでポケモンが来たときそのポケモンのグラフィックがおかしい」）→ 札で空いている所に
            if (sGacha->monSpriteId != 0xFFFF)
            {
                //[hack] 2026-10-10 素の CreateMonPicSprite が入れた色がリンドリス（いちばん後ろの種族）の色になっていた（ユーザーの画像）→
                //  受け取った子の色（戦闘と同じ GetMonSpritePalFromSpeciesAndPersonality・色違いも）を、その絵のパレットに入れ直す
                LoadCompressedPalette(GetMonSpritePalFromSpeciesAndPersonality(sGacha->CalculatedSpecies, otId, personality), 0x100 + gSprites[sGacha->monSpriteId].oam.paletteNum * 16, 32);
                gSprites[sGacha->monSpriteId].callback = SpriteCB_Null;
                gSprites[sGacha->monSpriteId].oam.priority = 0;
                gSprites[sGacha->monSpriteId].invisible = TRUE;
                sGacha->monShown = TRUE;
            }
            sGacha->state++;
        }
        break;
    case STATE_SHOW_NEW_MON:
        FreeSpriteOamMatrix(&gSprites[sGacha->bouncingPokeballSpriteId]);
        DestroySprite(&gSprites[sGacha->bouncingPokeballSpriteId]);
        if (sGacha->monShown)
        {
            gSprites[sGacha->monSpriteId].invisible = FALSE;
            BlendPalettes(1 << (16 + gSprites[sGacha->monSpriteId].oam.paletteNum), 16, RGB_WHITE);
            BeginNormalPaletteFade(1 << (16 + gSprites[sGacha->monSpriteId].oam.paletteNum), 1, 16, 0, RGB_WHITE);
        }
        PlayCry1(sGacha->CalculatedSpecies, 0);
        sGacha->state++;
        break;
    case STATE_NEW_MON_MSG:
        //Wait for the white flash on the Pokemon to finish
        if (!gPaletteFade->active)
            sGacha->state++;
        break;
    case NEW_1:
        // "{mon} hatched from egg" message/fanfare
        ShowFinalMessage();
        PlayFanfare(MUS_EVOLVED);
        sGacha->state++;
        //PutWindowTilemap(0);
        //CopyWindowToVram(0, COPYWIN_FULL);
        break;
    case NEW_2:
        if (IsFanfareTaskInactive())
            sGacha->state++;
        break;
    case STATE_SET_EXIT:
        // Ready the nickname prompt
        if (!FlagGet(FLAG_SYS_POKEMON_GET))
        {
            FlagSet(FLAG_SYS_POKEMON_GET);
        }
        sGacha->state = GACHA_STATE_START_EXIT;
        break;
    }
}

static bool32 GachaGfxSetup(void)
{
    switch (gMain.state)
    {
    case 0:
        sGacha->GachaId = gSpecialVar_0x8004;

        SetVBlankCallback(NULL);
        {
            u8 bgOffset;

            for (bgOffset = 0; bgOffset < 8; ++bgOffset)
                SetGpuReg(REG_OFFSET_BG0HOFS + bgOffset * 2, 0);
        }
        ResetVramOamAndBgCntRegs();
        ResetBgsAndClearDma3BusyFlags(0);
        sGacha->bgReady = FALSE; //BG設定がリセットされるので、BGSetupで再初期化させる
        ResetTempTileDataBuffers();
        ResetSpriteData();
        FreeAllSpritePalettes();
        ResetAllPicSprites();
        gPaletteFade->bufferTransferDisabled = TRUE;
        gMain.state++;
        break;
    case 1:
        BGSetup();
        gMain.state++;
        break;
    case 2:
        switch (sGacha->GachaId)
        {
        default:
        case GACHA_BASIC:
            LoadSpritePalettes(sSpritePalettesBasic);
            CreateHoppip();
            break;
        case GACHA_GREAT:
            LoadSpritePalettes(sSpritePalettesGreat);
            CreatePhanpy();
            break;
        case GACHA_ULTRA:
            LoadSpritePalettes(sSpritePalettesUltra);
            CreateTeddiursa();
            break;
        case GACHA_MASTER:
            LoadSpritePalettes(sSpritePalettesMaster);
            CreateBelossom();
            break;
        }
        gMain.state++;
        break;
    case 3:
        CreateArrows();
        CreateCTA();
        CreateDigitalText();    
        CreateKnob();
        CreateCreditSprites();
        CreatePlayerSprites();
        SetCreditDigits(GetCoins());
        SetPlayerDigits(0);    
        CreateCreditMenu();    
        CreatePlayerMenu();
        CreateLotteryJPN();
        gMain.state++;
        break;
    case 4:
        sGacha->newMonOdds = 0;
        InitWindows(sGachaWinTemplates);
        DeactivateAllTextPrinters();
        LoadPalette(gGachaTextPal, BG_PLTT_ID(15), PLTT_SIZE_4BPP);
        ShowMessage();
        gMain.state++;
        break;
    case 5:
        UpdateCursorPosition(gSprites[sGacha->ArrowsSpriteId].pos1.x);
        sGacha->waitTimer = 0;
        sGacha->canGiveMon = !IsPlayerPartyAndPokemonStorageFull();
        GetPokemonOwned();
        gMain.state++;
        break;
    case 6:
        CopyBgTilemapBufferToVram(GACHA_BG_BASE);
        CopyBgTilemapBufferToVram(GACHA_MENUS);
        SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON | DISPCNT_BG2_ON);
        ShowBg(GACHA_BG_BASE);
        ShowBg(GACHA_MENUS);
        gMain.state++;
        break;
    case 7:
        CreateTask(GachaMain, 1);
        gMain.state++;
        break;
    case 8:
        BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);
        gPaletteFade->bufferTransferDisabled = FALSE;
        gMain.state++;
        break;
    case 9:
        BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);
        gMain.state++;
        break;
    default:
        SetVBlankCallback(GachaVBlankCallback);
        PlayBGM(GAME_CORNER_MUSIC);
        SetMainCallback2(GachaMainCallback);
        return TRUE;
    }
    return FALSE;
}

static void InitGachaScreen(void)
{
    while (TRUE)
    {
        if (GachaGfxSetup())
            break;
    }
}
