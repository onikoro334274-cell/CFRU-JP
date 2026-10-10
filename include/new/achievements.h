#pragma once

#include "../global.h"

/**
 * \file achievements.h
 * \brief Achievements system (ported from SoulGold). The unlocked state is stored in
 *        expanded flags starting at ACH_FLAG_START.
 */

//First flag used for saving (a free part of the expanded flag area 0x900-0x18FF; change it if it clashes. 0x1800-0x186C is used by raid battles).
//  ACH_FLAG_START + id        : unlocked
//  ACH_FLAG_START + 128 + id  : unlock message already shown
//NOTE: Changing this makes the unlock state of existing saves unreadable.
#define ACH_FLAG_START 0xB00   // [hack] 2026-10-10 0x1600 はホウエンのトレーナー 1280〜1354 のフラグ（0x1400 + 番号 - 768）と重なる → 使われていない 0xB00〜0xCFF
#define ACH_MAX_COUNT 128

//Milestone rewards: ACH_FLAG_REWARD_START + index is set once the reward has been received
#define ACH_FLAG_REWARD_START (ACH_FLAG_START + 2 * ACH_MAX_COUNT)

//Per-achievement rewards (claimed from the list screen): ACH_FLAG_CLAIMED_START + id is set once received.
//Uses the last 128 flags of the achievement area (0x1780-0x17FF with the default ACH_FLAG_START).
#define ACH_FLAG_CLAIMED_START (ACH_FLAG_START + 3 * ACH_MAX_COUNT)

enum AchievementRewardType
{
	ACH_REWARD_NONE,
	ACH_REWARD_ITEM, //rewardId = item, rewardAmount = how many
	ACH_REWARD_MON,  //rewardId = species, rewardAmount = level, rewardHeldItem = held item
};

enum AchievementTier
{
	ACH_TIER_BRONZE,
	ACH_TIER_SILVER,
	ACH_TIER_GOLD,
	ACH_TIER_PLATINUM,
};

enum AchievementKind
{
	ACH_KIND_FLAG,      //param = flag; achieved when it is set
	ACH_KIND_STAT,      //param = GAME_STAT index; achieved when >= target
	ACH_KIND_DEX_COUNT, //Achieved when the number of caught species >= target
	ACH_KIND_SPECIES,   //param = species; achieved when it is registered as caught
	ACH_KIND_FUNC,      //param = enum AchievementFunc (custom check)
	ACH_KIND_ITEM,      //[hack] 2026-10-10 param = item; achieved when it is in the bag (key items obtained in this hack)
};

enum AchievementFunc
{
	ACH_FUNC_KANTO_DEX,
	ACH_FUNC_SHINY,
	ACH_FUNC_LEVEL_100,
	ACH_FUNC_LEGEND_BIRDS,
	ACH_FUNC_LEGEND_BEASTS,
	ACH_FUNC_TM_10,
	ACH_FUNC_TM_30,
	ACH_FUNC_TM_50,
	ACH_FUNC_ALL_OTHERS,
	ACH_FUNC_SURF_PIKACHU,
	ACH_FUNC_PARTY_ALL_LEVEL_100,
	ACH_FUNC_PARTY_SAME_TYPE,
	ACH_FUNC_NICKNAME,
	ACH_FUNC_MEGA_RING,
	ACH_FUNC_MEGA_STONE,
	ACH_FUNC_Z_RING,
	ACH_FUNC_Z_CRYSTAL,
	ACH_FUNC_DYNAMAX_BAND,
	ACH_FUNC_GIGANTAMAX,
	ACH_FUNC_TERA_ORB,
	ACH_FUNC_HIDDEN_ABILITY,
	ACH_FUNC_MAX_FRIENDSHIP,
	ACH_FUNC_POKERUS,
	ACH_FUNC_PERFECT_IV,
	ACH_FUNC_MAX_EV,
	ACH_FUNC_SHINY_CHARM,
	ACH_FUNC_OVAL_CHARM,
	ACH_FUNC_ALL_REGIONS,
	//Species sets (sAchSpeciesSets in achievements.c): caught all of them; progress shows how many
	ACH_FUNC_EEVEELUTIONS,
	ACH_FUNC_WEATHER_TRIO,
	ACH_FUNC_REGI_TRIO,
	ACH_FUNC_LAKE_TRIO,
	ACH_FUNC_CREATION_TRIO,
	ACH_FUNC_TAO_TRIO,
	ACH_FUNC_XY_DUO,
	ACH_FUNC_SUN_MOON_DUO,
	ACH_FUNC_HERO_DUO,
	ACH_FUNC_PSEUDO_LEGENDS,
	ACH_FUNC_ALL_STARTERS,
	ACH_FUNC_ULTRA_BEAST, //Any one of them
};

struct Achievement
{
	const u8* name;
	const u8* description;
	u8 tier;
	u8 kind;
	u16 param;
	u32 target;
	u8 rewardType; //enum AchievementRewardType
	u8 rewardAmount;
	u16 rewardId;
	u16 rewardHeldItem;
};

//Exported Functions
u16 Achievements_GetCount(void);
const struct Achievement* Achievements_Get(u16 id);
bool8 Achievements_IsUnlocked(u16 id);
u16 Achievements_CountUnlocked(void);
bool8 Achievements_Unlock(u16 id);
u32 Achievements_GetProgress(u16 id);
void Achievements_CheckAll(bool8 includeSlowChecks);

//Functions Hooked In
bool8 Achievements_FieldFrameUpdate(void);
bool8 StartMenuAchievementsCallback(void);

//Called from scripts via callasm
void AchievementsPreparePopup(void);
void AchievementsGetCountForScript(void);
void AchievementsPrepareReward(void);
void AchievementsPrepareAutoReward(void);
void AchievementsPrepareGift(void);
void AchievementsPrepareAutoGift(void);
void AchievementsMarkGiftDone(void);
void AchievementsPrepareNpcDialog(void);

//Script
extern const u8 EventScript_AchievementUnlocked[];
