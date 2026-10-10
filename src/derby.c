#include "defines.h"
#include "../include/bg.h"
#include "../include/coins.h"
#include "../include/decompress.h"
#include "../include/event_data.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/malloc.h"
#include "../include/menu.h"
#include "../include/menu_helpers.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/random.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/window.h"
#include "../include/constants/rgb.h"
#include "../include/constants/songs.h"
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
derby.c
	Derby (horse racing) ported from SoulGold / pokeemerald-expansion game corner
	Script usage:  callasm StartDerby ; waitstate
	The bet is taken from the player's coins during the game; the prize coins are stored in
	DERBY_VAR_WINNINGS and must be paid out by the script afterwards (see derby.s).
*/

//Declared in new/Vanilla_functions.h, which conflicts with gpu_regs.h, so declare here
void __attribute__((long_call)) ResetBgsAndClearDma3BusyFlags(u8 bg);
void __attribute__((long_call)) ResetVramOamAndBgCntRegs(void);
void __attribute__((long_call)) DeactivateAllTextPrinters(void);

//Variables used by this minigame (saved variables, make sure they are unused elsewhere)
//[hack] 2026-10-10 0x5120〜0x512E はバトルチューブの予約（0x511F〜0x5133）とぶつかる → 0x50DE〜0x50EC（tools/rom/build_hoenn_scripts.py の HOENN_VAR_END を 0x50DE に下げた）
#define DERBY_VAR_RACER_1       0x50DE //Species (hundreds), shiny (tens), condition (ones)
#define DERBY_VAR_RACER_2       0x50DF
#define DERBY_VAR_RACER_3       0x50E0
#define DERBY_VAR_RACER_4       0x50E1
#define DERBY_VAR_RACER_5       0x50E2
#define DERBY_VAR_RACER_6       0x50E3
#define DERBY_VAR_RACER_NAME_1  0x50E4 //Index of the name of each racer
#define DERBY_VAR_RACER_NAME_2  0x50E5
#define DERBY_VAR_RACER_NAME_3  0x50E6
#define DERBY_VAR_RACER_NAME_4  0x50E7
#define DERBY_VAR_RACER_NAME_5  0x50E8
#define DERBY_VAR_RACER_NAME_6  0x50E9
#define DERBY_FLAG_RESET        0x50EA //(not a real flag, a variable used as one) 1 = racers are already generated
#define DERBY_FLAG_NICKNAME     0x50EB //(not a real flag, a variable used as one) 1 = names are already generated
#define GAME_CORNER_VAR_WINNINGS 0x50EC //Must match DERBY_VAR_WINNINGS in assembly/overworld_scripts/derby.s

#define DerbyFlagGet(var) (VarGet(var) != 0)
#define DerbyFlagSet(var) VarSet(var, 1)
#define DerbyFlagClear(var) VarSet(var, 0)

#define FONT_DERBY 1 //Font used for the racer names (change if the text looks wrong)

//Sound effects (BPRJ equivalents of the Emerald ones)
#define SE_FAILURE SE_ERROR
#define SE_SHOP 88 //SoulGold SE_SHOP (SE_REGI) = ROM song 88
#define SE_BALL_TRAY_BALL 108 //SoulGold SE_BALL_TRAY_BALL (SE_TB_KON) = ROM song 108
#define SE_CONTEST_PLACE 24 //SoulGold SE_CONTEST_PLACE (SE_JYUNI) = ROM song 24
#define SE_EXP_MAX 84 //SoulGold SE_EXP_MAX (SE_EXPMAX) = ROM song 84
#define SE_APPLAUSE 98 //SoulGold SE_APPLAUSE (SE_DENDOU) = ROM song 98
#define MUS_NONE MUS_DUMMY

//Moves a racer sprite: the original subtracts (speed / 2) as a float and truncates, speeds here are fixed point (x256)
#define DerbyStep(speed) (((speed) + 511) >> 9)

//Text color palette for the racer name window (BG palette 15): transparent, white, dark purple
static const u16 sDerbyTextPal[16] = { 0x0000, 0x7FFF, 0x4C0A, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000 };

//Graphics (src/derby_gfx.c)
extern const u8 gDerby_Names_Ponyta[][16];
extern const u8 gDerby_Names_Rattata[][16];
extern const u8 gDerby_Names_Rapidash[][16];
extern const u8 gDerby_Names_Feebas[][16];
extern const u8 gDerby_BG_Bet_Img[];
extern const u8 gDerby_BG_Bet_Tilemap[];
extern const u16 gDerby_BG_Bet_Pal[];
extern const u8 gDerby_BG_Bet2_Img[];
extern const u8 gDerby_BG_Bet2_Tilemap[];
extern const u16 gDerby_BG_Bet2_Pal[];
extern const u8 gDerby_BG_Race_Img[];
extern const u8 gDerby_BG_Race_Tilemap[];
extern const u16 gDerby_BG_Race_Pal[];
extern const u8 gDerby_PonytaGFX[];
extern const u16 gDerby_PonytaNormalPAL[];
extern const u16 gDerby_PonytaShinyPAL[];
extern const u8 gDerby_RapidashGFX[];
extern const u16 gDerby_RapidashNormalPAL[];
extern const u16 gDerby_RapidashShinyPAL[];
extern const u8 gDerby_RattataGFX[];
extern const u16 gDerby_RattataNormalPAL[];
extern const u16 gDerby_RattataShinyPAL[];
extern const u8 gDerby_FeebasGFX[];
extern const u16 gDerby_FeebasNormalPAL[];
extern const u16 gDerby_FeebasShinyPAL[];
extern const u8 gDerby_Name_Ponyta[];
extern const u8 gDerby_Name_Rapidash[];
extern const u8 gDerby_Name_Rattata[];
extern const u8 gDerby_Name_Feebas[];
extern const u16 gDerby_TextPAL[];
extern const u8 gDerby_Condition_1[];
extern const u8 gDerby_Condition_2[];
extern const u8 gDerby_Condition_3[];
extern const u8 gDerby_Condition_4[];
extern const u8 gDerby_Condition_5[];
extern const u16 gDerby_ConditionPAL[];
extern const u8 gDerby_Payout_1[];
extern const u8 gDerby_Payout_2[];
extern const u8 gDerby_Payout_3[];
extern const u8 gDerby_Payout_4[];
extern const u8 gDerby_Payout_5[];
extern const u8 gDerby_Payout_6[];
extern const u8 gDerby_Payout_7[];
extern const u8 gDerby_UI_1[];
extern const u8 gDerby_UI_2[];
extern const u8 gDerby_UI_3[];
extern const u8 gDerby_UI_4[];
extern const u8 gDerby_UI_5[];
extern const u8 gDerby_UI_6[];
extern const u16 gDerby_BetMenuUIPAL[];
extern const u8 gDerby_Arrow_1[];
extern const u8 gDerby_Arrow_2[];
extern const u8 gDerby_Credit_Icon[];
extern const u8 gDerby_Credit_Red_Icon[];
extern const u16 gDerby_Credit_Icon_PAL[];
extern const u8 gDerby_Digits[];
extern const u8 gDerby_Digits_2[];
extern const u16 gDerby_Digits_Pal[];
extern const u8 gDerby_P10[];
extern const u8 gDerby_Feebas_OW[];
extern const u8 gDerby_Ponyta_OW[];
extern const u8 gDerby_Rattata_OW[];
extern const u8 gDerby_Rapidash_OW[];
extern const u8 gDerby_Cursor[];
extern const u16 gDerby_Cursor_PAL[];
extern const u8 gDerby_Countdown_3[];
extern const u8 gDerby_Countdown_2[];
extern const u8 gDerby_Countdown_1[];
extern const u8 gDerby_Go[];
extern const u16 gDerby_Countdown_PAL[];
extern const u8 gDerby_Clef[];
extern const u16 gDerby_Clef_PAL[];

enum
{
    DERBY_STATE_INIT,
    DERBY_STATE_PROCESS_INPUT,
    DERBY_STATE_BET_2_START,
    DERBY_STATE_BET_2,
    DERBY_STATE_RACE_DELAY,
    DERBY_STATE_START_FADE,
    DERBY_STATE_FADEIN,
    DERBY_STATE_DELAY_2,
    DERBY_STATE_COUNTDOWN,
    DERBY_STATE_RACE_START,
    DERBY_STATE_RACE_MID,
    DERBY_STATE_FINAL_DELAY,
    DERBY_STATE_START_EXIT,
    DERBY_STATE_EXIT,
};

enum {
    SPR_CREDIT_DIG_1,
    SPR_CREDIT_DIG_10,
    SPR_CREDIT_DIG_100,
    SPR_CREDIT_DIG_1000,
};

enum {
    SPR_BET_DIG_1,
    SPR_BET_DIG_10,
    SPR_BET_DIG_100,
    SPR_BET_DIG_1000,
};

enum {
    SPR_POT_DIG_1,
    SPR_POT_DIG_10,
    SPR_POT_DIG_100,
    SPR_POT_DIG_1000,
};

enum {
    DERBY_RACER_1,
    DERBY_RACER_2,
    DERBY_RACER_3,
    DERBY_RACER_4,
    DERBY_RACER_5,
    DERBY_RACER_6,
    DERBY_RACER_COUNT,
};

enum {
    CONDITION_1_STAR,
    CONDITION_2_STAR,
    CONDITION_3_STAR,
    CONDITION_4_STAR,
    CONDITION_5_STAR,
};

enum {
    DERBY_SPECIES_PONYTA_1,
    DERBY_SPECIES_PONYTA_2,
    DERBY_SPECIES_PONYTA_3,
    DERBY_SPECIES_PONYTA_4,
    DERBY_SPECIES_PONYTA_5,
    DERBY_SPECIES_RATTATA,
    DERBY_SPECIES_RAPIDASH,
    DERBY_SPECIES_FEEBAS,
    DERBY_SPECIES_COUNT,
};

enum {
    BET_MULTIPLIER_NONE,
    BET_MULTIPLIER_1_1,
    BET_MULTIPLIER_1_2,
    BET_MULTIPLIER_1_3,
    BET_MULTIPLIER_1_5,
    BET_MULTIPLIER_2_0,
    BET_MULTIPLIER_3_0,
    BET_MULTIPLIER_JACKPOT,
};

#define DERBY_SPECIES_PONYTA_COUNT DERBY_SPECIES_RATTATA

#define SPR_CREDIT_DIGITS SPR_CREDIT_DIG_1
#define SPR_BET_DIGITS SPR_BET_DIG_1
#define SPR_POTENTIAL_DIGITS SPR_POT_DIG_1

#define MAX_SPRITES_CREDIT 4
#define MAX_SPRITES_BET 4
#define MAX_SPRITES_POTENTIAL 4

struct Derby {
    u8 state;
    u8 racerSpecies[DERBY_RACER_COUNT];
    u8 racerShiny[DERBY_RACER_COUNT];
    u8 racerCondition[DERBY_RACER_COUNT];
    u8 racerSpeciesSpriteId[DERBY_RACER_COUNT];
    u8 racerSpeedPay[DERBY_RACER_COUNT];
    s32 racerSpeedStart[DERBY_RACER_COUNT];
    s32 racerSpeedRace[DERBY_RACER_COUNT];
    u8 MenuPosition; // 0-5
    u8 exitToggle;
    u8 SpeciesNameSpriteId;
    u8 ConditionSpriteId;
    u8 PayoutSpriteId;
    u8 uiSpriteId;
    u8 Arrow1SpriteId;
    u8 Arrow2SpriteId;
    u8 CreditIconSpriteId[2];
    u8 CreditSpriteIds[MAX_SPRITES_CREDIT];
    u8 BetSpriteIds[MAX_SPRITES_BET];
    u8 PotentialSpriteIds[MAX_SPRITES_POTENTIAL];
    u8 Menu;
    u8 RacerSelected;
    u8 Swap1;
    u8 P10SpriteId;
    u32 Bet;
    u32 PotentialWin;
    u32 delay;
    u8 RacerSprite1;
    u8 RacerSprite2;
    u8 RacerSprite3;
    u8 RacerSprite4;
    u8 RacerSprite5;
    u8 RacerSprite6;
    u8 CursorSpriteId;
    u8 Timer;
    u8 Swap2;
    s16 CountdownX;
    s16 CountdownY;
    u8 CountdownSpriteId;
    u32 frames;
    u8 place1;
    u8 place2;
    u8 place3;
    u8 place4;
    u8 place5;
    u8 place6;
    u8 ClefSpriteId;
};    

#define sDerby (*((struct Derby **) 0x2023B44)) //Fixed RAM (CFRU cannot place writable variables in RAM): reuses gBattleMons, free outside battle
#define sDerbyBgBuffer (*((u8 **) 0x2023B48)) //Fixed RAM (CFRU cannot place writable variables in RAM): reuses gBattleMons, free outside battle

static void FadeToDerbyScreen(u8 taskId);
static void InitDerbyScreen(void);
static void DerbyVBlankCallback(void);


// BG Images/Tilemaps

// Betting Slip

// Betting Slip 2

// Racetrack

// Betting Menu Pokemon Sprites and Palettes





// Betting Menu Text

// Pokemon Names



// Conditions



// Payout


// UI



// Arrows


// Credit Icon


// Numbers


// P 10


// Pokemon OW Sprites


// Cursor


// Countdown



