#include "defines.h"
#include "../include/menu.h"
#include "../include/sprite.h"
#include "../include/string_util.h"
#include "../include/pokemon_summary_screen.h"
#include "../include/constants/moves.h"
#include "../include/constants/pokemon.h"


static const struct TextColor sPSSTextColors =
{
	.bgColor = TEXT_COLOR_TRANSPARENT,
	.fgColor = TEXT_DYNAMIC_COLOR_5,
	.shadowColor = TEXT_DYNAMIC_COLOR_1,
};


#define TEXT_SKIP_DRAW 0xFF
enum {
    FONT_SMALL,
    FONT_NORMAL_COPY_1,
    FONT_NORMAL,
    FONT_NORMAL_COPY_2,
    FONT_MALE,
    FONT_FEMALE,
    FONT_BRAILLE,
    FONT_BOLD,
};

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
#define sIVRankingsPal sIVRankings_E_MinusPal

static const u8 * const sIVRankings_Images[] = 
{
    [0 ... 2] = sIVRankings_E_MinusTiles,
    [3 ... 4] = sIVRankings_ETiles,
    [5 ... 6] = sIVRankings_E_PlusTiles,
    [7 ... 8] = sIVRankings_D_MinusTiles,
    [9 ... 10] = sIVRankings_DTiles,
    [11 ... 12] = sIVRankings_D_PlusTiles,
    [13 ... 14] = sIVRankings_C_MinusTiles,
    [15 ... 16] = sIVRankings_CTiles,
    [17 ... 18] = sIVRankings_C_PlusTiles,
    [19 ... 20] = sIVRankings_B_MinusTiles,
    [21 ... 22] = sIVRankings_BTiles,
    [23 ... 24] = sIVRankings_B_PlusTiles,
    [25 ... 26] = sIVRankings_A_MinusTiles,
    [27 ... 28] = sIVRankings_ATiles,
    [29 ... 30] = sIVRankings_A_PlusTiles,
    [31] = sIVRankings_STiles,
};

static const u8 sNatureTextColors[][3] =
{
    {0, 14, 10},
    {0, 1, 10},
    {0, 7, 10},
};

static const u8 __attribute__((unused)) sLevelNickTextColors[][3] =
{
    {0, 14, 10},
    {0, 1, 2},
    {0, 9, 8},
    {0, 5, 4},
    {0, 2, 3},
    {0, 11, 10},
};

static const s8 sNatureStatTable[][5] =
{
    // Atk Def Spd Sp.Atk Sp.Def
    {    0,  0,  0,     0,     0}, // Hardy
    {   +1, -1,  0,     0,     0}, // Lonely
    {   +1,  0, -1,     0,     0}, // Brave
    {   +1,  0,  0,    -1,     0}, // Adamant
    {   +1,  0,  0,     0,    -1}, // Naughty
    {   -1, +1,  0,     0,     0}, // Bold
    {    0,  0,  0,     0,     0}, // Docile
    {    0, +1, -1,     0,     0}, // Relaxed
    {    0, +1,  0,    -1,     0}, // Impish
    {    0, +1,  0,     0,    -1}, // Lax
    {   -1,  0, +1,     0,     0}, // Timid
    {    0, -1, +1,     0,     0}, // Hasty
    {    0,  0,  0,     0,     0}, // Serious
    {    0,  0, +1,    -1,     0}, // Jolly
    {    0,  0, +1,     0,    -1}, // Naive
    {   -1,  0,  0,    +1,     0}, // Modest
    {    0, -1,  0,    +1,     0}, // Mild
    {    0,  0, -1,    +1,     0}, // Quiet
    {    0,  0,  0,     0,     0}, // Bashful
    {    0,  0,  0,    +1,    -1}, // Rash
    {   -1,  0,  0,     0,    +1}, // Calm
    {    0, -1,  0,     0,    +1}, // Gentle
    {    0,  0, -1,     0,    +1}, // Sassy
    {    0,  0,  0,    -1,    +1}, // Careful
    {    0,  0,  0,     0,     0}, // Quirky
};

static u8 GetNatureForStats(struct Pokemon *mon)
{
    if (mon->natureMint != 0)  // ƒ~ƒ“ƒg‚ªÝ’è‚³‚ê‚Ä‚¢‚é
        return mon->natureMint - 1;
    return GetNature(mon);     // ’Êí‚Ì«Ši
}

