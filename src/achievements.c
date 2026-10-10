#include "defines.h"
#include "../include/bg.h"
#include "../include/event_data.h"
#include "../include/field_weather.h"
#include "../include/gpu_regs.h"
#include "../include/international_string_util.h"
#include "../include/item.h"
#include "../include/list_menu.h"
#include "../include/main.h"
#include "../include/map_name_popup.h"
#include "../include/menu.h"
#include "../include/new_menu_helpers.h"
#include "../include/overworld.h"
#include "../include/palette.h"
#include "../include/pokedex.h"
#include "../include/pokemon.h"
#include "../include/scanline_effect.h"
#include "../include/script.h"
#include "../include/sound.h"
#include "../include/sprite.h"
#include "../include/start_menu.h"
#include "../include/string_util.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/text_window.h"
#include "../include/window.h"
#include "../include/constants/items.h"
#include "../include/constants/moves.h"
#include "../include/constants/songs.h"
#include "../include/constants/species.h"
#include "../include/gba/io_reg.h"
#include "../include/new/achievements.h"
#include "../include/new/Vanilla_functions.h"
#include "../include/new/item.h"
#include "../include/new/rom_locs.h"
#include "../include/new/util.h"

/*
achievements.c
	Achievements system (ported from SoulGold's achievement system)

	- The unlocked state is kept in expanded flags (ACH_FLAG_START...). The save structs are untouched.
	- Conditions are polled in the overworld (called from TryRunOnFrameMapScript, so no new ROM hook is needed).
	- Unlocks are announced by a SoulGold-style popup in the top right corner (TryCreatePopup), or by the
	  message box script EventScript_AchievementUnlocked when ACH_UNLOCK_MESSAGEBOX is defined.
	  Milestone rewards / gifts always use that script (assembly/overworld_scripts/achievements.s).
	- The list screen is opened from the "tools" side (right key) of the start menu.
	- See Achievements.md
*/

#include "achievements_data.h" //Generated: gAchievements[] (scripts/achievements_gen.py)
#include "achievements_gfx.h" //Generated (scripts/achievements_gfx.py)

extern const u8 gText_Ach_Slash[];

#define ACH_FLAG_NOTIFIED_START (ACH_FLAG_START + ACH_MAX_COUNT)
#define ACH_MANY_POPUPS_THRESHOLD 4 //When this many (or more) unlock at once, show a single combined message
#define KANTO_DEX_SIZE 151
#define NUM_BASE_TMS 50

#define PENDING_CHECK_INTERVAL 8 //Frames between checks for popups / rewards / gifts that are due

//Vanilla functions:
void __attribute__((long_call)) TransferPlttBuffer(void);

//Milestone rewards: each entry is given once when the unlock count is reached.
//The table is generated from scripts/achievements.json (edit it with achievements_editor.bat).
//Do not reorder existing entries: the position is the save flag index (ACH_FLAG_REWARD_START + index).
struct AchievementReward
{
	u8 requiredCount;
	u16 item;
	u8 amount;
};

#include "achievements_rewards.h" //Generated: sAchievementRewards[] (scripts/achievements_gen.py)

//The flag after the last reward is used by the Pikachu gift
#define ACH_MAX_REWARDS 32
#if NUM_ACH_REWARDS > ACH_MAX_REWARDS
	#error "Too many achievement rewards (max 32)"
#endif

//Next reward that is due (reached the required count and not received yet), whether or not it fits in the bag
static s8 GetNextRewardDue(void)
{
	u16 count = Achievements_CountUnlocked();

	for (u8 i = 0; i < NUM_ACH_REWARDS; ++i)
	{
		if (count >= sAchievementRewards[i].requiredCount && !FlagGet(ACH_FLAG_REWARD_START + i))
			return i;
	}

	return -1;
}

//Next reward that is due AND fits in the bag (if the bag is full it simply waits until there is room).
//Always -1 when rewards are handed out by an NPC (ACH_REWARDS_VIA_NPC).
static s8 GetNextReward(void)
{
	#ifdef ACH_REWARDS_VIA_NPC
	return -1;
	#else
	s8 i = GetNextRewardDue();

	if (i >= 0 && !CheckBagHasSpace(sAchievementRewards[i].item, sAchievementRewards[i].amount))
		return -1;

	return i;
	#endif
}

//Pokemon gift (test): Pikachu is given once, when "ACH_PARTNER" is unlocked. The Pokemon itself is given by
//the "givepokemon" command in assembly/overworld_scripts/achievements.s (change species/level there).
#define ACH_FLAG_GIFT_PIKACHU (ACH_FLAG_REWARD_START + ACH_MAX_REWARDS)

static bool8 IsGiftDue(void)
{
	return Achievements_IsUnlocked(ACH_PARTNER) && !FlagGet(ACH_FLAG_GIFT_PIKACHU);
}

//Given automatically by the unlock script. Always FALSE when an NPC hands out the rewards (ACH_REWARDS_VIA_NPC).
static bool8 IsAutoGiftDue(void)
{
	#ifdef ACH_REWARDS_VIA_NPC
	return FALSE;
	#else
	return IsGiftDue();
	#endif
}

//This file's functions:
static bool8 IsSpeciesCaught(u16 species);
static bool8 IsFuncConditionMet(u16 func);
static bool8 IsConditionMet(u16 id);
static u16 CountPending(void);
static u32 CountTMsInBag(void);

// ================================================= //
// ================== Core Logic =================== //
// ================================================= //

u16 Achievements_GetCount(void)
{
	return ACH_COUNT;
}

const struct Achievement* Achievements_Get(u16 id)
{
	if (id >= ACH_COUNT)
		return NULL;

	return &gAchievements[id];
}

bool8 Achievements_IsUnlocked(u16 id)
{
	return id < ACH_COUNT && FlagGet(ACH_FLAG_START + id);
}

static bool8 IsNotified(u16 id)
{
	return FlagGet(ACH_FLAG_NOTIFIED_START + id);
}

u16 Achievements_CountUnlocked(void)
{
	u16 count = 0;

	for (u16 i = 0; i < ACH_COUNT; ++i)
	{
		if (Achievements_IsUnlocked(i))
			++count;
	}

	return count;
}

static u16 CountPending(void)
{
	u16 count = 0;

	for (u16 i = 0; i < ACH_COUNT; ++i)
	{
		if (Achievements_IsUnlocked(i) && !IsNotified(i))
			++count;
	}

	return count;
}

bool8 Achievements_Unlock(u16 id)
{
	if (id >= ACH_COUNT || Achievements_IsUnlocked(id))
		return FALSE;

	FlagSet(ACH_FLAG_START + id);
	return TRUE;
}

static bool8 IsSpeciesCaught(u16 species)
{
	return GetSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_GET_CAUGHT) != 0;
}

static u32 CountTMsInBag(void)
{
	u32 count = 0;

	for (u16 item = ITEM_TM01; item < ITEM_TM01 + NUM_BASE_TMS; ++item)
	{
		if (CheckBagHasItem(item, 1))
			++count;
	}

	return count;
}

//Species sets for the "catch all of these" FUNC achievements
static const u16 sEeveelutions[] = {SPECIES_VAPOREON, SPECIES_JOLTEON, SPECIES_FLAREON, SPECIES_ESPEON,
                                    SPECIES_UMBREON, SPECIES_LEAFEON, SPECIES_GLACEON, SPECIES_SYLVEON};
static const u16 sWeatherTrio[] = {SPECIES_GROUDON, SPECIES_KYOGRE, SPECIES_RAYQUAZA};
static const u16 sRegiTrio[] = {SPECIES_REGIROCK, SPECIES_REGICE, SPECIES_REGISTEEL};
static const u16 sLakeTrio[] = {SPECIES_UXIE, SPECIES_MESPRIT, SPECIES_AZELF};
static const u16 sCreationTrio[] = {SPECIES_DIALGA, SPECIES_PALKIA, SPECIES_GIRATINA};
static const u16 sTaoTrio[] = {SPECIES_RESHIRAM, SPECIES_ZEKROM, SPECIES_KYUREM};
static const u16 sXYDuo[] = {SPECIES_XERNEAS, SPECIES_YVELTAL};
static const u16 sSunMoonDuo[] = {SPECIES_SOLGALEO, SPECIES_LUNALA};
static const u16 sHeroDuo[] = {SPECIES_ZACIAN, SPECIES_ZAMAZENTA};
static const u16 sPseudoLegends[] = {SPECIES_DRAGONITE, SPECIES_TYRANITAR, SPECIES_SALAMENCE, SPECIES_METAGROSS, SPECIES_GARCHOMP,
                                     SPECIES_HYDREIGON, SPECIES_GOODRA, SPECIES_KOMMO_O, SPECIES_DRAGAPULT};