// Clefairy


#define DERBY_BG_BASE 1
#define DERBY_MENUS 0

static const struct BgTemplate sDerbyBGtemplates[] = {
    {
       .bg = DERBY_BG_BASE,
       .charBaseIndex = 2,
       .mapBaseIndex = 31,
       .screenSize = 0,
       .paletteMode = 0,
       .priority = 3,
       .baseTile = 0
   },
   {
        .bg = DERBY_MENUS,
        .charBaseIndex = 0,
        .mapBaseIndex = 0x17,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    }
};

static const struct WindowTemplate sDerbyWinTemplates[] = {
    {
        .bg = DERBY_MENUS,
        .tilemapLeft = 5,
        .tilemapTop = 5,
        .width = 10,
        .height = 2,
        .paletteNum = 0xF,
        .baseBlock = 0x194,        
    },
    DUMMY_WIN_TEMPLATE,
};

#define GFX_BG_BETTING 1
#define GFX_BG_RACETRACK 2
#define GFX_PONYTA 3
#define GFX_RAPIDASH 4
#define GFX_RATTATA 5
#define GFX_FEEBAS 6
#define GFX_NAME_PONYTA 7
#define GFX_NAME_RATTATA 8
#define GFX_NAME_RAPIDASH 9
#define GFX_NAME_FEEBAS 10
#define GFX_CONDITION_1 11
#define GFX_CONDITION_2 12
#define GFX_CONDITION_3 13
#define GFX_CONDITION_4 14
#define GFX_CONDITION_5 15
#define GFX_PAYOUT_1 16
#define GFX_PAYOUT_2 17
#define GFX_PAYOUT_3 18
#define GFX_PAYOUT_4 19
#define GFX_PAYOUT_5 20
#define GFX_PAYOUT_6 21
#define GFX_PAYOUT_7 22
#define GFX_UI_1 23
#define GFX_UI_2 24
#define GFX_UI_3 25
#define GFX_UI_4 26
#define GFX_UI_5 27
#define GFX_UI_6 28
#define GFX_ARROW_1 29
#define GFX_ARROW_2 30
#define GFX_CREDIT_ICON 31
#define GFX_CREDIT_DIGITS 32
#define GFX_BET_DIGITS 33
#define GFX_CREDIT_ICON_RED 34
#define GFX_POTENTIAL_DIGITS 35
#define GFX_P10 36
#define GFX_PONYTA_OW 37
#define GFX_RATTATA_OW 38
#define GFX_RAPIDASH_OW 39
#define GFX_FEEBAS_OW 40
#define GFX_CURSOR 41
#define GFX_COUNTDOWN_3 42
#define GFX_COUNTDOWN_2 43
#define GFX_COUNTDOWN_1 44
#define GFX_GO 45
#define GFX_CLEFAIRY 46

#define PAL_BG_BETTING 1
#define PAL_BG_RACETRACK 2
#define PAL_PONYTA 3
#define PAL_PONYTA_SHINY 4
#define PAL_RAPIDASH 5
#define PAL_RAPIDASH_SHINY 6
#define PAL_RATTATA 7
#define PAL_RATTATA_SHINY 8
#define PAL_FEEBAS 9
#define PAL_FEEBAS_SHINY 10
#define PAL_BET_MENU_TEXT 11
#define PAL_CONDITION 12
#define PAL_UI 13
#define PAL_CREDIT_ICON 14
#define PAL_DIGITS 15
#define PAL_CURSOR 16
#define PAL_COUNTDOWN 17
#define PAL_CLEFAIRY 18

static const struct SpritePalette sSpritePalettes[] =
{
    { .data = gDerby_PonytaNormalPAL,         .tag = PAL_PONYTA },
    { .data = gDerby_PonytaShinyPAL,         .tag = PAL_PONYTA_SHINY },
    { .data = gDerby_RapidashNormalPAL,     .tag = PAL_RAPIDASH },
    { .data = gDerby_RapidashShinyPAL,         .tag = PAL_RAPIDASH_SHINY },
    { .data = gDerby_RattataNormalPAL,         .tag = PAL_RATTATA },
    { .data = gDerby_RattataShinyPAL,         .tag = PAL_RATTATA_SHINY },
    { .data = gDerby_FeebasNormalPAL,         .tag = PAL_FEEBAS },
    { .data = gDerby_FeebasShinyPAL,         .tag = PAL_FEEBAS_SHINY },
    { .data = gDerby_TextPAL,                 .tag = PAL_BET_MENU_TEXT },
    { .data = gDerby_ConditionPAL,             .tag = PAL_CONDITION },
    { .data = gDerby_BetMenuUIPAL,             .tag = PAL_UI },
    { .data = gDerby_Credit_Icon_PAL,         .tag = PAL_CREDIT_ICON },
    { .data = gDerby_Digits_Pal,             .tag = PAL_DIGITS },
    { .data = gDerby_Cursor_PAL,             .tag = PAL_CURSOR },
    { .data = gDerby_Countdown_PAL,         .tag = PAL_COUNTDOWN },
    {}
};

static const struct SpritePalette sSpritePalettes2[] =
{
    { .data = gDerby_Clef_PAL,                 .tag = PAL_CLEFAIRY },
    {}
};

static const struct SpriteSheet sSpriteSheet_Clefairy =
{
    .data = gDerby_Clef,
    .size = 0x1000,
    .tag = GFX_CLEFAIRY,
};

static const struct OamData sOamData_Clefairy =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .tileNum = 0,
    .priority = 0,
};

static const union AnimCmd sClefAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_FRAME(32, 20),
    ANIMCMD_FRAME(64, 20),
    ANIMCMD_FRAME(96, 20),
    ANIMCMD_JUMP(0)         // Loop back to the first frame (Frame 0)
};