u8 GetNatureTextColor(s8 natureMod)
{
    if (natureMod == 0)
        return 0;
    else if (natureMod > 0)
        return 1;
    else
        return 2;
}

#define HP_TEXT_X_POS 6 //default:14
#define STATS_TEXT_POS 50
#define EXP_TEXT_X_POS1 15
#define EXP_TEXT_X_POS2 15

#define HP_TEXT_Y_POS 4
#define ATK_TEXT_Y_POS 22
#define DEF_TEXT_Y_POS 35
#define SPATK_TEXT_Y_POS 48
#define SPDEF_TEXT_Y_POS 61
#define SPD_TEXT_Y_POS 74
#define EXP1_TEXT_Y_POS 87
#define EXP2_TEXT_Y_POS 100

#define STATS_IV_POS_X 7
#define HP_IV_POS_Y 6
#define ATK_IV_POS_Y 24
#define DEF_IV_POS_Y 37
#define SPATK_IV_POS_Y 50
#define SPDEF_IV_POS_Y 63
#define SPD_IV_POS_Y 76

#define sMonSummaryScreen (*(struct PokemonSummaryScreenData **)0x0203B0B4)
#define sMonSkillsPrinterXpos (*(struct Struct203B0B8 **)0x0203B0B8)

struct Struct203B0B8
{
    u16 unk00;
    u16 curHpStr;
    u16 atkStr;
    u16 defStr;
    u16 spAStr;
    u16 spDStr;
    u16 speStr;
    u16 expStr;
    u16 toNextLevel;

    u16 curPp[5];
    u16 maxPp[5];

    u16 unk26;
};


void PokeSum_PrintMonTypeIcons(void)
{
    switch (sMonSummaryScreen->curPageIndex)
    {
    case PSS_PAGE_INFO:
        if (!sMonSummaryScreen->isEgg)
        {
            blit_move_info_icon(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sMonSummaryScreen->monTypes[0] + 1, 26, 30);

            if (sMonSummaryScreen->monTypes[0] != sMonSummaryScreen->monTypes[1])
                blit_move_info_icon(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sMonSummaryScreen->monTypes[1] + 1, 62, 30);
                
	#ifdef TERASTAL_FEATURE
            blit_move_info_icon(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sMonSummaryScreen->currentMon.teratype + 1, 63, 5);
	#endif
        }
        break;
    case PSS_PAGE_SKILLS:

        break;
    case PSS_PAGE_MOVES:
        break;
    case PSS_PAGE_MOVES_INFO:
        FillWindowPixelBuffer(sMonSummaryScreen->windowIds[6], 0);
        blit_move_info_icon(sMonSummaryScreen->windowIds[6], sMonSummaryScreen->monTypes[0] + 1, 0, 3);

        if (sMonSummaryScreen->monTypes[0] != sMonSummaryScreen->monTypes[1])
            blit_move_info_icon(sMonSummaryScreen->windowIds[6], sMonSummaryScreen->monTypes[1] + 1, 36, 3);

        PutWindowTilemap(sMonSummaryScreen->windowIds[6]);
        break;
    }
}

void PokeSum_PrintOTGender(void)
{
    u8 buff[10];

    buff[0] = 0xFC;
    buff[1] = 0x1;

    if (GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_OT_GENDER, NULL))
        buff[2] = 0xC;
    else
        buff[2] = 0x7;

    StringCopy(&buff[3], sMonSummaryScreen->summary.otNameStrBuf);
    WindowPrint(sMonSummaryScreen->windowIds[3], 2, 26, 43, &sPSSTextColors, 0xFF, buff);
}