static const u16 sStarters[] =
{
	SPECIES_BULBASAUR, SPECIES_CHARMANDER, SPECIES_SQUIRTLE, SPECIES_CHIKORITA, SPECIES_CYNDAQUIL, SPECIES_TOTODILE,
	SPECIES_TREECKO, SPECIES_TORCHIC, SPECIES_MUDKIP, SPECIES_TURTWIG, SPECIES_CHIMCHAR, SPECIES_PIPLUP,
	SPECIES_SNIVY, SPECIES_TEPIG, SPECIES_OSHAWOTT, SPECIES_CHESPIN, SPECIES_FENNEKIN, SPECIES_FROAKIE,
	SPECIES_ROWLET, SPECIES_LITTEN, SPECIES_POPPLIO, SPECIES_GROOKEY, SPECIES_SCORBUNNY, SPECIES_SOBBLE,
};
static const u16 sUltraBeasts[] = {SPECIES_NIHILEGO, SPECIES_BUZZWOLE, SPECIES_PHEROMOSA, SPECIES_XURKITREE, SPECIES_CELESTEELA,
                                   SPECIES_KARTANA, SPECIES_GUZZLORD, SPECIES_POIPOLE, SPECIES_NAGANADEL, SPECIES_STAKATAKA,
                                   SPECIES_BLACEPHALON};

struct AchSpeciesSet
{
	u16 func;
	const u16* species;
	u8 count;
};

static const struct AchSpeciesSet sAchSpeciesSets[] =
{
	{ACH_FUNC_EEVEELUTIONS,   sEeveelutions,  ARRAY_COUNT(sEeveelutions)},
	{ACH_FUNC_WEATHER_TRIO,   sWeatherTrio,   ARRAY_COUNT(sWeatherTrio)},
	{ACH_FUNC_REGI_TRIO,      sRegiTrio,      ARRAY_COUNT(sRegiTrio)},
	{ACH_FUNC_LAKE_TRIO,      sLakeTrio,      ARRAY_COUNT(sLakeTrio)},
	{ACH_FUNC_CREATION_TRIO,  sCreationTrio,  ARRAY_COUNT(sCreationTrio)},
	{ACH_FUNC_TAO_TRIO,       sTaoTrio,       ARRAY_COUNT(sTaoTrio)},
	{ACH_FUNC_XY_DUO,         sXYDuo,         ARRAY_COUNT(sXYDuo)},
	{ACH_FUNC_SUN_MOON_DUO,   sSunMoonDuo,    ARRAY_COUNT(sSunMoonDuo)},
	{ACH_FUNC_HERO_DUO,       sHeroDuo,       ARRAY_COUNT(sHeroDuo)},
	{ACH_FUNC_PSEUDO_LEGENDS, sPseudoLegends, ARRAY_COUNT(sPseudoLegends)},
	{ACH_FUNC_ALL_STARTERS,   sStarters,      ARRAY_COUNT(sStarters)},
	{ACH_FUNC_ULTRA_BEAST,    sUltraBeasts,   ARRAY_COUNT(sUltraBeasts)},
};

static const struct AchSpeciesSet* GetSpeciesSet(u16 func)
{
	for (u8 i = 0; i < ARRAY_COUNT(sAchSpeciesSets); ++i)
	{
		if (sAchSpeciesSets[i].func == func)
			return &sAchSpeciesSets[i];
	}

	return NULL;
}

static u8 CountSpeciesSetCaught(const struct AchSpeciesSet* set)
{
	u8 count = 0;

	for (u8 i = 0; i < set->count; ++i)
	{
		if (IsSpeciesCaught(set->species[i]))
			++count;
	}

	return count;
}

//National Dex ranges of the 8 regions (Kanto - Galar)
static const u16 sRegionDexEnd[] = {151, 251, 386, 493, 649, 721, 809, 898};

static bool8 IsAnyCaughtInDexRange(u16 first, u16 last)
{
	for (u16 i = first; i <= last; ++i)
	{
		if (GetSetPokedexFlag(i, FLAG_GET_CAUGHT))
			return TRUE;
	}

	return FALSE;
}

static u8 CountRegionsCaught(void)
{
	u8 count = 0;
	u16 first = 1;

	for (u8 i = 0; i < ARRAY_COUNT(sRegionDexEnd); ++i)
	{
		if (IsAnyCaughtInDexRange(first, sRegionDexEnd[i]))
			++count;
		first = sRegionDexEnd[i] + 1;
	}

	return count;
}

static bool8 IsValidPartyMon(struct Pokemon* mon)
{
	return GetMonData(mon, MON_DATA_SPECIES, NULL) != SPECIES_NONE && !GetMonData(mon, MON_DATA_IS_EGG, NULL);
}

static bool8 PartyMonMatches(u16 func, struct Pokemon* mon)
{
	u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);

	switch (func) {
		case ACH_FUNC_MEGA_STONE:
			return IsMegaStone(item);
		case ACH_FUNC_Z_CRYSTAL:
			return IsZCrystal(item);
		case ACH_FUNC_GIGANTAMAX:
			return mon->gigantamax;
		case ACH_FUNC_HIDDEN_ABILITY:
			return mon->hiddenAbility;
		case ACH_FUNC_MAX_FRIENDSHIP:
			return GetMonData(mon, MON_DATA_FRIENDSHIP, NULL) >= 255;
		case ACH_FUNC_POKERUS:
			return GetMonData(mon, MON_DATA_POKERUS, NULL) != 0;
		case ACH_FUNC_PERFECT_IV:
			for (u8 stat = 0; stat < NUM_STATS; ++stat)
			{
				if (GetMonData(mon, MON_DATA_HP_IV + stat, NULL) < 31)
					return FALSE;
			}
			return TRUE;
		case ACH_FUNC_MAX_EV:
		{
			u16 total = 0;
			for (u8 stat = 0; stat < NUM_STATS; ++stat)
				total += GetMonData(mon, MON_DATA_HP_EV + stat, NULL);
			return total >= 510;
		}
	}

	return FALSE;
}

static bool8 AnyPartyMonMatches(u16 func)
{
	for (u8 i = 0; i < gPlayerPartyCount && i < PARTY_SIZE; ++i)
	{
		if (IsValidPartyMon(&gPlayerParty[i]) && PartyMonMatches(func, &gPlayerParty[i]))
			return TRUE;
	}

	return FALSE;
}