static const union AnimCmd *const sClefAnimCmds[] = {
    sClefAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Clefairy =
{
    .tileTag = GFX_CLEFAIRY,
    .paletteTag = PAL_CLEFAIRY,
    .oam = &sOamData_Clefairy,
    .anims = sClefAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Countdown_3 =
{
    .data = gDerby_Countdown_3,
    .size = 0x800,
    .tag = GFX_COUNTDOWN_3,
};

static const struct OamData sOamData_Countdown =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
};

static const struct SpriteTemplate sSpriteTemplate_Countdown_3 =
{
    .tileTag = GFX_COUNTDOWN_3,
    .paletteTag = PAL_COUNTDOWN,
    .oam = &sOamData_Countdown,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Countdown_2 =
{
    .data = gDerby_Countdown_2,
    .size = 0x800,
    .tag = GFX_COUNTDOWN_2,
};

static const struct SpriteTemplate sSpriteTemplate_Countdown_2 =
{
    .tileTag = GFX_COUNTDOWN_2,
    .paletteTag = PAL_COUNTDOWN,
    .oam = &sOamData_Countdown,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Countdown_1 =
{
    .data = gDerby_Countdown_1,
    .size = 0x800,
    .tag = GFX_COUNTDOWN_1,
};

static const struct SpriteTemplate sSpriteTemplate_Countdown_1 =
{
    .tileTag = GFX_COUNTDOWN_1,
    .paletteTag = PAL_COUNTDOWN,
    .oam = &sOamData_Countdown,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Go =
{
    .data = gDerby_Go,
    .size = 0x800,
    .tag = GFX_GO,
};

static const struct SpriteTemplate sSpriteTemplate_Go =
{
    .tileTag = GFX_GO,
    .paletteTag = PAL_COUNTDOWN,
    .oam = &sOamData_Countdown,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Ponyta_OW =
{
    .data = gDerby_Ponyta_OW,
    .size = 0x600,
    .tag = GFX_PONYTA_OW,
};

static const struct OamData sOamData_Horse_OW =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
};

static const union AnimCmd sHorseAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_END
};

static const union AnimCmd sHorseAnimCmd_1[] = 
{
    ANIMCMD_FRAME(0, 5),
    ANIMCMD_FRAME(16, 5),
    ANIMCMD_FRAME(32, 5),
    ANIMCMD_JUMP(0)         // Loop back to the first frame (Frame 0)
};

static const union AnimCmd *const sHorseAnimCmds[] = {
    sHorseAnimCmd_0,  
    sHorseAnimCmd_1,
};

static const struct SpriteTemplate sSpriteTemplate_Ponyta_OW_Normal =
{
    .tileTag = GFX_PONYTA_OW,
    .paletteTag = PAL_PONYTA,
    .oam = &sOamData_Horse_OW,
    .anims = sHorseAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Ponyta_OW_Shiny =
{
    .tileTag = GFX_PONYTA_OW,
    .paletteTag = PAL_PONYTA_SHINY,
    .oam = &sOamData_Horse_OW,
    .anims = sHorseAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Rapidash_OW =
{
    .data = gDerby_Rapidash_OW,
    .size = 0x600,
    .tag = GFX_RAPIDASH_OW,
};

static const struct SpriteTemplate sSpriteTemplate_Rapidash_OW_Normal =
{
    .tileTag = GFX_RAPIDASH_OW,
    .paletteTag = PAL_RAPIDASH,
    .oam = &sOamData_Horse_OW,
    .anims = sHorseAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Rapidash_OW_Shiny =
{
    .tileTag = GFX_RAPIDASH_OW,
    .paletteTag = PAL_RAPIDASH_SHINY,
    .oam = &sOamData_Horse_OW,
    .anims = sHorseAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Rattata_OW =
{
    .data = gDerby_Rattata_OW,
    .size = 0x200,
    .tag = GFX_RATTATA_OW,
};

static const struct OamData sOamData_RatFish_OW =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x16),
    .size = SPRITE_SIZE(32x16),
    .tileNum = 0,
    .priority = 0,
};

static const union AnimCmd sRattataAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_END
};

static const union AnimCmd sRattataAnimCmd_1[] = 
{
    ANIMCMD_FRAME(0, 5),
    ANIMCMD_FRAME(8, 5),
    ANIMCMD_JUMP(0)         // Loop back to the first frame (Frame 0)
};

static const union AnimCmd *const sRattataAnimCmds[] = {
    sRattataAnimCmd_0,  
    sRattataAnimCmd_1,
};

static const struct SpriteTemplate sSpriteTemplate_Rattata_OW_Normal =
{
    .tileTag = GFX_RATTATA_OW,
    .paletteTag = PAL_RATTATA,
    .oam = &sOamData_RatFish_OW,
    .anims = sRattataAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Rattata_OW_Shiny =
{
    .tileTag = GFX_RATTATA_OW,
    .paletteTag = PAL_RATTATA_SHINY,
    .oam = &sOamData_RatFish_OW,
    .anims = sRattataAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Feebas_OW =
{
    .data = gDerby_Feebas_OW,
    .size = 0x200,
    .tag = GFX_FEEBAS_OW,
};

static const union AnimCmd sFeebasAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_FRAME(8, 20),
    ANIMCMD_JUMP(0)         // Loop back to the first frame (Frame 0)
};

static const union AnimCmd *const sFeebasAnimCmds[] = {
    sFeebasAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Feebas_OW_Normal =
{
    .tileTag = GFX_FEEBAS_OW,
    .paletteTag = PAL_FEEBAS,
    .oam = &sOamData_RatFish_OW,
    .anims = sFeebasAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Feebas_OW_Shiny =
{
    .tileTag = GFX_FEEBAS_OW,
    .paletteTag = PAL_FEEBAS_SHINY,
    .oam = &sOamData_RatFish_OW,
    .anims = sFeebasAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheets_CreditsInterface[] =
{
    {
        .data = gDerby_Digits,
        .size = 0x280,
        .tag = GFX_CREDIT_DIGITS
    },
    {}
};

static const struct OamData sOam_CreditDigit =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(8x16),
    .size = SPRITE_SIZE(8x16),
    .priority = 2,
};

static const struct SpriteTemplate sSpriteTemplate_CreditDigit =
{
    .tileTag = GFX_CREDIT_DIGITS,
    .paletteTag = PAL_DIGITS,
    .oam = &sOam_CreditDigit,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct SpriteSheet sSpriteSheets_BetInterface =
{
    .data = gDerby_Digits_2,
    .size = 0x280,
    .tag = GFX_BET_DIGITS
};

static const struct SpriteTemplate sSpriteTemplate_BetDigit =
{
    .tileTag = GFX_BET_DIGITS,
    .paletteTag = PAL_DIGITS,
    .oam = &sOam_CreditDigit,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct SpriteSheet sSpriteSheets_PotentialInterface =
{
    .data = gDerby_Digits_2,
    .size = 0x280,
    .tag = GFX_POTENTIAL_DIGITS
};

static const struct SpriteTemplate sSpriteTemplate_PotentialDigit =
{
    .tileTag = GFX_POTENTIAL_DIGITS,
    .paletteTag = PAL_DIGITS,
    .oam = &sOam_CreditDigit,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy
};

static const struct SpriteSheet sSpriteSheet_P10 =
{
    .data = gDerby_P10,
    .size = 0x200,
    .tag = GFX_P10,
};

static const struct OamData sOamData_P10 =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x16),
    .size = SPRITE_SIZE(32x16),
    .tileNum = 0,
    .priority = 0,
};

static const union AnimCmd sP10AnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_FRAME(8, 20),
    ANIMCMD_JUMP(0)         // Loop back to the first frame (Frame 0)
};

static const union AnimCmd *const sP10AnimCmds[] = {
    sP10AnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_P10 =
{
    .tileTag = GFX_P10,
    .paletteTag = PAL_DIGITS,
    .oam = &sOamData_P10,
    .anims = sP10AnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Credit_Icon =
{
    .data = gDerby_Credit_Icon,
    .size = 0x80,
    .tag = GFX_CREDIT_ICON,
};

static const struct SpriteSheet sSpriteSheet_Credit_Icon_Red =
{
    .data = gDerby_Credit_Red_Icon,
    .size = 0x80,
    .tag = GFX_CREDIT_ICON_RED,
};

static const struct OamData sOamData_Credit_Icon =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x8),
    .size = SPRITE_SIZE(32x8),
    .tileNum = 0,
    .priority = 0,
};

static const struct SpriteTemplate sSpriteTemplate_Credit_Icon =
{
    .tileTag = GFX_CREDIT_ICON,
    .paletteTag = PAL_CREDIT_ICON,
    .oam = &sOamData_Credit_Icon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_Credit_Icon_Red =
{
    .tileTag = GFX_CREDIT_ICON_RED,
    .paletteTag = PAL_CREDIT_ICON,
    .oam = &sOamData_Credit_Icon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Name_Ponyta =
{
    .data = gDerby_Name_Ponyta,
    .size = 0x400,
    .tag = GFX_NAME_PONYTA,
};

static const struct OamData sOamData_Menu_Data =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x32),
    .size = SPRITE_SIZE(64x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct OamData sOamData_MenuShort_Data =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(32x32),
    .size = SPRITE_SIZE(32x32),
    .tileNum = 0,
    .priority = 0,
};

static const struct SpriteTemplate sSpriteTemplate_Name_Ponyta =
{
    .tileTag = GFX_NAME_PONYTA,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Name_Rapidash =
{
    .data = gDerby_Name_Rapidash,
    .size = 0x400,
    .tag = GFX_NAME_RAPIDASH,
};

static const struct SpriteTemplate sSpriteTemplate_Name_Rapidash =
{
    .tileTag = GFX_NAME_RAPIDASH,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Name_Rattata =
{
    .data = gDerby_Name_Rattata,
    .size = 0x400,
    .tag = GFX_NAME_RATTATA,
};

static const struct SpriteTemplate sSpriteTemplate_Name_Rattata =
{
    .tileTag = GFX_NAME_RATTATA,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Name_Feebas =
{
    .data = gDerby_Name_Feebas,
    .size = 0x400,
    .tag = GFX_NAME_FEEBAS,
};

static const struct SpriteTemplate sSpriteTemplate_Name_Feebas =
{
    .tileTag = GFX_NAME_FEEBAS,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Payout_1 =
{
    .data = gDerby_Payout_1,
    .size = 0x200,
    .tag = GFX_PAYOUT_1,
};

static const struct SpriteTemplate sSpriteTemplate_Payout_1 =
{
    .tileTag = GFX_PAYOUT_1,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_MenuShort_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Payout_2 =
{
    .data = gDerby_Payout_2,
    .size = 0x200,
    .tag = GFX_PAYOUT_2,
};

static const struct SpriteTemplate sSpriteTemplate_Payout_2 =
{
    .tileTag = GFX_PAYOUT_2,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_MenuShort_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Payout_3 =
{
    .data = gDerby_Payout_3,
    .size = 0x200,
    .tag = GFX_PAYOUT_3,
};

static const struct SpriteTemplate sSpriteTemplate_Payout_3 =
{
    .tileTag = GFX_PAYOUT_3,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_MenuShort_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Payout_4 =
{
    .data = gDerby_Payout_4,
    .size = 0x200,
    .tag = GFX_PAYOUT_4,
};

static const struct SpriteTemplate sSpriteTemplate_Payout_4 =
{
    .tileTag = GFX_PAYOUT_4,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_MenuShort_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Payout_5 =
{
    .data = gDerby_Payout_5,
    .size = 0x200,
    .tag = GFX_PAYOUT_5,
};

static const struct SpriteTemplate sSpriteTemplate_Payout_5 =
{
    .tileTag = GFX_PAYOUT_5,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_MenuShort_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Payout_6 =
{
    .data = gDerby_Payout_6,
    .size = 0x200,
    .tag = GFX_PAYOUT_6,
};

static const struct SpriteTemplate sSpriteTemplate_Payout_6 =
{
    .tileTag = GFX_PAYOUT_6,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_MenuShort_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Payout_7 =
{
    .data = gDerby_Payout_7,
    .size = 0x400,
    .tag = GFX_PAYOUT_7,
};

static const struct SpriteTemplate sSpriteTemplate_Payout_7 =
{
    .tileTag = GFX_PAYOUT_7,
    .paletteTag = PAL_BET_MENU_TEXT,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_UI_1 =
{
    .data = gDerby_UI_1,
    .size = 0x400,
    .tag = GFX_UI_1,
};

static const struct SpriteTemplate sSpriteTemplate_UI_1 =
{
    .tileTag = GFX_UI_1,
    .paletteTag = PAL_UI,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_UI_2 =
{
    .data = gDerby_UI_2,
    .size = 0x400,
    .tag = GFX_UI_2,
};

static const struct SpriteTemplate sSpriteTemplate_UI_2 =
{
    .tileTag = GFX_UI_2,
    .paletteTag = PAL_UI,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_UI_3 =
{
    .data = gDerby_UI_3,
    .size = 0x400,
    .tag = GFX_UI_3,
};

static const struct SpriteTemplate sSpriteTemplate_UI_3 =
{
    .tileTag = GFX_UI_3,
    .paletteTag = PAL_UI,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_UI_4 =
{
    .data = gDerby_UI_4,
    .size = 0x400,
    .tag = GFX_UI_4,
};

static const struct SpriteTemplate sSpriteTemplate_UI_4 =
{
    .tileTag = GFX_UI_4,
    .paletteTag = PAL_UI,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_UI_5 =
{
    .data = gDerby_UI_5,
    .size = 0x400,
    .tag = GFX_UI_5,
};

static const struct SpriteTemplate sSpriteTemplate_UI_5 =
{
    .tileTag = GFX_UI_5,
    .paletteTag = PAL_UI,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_UI_6 =
{
    .data = gDerby_UI_6,
    .size = 0x400,
    .tag = GFX_UI_6,
};

static const struct SpriteTemplate sSpriteTemplate_UI_6 =
{
    .tileTag = GFX_UI_6,
    .paletteTag = PAL_UI,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Arrow_1 =
{
    .data = gDerby_Arrow_1,
    .size = 0x100,
    .tag = GFX_ARROW_1,
};

static const struct OamData sOamData_Arrows =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(16x16),
    .size = SPRITE_SIZE(16x16),
    .tileNum = 0,
    .priority = 0,
};

static const union AnimCmd sArrowAnimCmd_0[] = 
{
    ANIMCMD_FRAME(0, 20),
    ANIMCMD_FRAME(4, 20),
    ANIMCMD_JUMP(0)         // Loop back to the first frame (Frame 0)
};

static const union AnimCmd *const sArrowAnimCmds[] = {
    sArrowAnimCmd_0,  // Looping animation
};

static const struct SpriteTemplate sSpriteTemplate_Arrow_1 =
{
    .tileTag = GFX_ARROW_1,
    .paletteTag = PAL_UI,
    .oam = &sOamData_Arrows,
    .anims = sArrowAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Arrow_2 =
{
    .data = gDerby_Arrow_2,
    .size = 0x100,
    .tag = GFX_ARROW_2,
};

static const struct SpriteTemplate sSpriteTemplate_Arrow_2 =
{
    .tileTag = GFX_ARROW_2,
    .paletteTag = PAL_UI,
    .oam = &sOamData_Arrows,
    .anims = sArrowAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Cursor =
{
    .data = gDerby_Cursor,
    .size = 0x100,
    .tag = GFX_CURSOR,
};

static const struct SpriteTemplate sSpriteTemplate_Cursor =
{
    .tileTag = GFX_CURSOR,
    .paletteTag = PAL_CURSOR,
    .oam = &sOamData_Arrows,
    .anims = sArrowAnimCmds,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Condition_1 =
{
    .data = gDerby_Condition_1,
    .size = 0x400,
    .tag = GFX_CONDITION_1,
};

static const struct SpriteTemplate sSpriteTemplate_Condition_1 =
{
    .tileTag = GFX_CONDITION_1,
    .paletteTag = PAL_CONDITION,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Condition_2 =
{
    .data = gDerby_Condition_2,
    .size = 0x400,
    .tag = GFX_CONDITION_2,
};

static const struct SpriteTemplate sSpriteTemplate_Condition_2 =
{
    .tileTag = GFX_CONDITION_2,
    .paletteTag = PAL_CONDITION,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Condition_3 =
{
    .data = gDerby_Condition_3,
    .size = 0x400,
    .tag = GFX_CONDITION_3,
};

static const struct SpriteTemplate sSpriteTemplate_Condition_3 =
{
    .tileTag = GFX_CONDITION_3,
    .paletteTag = PAL_CONDITION,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Condition_4 =
{
    .data = gDerby_Condition_4,
    .size = 0x400,
    .tag = GFX_CONDITION_4,
};

static const struct SpriteTemplate sSpriteTemplate_Condition_4 =
{
    .tileTag = GFX_CONDITION_4,
    .paletteTag = PAL_CONDITION,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Condition_5 =
{
    .data = gDerby_Condition_5,
    .size = 0x400,
    .tag = GFX_CONDITION_5,
};

static const struct SpriteTemplate sSpriteTemplate_Condition_5 =
{
    .tileTag = GFX_CONDITION_5,
    .paletteTag = PAL_CONDITION,
    .oam = &sOamData_Menu_Data,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Ponyta =
{
    .data = gDerby_PonytaGFX,
    .size = 0x800,
    .tag = GFX_PONYTA,
};

static const struct OamData sOamData_Pokemon =
{
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x64),
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
};

static const struct SpriteTemplate sSpriteTemplate_PonytaNormal =
{
    .tileTag = GFX_PONYTA,
    .paletteTag = PAL_PONYTA,
    .oam = &sOamData_Pokemon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_PonytaShiny =
{
    .tileTag = GFX_PONYTA,
    .paletteTag = PAL_PONYTA_SHINY,
    .oam = &sOamData_Pokemon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Rapidash =
{
    .data = gDerby_RapidashGFX,
    .size = 0x800,
    .tag = GFX_RAPIDASH,
};

static const struct SpriteTemplate sSpriteTemplate_RapidashNormal =
{
    .tileTag = GFX_RAPIDASH,
    .paletteTag = PAL_RAPIDASH,
    .oam = &sOamData_Pokemon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_RapidashShiny =
{
    .tileTag = GFX_RAPIDASH,
    .paletteTag = PAL_RAPIDASH_SHINY,
    .oam = &sOamData_Pokemon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Rattata =
{
    .data = gDerby_RattataGFX,
    .size = 0x800,
    .tag = GFX_RATTATA,
};

static const struct SpriteTemplate sSpriteTemplate_RattataNormal =
{
    .tileTag = GFX_RATTATA,
    .paletteTag = PAL_RATTATA,
    .oam = &sOamData_Pokemon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_RattataShiny =
{
    .tileTag = GFX_RATTATA,
    .paletteTag = PAL_RATTATA_SHINY,
    .oam = &sOamData_Pokemon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteSheet sSpriteSheet_Feebas =
{
    .data = gDerby_FeebasGFX,
    .size = 0x800,
    .tag = GFX_FEEBAS,
};

static const struct SpriteTemplate sSpriteTemplate_FeebasNormal =
{
    .tileTag = GFX_FEEBAS,
    .paletteTag = PAL_FEEBAS,
    .oam = &sOamData_Pokemon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

static const struct SpriteTemplate sSpriteTemplate_FeebasShiny =
{
    .tileTag = GFX_FEEBAS,
    .paletteTag = PAL_FEEBAS_SHINY,
    .oam = &sOamData_Pokemon,
    .anims = gDummySpriteAnimTable,
    .images = NULL,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCallbackDummy,
};

//Script entry: callasm StartDerby, then waitstate
void StartDerby(void)
{
    ScriptContext2_Enable();
    sDerby = AllocZeroed(sizeof(struct Derby));
    sDerbyBgBuffer = AllocZeroed(BG_SCREEN_SIZE);
    CreateTask(FadeToDerbyScreen, 0);
}

typedef struct  {
    int SpeciesId; // 0-4 Ponyta, 5 Rattata, 6 Rapidash, 7 Feebas
    u8 StartSpeed;
    u8 RaceSpeed;
} RaceSpeeds;

static const RaceSpeeds RaceSpeedsValues[] =
{
    {DERBY_SPECIES_PONYTA_1, 4, 5}, // Ponyta 1
    {DERBY_SPECIES_PONYTA_2, 3, 5}, // Ponyta 2
    {DERBY_SPECIES_PONYTA_3, 5, 4}, // Ponyta 3
    {DERBY_SPECIES_PONYTA_4, 5, 5}, // Ponyta 4
    {DERBY_SPECIES_PONYTA_5, 3, 6}, // Ponyta 5
    {DERBY_SPECIES_RATTATA, 5, 4}, // Rattata
    {DERBY_SPECIES_RAPIDASH, 4, 7}, // Rapidash
    {DERBY_SPECIES_FEEBAS, 0, 0}, // Feebas
};

#define MAX_NAMES 53

typedef struct  {
    u16 racerVar;
    u16 racerNicknameVar;
} DerbyRacerVars;

static const DerbyRacerVars sDerbyRacerVars[] =
{
    [DERBY_RACER_1] =
    {
        .racerVar = DERBY_VAR_RACER_1,
        .racerNicknameVar = DERBY_VAR_RACER_NAME_1,
    },
    [DERBY_RACER_2] =
    {
        .racerVar = DERBY_VAR_RACER_2,
        .racerNicknameVar = DERBY_VAR_RACER_NAME_2,
    },
    [DERBY_RACER_3] =
    {
        .racerVar = DERBY_VAR_RACER_3,
        .racerNicknameVar = DERBY_VAR_RACER_NAME_3,
    },
    [DERBY_RACER_4] =
    {
        .racerVar = DERBY_VAR_RACER_4,
        .racerNicknameVar = DERBY_VAR_RACER_NAME_4,
    },
    [DERBY_RACER_5] =
    {
        .racerVar = DERBY_VAR_RACER_5,
        .racerNicknameVar = DERBY_VAR_RACER_NAME_5,
    },
    [DERBY_RACER_6] =
    {
        .racerVar = DERBY_VAR_RACER_6,
        .racerNicknameVar = DERBY_VAR_RACER_NAME_6,
    },
};

static inline u32 GetDerbySpeciesSpeedPay(u32 species, u32 condition)
{
    switch (species)
    {
    case DERBY_SPECIES_FEEBAS:
        return BET_MULTIPLIER_JACKPOT; // Will never actually win lol
    case DERBY_SPECIES_RAPIDASH:
        if (condition >= CONDITION_4_STAR)
            return BET_MULTIPLIER_1_1;
        return BET_MULTIPLIER_1_2;
    case DERBY_SPECIES_RATTATA:
        if (condition == CONDITION_5_STAR)
            return BET_MULTIPLIER_1_3;
        else if ((condition == CONDITION_4_STAR) || (condition == CONDITION_3_STAR))
            return BET_MULTIPLIER_1_5;
        else if (condition == CONDITION_2_STAR)
            return BET_MULTIPLIER_2_0;
        return BET_MULTIPLIER_3_0;
    default:
        if (condition == CONDITION_5_STAR)
            return BET_MULTIPLIER_1_2;
        else if (condition == CONDITION_4_STAR)
            return BET_MULTIPLIER_1_3;
        else if ((condition == CONDITION_3_STAR) || (condition == CONDITION_2_STAR))
            return BET_MULTIPLIER_1_5;
        return BET_MULTIPLIER_2_0;
    }
}

static void GetRacerSpeeds(u8 racerSpecies, u8 racer)
{
    u8 sSpeed = 0;
    u8 rSpeed = 0;
    u8 condition;
    int i;

    //(The original loop only checked the first 6 entries, so Rapidash got no speed values. All 8 are checked here.)
    for (i = 0; i < (int)ARRAY_COUNT(RaceSpeedsValues); i++)
    {
        if (RaceSpeedsValues[i].SpeciesId == racerSpecies)
        {
            sSpeed = RaceSpeedsValues[i].StartSpeed;
            rSpeed = RaceSpeedsValues[i].RaceSpeed;
        }
    }

    //Speeds are fixed point (x256): speed * (1 + condition / 10)
    sDerby->racerSpeedStart[racer] = (sSpeed * (10 + sDerby->racerCondition[racer]) * 256) / 10;
    sDerby->racerSpeedRace[racer] = (rSpeed * (10 + sDerby->racerCondition[racer]) * 256) / 10;
    condition = sDerby->racerCondition[racer];
    sDerby->racerSpeedPay[racer] = GetDerbySpeciesSpeedPay(racerSpecies, condition);
}

static inline bool32 GetDerbyShinyChance(void)
{
    if ((Random() % 8192) == 777) // Shiny
        return TRUE;
    return FALSE;
}

static void GetNewDerby(void)
{
    u32 odds;
    u8 odd_one;

    int i;
    u8 conditions[DERBY_RACER_COUNT];
    u8 derbySpecies[DERBY_RACER_COUNT];
    u8 derbyShiny[DERBY_RACER_COUNT];
    u16 racerVar;
    int condition4Index;
    int condition3Index;
    int condition2Index;

    // Generate all derby species with random Ponyta by default
    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        derbySpecies[i] = (Random() % DERBY_SPECIES_PONYTA_COUNT); // Ponyta 0-4
    }

    odds = (Random() % 100); // Feebas: 0-99
    if (odds != 99) // No Feebas
    {
        odds = (Random() % 300); // Ponyta/Rapidash/Rattata 0-2

        if ((odds >= 100) && (odds < 200)) // Rattata
        {
            odd_one = (Random() % DERBY_RACER_COUNT); // Picks a random racer
            derbySpecies[odd_one] = DERBY_SPECIES_RATTATA;
        }
        else if (odds >= 200) // Rapidash
        {
            odd_one = (Random() % DERBY_RACER_COUNT); // Picks a random racer
            derbySpecies[odd_one] = DERBY_SPECIES_RAPIDASH;
        }
    }
    else // Feebas
    {
        odd_one = (Random() % DERBY_RACER_COUNT); // Picks a random racer
        derbySpecies[odd_one] = DERBY_SPECIES_FEEBAS;
    }

    // Generate all derby species with random Ponyta by default
    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        derbyShiny[i] = GetDerbyShinyChance();
    }

    // Generate all racers with low stars by default
    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        conditions[i] = Random() % CONDITION_3_STAR;
    }

    condition4Index = Random() % DERBY_RACER_COUNT;
    conditions[condition4Index] = CONDITION_5_STAR;

    do {
        condition3Index = Random() % DERBY_RACER_COUNT;
    } while (condition3Index == condition4Index);  
    conditions[condition3Index] = CONDITION_4_STAR;

    do {
        condition2Index = Random() % DERBY_RACER_COUNT;
    } while (condition2Index == condition4Index || condition2Index == condition3Index);  
    conditions[condition2Index] = CONDITION_3_STAR;

    // Generate all racers with low stars by default
    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        racerVar = sDerbyRacerVars[i].racerVar;
        VarSet(racerVar, (derbySpecies[i] * 100) + (derbyShiny[i] * 10) + conditions[i]);
    }

    DerbyFlagSet(DERBY_FLAG_RESET);
}

static void InitiateRacers(void)
{
    u32 number;
    u32 i;
    u16 racerVar;

    // Generate all racers with low stars by default
    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        racerVar = sDerbyRacerVars[i].racerVar;
        number = VarGet(racerVar);
        sDerby->racerSpecies[i] = number / 100;           // Hundreds place
        sDerby->racerShiny[i] = (number / 10) % 10;     // Tens place
        sDerby->racerCondition[i] = number % 10;            // Ones place
    }
}

static inline u32 CreatePonytaSprite(u32 shiny, u32 x, u32 y)
{
    if (shiny)
        return CreateSprite(&sSpriteTemplate_PonytaShiny, x, y, 0);
    return CreateSprite(&sSpriteTemplate_PonytaNormal, x, y, 0);
}

static inline u32 CreateRattataSprite(u32 shiny, u32 x, u32 y)
{
    if (shiny)
        return CreateSprite(&sSpriteTemplate_RattataShiny, x, y, 0);
    return CreateSprite(&sSpriteTemplate_RattataNormal, x, y, 0);
}

static inline u32 CreateRapidashSprite(u32 shiny, u32 x, u32 y)
{
    if (shiny)
        return CreateSprite(&sSpriteTemplate_RapidashShiny, x, y, 0);
    return CreateSprite(&sSpriteTemplate_RapidashNormal, x, y, 0);
}

static inline u32 CreateFeebasSprite(u32 shiny, u32 x, u32 y)
{
    if (shiny)
        return CreateSprite(&sSpriteTemplate_FeebasShiny, x, y, 0);
    return CreateSprite(&sSpriteTemplate_FeebasNormal, x, y, 0);
}

static inline u32 CreateDerbyMonSprite(u32 species, u32 shiny, u32 x)
{
    switch (species)
    {
    default:
        return CreatePonytaSprite(shiny, x, 96);
    case DERBY_SPECIES_RATTATA:
        return CreateRattataSprite(shiny, x, 100);
    case DERBY_SPECIES_RAPIDASH:
        return CreateRapidashSprite(shiny, x, 88);
    case DERBY_SPECIES_FEEBAS:
        return CreateFeebasSprite(shiny, x, 96);
    }
}

static void CreatePokemonSprites(void)
{
    s16 x;
    u32 i;

    x = 64;

    LoadSpriteSheet(&sSpriteSheet_Ponyta);
    LoadSpriteSheet(&sSpriteSheet_Rattata);
    LoadSpriteSheet(&sSpriteSheet_Rapidash);
    LoadSpriteSheet(&sSpriteSheet_Feebas);

    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        sDerby->racerSpeciesSpriteId[i] = CreateDerbyMonSprite(sDerby->racerSpecies[i], sDerby->racerShiny[i], x);
        gSprites[sDerby->racerSpeciesSpriteId[i]].invisible = FALSE;
    }
}

static void FadeToDerbyScreen(u8 taskId)
{
    switch (gTasks[taskId].data[0])
    {
    case 0:
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].data[0]++;
        break;
    case 1:
        if (!gPaletteFade->active)
        {
            SetMainCallback2(InitDerbyScreen);
            DestroyTask(taskId);
        }
        break;
    }
}

static void UpdatePokemonSprites(void)
{
    u32 i;
    // Generate all racers with low stars by default
    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        gSprites[sDerby->racerSpeciesSpriteId[i]].invisible = TRUE;
    }

    gSprites[sDerby->racerSpeciesSpriteId[sDerby->MenuPosition]].invisible = FALSE;
}

static void CreateMenuData(void)
{
    LoadSpriteSheet(&sSpriteSheet_Name_Ponyta);
    LoadSpriteSheet(&sSpriteSheet_Name_Rattata);
    LoadSpriteSheet(&sSpriteSheet_Name_Rapidash);
    LoadSpriteSheet(&sSpriteSheet_Name_Feebas);
    sDerby->SpeciesNameSpriteId = CreateSprite(&sSpriteTemplate_Name_Ponyta, 164, 60, 0);

    LoadSpriteSheet(&sSpriteSheet_Condition_1);
    LoadSpriteSheet(&sSpriteSheet_Condition_2);
    LoadSpriteSheet(&sSpriteSheet_Condition_3);
    LoadSpriteSheet(&sSpriteSheet_Condition_4);
    LoadSpriteSheet(&sSpriteSheet_Condition_5);
    sDerby->ConditionSpriteId = CreateSprite(&sSpriteTemplate_Condition_1, 164, 93, 0);

    LoadSpriteSheet(&sSpriteSheet_Payout_1);
    LoadSpriteSheet(&sSpriteSheet_Payout_2);
    LoadSpriteSheet(&sSpriteSheet_Payout_3);
    LoadSpriteSheet(&sSpriteSheet_Payout_4);
    LoadSpriteSheet(&sSpriteSheet_Payout_5);
    LoadSpriteSheet(&sSpriteSheet_Payout_6);
    LoadSpriteSheet(&sSpriteSheet_Payout_7);
    sDerby->PayoutSpriteId = CreateSprite(&sSpriteTemplate_Payout_1, 164, 124, 0);

    LoadSpriteSheet(&sSpriteSheet_UI_1);
    LoadSpriteSheet(&sSpriteSheet_UI_2);
    LoadSpriteSheet(&sSpriteSheet_UI_3);
    LoadSpriteSheet(&sSpriteSheet_UI_4);
    LoadSpriteSheet(&sSpriteSheet_UI_5);
    LoadSpriteSheet(&sSpriteSheet_UI_6);
    sDerby->uiSpriteId = CreateSprite(&sSpriteTemplate_UI_1, 64, 64, 0);
}

static void UpdatePokemonSpeciesNames(void)
{
    u8 species = 0;

    species = sDerby->racerSpecies[sDerby->MenuPosition];

    DestroySprite(&gSprites[sDerby->SpeciesNameSpriteId]);
    LoadSpritePalettes(sSpritePalettes);

    switch (species)
    {
    default:
        //LoadSpriteSheet(&sSpriteSheet_Name_Ponyta);
        sDerby->SpeciesNameSpriteId = CreateSprite(&sSpriteTemplate_Name_Ponyta, 164, 60, 0);
        break;
    case DERBY_SPECIES_RATTATA:
        //LoadSpriteSheet(&sSpriteSheet_Name_Rattata);
        sDerby->SpeciesNameSpriteId = CreateSprite(&sSpriteTemplate_Name_Rattata, 164, 60, 0);
        break;
    case DERBY_SPECIES_RAPIDASH:
        //LoadSpriteSheet(&sSpriteSheet_Name_Rapidash);
        sDerby->SpeciesNameSpriteId = CreateSprite(&sSpriteTemplate_Name_Rapidash, 164, 60, 0);
        break;
    case DERBY_SPECIES_FEEBAS:
        //LoadSpriteSheet(&sSpriteSheet_Name_Feebas);
        sDerby->SpeciesNameSpriteId = CreateSprite(&sSpriteTemplate_Name_Feebas, 164, 60, 0);
        break;
    }
}

static void UpdateCondition(void)
{
    u8 stars = 0;
    
    stars = sDerby->racerCondition[sDerby->MenuPosition];

    DestroySprite(&gSprites[sDerby->ConditionSpriteId]);
    LoadSpritePalettes(sSpritePalettes);

    switch (stars)
    {
    case CONDITION_1_STAR:
        //LoadSpriteSheet(&sSpriteSheet_Condition_1);
        sDerby->ConditionSpriteId = CreateSprite(&sSpriteTemplate_Condition_1, 164, 93, 0);
        break;
    case CONDITION_2_STAR:
        //LoadSpriteSheet(&sSpriteSheet_Condition_2);
        sDerby->ConditionSpriteId = CreateSprite(&sSpriteTemplate_Condition_2, 164, 93, 0);
        break;
    case CONDITION_3_STAR:
        //LoadSpriteSheet(&sSpriteSheet_Condition_3);
        sDerby->ConditionSpriteId = CreateSprite(&sSpriteTemplate_Condition_3, 164, 93, 0);
        break;
    case CONDITION_4_STAR:
        //LoadSpriteSheet(&sSpriteSheet_Condition_4);
        sDerby->ConditionSpriteId = CreateSprite(&sSpriteTemplate_Condition_4, 164, 93, 0);
        break;
    case CONDITION_5_STAR:
        //LoadSpriteSheet(&sSpriteSheet_Condition_5);
        sDerby->ConditionSpriteId = CreateSprite(&sSpriteTemplate_Condition_5, 164, 93, 0);
        break;
    }
}

static void UpdatePayout(void)
{
    u8 multi = 0;
    s16 y;
    
    y = 124;
    
    multi = sDerby->racerSpeedPay[sDerby->MenuPosition];
    
    DestroySprite(&gSprites[sDerby->PayoutSpriteId]);
    LoadSpritePalettes(sSpritePalettes);

    switch (multi)
    {
    case BET_MULTIPLIER_1_1:
        //LoadSpriteSheet(&sSpriteSheet_Payout_1);
        sDerby->PayoutSpriteId = CreateSprite(&sSpriteTemplate_Payout_1, 164, y, 0);
        break;
    case BET_MULTIPLIER_1_2:
        //LoadSpriteSheet(&sSpriteSheet_Payout_2);
        sDerby->PayoutSpriteId = CreateSprite(&sSpriteTemplate_Payout_2, 164, y, 0);
        break;
    case BET_MULTIPLIER_1_3:
        //LoadSpriteSheet(&sSpriteSheet_Payout_3);
        sDerby->PayoutSpriteId = CreateSprite(&sSpriteTemplate_Payout_3, 164, y, 0);
        break;
    case BET_MULTIPLIER_1_5:
        //LoadSpriteSheet(&sSpriteSheet_Payout_4);
        sDerby->PayoutSpriteId = CreateSprite(&sSpriteTemplate_Payout_4, 164, y, 0);
        break;
    case BET_MULTIPLIER_2_0:
        //LoadSpriteSheet(&sSpriteSheet_Payout_5);
        sDerby->PayoutSpriteId = CreateSprite(&sSpriteTemplate_Payout_5, 164, y, 0);
        break;
    case BET_MULTIPLIER_3_0:
        //LoadSpriteSheet(&sSpriteSheet_Payout_6);
        sDerby->PayoutSpriteId = CreateSprite(&sSpriteTemplate_Payout_6, 164, y, 0);
        break;
    case BET_MULTIPLIER_JACKPOT:
        //LoadSpriteSheet(&sSpriteSheet_Payout_7);
        sDerby->PayoutSpriteId = CreateSprite(&sSpriteTemplate_Payout_7, 164, y, 0);
        break;
    }
}

static void UpdateUI(void)
{
    DestroySprite(&gSprites[sDerby->uiSpriteId]);
    LoadSpritePalettes(sSpritePalettes);
    
    if (sDerby->MenuPosition == 0)
    {
        //LoadSpriteSheet(&sSpriteSheet_UI_1);
        sDerby->uiSpriteId = CreateSprite(&sSpriteTemplate_UI_1, 64, 64, 0);
    }
    else if (sDerby->MenuPosition == 1)
    {
        //LoadSpriteSheet(&sSpriteSheet_UI_2);
        sDerby->uiSpriteId = CreateSprite(&sSpriteTemplate_UI_2, 64, 64, 0);
    }
    else if (sDerby->MenuPosition == 2)
    {
        //LoadSpriteSheet(&sSpriteSheet_UI_3);
        sDerby->uiSpriteId = CreateSprite(&sSpriteTemplate_UI_3, 64, 64, 0);
    }
    else if (sDerby->MenuPosition == 3)
    {
        //LoadSpriteSheet(&sSpriteSheet_UI_4);
        sDerby->uiSpriteId = CreateSprite(&sSpriteTemplate_UI_4, 64, 64, 0);
    }
    else if (sDerby->MenuPosition == 4)
    {
        //LoadSpriteSheet(&sSpriteSheet_UI_5);
        sDerby->uiSpriteId = CreateSprite(&sSpriteTemplate_UI_5, 64, 64, 0);
    }
    else if (sDerby->MenuPosition == 5)
    {
        //LoadSpriteSheet(&sSpriteSheet_UI_6);
        sDerby->uiSpriteId = CreateSprite(&sSpriteTemplate_UI_6, 64, 64, 0);
    }
}

static void CreateP10(void)
{
    LoadSpritePalettes(sSpritePalettes);
    LoadSpriteSheet(&sSpriteSheet_P10);
    sDerby->P10SpriteId = CreateSprite(&sSpriteTemplate_P10, 172, 80, 0);
}

static void CreateCreditIcon(void)
{
    LoadSpritePalettes(sSpritePalettes);
    LoadSpriteSheet(&sSpriteSheet_Credit_Icon);
    sDerby->CreditIconSpriteId[0] = CreateSprite(&sSpriteTemplate_Credit_Icon, 28, 126, 0);
}

static void CreateCreditIconRed(void)
{
    LoadSpritePalettes(sSpritePalettes);
    LoadSpriteSheet(&sSpriteSheet_Credit_Icon_Red);
    sDerby->CreditIconSpriteId[1] = CreateSprite(&sSpriteTemplate_Credit_Icon_Red, 28, 126, 0);
}

static void CreateArrows(void)
{
    s16 x;
    s16 y;
    
    x = 32;
    y = 88;

    LoadSpriteSheet(&sSpriteSheet_Arrow_1);
    sDerby->Arrow1SpriteId = CreateSprite(&sSpriteTemplate_Arrow_1, x, y, 0);

    LoadSpriteSheet(&sSpriteSheet_Arrow_2);
    sDerby->Arrow2SpriteId = CreateSprite(&sSpriteTemplate_Arrow_2, x + 64, y, 0);
}

static void ShowName(const u8 *str)
{
    const u8 color[3] = { 0, 1, 2 };

    u8 buf[2];
    u8 n = 0;
    u8 i;
    u8 step = 10;
    u8 x;

    while (n < 15 && str[n] != 0xFF)
        n++;

    //枠(約76px)に収まるように1文字ずつ位置を決めて、中央寄せで表示する
    if (n * 10 > 76)
        step = 76 / n;
    x = (80 - (step * (n - 1) + 8)) / 2 + 1;

    FillWindowPixelBuffer(DERBY_MENUS, PIXEL_FILL(0));
    for (i = 0; i < n; i++)
    {
        buf[0] = str[i];
        buf[1] = 0xFF;
        AddTextPrinterParameterized3(DERBY_MENUS, FONT_DERBY, x + step * i, 2, color, 0, buf);
    }
    PutWindowTilemap(DERBY_MENUS);
    CopyWindowToVram(DERBY_MENUS, 3);
}

static void UpdateNicknames(void)
{
    u32 racerSpecies;
    u16 index;

    racerSpecies = sDerby->racerSpecies[sDerby->MenuPosition];
    index = VarGet(sDerbyRacerVars[sDerby->MenuPosition].racerNicknameVar);
    if (index >= MAX_NAMES)
        index = 0;

    switch (racerSpecies)
    {
    default:
        ShowName(gDerby_Names_Ponyta[index]);
        break;
    case DERBY_SPECIES_RATTATA:
        ShowName(gDerby_Names_Rattata[index]);
        break;
    case DERBY_SPECIES_RAPIDASH:
        ShowName(gDerby_Names_Rapidash[index]);
        break;
    case DERBY_SPECIES_FEEBAS:
        ShowName(gDerby_Names_Feebas[index]);
        break;
    }
}

static u8 GetUniquePonytaName(u8 currentSpecies, u8 previousNames[])
{
    u8 newName;

    // Only apply uniqueness check for Ponyta (species < 5)
    if (currentSpecies < DERBY_SPECIES_PONYTA_COUNT)
    {
        do {
            newName = Random() % MAX_NAMES;  // Generate a random nickname index
        } while (previousNames[newName] != 0);  // Check if this name has already been taken
        
        // Mark this name as taken
        previousNames[newName] = 1;
    }
    else
    {
        // For non-Ponyta species, no need to check for uniqueness
        newName = Random() % MAX_NAMES;
    }
    
    return newName;
}

static void SetNicknames(void)
{
    u8 previousNames[MAX_NAMES];  // Array to track taken nicknames (0 = available, 1 = taken)
    u8 names[DERBY_RACER_COUNT];
    u32 i;
    u16 racerNicknameVar;

    for (i = 0; i < MAX_NAMES; i++)
        previousNames[i] = 0;

    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        names[i] = GetUniquePonytaName(sDerby->racerSpecies[i], previousNames);
    }

    // Generate all racers with low stars by default
    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        racerNicknameVar = sDerbyRacerVars[i].racerNicknameVar;
        VarSet(racerNicknameVar, names[i]);
    }

    DerbyFlagSet(DERBY_FLAG_NICKNAME);
}

static void UpdatePokemonData(void)
{
    UpdatePokemonSprites();
    UpdatePokemonSpeciesNames();
    UpdateCondition();
    UpdatePayout();
    UpdateUI();
    if (!DerbyFlagGet(DERBY_FLAG_NICKNAME))
        SetNicknames();
    UpdateNicknames();
    
}

static void SetBetDigits(u16 num)
{
    u8 i;
    u16 d = 1000;  // Start with the thousands place

    for (i = 0; i < 4; i++)  // Always show 4 digits
    {
        u8 digit = num / d;

        // Show the digit (all digits are visible)
        //gSprites[sDerby->BetSpriteIds[i + SPR_BET_DIGITS]].invisible = TRUE;

        // If it's a smaller number, show 0 for the higher place values
        if (i == 0 && num < 1000) {
            digit = 0;  // Force 0 for the thousands place if the number is less than 1000
        }

        // Set the tileNum based on the current digit
        gSprites[sDerby->BetSpriteIds[i + SPR_BET_DIGITS]].oam.tileNum =
            gSprites[sDerby->BetSpriteIds[i + SPR_BET_DIGITS]].sheetTileStart + (digit * 2);

        gSprites[sDerby->BetSpriteIds[i + SPR_BET_DIGITS]].oam.priority = 0;

        // Reduce num for the next digit
        num = num % d;
        d = d / 10;
    }
    
    BuildOamBuffer();
}

static void CreateBetSprites(void)
{
    u8 i;

    LoadSpriteSheet(&sSpriteSheets_BetInterface);

    for (i = 0; i < 4; i++)
    {
        sDerby->BetSpriteIds[i + SPR_BET_DIGITS] = CreateSprite(&sSpriteTemplate_BetDigit, i * 5 + 148, 103, 2);
        gSprites[sDerby->BetSpriteIds[i + SPR_BET_DIGITS]].oam.priority = 0;
        gSprites[sDerby->BetSpriteIds[i + SPR_BET_DIGITS]].invisible = TRUE;
    }
}

static void SetPotentialDigits(u16 num)
{
    u8 i;
    u16 d = 1000;  // Start with the thousands place

    for (i = 0; i < 4; i++)  // Always show 4 digits
    {
        u8 digit = num / d;

        // Show the digit (all digits are visible)
        //gSprites[sDerby->PotentialSpriteIds[i + SPR_POTENTIAL_DIGITS]].invisible = TRUE;

        // If it's a smaller number, show 0 for the higher place values
        if (i == 0 && num < 1000) {
            digit = 0;  // Force 0 for the thousands place if the number is less than 1000
        }

        // Set the tileNum based on the current digit
        gSprites[sDerby->PotentialSpriteIds[i + SPR_POTENTIAL_DIGITS]].oam.tileNum =
            gSprites[sDerby->PotentialSpriteIds[i + SPR_POTENTIAL_DIGITS]].sheetTileStart + (digit * 2);

        gSprites[sDerby->PotentialSpriteIds[i + SPR_POTENTIAL_DIGITS]].oam.priority = 0;

        // Reduce num for the next digit
        num = num % d;
        d = d / 10;
    }
    
    BuildOamBuffer();
}

static void CreatePotentialSprites(void)
{
    u8 i;

    LoadSpriteSheet(&sSpriteSheets_PotentialInterface);

    for (i = 0; i < 4; i++)
    {
        sDerby->PotentialSpriteIds[i + SPR_POTENTIAL_DIGITS] = CreateSprite(&sSpriteTemplate_PotentialDigit, i * 6 + 186, 125, 2);
        gSprites[sDerby->PotentialSpriteIds[i + SPR_POTENTIAL_DIGITS]].oam.priority = 0;
        gSprites[sDerby->PotentialSpriteIds[i + SPR_POTENTIAL_DIGITS]].invisible = TRUE;
    }
}

static void SetCreditDigits(u32 numRaw)
{
    u16 num = (numRaw > 9999) ? 9999 : numRaw; //display only: the digit sprites hold 4 digits
    u8 i;
    u16 d = 1000;  // Start with the thousands place

    for (i = 0; i < 4; i++)  // Always show 4 digits
    {
        u8 digit = num / d;

        // Show the digit (all digits are visible)
        gSprites[sDerby->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].invisible = FALSE;

        // If it's a smaller number, show 0 for the higher place values
        if (i == 0 && num < 1000) {
            digit = 0;  // Force 0 for the thousands place if the number is less than 1000
        }

        // Set the tileNum based on the current digit
        gSprites[sDerby->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].oam.tileNum =
            gSprites[sDerby->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].sheetTileStart + (digit * 2);

        gSprites[sDerby->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].oam.priority = 0;

        // Reduce num for the next digit
        num = num % d;
        d = d / 10;
    }
    
    BuildOamBuffer();
}

static void CreateCreditSprites(void)
{
    u8 i;

    for (i = 0; i < ARRAY_COUNT(sSpriteSheets_CreditsInterface) - 1; i++)  
    {
        LoadSpriteSheet(&sSpriteSheets_CreditsInterface[i]);
    }

    for (i = 0; i < 4; i++)
    {
        sDerby->CreditSpriteIds[i + SPR_CREDIT_DIGITS] = CreateSprite(&sSpriteTemplate_CreditDigit, i * 8 + 50, 136, 2);
        gSprites[sDerby->CreditSpriteIds[i + SPR_CREDIT_DIGITS]].oam.priority = 0;
    }
}

static void DerbyVBlankCallback(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void DerbyMainCallback(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    UpdatePaletteFade();
}

static void StartExitDerby(void)
{
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
    sDerby->state = DERBY_STATE_EXIT;
}

static void ExitDerby(u8 taskId)
{
    if (!gPaletteFade->active)
    {
        FreeAllWindowBuffers();
        FREE_AND_SET_NULL(sDerby);
        FREE_AND_SET_NULL(sDerbyBgBuffer);
        DestroyTask(taskId);
        RestoreMapMusicAfterGame();
        SetMainCallback2(CB2_ReturnToFieldContinueScript);
    }
}

static void ShiftData(u8 direction)
{
    // 1 = Right, 0 = Left
    
    if (direction == 1) // Right
    {
        PlaySE(SE_SELECT);
        if (sDerby->MenuPosition >= DERBY_RACER_6)
        {
            sDerby->MenuPosition = DERBY_RACER_1;
        }
        else
        {
            sDerby->MenuPosition++;
        }
    }
    else if (direction == 0) // Left
    {
        PlaySE(SE_SELECT);
        if (sDerby->MenuPosition <= DERBY_RACER_1)
        {
            sDerby->MenuPosition = DERBY_RACER_6;
        }
        else
        {
            sDerby->MenuPosition--;
        }
    }
    UpdatePokemonData();
}

static void HandleInput(void)
{
    if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        sDerby->state = DERBY_STATE_BET_2_START;
    }
    else if (JOY_NEW(B_BUTTON))
    {
        if (sDerby->exitToggle == 0) {
        PlaySE(SE_SELECT);
        sDerby->state = DERBY_STATE_START_EXIT;
        }
    }
    else if (JOY_NEW(DPAD_RIGHT))
    {
        ShiftData(1);
    }
    else if (JOY_NEW(DPAD_LEFT))
    {
        ShiftData(0);
    }
}

static void UpdateBetBG(void)
{
    if (sDerby->Menu == 0)
    {
        //InitBgsFromTemplates(0, sDerbyBGtemplates, ARRAY_COUNT(sDerbyBGtemplates));
        //SetBgTilemapBuffer(DERBY_BG_BASE, sDerbyBgBuffer);
        LoadBgTiles(DERBY_BG_BASE, gDerby_BG_Bet2_Img, 6784, 0);
        CopyToBgTilemapBuffer(DERBY_BG_BASE, gDerby_BG_Bet2_Tilemap, 1280, 0);
        ResetPaletteFade();
        LoadPalette(gDerby_BG_Bet2_Pal, 0, 32); // Load the second palette
        CopyBgTilemapBufferToVram(DERBY_BG_BASE);
        return;
    }
    else if (sDerby->Menu == 1)
    {
        //InitBgsFromTemplates(0, sDerbyBGtemplates, ARRAY_COUNT(sDerbyBGtemplates));
        //SetBgTilemapBuffer(DERBY_BG_BASE, sDerbyBgBuffer);
        LoadBgTiles(DERBY_BG_BASE, gDerby_BG_Bet_Img, 6784, 0);
        CopyToBgTilemapBuffer(DERBY_BG_BASE, gDerby_BG_Bet_Tilemap, 1280, 0);
        ResetPaletteFade();
        LoadPalette(gDerby_BG_Bet_Pal, 0, 32); // Load the second palette
        CopyBgTilemapBufferToVram(DERBY_BG_BASE);
        return;
    }
}

static void HideBetSprites(void)
{
    if (sDerby->Menu == 0)
    {
        gSprites[sDerby->CreditIconSpriteId[0]].invisible = TRUE;
        gSprites[sDerby->P10SpriteId].invisible = FALSE;
        gSprites[sDerby->CreditIconSpriteId[1]].invisible = FALSE;
        
        gSprites[sDerby->SpeciesNameSpriteId].invisible = TRUE;
        gSprites[sDerby->ConditionSpriteId].invisible = TRUE;
        gSprites[sDerby->PayoutSpriteId].invisible = TRUE;
        gSprites[sDerby->Arrow1SpriteId].invisible = TRUE;
        gSprites[sDerby->Arrow2SpriteId].invisible = TRUE;
        gSprites[sDerby->uiSpriteId].invisible = TRUE;
        
        gSprites[sDerby->PotentialSpriteIds[0]].invisible = FALSE;
        gSprites[sDerby->PotentialSpriteIds[1]].invisible = FALSE;
        gSprites[sDerby->PotentialSpriteIds[2]].invisible = FALSE;
        gSprites[sDerby->PotentialSpriteIds[3]].invisible = FALSE;
        gSprites[sDerby->BetSpriteIds[0]].invisible = FALSE;
        gSprites[sDerby->BetSpriteIds[1]].invisible = FALSE;
        gSprites[sDerby->BetSpriteIds[2]].invisible = FALSE;
        gSprites[sDerby->BetSpriteIds[3]].invisible = FALSE;
    }
    else if (sDerby->Menu == 1)
    {
        gSprites[sDerby->CreditIconSpriteId[0]].invisible = FALSE;
        gSprites[sDerby->P10SpriteId].invisible = TRUE;
        gSprites[sDerby->CreditIconSpriteId[1]].invisible = TRUE;
        
        gSprites[sDerby->PotentialSpriteIds[0]].invisible = TRUE;
        gSprites[sDerby->PotentialSpriteIds[1]].invisible = TRUE;
        gSprites[sDerby->PotentialSpriteIds[2]].invisible = TRUE;
        gSprites[sDerby->PotentialSpriteIds[3]].invisible = TRUE;
        gSprites[sDerby->BetSpriteIds[0]].invisible = TRUE;
        gSprites[sDerby->BetSpriteIds[1]].invisible = TRUE;
        gSprites[sDerby->BetSpriteIds[2]].invisible = TRUE;
        gSprites[sDerby->BetSpriteIds[3]].invisible = TRUE;
        
        gSprites[sDerby->SpeciesNameSpriteId].invisible = FALSE;
        gSprites[sDerby->ConditionSpriteId].invisible = FALSE;
        gSprites[sDerby->PayoutSpriteId].invisible = FALSE;
        gSprites[sDerby->Arrow1SpriteId].invisible = FALSE;
        gSprites[sDerby->Arrow2SpriteId].invisible = FALSE;
        gSprites[sDerby->uiSpriteId].invisible = FALSE;
    }
}

static void AddBet(void)
{
    u8 PayoutId;
    
    PayoutId = sDerby->racerSpeedPay[sDerby->MenuPosition];
    
    if ((sDerby->Bet + 10) <= GetCoins())
    {
        PlaySE(SE_BALL_TRAY_BALL);
        sDerby->Bet = (sDerby->Bet + 10);
        SetBetDigits(sDerby->Bet);

        switch (PayoutId)
        {
        case BET_MULTIPLIER_1_1:
            sDerby->PotentialWin = ((sDerby->Bet * 11) / 10);
            break;
        case BET_MULTIPLIER_1_2:
            sDerby->PotentialWin = ((sDerby->Bet * 12) / 10);
            break;
        case BET_MULTIPLIER_1_3:
            sDerby->PotentialWin = ((sDerby->Bet * 13) / 10);
            break;
        case BET_MULTIPLIER_1_5:
            sDerby->PotentialWin = ((sDerby->Bet * 15) / 10);
            break;
        case BET_MULTIPLIER_2_0:
            sDerby->PotentialWin = (sDerby->Bet * 2);
            break;
        case BET_MULTIPLIER_3_0:
            sDerby->PotentialWin = (sDerby->Bet * 3);
            break;
        case BET_MULTIPLIER_JACKPOT:
            sDerby->PotentialWin = 9999;
            break;
        }

        if (sDerby->PotentialWin > 9999)
        {
            sDerby->PotentialWin = 9999;
        }
        SetPotentialDigits(sDerby->PotentialWin);
    }
    else
    {
        PlaySE(SE_FAILURE);
    }
}

static void SubBet(void)
{
    u8 PayoutId = 0;
    
    PayoutId = sDerby->racerSpeedPay[sDerby->MenuPosition];
    
    if (sDerby->Bet > 0)
    {
        PlaySE(SE_BALL_TRAY_BALL);
        sDerby->Bet = (sDerby->Bet - 10);
        SetBetDigits(sDerby->Bet);

        switch (PayoutId)
        {
        case BET_MULTIPLIER_1_1:
            sDerby->PotentialWin = ((sDerby->Bet * 11) / 10);
            break;
        case BET_MULTIPLIER_1_2:
            sDerby->PotentialWin = ((sDerby->Bet * 12) / 10);
            break;
        case BET_MULTIPLIER_1_3:
            sDerby->PotentialWin = ((sDerby->Bet * 13) / 10);
            break;
        case BET_MULTIPLIER_1_5:
            sDerby->PotentialWin = ((sDerby->Bet * 15) / 10);
            break;
        case BET_MULTIPLIER_2_0:
            sDerby->PotentialWin = (sDerby->Bet * 2);
            break;
        case BET_MULTIPLIER_3_0:
            sDerby->PotentialWin = (sDerby->Bet * 3);
            break;
        case BET_MULTIPLIER_JACKPOT:
            if (sDerby->Bet > 0)
                sDerby->PotentialWin = 9999;
            else
                sDerby->PotentialWin = 0;
            break;
        }

        SetPotentialDigits(sDerby->PotentialWin);
    }
    else
    {
        PlaySE(SE_FAILURE);
    }    
}

static void AdjustByRandom(void)
{
    int i;
    u8 chance;
    s32 adjustment;

    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        chance = Random() % 55;
        if (chance < 50)
        {
            adjustment = ((Random() % 251) * 256) / 100; //0.00 - 2.50
            if (Random() % 2 == 0)
                sDerby->racerSpeedRace[i] += adjustment;
            else
                sDerby->racerSpeedRace[i] -= adjustment;
        }
    }

    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        chance = Random() % 65;
        if (chance < 50)
        {
            adjustment = ((Random() % 51) * 256) / 100; //0.00 - 0.50
            if (Random() % 2 == 0)
                sDerby->racerSpeedStart[i] += adjustment;
            else
                sDerby->racerSpeedStart[i] -= adjustment;
        }
    }
}

static void HandleInput2(void)
{
    if (JOY_NEW(A_BUTTON))
    {
        if (sDerby->Bet > 0)
        {
            PlaySE(SE_SHOP);
            DestroySpriteAndFreeResources(&gSprites[sDerby->P10SpriteId]);
            PlayBGM(MUS_NONE);
            SetCoins((GetCoins() - sDerby->Bet));
            SetCreditDigits(GetCoins());
            sDerby->delay = 30;
            AdjustByRandom();
            sDerby->state = DERBY_STATE_RACE_DELAY;
        }
        else
        {
            PlaySE(SE_FAILURE);
        }
    }
    else if (JOY_NEW(B_BUTTON))
    {
        if (sDerby->exitToggle == 0) {
        PlaySE(SE_SELECT);
        sDerby->Bet = 0;
        sDerby->PotentialWin = 0;
        SetBetDigits(0);
        SetPotentialDigits(0);
        HideBetSprites();
        UpdateBetBG();
        sDerby->Menu = 0;
        sDerby->state = DERBY_STATE_PROCESS_INPUT;
        }
    }
    else if (JOY_NEW(DPAD_UP))
    {
        AddBet();
    }
    else if (JOY_NEW(DPAD_DOWN))
    {
        SubBet();
    }
}

static inline u32 CreatePonytaOWSprite(u32 shiny, u32 x, u32 y, u32 position)
{
    if (shiny)
        return CreateSprite(&sSpriteTemplate_Ponyta_OW_Shiny, x, y, position);
    return CreateSprite(&sSpriteTemplate_Ponyta_OW_Normal, x, y, position);
}

static inline u32 CreateRattataOWSprite(u32 shiny, u32 x, u32 y, u32 position)
{
    if (shiny)
        return CreateSprite(&sSpriteTemplate_Rattata_OW_Shiny, x, y, position);
    return CreateSprite(&sSpriteTemplate_Rattata_OW_Normal, x, y, position);
}

static inline u32 CreateRapidashOWSprite(u32 shiny, u32 x, u32 y, u32 position)
{
    if (shiny)
        return CreateSprite(&sSpriteTemplate_Rapidash_OW_Shiny, x, y, position);
    return CreateSprite(&sSpriteTemplate_Rapidash_OW_Normal, x, y, position);
}

static inline u32 CreateFeebasOWSprite(u32 shiny, u32 x, u32 y, u32 position)
{
    if (shiny)
        return CreateSprite(&sSpriteTemplate_Feebas_OW_Shiny, x, y, position);
    return CreateSprite(&sSpriteTemplate_Feebas_OW_Normal, x, y, position);
}

static inline u32 CreateDerbyMonOWSprite(u32 species, u32 shiny, u32 x, u32 yAdd, u32 position)
{
    u32 speciesY = 48;
    switch (species)
    {
    default:
        return CreatePonytaOWSprite(shiny, x, speciesY + yAdd, position);
    case DERBY_SPECIES_RATTATA:
        speciesY = 56;
        return CreateRattataOWSprite(shiny, x, speciesY + yAdd, position);
    case DERBY_SPECIES_RAPIDASH:
        return CreateRapidashOWSprite(shiny, x, speciesY + yAdd, position);
    case DERBY_SPECIES_FEEBAS:
        speciesY = 56;
        return CreateFeebasOWSprite(shiny, x, speciesY + yAdd, position);
    }
}

static void CreateRacers(void)
{
    u8 position = 11;
    u32 yAdd = 0;

    LoadSpritePalettes(sSpritePalettes);

    LoadSpriteSheet(&sSpriteSheet_Ponyta_OW);
    LoadSpriteSheet(&sSpriteSheet_Rattata_OW);
    LoadSpriteSheet(&sSpriteSheet_Rapidash_OW);
    LoadSpriteSheet(&sSpriteSheet_Feebas_OW);

    sDerby->RacerSprite1 = CreateDerbyMonOWSprite(sDerby->racerSpecies[DERBY_RACER_1], sDerby->racerShiny[DERBY_RACER_1], 224, yAdd, position);

    yAdd = yAdd + 16;
    position = position - 2;
    sDerby->RacerSprite2 = CreateDerbyMonOWSprite(sDerby->racerSpecies[DERBY_RACER_2], sDerby->racerShiny[DERBY_RACER_2], 224, yAdd, position);

    yAdd = yAdd + 16;
    position = position - 2;
    sDerby->RacerSprite3 = CreateDerbyMonOWSprite(sDerby->racerSpecies[DERBY_RACER_3], sDerby->racerShiny[DERBY_RACER_3], 224, yAdd, position);

    yAdd = yAdd + 16;
    position = position - 2;
    sDerby->RacerSprite4 = CreateDerbyMonOWSprite(sDerby->racerSpecies[DERBY_RACER_4], sDerby->racerShiny[DERBY_RACER_4], 224, yAdd, position);

    yAdd = yAdd + 16;
    position = position - 2;
    sDerby->RacerSprite5 = CreateDerbyMonOWSprite(sDerby->racerSpecies[DERBY_RACER_5], sDerby->racerShiny[DERBY_RACER_5], 224, yAdd, position);

    yAdd = yAdd + 16;
    position = position - 2;
    sDerby->RacerSprite6 = CreateDerbyMonOWSprite(sDerby->racerSpecies[DERBY_RACER_6], sDerby->racerShiny[DERBY_RACER_6], 224, yAdd, position);
}

static void CreateCursor(void)
{
    u8 y = 0;
    u8 position = 0;
    
    LoadSpritePalettes(sSpritePalettes);
    
    if (sDerby->RacerSelected == 1)
    {
        position = 10;
        y = 40;
    }
    else if (sDerby->RacerSelected == 2)
    {
        position = 8;
        y = 40 + 16;
    }
    else if (sDerby->RacerSelected == 3)
    {
        position = 6;
        y = 40 + 16 + 16;
    }
    else if (sDerby->RacerSelected == 4)
    {
        position = 4;
        y = 40 + 16 + 16 + 16;
    }
    else if (sDerby->RacerSelected == 5)
    {
        position = 2;
        y = 40 + 16 + 16 + 16 + 16;
    }
    else if (sDerby->RacerSelected == 6)
    {
        position = 0;
        y = 40 + 16 + 16 + 16 + 16 + 16;
    }
    LoadSpriteSheet(&sSpriteSheet_Cursor);
            
    sDerby->CursorSpriteId = CreateSprite(&sSpriteTemplate_Cursor, 224, y, position);
}

static void InitRacetrack(void)
{
    ResetVramOamAndBgCntRegs();
    ResetBgsAndClearDma3BusyFlags(0);
    ResetTempTileDataBuffers();
    InitBgsFromTemplates(0, sDerbyBGtemplates, ARRAY_COUNT(sDerbyBGtemplates));
    SetBgTilemapBuffer(DERBY_BG_BASE, sDerbyBgBuffer);
    LoadBgTiles(DERBY_BG_BASE, gDerby_BG_Race_Img, 1024, 0);
    CopyToBgTilemapBuffer(DERBY_BG_BASE, gDerby_BG_Race_Tilemap, 1280, 0);
    ResetPaletteFade();
    LoadPalette(gDerby_BG_Race_Pal, 0, 32);
    ResetSpriteData();
    FreeAllSpritePalettes();
    LoadSpritePalettes(sSpritePalettes);
    
    CreateRacers();
    CreateCursor();
    
    CopyBgTilemapBufferToVram(DERBY_BG_BASE);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON | DISPCNT_BG2_ON);
    ShowBg(DERBY_BG_BASE);
    ShowBg(DERBY_MENUS);
}

static void AssignRacerPlacements(void)
{
    struct Racer {
        int xValue;
        int index;
    };
    
    struct Racer racers[6] = {
        {gSprites[sDerby->RacerSprite1].pos1.x, 1},
        {gSprites[sDerby->RacerSprite2].pos1.x, 2},
        {gSprites[sDerby->RacerSprite3].pos1.x, 3},
        {gSprites[sDerby->RacerSprite4].pos1.x, 4},
        {gSprites[sDerby->RacerSprite5].pos1.x, 5},
        {gSprites[sDerby->RacerSprite6].pos1.x, 6}
    };
    
    int i;
    int j;
    struct Racer temp;
    u8 places[DERBY_RACER_COUNT] = {0, 0, 0, 0, 0, 0};

    for (i = 0; i < 5; i++)
    {
        for (j = i + 1; j < 6; j++)
        {
            if (racers[i].xValue > racers[j].xValue)
            {
                temp = racers[i];
                racers[i] = racers[j];
                racers[j] = temp;
            }
        }
    }

    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        if (i > 0 && racers[i].xValue == racers[i - 1].xValue)
            places[racers[i].index - 1] = places[racers[i - 1].index - 1];
        else
            places[racers[i].index - 1] = i == 0 ? 2 : (i == 1 ? 1 : 0);
    }

    sDerby->place1 = places[0];  
    sDerby->place2 = places[1];  
    sDerby->place3 = places[2];  
    sDerby->place4 = places[3];  
    sDerby->place5 = places[4];  
    sDerby->place6 = places[5];  
}

static void DerbyMain(u8 taskId)
{
    switch (sDerby->state)
    {
    case DERBY_STATE_INIT:
        if (!gPaletteFade->active)
            sDerby->state = DERBY_STATE_PROCESS_INPUT;
        break;
    case DERBY_STATE_PROCESS_INPUT:
        HandleInput();
        break;
    case DERBY_STATE_BET_2_START:
        UpdateBetBG();
        HideBetSprites();
        sDerby->Menu = 1;
        sDerby->RacerSelected = sDerby->MenuPosition + 1;
        sDerby->state = DERBY_STATE_BET_2;
        break;
    case DERBY_STATE_BET_2:
        HandleInput2();
        break;
    case DERBY_STATE_RACE_DELAY:
        if (sDerby->delay != 0)
        {
            sDerby->delay--;
        }
        else
        {
            BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
            sDerby->state = DERBY_STATE_START_FADE;
        }
        break;
    case DERBY_STATE_START_FADE:
        if (!gPaletteFade->active)
        {
            PlayBGM(BGM_CYCLING);
            InitRacetrack();
            BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
            sDerby->state = DERBY_STATE_FADEIN;
        }
        break;
    case DERBY_STATE_FADEIN:    
        if (!gPaletteFade->active)
        {
            sDerby->delay = 30;
            sDerby->state = DERBY_STATE_DELAY_2;
        }
        break;
    case DERBY_STATE_DELAY_2:    
        if (sDerby->delay != 0)
        {
            sDerby->delay--;
        }
        else
        {
            sDerby->delay = 60;
            sDerby->Timer = 3;
            sDerby->Swap1 = 1;
            sDerby->Swap2 = 0;
            sDerby->CountdownX = 120;
            sDerby->CountdownY = 76;
            PlaySE(SE_CONTEST_PLACE); // 3

            LoadSpriteSheet(&sSpriteSheet_Countdown_3);
            sDerby->CountdownSpriteId = CreateSprite(&sSpriteTemplate_Countdown_3, sDerby->CountdownX, sDerby->CountdownY, 0);

            sDerby->state = DERBY_STATE_COUNTDOWN;
        }
        break;
    case DERBY_STATE_COUNTDOWN:
        if ((sDerby->Timer > 0) && (sDerby->Swap1 == 1) && (sDerby->Swap2 == 0))
        {
            if (sDerby->delay > 0)
            {
                sDerby->delay--;
            }
            else if (sDerby->delay == 0)
            {
                sDerby->Timer--; // 2
                sDerby->Swap2 = 1;
            }
        }
        else if ((sDerby->Timer > 0) && (sDerby->Swap1 > 0) && (sDerby->Swap2 > 0))
        {
            PlaySE(SE_CONTEST_PLACE); // 2

            DestroySpriteAndFreeResources(&gSprites[sDerby->CountdownSpriteId]);
            LoadSpritePalettes(sSpritePalettes);
            LoadSpriteSheet(&sSpriteSheet_Countdown_2);

            sDerby->CountdownSpriteId = CreateSprite(&sSpriteTemplate_Countdown_2, sDerby->CountdownX, sDerby->CountdownY, 0);

            sDerby->delay = 60;
            sDerby->Swap1 = 0;
            sDerby->Swap2 = 0;
        }
        else if ((sDerby->Timer > 0) && (sDerby->Swap1 == 0) && (sDerby->Swap2 == 0))
        {
            if (sDerby->delay > 0)
            {
                sDerby->delay--;
            }
            else if (sDerby->delay == 0)
            {
                sDerby->Timer--; // 1
                sDerby->Swap2 = 1;
            }
        }
        else if ((sDerby->Timer > 0) && (sDerby->Swap1 == 0) && (sDerby->Swap2 == 1))
        {
            PlaySE(SE_CONTEST_PLACE); // 1

            DestroySpriteAndFreeResources(&gSprites[sDerby->CountdownSpriteId]);
            LoadSpritePalettes(sSpritePalettes);
            LoadSpriteSheet(&sSpriteSheet_Countdown_1);

            sDerby->CountdownSpriteId = CreateSprite(&sSpriteTemplate_Countdown_1, sDerby->CountdownX, sDerby->CountdownY, 0);

            sDerby->delay = 60;
            sDerby->Swap1 = 2;
            sDerby->Swap2 = 0;
        }
        else if ((sDerby->Timer > 0) && (sDerby->Swap1 == 2) && (sDerby->Swap2 == 0))
        {
            if (sDerby->delay > 0)
            {
                sDerby->delay--;
            }
            else if (sDerby->delay == 0)
            {
                sDerby->Timer--; // 1
                sDerby->Swap2 = 3;
            }
        }
        else if (sDerby->Timer == 0)
        {
            if ((sDerby->Swap2 == 3) && (sDerby->delay == 0))
            {
                PlaySE(SE_EXP_MAX);
                
                DestroySpriteAndFreeResources(&gSprites[sDerby->CountdownSpriteId]);
                LoadSpritePalettes(sSpritePalettes);
                LoadSpriteSheet(&sSpriteSheet_Go);
                
                sDerby->CountdownSpriteId = CreateSprite(&sSpriteTemplate_Go, sDerby->CountdownX, sDerby->CountdownY, 0);
                
                sDerby->Swap2 = 4;
                sDerby->delay = 60;
                
            }
            else if (sDerby->delay > 0)
            {
                sDerby->delay--;
            }
            else if (sDerby->delay == 0)
            {
                DestroySpriteAndFreeResources(&gSprites[sDerby->CountdownSpriteId]);
                if (sDerby->racerSpecies[DERBY_RACER_1] != DERBY_SPECIES_FEEBAS)
                {
                    gSprites[sDerby->RacerSprite1].animNum = 1;
                }
                if (sDerby->racerSpecies[DERBY_RACER_2] != DERBY_SPECIES_FEEBAS)
                {
                    gSprites[sDerby->RacerSprite2].animNum = 1;
                }
                if (sDerby->racerSpecies[DERBY_RACER_3] != DERBY_SPECIES_FEEBAS)
                {
                    gSprites[sDerby->RacerSprite3].animNum = 1;
                }
                if (sDerby->racerSpecies[DERBY_RACER_4] != DERBY_SPECIES_FEEBAS)
                {
                    gSprites[sDerby->RacerSprite4].animNum = 1;
                }
                if (sDerby->racerSpecies[DERBY_RACER_5] != DERBY_SPECIES_FEEBAS)
                {
                    gSprites[sDerby->RacerSprite5].animNum = 1;
                }
                if (sDerby->racerSpecies[DERBY_RACER_6] != DERBY_SPECIES_FEEBAS)
                {
                    gSprites[sDerby->RacerSprite6].animNum = 1;
                }
                sDerby->frames = 3;
                sDerby->delay = 15;
                sDerby->state = DERBY_STATE_RACE_START;
            }
        }
        break;
    case DERBY_STATE_RACE_START:
        if (sDerby->frames == 0)
        {
            if (sDerby->racerSpecies[DERBY_RACER_1] != DERBY_SPECIES_FEEBAS)
            {
                gSprites[sDerby->RacerSprite1].pos1.x = gSprites[sDerby->RacerSprite1].pos1.x - DerbyStep(sDerby->racerSpeedStart[DERBY_RACER_1]);
            }
            if (sDerby->racerSpecies[DERBY_RACER_2] != DERBY_SPECIES_FEEBAS)
            {
                gSprites[sDerby->RacerSprite2].pos1.x = gSprites[sDerby->RacerSprite2].pos1.x - DerbyStep(sDerby->racerSpeedStart[DERBY_RACER_2]);
            }
            if (sDerby->racerSpecies[DERBY_RACER_3] != DERBY_SPECIES_FEEBAS)
            {
                gSprites[sDerby->RacerSprite3].pos1.x = gSprites[sDerby->RacerSprite3].pos1.x - DerbyStep(sDerby->racerSpeedStart[DERBY_RACER_3]);
            }
            if (sDerby->racerSpecies[DERBY_RACER_4] != DERBY_SPECIES_FEEBAS)
            {
                gSprites[sDerby->RacerSprite4].pos1.x = gSprites[sDerby->RacerSprite4].pos1.x - DerbyStep(sDerby->racerSpeedStart[DERBY_RACER_4]);
            }
            if (sDerby->racerSpecies[DERBY_RACER_5] != DERBY_SPECIES_FEEBAS)
            {
                gSprites[sDerby->RacerSprite5].pos1.x = gSprites[sDerby->RacerSprite5].pos1.x - DerbyStep(sDerby->racerSpeedStart[DERBY_RACER_5]);
            }
            if (sDerby->racerSpecies[DERBY_RACER_6] != DERBY_SPECIES_FEEBAS)
            {
                gSprites[sDerby->RacerSprite6].pos1.x = gSprites[sDerby->RacerSprite6].pos1.x - DerbyStep(sDerby->racerSpeedStart[DERBY_RACER_6]);
            }
            
            if (sDerby->RacerSelected == 1)
            {
                gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite1].pos1.x;
            }
            else if (sDerby->RacerSelected == 2)
            {
                gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite2].pos1.x;
            }
            else if (sDerby->RacerSelected == 3)
            {
                gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite3].pos1.x;
            }
            else if (sDerby->RacerSelected == 4)
            {
                gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite4].pos1.x;
            }
            else if (sDerby->RacerSelected == 5)
            {
                gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite5].pos1.x;
            }
            else if (sDerby->RacerSelected == 6)
            {
                gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite6].pos1.x;
            }
            
            sDerby->frames = 3;
            sDerby->delay--;
        }
        else if (sDerby->delay == 0)
        {
            sDerby->state = DERBY_STATE_RACE_MID;
        }
        sDerby->frames--;
        break;
    case DERBY_STATE_RACE_MID:
        if ((gSprites[sDerby->RacerSprite1].pos1.x > 16) &&
            (gSprites[sDerby->RacerSprite2].pos1.x > 16) &&
            (gSprites[sDerby->RacerSprite3].pos1.x > 16) &&
            (gSprites[sDerby->RacerSprite4].pos1.x > 16) &&
            (gSprites[sDerby->RacerSprite5].pos1.x > 16) &&
            (gSprites[sDerby->RacerSprite6].pos1.x > 16))
            {
                if (sDerby->frames == 0)
                {
                    if (sDerby->racerSpecies[DERBY_RACER_1] != DERBY_SPECIES_FEEBAS)
                    {
                        gSprites[sDerby->RacerSprite1].pos1.x = gSprites[sDerby->RacerSprite1].pos1.x - DerbyStep(sDerby->racerSpeedRace[DERBY_RACER_1]);
                    }
                    if (sDerby->racerSpecies[DERBY_RACER_2] != DERBY_SPECIES_FEEBAS)
                    {
                        gSprites[sDerby->RacerSprite2].pos1.x = gSprites[sDerby->RacerSprite2].pos1.x - DerbyStep(sDerby->racerSpeedRace[DERBY_RACER_2]);
                    }
                    if (sDerby->racerSpecies[DERBY_RACER_3] != DERBY_SPECIES_FEEBAS)
                    {
                        gSprites[sDerby->RacerSprite3].pos1.x = gSprites[sDerby->RacerSprite3].pos1.x - DerbyStep(sDerby->racerSpeedRace[DERBY_RACER_3]);
                    }
                    if (sDerby->racerSpecies[DERBY_RACER_4] != DERBY_SPECIES_FEEBAS)
                    {
                        gSprites[sDerby->RacerSprite4].pos1.x = gSprites[sDerby->RacerSprite4].pos1.x - DerbyStep(sDerby->racerSpeedRace[DERBY_RACER_4]);
                    }
                    if (sDerby->racerSpecies[DERBY_RACER_5] != DERBY_SPECIES_FEEBAS)
                    {
                        gSprites[sDerby->RacerSprite5].pos1.x = gSprites[sDerby->RacerSprite5].pos1.x - DerbyStep(sDerby->racerSpeedRace[DERBY_RACER_5]);
                    }
                    if (sDerby->racerSpecies[DERBY_RACER_6] != DERBY_SPECIES_FEEBAS)
                    {
                        gSprites[sDerby->RacerSprite6].pos1.x = gSprites[sDerby->RacerSprite6].pos1.x - DerbyStep(sDerby->racerSpeedRace[DERBY_RACER_6]);
                    }
                    
                    if (sDerby->RacerSelected == 1)
                    {
                        gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite1].pos1.x;
                    }
                    else if (sDerby->RacerSelected == 2)
                    {
                        gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite2].pos1.x;
                    }
                    else if (sDerby->RacerSelected == 3)
                    {
                        gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite3].pos1.x;
                    }
                    else if (sDerby->RacerSelected == 4)
                    {
                        gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite4].pos1.x;
                    }
                    else if (sDerby->RacerSelected == 5)
                    {
                        gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite5].pos1.x;
                    }
                    else if (sDerby->RacerSelected == 6)
                    {
                        gSprites[sDerby->CursorSpriteId].pos1.x = gSprites[sDerby->RacerSprite6].pos1.x;
                    }
                    
                    sDerby->frames = 3;
                }
                sDerby->frames--;
            }
            else
            {
                PlayBGM(MUS_NONE);
                AssignRacerPlacements();
                if (sDerby->RacerSelected == 1)
                {
                    if (sDerby->place1 == 2) // Win
                        VarSet(GAME_CORNER_VAR_WINNINGS, sDerby->PotentialWin);
                    else
                        VarSet(GAME_CORNER_VAR_WINNINGS, 0);
                }
                else if (sDerby->RacerSelected == 2)
                {
                    if (sDerby->place2 == 2) // Win
                        VarSet(GAME_CORNER_VAR_WINNINGS, sDerby->PotentialWin);
                    else
                        VarSet(GAME_CORNER_VAR_WINNINGS, 0);
                }
                else if (sDerby->RacerSelected == 3)
                {
                    if (sDerby->place3 == 2) // Win
                        VarSet(GAME_CORNER_VAR_WINNINGS, sDerby->PotentialWin);
                    else
                        VarSet(GAME_CORNER_VAR_WINNINGS, 0);
                }
                else if (sDerby->RacerSelected == 4)
                {
                    if (sDerby->place4 == 2) // Win
                        VarSet(GAME_CORNER_VAR_WINNINGS, sDerby->PotentialWin);
                    else
                        VarSet(GAME_CORNER_VAR_WINNINGS, 0);
                }
                else if (sDerby->RacerSelected == 5)
                {
                    if (sDerby->place5 == 2) // Win
                        VarSet(GAME_CORNER_VAR_WINNINGS, sDerby->PotentialWin);
                    else
                        VarSet(GAME_CORNER_VAR_WINNINGS, 0);
                }
                else if (sDerby->RacerSelected == 6)
                {
                    if (sDerby->place6 == 2) // Win
                        VarSet(GAME_CORNER_VAR_WINNINGS, sDerby->PotentialWin);
                    else
                        VarSet(GAME_CORNER_VAR_WINNINGS, 0);
                }
                PlaySE(SE_APPLAUSE);
                
                LoadSpritePalettes(sSpritePalettes2);
                LoadSpriteSheet(&sSpriteSheet_Clefairy);
                
                sDerby->ClefSpriteId = CreateSprite(&sSpriteTemplate_Clefairy, sDerby->CountdownX, sDerby->CountdownY, 0);
                DerbyFlagClear(DERBY_FLAG_RESET);
                DerbyFlagClear(DERBY_FLAG_NICKNAME);
                sDerby->delay = 120;
                sDerby->state = DERBY_STATE_FINAL_DELAY;
            }
        break;
    case DERBY_STATE_FINAL_DELAY:
        if (sDerby->delay == 0)
        {
            sDerby->state = DERBY_STATE_START_EXIT;
        }
        else
        {
            sDerby->delay--;
        }
        break;
    case DERBY_STATE_START_EXIT:
        StartExitDerby();
        break;
    case DERBY_STATE_EXIT:
        ExitDerby(taskId);
        break;
    }
}

static void InitDerbyScreen(void)
{    
    int i;

    SetVBlankCallback(NULL);
    ResetVramOamAndBgCntRegs();
    ResetBgsAndClearDma3BusyFlags(0);
    ResetTempTileDataBuffers();
    InitBgsFromTemplates(0, sDerbyBGtemplates, ARRAY_COUNT(sDerbyBGtemplates));
    ChangeBgX(DERBY_BG_BASE, 0, 0);
    ChangeBgY(DERBY_BG_BASE, 0, 0);
    ChangeBgX(DERBY_MENUS, 0, 0);
    ChangeBgY(DERBY_MENUS, 0, 0);
    SetBgTilemapBuffer(DERBY_BG_BASE, sDerbyBgBuffer);
    InitWindows(sDerbyWinTemplates);
    DeactivateAllTextPrinters();
    LoadBgTiles(DERBY_BG_BASE, gDerby_BG_Bet_Img, 6784, 0);
    CopyToBgTilemapBuffer(DERBY_BG_BASE, gDerby_BG_Bet_Tilemap, 1280, 0);
    ResetPaletteFade();
    LoadPalette(gDerby_BG_Bet_Pal, 0, 32);
    LoadPalette(sDerbyTextPal, 15 * 16, 32);
    ResetSpriteData();
    FreeAllSpritePalettes();
    
    if (!DerbyFlagGet(DERBY_FLAG_RESET))
    {
        GetNewDerby();
    }
    InitiateRacers();
    for (i = DERBY_RACER_1; i < DERBY_RACER_COUNT; i++)
    {
        GetRacerSpeeds(sDerby->racerSpecies[i], i);
    }
    
    LoadSpritePalettes(sSpritePalettes);
    CreatePokemonSprites();
    CreateMenuData();
    CreateArrows();
    CreateCreditIcon();
    CreateCreditIconRed();
    CreateP10();
    gSprites[sDerby->P10SpriteId].invisible = TRUE;
    gSprites[sDerby->CreditIconSpriteId[1]].invisible = TRUE;
    CreateCreditSprites();
    SetCreditDigits(GetCoins());
    CreateBetSprites();
    SetBetDigits(0);
    CreatePotentialSprites();
    SetPotentialDigits(0);
    
    UpdatePokemonData();
    
    CopyBgTilemapBufferToVram(DERBY_BG_BASE);
    CopyBgTilemapBufferToVram(DERBY_MENUS);
    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_MODE_0 | DISPCNT_OBJ_1D_MAP | DISPCNT_OBJ_ON | DISPCNT_BG2_ON);
    ShowBg(DERBY_BG_BASE);
    ShowBg(DERBY_MENUS);
    BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
    SetVBlankCallback(DerbyVBlankCallback);
    PlayBGM(GAME_CORNER_MUSIC);
    SetMainCallback2(DerbyMainCallback);
    CreateTask(DerbyMain, 1);
}