void PrintSkillsPage(void)
{
		u8 nature = GetNatureForStats(&sMonSummaryScreen->currentMon);
		const s8 *natureMod = sNatureStatTable[nature];
		//const s8 *natureMod = sNatureStatTable[GetNature(&sMonSummaryScreen->currentMon)];
		
		u8 hpIv, atkIv, defIv, spAtkIv, spDefIv, spdIv;
		hpIv = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_HP_IV, NULL);
		atkIv = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_ATK_IV, NULL);
		defIv = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_DEF_IV, NULL);
		spAtkIv = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_SPATK_IV, NULL);
		spDefIv = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_SPDEF_IV, NULL);
		spdIv = GetMonData(&sMonSummaryScreen->currentMon, MON_DATA_SPEED_IV, NULL);
		
		AddTextPrinterParameterized3(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], FONT_NORMAL, HP_TEXT_X_POS + sMonSkillsPrinterXpos->curHpStr, HP_TEXT_Y_POS, sLevelNickTextColors[0], TEXT_SKIP_DRAW, sMonSummaryScreen->summary.curHpStrBuf);
		AddTextPrinterParameterized3(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], FONT_NORMAL, STATS_TEXT_POS + sMonSkillsPrinterXpos->atkStr, ATK_TEXT_Y_POS, sNatureTextColors[GetNatureTextColor(natureMod[STAT_ATK - 1])], TEXT_SKIP_DRAW, sMonSummaryScreen->summary.statValueStrBufs[PSS_STAT_ATK]);
		AddTextPrinterParameterized3(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], FONT_NORMAL, STATS_TEXT_POS + sMonSkillsPrinterXpos->defStr, DEF_TEXT_Y_POS, sNatureTextColors[GetNatureTextColor(natureMod[STAT_DEF - 1])], TEXT_SKIP_DRAW, sMonSummaryScreen->summary.statValueStrBufs[PSS_STAT_DEF]);
		AddTextPrinterParameterized3(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], FONT_NORMAL, STATS_TEXT_POS + sMonSkillsPrinterXpos->spAStr, SPATK_TEXT_Y_POS, sNatureTextColors[GetNatureTextColor(natureMod[STAT_SPATK - 1])], TEXT_SKIP_DRAW, sMonSummaryScreen->summary.statValueStrBufs[PSS_STAT_SPA]);
		AddTextPrinterParameterized3(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], FONT_NORMAL, STATS_TEXT_POS + sMonSkillsPrinterXpos->spDStr, SPDEF_TEXT_Y_POS, sNatureTextColors[GetNatureTextColor(natureMod[STAT_SPDEF - 1])], TEXT_SKIP_DRAW, sMonSummaryScreen->summary.statValueStrBufs[PSS_STAT_SPD]);
		AddTextPrinterParameterized3(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], FONT_NORMAL, STATS_TEXT_POS + sMonSkillsPrinterXpos->speStr, SPD_TEXT_Y_POS, sNatureTextColors[GetNatureTextColor(natureMod[STAT_SPEED - 1])], TEXT_SKIP_DRAW, sMonSummaryScreen->summary.statValueStrBufs[PSS_STAT_SPE]);
		AddTextPrinterParameterized3(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], FONT_NORMAL, EXP_TEXT_X_POS1 + sMonSkillsPrinterXpos->expStr, EXP1_TEXT_Y_POS, sLevelNickTextColors[0], TEXT_SKIP_DRAW, sMonSummaryScreen->summary.expPointsStrBuf);
		AddTextPrinterParameterized3(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], FONT_NORMAL, EXP_TEXT_X_POS2 + sMonSkillsPrinterXpos->toNextLevel, EXP2_TEXT_Y_POS, sLevelNickTextColors[0], TEXT_SKIP_DRAW, sMonSummaryScreen->summary.expToNextLevelStrBuf);

		
		BlitBitmapRectToWindow(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sIVRankings_Images[hpIv], 0, 0, 8, 8, STATS_IV_POS_X , HP_IV_POS_Y , 16, 8);
		BlitBitmapRectToWindow(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sIVRankings_Images[atkIv], 0, 0, 8, 8, STATS_IV_POS_X , ATK_IV_POS_Y , 16, 8);
		BlitBitmapRectToWindow(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sIVRankings_Images[defIv], 0, 0, 8, 8, STATS_IV_POS_X , DEF_IV_POS_Y , 16, 8);
		BlitBitmapRectToWindow(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sIVRankings_Images[spAtkIv], 0, 0, 8, 8, STATS_IV_POS_X , SPATK_IV_POS_Y , 16, 8);
		BlitBitmapRectToWindow(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sIVRankings_Images[spDefIv], 0, 0, 8, 8, STATS_IV_POS_X , SPDEF_IV_POS_Y , 16, 8);
		BlitBitmapRectToWindow(sMonSummaryScreen->windowIds[POKESUM_WIN_RIGHT_PANE], sIVRankings_Images[spdIv], 0, 0, 8, 8, STATS_IV_POS_X , SPD_IV_POS_Y , 16, 8);

}