static bool8 IsFuncConditionMet(u16 func)
{
	const struct AchSpeciesSet* set = GetSpeciesSet(func);

	if (set != NULL)
	{
		u8 caught = CountSpeciesSetCaught(set);
		return func == ACH_FUNC_ULTRA_BEAST ? caught > 0 : caught >= set->count;
	}

	switch (func) {
		case ACH_FUNC_MEGA_RING:
			return CheckBagHasItem(ITEM_MEGA_RING, 1);
		case ACH_FUNC_Z_RING:
			return CheckBagHasItem(ITEM_Z_POWER_RING, 1);
		case ACH_FUNC_DYNAMAX_BAND:
			return CheckBagHasItem(ITEM_DYNAMAX_BAND, 1);
		case ACH_FUNC_TERA_ORB:
			return CheckBagHasItem(ITEM_TERA_ORB, 1);
		case ACH_FUNC_SHINY_CHARM:
			return CheckBagHasItem(ITEM_SHINY_CHARM, 1);
		case ACH_FUNC_OVAL_CHARM:
			return CheckBagHasItem(ITEM_OVAL_CHARM, 1);

		case ACH_FUNC_MEGA_STONE:
		case ACH_FUNC_Z_CRYSTAL:
		case ACH_FUNC_GIGANTAMAX:
		case ACH_FUNC_HIDDEN_ABILITY:
		case ACH_FUNC_MAX_FRIENDSHIP:
		case ACH_FUNC_POKERUS:
		case ACH_FUNC_PERFECT_IV:
		case ACH_FUNC_MAX_EV:
			return AnyPartyMonMatches(func);

		case ACH_FUNC_ALL_REGIONS:
			return CountRegionsCaught() >= ARRAY_COUNT(sRegionDexEnd);
	}

	switch (func) {
		case ACH_FUNC_KANTO_DEX:
			for (u16 i = 1; i <= KANTO_DEX_SIZE; ++i)
			{
				if (!GetSetPokedexFlag(i, FLAG_GET_CAUGHT))
					return FALSE;
			}
			return TRUE;

		case ACH_FUNC_SHINY:
			for (u8 i = 0; i < gPlayerPartyCount && i < PARTY_SIZE; ++i)
			{
				struct Pokemon* mon = &gPlayerParty[i];
				if (GetMonData(mon, MON_DATA_SPECIES, NULL) != SPECIES_NONE
				&& !GetMonData(mon, MON_DATA_IS_EGG, NULL)
				&& IsMonShiny(mon))
					return TRUE;
			}
			return FALSE;

		case ACH_FUNC_LEVEL_100:
			for (u8 i = 0; i < gPlayerPartyCount && i < PARTY_SIZE; ++i)
			{
				struct Pokemon* mon = &gPlayerParty[i];
				if (GetMonData(mon, MON_DATA_SPECIES, NULL) != SPECIES_NONE
				&& !GetMonData(mon, MON_DATA_IS_EGG, NULL)
				&& GetMonData(mon, MON_DATA_LEVEL, NULL) >= 100)
					return TRUE;
			}
			return FALSE;

		case ACH_FUNC_LEGEND_BIRDS:
			return IsSpeciesCaught(SPECIES_ARTICUNO)
			    && IsSpeciesCaught(SPECIES_ZAPDOS)
			    && IsSpeciesCaught(SPECIES_MOLTRES);

		case ACH_FUNC_LEGEND_BEASTS:
			return IsSpeciesCaught(SPECIES_RAIKOU)
			    && IsSpeciesCaught(SPECIES_ENTEI)
			    && IsSpeciesCaught(SPECIES_SUICUNE);

		case ACH_FUNC_TM_10:
			return CountTMsInBag() >= 10;

		case ACH_FUNC_TM_30:
			return CountTMsInBag() >= 30;

		case ACH_FUNC_TM_50:
			return CountTMsInBag() >= 50;

		case ACH_FUNC_SURF_PIKACHU: //A Pikachu that knows Surf is in the party
			for (u8 i = 0; i < gPlayerPartyCount && i < PARTY_SIZE; ++i)
			{
				struct Pokemon* mon = &gPlayerParty[i];
				if (GetMonData(mon, MON_DATA_SPECIES, NULL) != SPECIES_PIKACHU || GetMonData(mon, MON_DATA_IS_EGG, NULL))
					continue;

				for (u8 j = 0; j < MAX_MON_MOVES; ++j)
				{
					if (GetMonData(mon, MON_DATA_MOVE1 + j, NULL) == MOVE_SURF)
						return TRUE;
				}
			}
			return FALSE;

		case ACH_FUNC_PARTY_ALL_LEVEL_100: //A full party where everyone is Lv100
			if (gPlayerPartyCount < PARTY_SIZE)
				return FALSE;

			for (u8 i = 0; i < PARTY_SIZE; ++i)
			{
				struct Pokemon* mon = &gPlayerParty[i];
				if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE
				|| GetMonData(mon, MON_DATA_IS_EGG, NULL)
				|| GetMonData(mon, MON_DATA_LEVEL, NULL) < 100)
					return FALSE;
			}
			return TRUE;

		case ACH_FUNC_PARTY_SAME_TYPE: //A full party where every member shares at least one type
		{
			u8 type;

			if (gPlayerPartyCount < PARTY_SIZE)
				return FALSE;

			for (type = 0; type < NUMBER_OF_MON_TYPES; ++type)
			{
				bool8 all = TRUE;

				for (u8 i = 0; i < PARTY_SIZE && all; ++i)
				{
					struct Pokemon* mon = &gPlayerParty[i];
					u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);

					if (species == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG, NULL)
					|| (gBaseStats[species].type1 != type && gBaseStats[species].type2 != type))
						all = FALSE;
				}

				if (all)
					return TRUE;
			}
			return FALSE;
		}

		case ACH_FUNC_NICKNAME: //Any party member (non-egg) whose name differs from its species name
			for (u8 i = 0; i < gPlayerPartyCount && i < PARTY_SIZE; ++i)
			{
				struct Pokemon* mon = &gPlayerParty[i];
				u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
				u8 name[POKEMON_NAME_LENGTH + 2];

				if (species == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG, NULL))
					continue;

				for (u8 k = 0; k < sizeof(name); ++k)
					name[k] = EOS;

				GetMonData(mon, MON_DATA_NICKNAME, name);

				//JP names are short (5 chars + EOS); compare char by char until either ends
				for (u8 k = 0; k < sizeof(SpeciesNames_t); ++k)
				{
					if (name[k] != gSpeciesNames[species][k])
						return TRUE;

					if (name[k] == EOS)
						break;
				}
			}
			return FALSE;

		case ACH_FUNC_ALL_OTHERS:
			for (u16 i = 0; i < ACH_COUNT; ++i)
			{
				if (gAchievements[i].kind == ACH_KIND_FUNC && gAchievements[i].param == ACH_FUNC_ALL_OTHERS)
					continue; //Itself

				if (!Achievements_IsUnlocked(i))
					return FALSE;
			}
			return TRUE;
	}

	return FALSE;
}

static bool8 IsConditionMet(u16 id)
{
	const struct Achievement* ach = &gAchievements[id];

	switch (ach->kind) {
		case ACH_KIND_FLAG:
			return FlagGet(ach->param);
		case ACH_KIND_STAT:
			return GetGameStat(ach->param) >= ach->target;
		case ACH_KIND_DEX_COUNT:
			return GetNationalPokedexCount(FLAG_GET_CAUGHT) >= ach->target;
		case ACH_KIND_SPECIES:
			return IsSpeciesCaught(ach->param);
		case ACH_KIND_FUNC:
			return IsFuncConditionMet(ach->param);
	}

	return FALSE;
}

//Slow checks scan large tables (Pokedex, Bag, party) so they aren't done every time
static bool8 IsSlowCheck(u16 id)
{
	u8 kind = gAchievements[id].kind;
	return kind == ACH_KIND_DEX_COUNT || kind == ACH_KIND_FUNC;
}

void Achievements_CheckAll(bool8 includeSlowChecks)
{
	for (u16 i = 0; i < ACH_COUNT; ++i)
	{
		if (Achievements_IsUnlocked(i))
			continue;

		if (!includeSlowChecks && IsSlowCheck(i))
			continue;

		if (IsConditionMet(i))
			Achievements_Unlock(i);
	}
}

//Progress shown in the list screen (e.g. 12/50)
u32 Achievements_GetProgress(u16 id)
{
	const struct Achievement* ach;
	u32 progress;

	if (id >= ACH_COUNT)
		return 0;

	ach = &gAchievements[id];
	switch (ach->kind) {
		case ACH_KIND_STAT:
			progress = GetGameStat(ach->param);
			break;
		case ACH_KIND_DEX_COUNT:
			progress = GetNationalPokedexCount(FLAG_GET_CAUGHT);
			break;
		case ACH_KIND_FUNC:
			if (GetSpeciesSet(ach->param) != NULL)
				progress = CountSpeciesSetCaught(GetSpeciesSet(ach->param));
			else if (ach->param == ACH_FUNC_ALL_REGIONS)
				progress = CountRegionsCaught();
			else
				progress = Achievements_IsUnlocked(id) ? ach->target : 0;
			break;
		default:
			progress = Achievements_IsUnlocked(id) ? ach->target : 0;
			break;
	}

	return progress > ach->target ? ach->target : progress;
}

// ================================================= //
// ============== Field / Popup Logic ============== //
// ================================================= //

#define POPUP_ID_MANY 0xFFFF //Several unlocks announced together

//Marks the next unlock (or every pending one, when many are pending) as shown.
//Returns its id, POPUP_ID_MANY if they were all marked, or ACH_COUNT if nothing is pending.
static u16 TakePendingNotification(void)
{
	u16 pending = CountPending();

	if (pending == 0)
		return ACH_COUNT;

	for (u16 i = 0; i < ACH_COUNT; ++i)
	{
		if (Achievements_IsUnlocked(i) && !IsNotified(i))
		{
			FlagSet(ACH_FLAG_NOTIFIED_START + i);

			if (pending < ACH_MANY_POPUPS_THRESHOLD)
				return i;
		}
	}

	return POPUP_ID_MANY;
}

