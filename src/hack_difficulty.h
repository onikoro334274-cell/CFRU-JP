#pragma once

#include "../include/global.h"

// Existing save values retain their meaning. Menu order is easy to hardest.
enum HackDifficulty
{
    DIFFICULTY_NORMAL,
    DIFFICULTY_EASY,
    DIFFICULTY_MEDIUM,
    DIFFICULTY_HARD,
    DIFFICULTY_MOMO_ICE,
    DIFFICULTY_COUNT,
};

enum HackAIRank
{
    AI_RANK_BASIC = 0,
    AI_RANK_SEMI_SMART = 2,
    AI_RANK_STANDARD = 4,
    AI_RANK_ADVANCED = 8,
    AI_RANK_EXPERT = 14,
    AI_RANK_MAX = 24,
};

struct AIRankContext
{
    u16 difficulty;
    u8 level;
    bool8 importantTrainer;
    bool8 lowHp;
    bool8 statused;
    bool8 prolongedBattle;
    bool8 boostedFoe;
    bool8 wild;
};

u8 HackAI_GetRank(const struct AIRankContext* context);
u8 HackAI_GetBattlerRank(u8 bank);
u8 HackAI_GetTrainerRank(u8 bank);
u8 HackAI_GetWildRank(u8 bank);
u32 HackAI_TrainerFlags(u32 flags, u8 bank);
u32 HackAI_WildFlags(u32 flags, u8 bank);

static inline u8 HackDifficulty_SelectionFromValue(u16 value)
{
    switch (value)
    {
    case DIFFICULTY_EASY: return 0;
    case DIFFICULTY_MEDIUM: return 1;
    case DIFFICULTY_HARD: return 3;
    case DIFFICULTY_MOMO_ICE: return 4;
    default: return 2;
    }
}

static inline u8 HackDifficulty_ValueFromSelection(u16 selection)
{
    switch (selection)
    {
    case 0: return DIFFICULTY_EASY;
    case 1: return DIFFICULTY_MEDIUM;
    case 3: return DIFFICULTY_HARD;
    case 4: return DIFFICULTY_MOMO_ICE;
    default: return DIFFICULTY_NORMAL;
    }
}