#ifndef ACH_UNLOCK_MESSAGEBOX
//SoulGold-style popup: a small window slides down in the top right corner while the player keeps walking.
//It is drawn on BG0 (the field text BG) and slid by scrolling BG0, like the map name popup.
#define POPUP_ICON_TAG 0xACE0
#define POPUP_TILEMAP_TOP 1
#define POPUP_HEIGHT 4
#define POPUP_MIN_WIDTH 12
#define POPUP_MAX_WIDTH 28
#define POPUP_BASE_BLOCK 0x2A0 //Free BG0 tiles in the overworld (up to POPUP_MAX_WIDTH * POPUP_HEIGHT tiles are used)
#define POPUP_TEXT_X 8
#define POPUP_NAME_X 22 //Leaves room for the ball icon
#define POPUP_NAME_Y 15
#define POPUP_ICON_Y (POPUP_TILEMAP_TOP * 8 + POPUP_NAME_Y + 7) //Ball centered on the name (same spacing as the list screen)
#define POPUP_OFFSCREEN_Y 48
#define POPUP_SLIDE_FRAMES 18
#define POPUP_SHOW_FRAMES 150
#define POPUP_NO_WINDOW 0xFF
#define POPUP_SE SE_RG_CARD3 //Sound played when it appears (include/constants/songs.h)

#define tState data[0]
#define tTimer data[1]
#define tWindowId data[2]
#define tSpriteId data[3]
#define tSavedBg0VOffset data[4]
#define tIconX data[5]

enum
{
	POPUP_STATE_SLIDE_IN,
	POPUP_STATE_WAIT,
	POPUP_STATE_SLIDE_OUT,
	POPUP_STATE_CLEANUP,
	POPUP_STATE_DESTROY,
};

extern const u8 gText_Ach_PopupTitle[];
extern const u8 gText_Ach_PopupMany[];

static const struct TextColor sPopupTextColor = {.bgColor = TEXT_COLOR_WHITE, .fgColor = TEXT_COLOR_DARK_GREY, .shadowColor = TEXT_COLOR_LIGHT_GREY};

static const struct OamData sPopupIconOam =
{
	.affineMode = ST_OAM_AFFINE_OFF,
	.objMode = ST_OAM_OBJ_NORMAL,
	.shape = SPRITE_SHAPE(16x16),
	.size = SPRITE_SIZE(16x16),
	.priority = 0,
};

static const struct SpriteTemplate sPopupIconTemplate =
{
	.tileTag = POPUP_ICON_TAG,
	.paletteTag = POPUP_ICON_TAG,
	.oam = &sPopupIconOam,
	.anims = gDummySpriteAnimTable,
	.images = NULL,
	.affineAnims = gDummySpriteAffineAnimTable,
	.callback = SpriteCallbackDummy,
};

static void Task_AchievementPopup(u8 taskId);

static bool8 IsPopupActive(void)
{
	return FindTaskIdByFunc(Task_AchievementPopup) != 0xFF;
}

//Anything else that uses the field text BG / scrolls BG0 makes the popup go away
static bool8 ShouldHidePopup(void)
{
	return gMain.callback2 != CB2_Overworld
		|| ScriptContext2_IsEnabled()
		|| gPaletteFade->active
		|| IsMapNamePopupTaskActive();
}

static void SetPopupOffset(u8 taskId, s16 yOffset)
{
	s16* data = gTasks[taskId].data;

	SetGpuReg(REG_OFFSET_BG0VOFS, tSavedBg0VOffset + yOffset);
	if (tSpriteId != MAX_SPRITES)
		gSprites[tSpriteId].pos1.y = POPUP_ICON_Y - yOffset;
}

static void DestroyPopup(u8 taskId)
{
	s16* data = gTasks[taskId].data;

	if (tSpriteId != MAX_SPRITES)
	{
		DestroySprite(&gSprites[tSpriteId]);
		FreeSpriteTilesByTag(POPUP_ICON_TAG);
		FreeSpritePaletteByTag(POPUP_ICON_TAG);
	}

	if (tWindowId != POPUP_NO_WINDOW)
	{
		if (tState != POPUP_STATE_DESTROY) //Already cleared in POPUP_STATE_CLEANUP
			ClearStdWindowAndFrame(tWindowId, TRUE);
		RemoveWindow(tWindowId);
	}

	SetGpuReg(REG_OFFSET_BG0VOFS, tSavedBg0VOffset);
	DestroyTask(taskId);
}

static u8 CreatePopupIcon(u8 tier, s16 x)
{
	struct SpriteSheet sheet;
	struct SpritePalette spritePal;
	u8 spriteId;

	if (tier > ACH_TIER_PLATINUM)
		tier = ACH_TIER_BRONZE;

	sheet.data = (const u8*) &sAchBallTiles[tier * 32];
	sheet.size = 32 * 4;
	sheet.tag = POPUP_ICON_TAG;
	LoadSpriteSheet(&sheet);

	spritePal.data = &sAchBallPals[tier * 16];
	spritePal.tag = POPUP_ICON_TAG;
	if (LoadSpritePalette(&spritePal) == 0xFF) //No free sprite palette: show the popup without the icon
	{
		FreeSpriteTilesByTag(POPUP_ICON_TAG);
		return MAX_SPRITES;
	}

	spriteId = CreateSprite(&sPopupIconTemplate, x, POPUP_ICON_Y - POPUP_OFFSCREEN_Y, 0);
	if (spriteId == MAX_SPRITES)
	{
		FreeSpriteTilesByTag(POPUP_ICON_TAG);
		FreeSpritePaletteByTag(POPUP_ICON_TAG);
	}

	return spriteId;
}

//id is an achievement or POPUP_ID_MANY (count = how many were unlocked)
static void TryCreatePopup(u16 id, u16 count)
{
	struct WindowTemplate template;
	const u8* line2;
	u8 countText[12];
	u8 nameX, width, windowId, taskId;
	u32 lineWidth, innerWidth;
	s16* data;

	//The counter shown next to the title: "12/60"
	u8* str = ConvertIntToDecimalStringN(countText, Achievements_CountUnlocked(), STR_CONV_MODE_LEFT_ALIGN, 3);
	str = StringCopy(str, gText_Ach_Slash);
	ConvertIntToDecimalStringN(str, ACH_COUNT, STR_CONV_MODE_LEFT_ALIGN, 3);

	if (id == POPUP_ID_MANY)
	{
		str = ConvertIntToDecimalStringN(gStringVar4, count, STR_CONV_MODE_LEFT_ALIGN, 3);
		StringCopy(str, gText_Ach_PopupMany);
		line2 = gStringVar4;
		nameX = POPUP_TEXT_X;
	}
	else
	{
		line2 = gAchievements[id].name;
		nameX = POPUP_NAME_X;
	}

	//The window is as wide as its text and sticks to the right edge of the screen
	innerWidth = POPUP_TEXT_X + GetStringWidth(0, gText_Ach_PopupTitle, 0) + 8 + GetStringWidth(0, countText, 0);
	lineWidth = nameX + GetStringWidth(1, line2, 0);
	if (lineWidth > innerWidth)
		innerWidth = lineWidth;
	width = (innerWidth + 8 + 7) / 8;
	if (width < POPUP_MIN_WIDTH)
		width = POPUP_MIN_WIDTH;
	else if (width > POPUP_MAX_WIDTH)
		width = POPUP_MAX_WIDTH;

	template.bg = 0;
	template.tilemapLeft = 29 - width; //The right frame lands on the last column
	template.tilemapTop = POPUP_TILEMAP_TOP;
	template.width = width;
	template.height = POPUP_HEIGHT;
	template.paletteNum = 15;
	template.baseBlock = POPUP_BASE_BLOCK;

	windowId = AddWindow(&template);
	if (windowId == POPUP_NO_WINDOW)
		return;

	taskId = CreateTask(Task_AchievementPopup, 0);
	data = gTasks[taskId].data;
	tState = POPUP_STATE_SLIDE_IN;
	tTimer = 0;
	tWindowId = windowId;
	tSpriteId = MAX_SPRITES;
	tSavedBg0VOffset = GetGpuReg(REG_OFFSET_BG0VOFS);
	tIconX = template.tilemapLeft * 8 + 12;

	SetPopupOffset(taskId, POPUP_OFFSCREEN_Y); //Start hidden above the screen

	FillWindowPixelBuffer(windowId, PIXEL_FILL(1));
	DrawStdWindowFrame(windowId, FALSE);
	WindowPrint(windowId, 0, POPUP_TEXT_X, 1, &sPopupTextColor, 0, gText_Ach_PopupTitle);
	WindowPrint(windowId, 0, width * 8 - 8 - GetStringWidth(0, countText, 0), 1, &sPopupTextColor, 0, countText);
	WindowPrint(windowId, 1, nameX, POPUP_NAME_Y, &sPopupTextColor, 0, line2);
	PutWindowTilemap(windowId);
	CopyWindowToVram(windowId, COPYWIN_BOTH);

	if (id != POPUP_ID_MANY)
		tSpriteId = CreatePopupIcon(gAchievements[id].tier, tIconX);

	PlaySE(POPUP_SE);
}

//Cubic easing. Slide in: fast then slow (ease out). Slide out: slow then fast (ease in).
static s16 GetSlideOffset(u16 timer, bool8 slidingIn)
{
	s32 t = slidingIn ? POPUP_SLIDE_FRAMES - timer : timer;

	return (POPUP_OFFSCREEN_Y * t * t * t) / (POPUP_SLIDE_FRAMES * POPUP_SLIDE_FRAMES * POPUP_SLIDE_FRAMES);
}

static void Task_AchievementPopup(u8 taskId)
{
	s16* data = gTasks[taskId].data;

	if (ShouldHidePopup())
	{
		DestroyPopup(taskId);
		return;
	}

	switch (tState) {
		case POPUP_STATE_SLIDE_IN:
			SetPopupOffset(taskId, GetSlideOffset(++tTimer, TRUE));
			if (tTimer >= POPUP_SLIDE_FRAMES)
			{
				tTimer = 0;
				tState = POPUP_STATE_WAIT;
			}
			break;
		case POPUP_STATE_WAIT:
			if (++tTimer > POPUP_SHOW_FRAMES)
			{
				tTimer = 0;
				tState = POPUP_STATE_SLIDE_OUT;
			}
			break;
		case POPUP_STATE_SLIDE_OUT:
			SetPopupOffset(taskId, GetSlideOffset(++tTimer, FALSE));
			if (tTimer >= POPUP_SLIDE_FRAMES)
				tState = POPUP_STATE_CLEANUP;
			break;
		case POPUP_STATE_CLEANUP:
			//Clear the window while it is still scrolled off screen, and restore the BG offset only after the
			//tilemap copy has reached VRAM (next frame) so it never flashes back in place
			ClearStdWindowAndFrame(tWindowId, TRUE);
			tState = POPUP_STATE_DESTROY;
			break;
		case POPUP_STATE_DESTROY:
			DestroyPopup(taskId);
			break;
	}
}

//Called while the player has control (before the field input is handled)
static void HandlePopupInput(void)
{
	u8 taskId = FindTaskIdByFunc(Task_AchievementPopup);

	if (taskId == 0xFF)
		return;

	if (JOY_NEW(START_BUTTON | SELECT_BUTTON | L_BUTTON | R_BUTTON))
	{
		//A menu may open on top of it this frame: remove it right away
		DestroyPopup(taskId);
	}
	else if (JOY_NEW(A_BUTTON | B_BUTTON) && gTasks[taskId].tState <= POPUP_STATE_WAIT)
	{
		s16* data = gTasks[taskId].data;

		//Slide out from wherever it currently is
		if (tState == POPUP_STATE_SLIDE_IN)
			tTimer = POPUP_SLIDE_FRAMES - tTimer;
		else
			tTimer = 0;
		tState = POPUP_STATE_SLIDE_OUT;
	}
}

static bool8 CanShowPopup(void)
{
	return gMain.callback2 == CB2_Overworld
		&& !gPaletteFade->active
		&& !IsMapNamePopupTaskActive()
		&& GetTaskCount() < NUM_TASKS;
}

#undef tState
#undef tTimer
#undef tWindowId
#undef tSpriteId
#undef tSavedBg0VOffset
#undef tIconX
#endif //ACH_UNLOCK_MESSAGEBOX

//Called every frame from TryRunOnFrameMapScript. Returns TRUE if a script was started.
bool8 Achievements_FieldFrameUpdate(void)
{
	u32 frame = gMain.vblankCounter2;
	u16 id = frame % ACH_COUNT;

	//Only ONE achievement is evaluated per frame (a full pass takes ACH_COUNT frames, ~1 second).
	//Checking everything at once (dex / bag / party scans) made the game hitch while walking.
	if (!Achievements_IsUnlocked(id) && IsConditionMet(id))
		Achievements_Unlock(id);

	#ifndef ACH_UNLOCK_MESSAGEBOX
	if (IsPopupActive())
	{
		HandlePopupInput(); //Rewards wait until the popup is gone
		return FALSE;
	}

	if (frame % PENDING_CHECK_INTERVAL != 0)
		return FALSE;

	if (CountPending() > 0)
	{
		if (CanShowPopup())
		{
			u16 count = CountPending();
			TryCreatePopup(TakePendingNotification(), count);
		}
		return FALSE;
	}

	if (GetNextReward() >= 0 || IsAutoGiftDue()) //The reward / gift messages still use the script
	{
		ScriptContext1_SetupScript(EventScript_AchievementUnlocked);
		return TRUE;
	}
	#else
	if (frame % PENDING_CHECK_INTERVAL == 0
	&& (CountPending() > 0 || GetNextReward() >= 0 || IsAutoGiftDue()))
	{
		ScriptContext1_SetupScript(EventScript_AchievementUnlocked);
		return TRUE;
	}
	#endif

	return FALSE;
}

//Script: callasm AchievementsPreparePopup
//  gSpecialVar_LastResult = 0: a single achievement ([BUFFER1] = its name)
//                         = 1: several at once ([BUFFER1] = how many)
//                         = 2: nothing pending (only a milestone reward is due)
void AchievementsPreparePopup(void)
{
	u16 pending = CountPending();
	u16 id = TakePendingNotification();

	if (id == ACH_COUNT)
	{
		gSpecialVar_LastResult = 2; //Only a reward is due
	}
	else if (id == POPUP_ID_MANY)
	{
		ConvertIntToDecimalStringN(gStringVar1, pending, STR_CONV_MODE_LEFT_ALIGN, 3);
		gSpecialVar_LastResult = 1;
	}
	else
	{
		StringCopy(gStringVar1, gAchievements[id].name);
		gSpecialVar_LastResult = 0;
	}
}

//Script: callasm AchievementsPrepareReward (gives the next due reward; used by the NPC script)
//  gSpecialVar_LastResult = 0: nothing due / 1: given ([BUFFER1] = item, [BUFFER2] = amount) / 2: due but the bag is full
void AchievementsPrepareReward(void)
{
	s8 i = GetNextRewardDue();

	gSpecialVar_LastResult = 0;
	if (i < 0)
		return;

	if (!CheckBagHasSpace(sAchievementRewards[i].item, sAchievementRewards[i].amount))
	{
		gSpecialVar_LastResult = 2;
		return;
	}

	AddBagItem(sAchievementRewards[i].item, sAchievementRewards[i].amount);
	FlagSet(ACH_FLAG_REWARD_START + i);
	StringCopy(gStringVar1, ItemId_GetName(sAchievementRewards[i].item));
	ConvertIntToDecimalStringN(gStringVar2, sAchievementRewards[i].amount, STR_CONV_MODE_LEFT_ALIGN, 3);
	gSpecialVar_LastResult = 1;
}

//Script: callasm AchievementsPrepareGift  (gSpecialVar_LastResult = 1 if the Pikachu gift is due; used by the NPC script)
void AchievementsPrepareGift(void)
{
	gSpecialVar_LastResult = IsGiftDue();
}

//Script: callasm AchievementsPrepareAutoGift (used by the unlock script; always 0 when an NPC gives the rewards)
void AchievementsPrepareAutoGift(void)
{
	gSpecialVar_LastResult = IsAutoGiftDue();
}

void AchievementsMarkGiftDone(void)
{
	FlagSet(ACH_FLAG_GIFT_PIKACHU);
}

//Script: callasm AchievementsPrepareAutoReward (used by the unlock script; does nothing when an NPC gives the rewards)
void AchievementsPrepareAutoReward(void)
{
	#ifdef ACH_REWARDS_VIA_NPC
	gSpecialVar_LastResult = 0;
	#else
	AchievementsPrepareReward();
	#endif
}

//Script: callasm AchievementsPrepareNpcDialog
//  [BUFFER1] = number unlocked, [BUFFER2] = count needed for the next reward
//  gSpecialVar_LastResult = 1 if there is a next reward, 0 if all rewards were already received
void AchievementsPrepareNpcDialog(void)
{
	u16 count;
	u16 next = 0xFFFF;

	Achievements_CheckAll(TRUE);
	count = Achievements_CountUnlocked();
	ConvertIntToDecimalStringN(gStringVar1, count, STR_CONV_MODE_LEFT_ALIGN, 3);
	gSpecialVar_LastResult = 0;

	//The smallest count among the rewards not received yet (entries added later may need fewer unlocks)
	for (u8 i = 0; i < NUM_ACH_REWARDS; ++i)
	{
		if (!FlagGet(ACH_FLAG_REWARD_START + i) && sAchievementRewards[i].requiredCount < next)
			next = sAchievementRewards[i].requiredCount;
	}

	if (next != 0xFFFF)
	{
		ConvertIntToDecimalStringN(gStringVar2, next, STR_CONV_MODE_LEFT_ALIGN, 3);
		gSpecialVar_LastResult = 1;
	}
}

//Script: callasm AchievementsGetCountForScript
//  For map scripts that need the count (gSpecialVar_LastResult = [BUFFER1] = number unlocked)
void AchievementsGetCountForScript(void)
{
	Achievements_CheckAll(TRUE);
	gSpecialVar_LastResult = Achievements_CountUnlocked();
	ConvertIntToDecimalStringN(gStringVar1, gSpecialVar_LastResult, STR_CONV_MODE_LEFT_ALIGN, 3);
}

// ================================================= //
// ================== List Screen ================== //
// ================================================= //
//Layout and graphics are taken from SoulGold's trophy screen (4 BGs: background, frame, text, footer)

#define NUM_VISIBLE_ROWS 6
#define ROW_HEIGHT 15
#define LIST_TEXT_Y(row) (4 + (row) * ROW_HEIGHT)
#define NAME_X 54
#define CURSOR_X 14
#define ICON_X 35
#define ICON_Y(row) (27 + (row) * ROW_HEIGHT)
#define STATUS_LEFT 192
#define STATUS_WIDTH 48
#define FOOTER_TEXT_X 8
#define FOOTER_TEXT_Y 26
#define BALL_TAG_BASE 0xAC00
#define SCROLL_ARROW_TAG 0xAC80
#define MENU_PAL_SLOT 1

enum
{
	BG_DETAIL,
	BG_TEXT,
	BG_MENU,
	BG_BACKGROUND,
};

enum
{
	WIN_TITLE,
	WIN_LIST,
	WIN_DESC,
	WIN_COUNT,
};

enum
{
	TD_CURSOR,
	TD_SCROLL, //Also read by the scroll arrows (as u16)
	TD_STATE,
	TD_ARROWS,
	TD_ICONS, //6 entries
};

enum
{
	STATE_FADE_IN,
	STATE_MAIN,
	STATE_FADE_OUT,
	STATE_FANFARE, //A reward was just received
};

#define REWARD_TEXT_X 56 //Reward of the selected achievement, printed in the title bar

extern const u8 gText_Ach_Title[];
extern const u8 gText_Ach_StatusDone[];
extern const u8 gText_Ach_StatusLocked[];
extern const u8 gText_Ach_StatusClaim[];
extern const u8 gText_Ach_Cursor[];
extern const u8 gText_Ach_RewardLabel[];
extern const u8 gText_Ach_RewardTimes[];
extern const u8 gText_Ach_RewardLv[];
extern const u8 gText_Ach_ClaimItem[];
extern const u8 gText_Ach_ClaimMonParty[];
extern const u8 gText_Ach_ClaimMonPC[];
extern const u8 gText_Ach_ClaimBagFull[];
extern const u8 gText_Ach_ClaimMonFull[];

u8 ScriptGiveMon(u16 species, u8 level, u16 item, u32 unused1, u32 customGivePokemon, u8 ballType); //src/build_pokemon.c

static const struct BgTemplate sAchBgTemplates[] =
{
	{.bg = BG_DETAIL,     .charBaseIndex = 1, .mapBaseIndex = 31, .screenSize = 0, .paletteMode = 0, .priority = 0, .baseTile = 0},
	{.bg = BG_TEXT,       .charBaseIndex = 1, .mapBaseIndex = 30, .screenSize = 0, .paletteMode = 0, .priority = 1, .baseTile = 0},
	{.bg = BG_MENU,       .charBaseIndex = 0, .mapBaseIndex = 29, .screenSize = 0, .paletteMode = 0, .priority = 2, .baseTile = 0},
	{.bg = BG_BACKGROUND, .charBaseIndex = 2, .mapBaseIndex = 28, .screenSize = 0, .paletteMode = 0, .priority = 3, .baseTile = 0},
};

static const struct WindowTemplate sAchWinTemplates[WIN_COUNT + 1] =
{
	[WIN_TITLE] = {.bg = BG_TEXT,   .tilemapLeft = 0, .tilemapTop = 0,  .width = 30, .height = 2,  .paletteNum = 15, .baseBlock = 0x024},
	[WIN_LIST]  = {.bg = BG_TEXT,   .tilemapLeft = 0, .tilemapTop = 2,  .width = 30, .height = 12, .paletteNum = 15, .baseBlock = 0x078},
	[WIN_DESC]  = {.bg = BG_DETAIL, .tilemapLeft = 0, .tilemapTop = 13, .width = 30, .height = 7,  .paletteNum = 15, .baseBlock = 0x240},
	DUMMY_WIN_TEMPLATE
};

static const struct TextColor sTextDark = {.bgColor = TEXT_COLOR_TRANSPARENT, .fgColor = TEXT_COLOR_DARK_GREY, .shadowColor = TEXT_COLOR_LIGHT_GREY};
static const struct TextColor sTextGreen = {.bgColor = TEXT_COLOR_TRANSPARENT, .fgColor = TEXT_COLOR_GREEN, .shadowColor = TEXT_COLOR_LIGHT_GREEN};
static const struct TextColor sTextWhite = {.bgColor = TEXT_COLOR_TRANSPARENT, .fgColor = TEXT_COLOR_WHITE, .shadowColor = TEXT_COLOR_DARK_GREY};
static const struct TextColor sTextRed = {.bgColor = TEXT_COLOR_TRANSPARENT, .fgColor = TEXT_COLOR_RED, .shadowColor = TEXT_COLOR_LIGHT_RED};

//Per-achievement rewards (set in scripts/achievements.json). Each one can be received once, with A on the list screen.
static bool8 HasReward(u16 id)
{
	return gAchievements[id].rewardType != ACH_REWARD_NONE;
}

static bool8 IsRewardClaimed(u16 id)
{
	return HasReward(id) && FlagGet(ACH_FLAG_CLAIMED_START + id);
}

static bool8 CanClaimReward(u16 id)
{
	return HasReward(id) && Achievements_IsUnlocked(id) && !IsRewardClaimed(id);
}

//"ほうしゅう:ふしぎなアメ x3" / "ほうしゅう:ピカチュウ Lv5"
static void GetRewardText(u16 id, u8* dst)
{
	const struct Achievement* ach = &gAchievements[id];

	dst = StringCopy(dst, gText_Ach_RewardLabel);
	if (ach->rewardType == ACH_REWARD_ITEM)
	{
		dst = StringCopy(dst, ItemId_GetName(ach->rewardId));
		if (ach->rewardAmount > 1)
		{
			*dst++ = 0x00; //Space
			dst = StringCopy(dst, gText_Ach_RewardTimes);
			ConvertIntToDecimalStringN(dst, ach->rewardAmount, STR_CONV_MODE_LEFT_ALIGN, 3);
		}
	}
	else
	{
		GetSpeciesName(dst, ach->rewardId);
		dst += StringLength(dst);
		*dst++ = 0x00; //Space
		dst = StringCopy(dst, gText_Ach_RewardLv);
		ConvertIntToDecimalStringN(dst, ach->rewardAmount, STR_CONV_MODE_LEFT_ALIGN, 3);
	}
}

static const struct OamData sBallIconOam =
{
	.affineMode = ST_OAM_AFFINE_OFF,
	.objMode = ST_OAM_OBJ_NORMAL,
	.shape = SPRITE_SHAPE(16x16),
	.size = SPRITE_SIZE(16x16),
	.priority = 0,
};

static const struct SpriteTemplate sBallIconTemplate =
{
	.tileTag = BALL_TAG_BASE,
	.paletteTag = BALL_TAG_BASE,
	.oam = &sBallIconOam,
	.anims = gDummySpriteAnimTable,
	.images = NULL,
	.affineAnims = gDummySpriteAffineAnimTable,
	.callback = SpriteCallbackDummy,
};

//Icons are plain sprites that are recreated on every redraw: their ids are kept in the task data
static void DestroyBallIcons(u8 taskId)
{
	s16* data = gTasks[taskId].data;

	for (u8 row = 0; row < NUM_VISIBLE_ROWS; ++row)
	{
		u16 tag = BALL_TAG_BASE + row;

		if (data[TD_ICONS + row] != MAX_SPRITES)
		{
			DestroySprite(&gSprites[data[TD_ICONS + row]]);
			data[TD_ICONS + row] = MAX_SPRITES;
			FreeSpriteTilesByTag(tag);
			FreeSpritePaletteByTag(tag);
		}
	}
}

static void CreateBallIcon(u8 taskId, u8 row, u16 id)
{
	u16 tag = BALL_TAG_BASE + row;
	u8 tier = gAchievements[id].tier;
	u16 palette[16];
	struct SpriteSheet sheet;
	struct SpritePalette spritePal;
	struct SpriteTemplate template;
	u8 spriteId;

	if (tier > ACH_TIER_PLATINUM)
		tier = ACH_TIER_BRONZE;

	sheet.data = (const u8*) &sAchBallTiles[tier * 32];
	sheet.size = 32 * 4;
	sheet.tag = tag;
	LoadSpriteSheet(&sheet);

	memcpy(palette, &sAchBallPals[tier * 16], sizeof(palette));
	if (!Achievements_IsUnlocked(id)) //Locked: gray
	{
		for (u8 i = 1; i < 16; ++i)
		{
			u16 c = palette[i];
			u16 gray = ((c & 0x1F) + ((c >> 5) & 0x1F) + ((c >> 10) & 0x1F)) / 3;
			palette[i] = gray | (gray << 5) | (gray << 10);
		}
	}
	spritePal.data = palette;
	spritePal.tag = tag;
	LoadSpritePalette(&spritePal);

	memcpy(&template, &sBallIconTemplate, sizeof(template));
	template.tileTag = tag;
	template.paletteTag = tag;
	spriteId = CreateSprite(&template, ICON_X, ICON_Y(row), 0);
	if (spriteId != MAX_SPRITES)
		gTasks[taskId].data[TD_ICONS + row] = spriteId;
}

static void PrintTitle(u8 cursor)
{
	FillWindowPixelBuffer(WIN_TITLE, PIXEL_FILL(0));
	WindowPrint(WIN_TITLE, 0, 10, 2, &sTextDark, 0, gText_Ach_Title);

	if (HasReward(cursor) && !IsRewardClaimed(cursor)) //Hidden once received
	{
		GetRewardText(cursor, gStringVar4);
		WindowPrint(WIN_TITLE, 0, REWARD_TEXT_X, 2, &sTextDark, 0, gStringVar4);
	}

	u8* str = ConvertIntToDecimalStringN(gStringVar4, Achievements_CountUnlocked(), STR_CONV_MODE_LEFT_ALIGN, 3);
	str = StringCopy(str, gText_Ach_Slash);
	ConvertIntToDecimalStringN(str, ACH_COUNT, STR_CONV_MODE_LEFT_ALIGN, 3);
	WindowPrint(WIN_TITLE, 0, 232 - GetStringWidth(0, gStringVar4, 0), 2, &sTextDark, 0, gStringVar4);

	PutWindowTilemap(WIN_TITLE);
	CopyWindowToVram(WIN_TITLE, COPYWIN_BOTH);
}

static void PrintStatus(u16 id, u8 y)
{
	const struct TextColor* color = &sTextDark;
	const u8* text = gText_Ach_StatusLocked;

	if (CanClaimReward(id))
	{
		text = gText_Ach_StatusClaim; //Unlocked, reward not received yet
		color = &sTextRed;
	}
	else if (Achievements_IsUnlocked(id)) //Also once the reward has been received
	{
		text = gText_Ach_StatusDone;
		color = &sTextGreen;
	}

	WindowPrint(WIN_LIST, 1, STATUS_LEFT + (STATUS_WIDTH - GetStringWidth(1, text, 0)) / 2, y, color, 0, text);
}

static void PrintList(u8 cursor, u8 scroll)
{
	FillWindowPixelBuffer(WIN_LIST, PIXEL_FILL(0));

	for (u8 row = 0; row < NUM_VISIBLE_ROWS; ++row)
	{
		u16 id = scroll + row;

		if (id >= ACH_COUNT)
			break;

		if (id == cursor)
			WindowPrint(WIN_LIST, 1, CURSOR_X, LIST_TEXT_Y(row), &sTextDark, 0, gText_Ach_Cursor);

		WindowPrint(WIN_LIST, 1, NAME_X, LIST_TEXT_Y(row), &sTextDark, 0, gAchievements[id].name);
		PrintStatus(id, LIST_TEXT_Y(row));
	}

	PutWindowTilemap(WIN_LIST);
	CopyWindowToVram(WIN_LIST, COPYWIN_BOTH);
}

static void PrintFooter(const u8* text)
{
	FillWindowPixelBuffer(WIN_DESC, PIXEL_FILL(0));
	WindowPrint(WIN_DESC, 1, FOOTER_TEXT_X, FOOTER_TEXT_Y, &sTextWhite, 0, text);
	PutWindowTilemap(WIN_DESC);
	CopyWindowToVram(WIN_DESC, COPYWIN_BOTH);
}

static void PrintDescription(u8 cursor)
{
	const u8* text = gAchievements[cursor].description;

	FillWindowPixelBuffer(WIN_DESC, PIXEL_FILL(0));
	if (!Achievements_IsUnlocked(cursor)
	&& (gAchievements[cursor].kind == ACH_KIND_STAT || gAchievements[cursor].kind == ACH_KIND_DEX_COUNT))
	{
		//Progress is appended to the description: "...   12/151"
		u8* str = StringCopy(gStringVar4, text);
		*str++ = 0x00; //Space
		*str++ = 0x00; //Space
		str = ConvertIntToDecimalStringN(str, Achievements_GetProgress(cursor), STR_CONV_MODE_LEFT_ALIGN, 6);
		str = StringCopy(str, gText_Ach_Slash);
		str = ConvertIntToDecimalStringN(str, gAchievements[cursor].target, STR_CONV_MODE_LEFT_ALIGN, 6);
		*str = EOS;
		text = gStringVar4;
	}

	PrintFooter(text);
}

//Gives the reward of an unlocked achievement and prints the result in the footer. Returns TRUE if it was received.
static bool8 ClaimReward(u16 id)
{
	const struct Achievement* ach = &gAchievements[id];
	const u8* msg;
	bool8 received = FALSE;

	if (ach->rewardType == ACH_REWARD_ITEM)
	{
		StringCopy(gStringVar1, ItemId_GetName(ach->rewardId));
		ConvertIntToDecimalStringN(gStringVar2, ach->rewardAmount, STR_CONV_MODE_LEFT_ALIGN, 3);

		if (CheckBagHasSpace(ach->rewardId, ach->rewardAmount))
		{
			AddBagItem(ach->rewardId, ach->rewardAmount);
			msg = gText_Ach_ClaimItem;
			received = TRUE;
		}
		else
			msg = gText_Ach_ClaimBagFull;
	}
	else
	{
		GetSpeciesName(gStringVar1, ach->rewardId);

		switch (ScriptGiveMon(ach->rewardId, ach->rewardAmount, ach->rewardHeldItem, 0, 0, 0)) {
			case 0: //Party
				msg = gText_Ach_ClaimMonParty;
				received = TRUE;
				break;
			case 1: //PC
				msg = gText_Ach_ClaimMonPC;
				received = TRUE;
				break;
			default: //Party and boxes are full
				msg = gText_Ach_ClaimMonFull;
				break;
		}
	}

	if (received)
		FlagSet(ACH_FLAG_CLAIMED_START + id);

	StringExpandPlaceholders(gStringVar4, msg);
	PrintFooter(gStringVar4); //Stays until the cursor moves
	return received;
}

static void RedrawScreen(u8 taskId)
{
	u8 cursor = gTasks[taskId].data[TD_CURSOR];
	u8 scroll = gTasks[taskId].data[TD_SCROLL];

	PrintTitle(cursor);
	PrintList(cursor, scroll);
	PrintDescription(cursor);

	DestroyBallIcons(taskId);
	for (u8 row = 0; row < NUM_VISIBLE_ROWS && scroll + row < ACH_COUNT; ++row)
		CreateBallIcon(taskId, row, scroll + row);
}

static void Task_AchievementsFadeOut(u8 taskId)
{
	if (!gPaletteFade->active)
	{
		//The arrows' task and sprites are cleared when the overworld is set up again
		FreeAllWindowBuffers();
		DestroyTask(taskId);
		SetMainCallback2(CB2_ReturnToFieldWithOpenMenu);
	}
}

static void Task_AchievementsMain(u8 taskId)
{
	s16* data = gTasks[taskId].data;
	s16 cursor = data[TD_CURSOR];
	s16 scroll = data[TD_SCROLL];

	switch (data[TD_STATE]) {
		case STATE_FADE_IN:
			if (!gPaletteFade->active)
				data[TD_STATE] = STATE_MAIN;
			return;

		case STATE_FADE_OUT:
			return;

		case STATE_FANFARE:
			if (IsFanfareTaskInactive())
				data[TD_STATE] = STATE_MAIN;
			return;
	}

	if (JOY_NEW(A_BUTTON))
	{
		if (CanClaimReward(cursor))
		{
			if (ClaimReward(cursor))
			{
				PlayFanfare(MUS_FANFA1);
				data[TD_STATE] = STATE_FANFARE;
				PrintTitle(cursor);
				PrintList(cursor, scroll); //"うけとる" -> "クリア"
			}
			else
				PlaySE(SE_ERROR);
		}
		return;
	}

	if (JOY_NEW(B_BUTTON) || JOY_NEW(START_BUTTON))
	{
		PlaySE(SE_SELECT);
		BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
		data[TD_STATE] = STATE_FADE_OUT;
		gTasks[taskId].func = Task_AchievementsFadeOut;
		return;
	}

	if (JOY_REPT(DPAD_UP))
		cursor = (cursor == 0) ? ACH_COUNT - 1 : cursor - 1;
	else if (JOY_REPT(DPAD_DOWN))
		cursor = (cursor == ACH_COUNT - 1) ? 0 : cursor + 1;
	else if (JOY_REPT(DPAD_LEFT))
		cursor = (cursor < NUM_VISIBLE_ROWS) ? 0 : cursor - NUM_VISIBLE_ROWS;
	else if (JOY_REPT(DPAD_RIGHT))
		cursor = (cursor + NUM_VISIBLE_ROWS >= ACH_COUNT) ? ACH_COUNT - 1 : cursor + NUM_VISIBLE_ROWS;
	else
		return;

	if (cursor != data[TD_CURSOR])
	{
		PlaySE(SE_SELECT);

		if (cursor < scroll)
			scroll = cursor;
		else if (cursor >= scroll + NUM_VISIBLE_ROWS)
			scroll = cursor - NUM_VISIBLE_ROWS + 1;

		data[TD_CURSOR] = cursor;
		data[TD_SCROLL] = scroll;
		RedrawScreen(taskId);
	}
}

static void MainCB2_Achievements(void)
{
	RunTasks();
	AnimateSprites();
	BuildOamBuffer();
	UpdatePaletteFade();
}

static void VBlankCB_Achievements(void)
{
	LoadOam();
	ProcessSpriteCopyRequests();
	TransferPlttBuffer();
}

static void ClearVramOamPlttRegs(void)
{
	DmaFill16(3, 0, VRAM, VRAM_SIZE);
	DmaFill32(3, 0, OAM, OAM_SIZE);
	DmaFill16(3, 0, PLTT, PLTT_SIZE);
	SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
	SetGpuReg(REG_OFFSET_BG3CNT, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG2CNT, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG1CNT, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG0CNT, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG3HOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG3VOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG2HOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG2VOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG1HOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG1VOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG0HOFS, DISPCNT_MODE_0);
	SetGpuReg(REG_OFFSET_BG0VOFS, DISPCNT_MODE_0);
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

//Copies a 30x20 tilemap into the (unbuffered) map of a BG. palette is OR'ed into every entry.
static void CopyTilemapToVram(const u16* map, u8 mapBase, u16 palette)
{
	for (u8 row = 0; row < 20; ++row)
	{
		u16* dst = (u16*) (VRAM + mapBase * 0x800 + row * 64);

		for (u8 x = 0; x < 30; ++x)
			dst[x] = map[row * 30 + x] | palette;
	}
}

static void CB2_Achievements(void)
{
	switch (gMain.state) {
		case 0:
		default:
			SetVBlankCallback(NULL);
			ClearVramOamPlttRegs();
			gMain.state++;
			break;
		case 1:
			ClearTasksAndGraphicalStructs();
			gMain.state++;
			break;
		case 2:
			ResetBgsAndClearDma3BusyFlags(0);
			InitBgsFromTemplates(0, sAchBgTemplates, NELEMS(sAchBgTemplates));
			ShowBg(BG_DETAIL);
			ShowBg(BG_TEXT);
			ShowBg(BG_MENU);
			ShowBg(BG_BACKGROUND);
			gMain.state++;
			break;
		case 3:
			InitWindows(sAchWinTemplates);
			DeactivateAllTextPrinters();
			Menu_LoadStdPalAt(15 * 0x10);
			LoadPalette(sAchBgPal, 0, sizeof(sAchBgPal));
			LoadPalette(sAchMenuPal, MENU_PAL_SLOT * 0x10, sizeof(sAchMenuPal));
			LoadBgTiles(BG_MENU, sAchMenuTiles, sizeof(sAchMenuTiles), 0);
			LoadBgTiles(BG_BACKGROUND, sAchBgTiles, sizeof(sAchBgTiles), 0);
			CopyTilemapToVram(sAchMenuTilemap, 29, MENU_PAL_SLOT << 12);
			CopyTilemapToVram(sAchBgTilemap, 28, 0);
			gMain.state++;
			break;
		case 4:
		{
			u8 taskId = CreateTask(Task_AchievementsMain, 0);
			gTasks[taskId].data[TD_CURSOR] = 0;
			gTasks[taskId].data[TD_SCROLL] = 0;
			gTasks[taskId].data[TD_STATE] = STATE_FADE_IN;
			gTasks[taskId].data[TD_ARROWS] = 0xFF;
			for (u8 i = 0; i < NUM_VISIBLE_ROWS; ++i)
				gTasks[taskId].data[TD_ICONS + i] = MAX_SPRITES;
			RedrawScreen(taskId);
			gTasks[taskId].data[TD_ARROWS] = AddScrollIndicatorArrowPairParameterized(
				SCROLL_ARROW_UP, 170, 18, 118, ACH_COUNT - NUM_VISIBLE_ROWS, SCROLL_ARROW_TAG, SCROLL_ARROW_TAG,
				(u16*) &gTasks[taskId].data[TD_SCROLL]);
			gMain.state++;
			break;
		}
		case 5:
			BeginNormalPaletteFade(0xFFFFFFFF, 0, 16, 0, RGB_BLACK);
			gMain.state++;
			break;
		case 6:
			SetVBlankCallback(VBlankCB_Achievements);
			SetMainCallback2(MainCB2_Achievements);
			gMain.state = 0;
			break;
	}
}

//Start menu callback
bool8 StartMenuAchievementsCallback(void)
{
	if (!gPaletteFade->active)
	{
		PlayRainStoppingSoundEffect();
		DestroySafariZoneStatsWindow();
		CleanupOverworldWindowsAndTilemaps();
		SetMainCallback2(CB2_Achievements);
		return TRUE;
	}

	return FALSE;
}
