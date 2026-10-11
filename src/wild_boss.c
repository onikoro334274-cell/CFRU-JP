#include "defines.h"
#include "hack_difficulty.h"
#include "../include/new/wild_boss.h"

static const struct WildBossPolicy sPolicies[DIFFICULTY_COUNT] =
{
    [DIFFICULTY_EASY] = {10, 10, 90},
    [DIFFICULTY_MEDIUM] = {30, 30, 60},
    [DIFFICULTY_NORMAL] = {50, 50, 40},
    [DIFFICULTY_HARD] = {100, 70, 20},
    [DIFFICULTY_MOMO_ICE] = {300, 100, 5},
};

const struct WildBossPolicy* WildBoss_GetPolicy(u16 difficulty)
{
    if (difficulty >= DIFFICULTY_COUNT)
        difficulty = DIFFICULTY_NORMAL;
    return &sPolicies[difficulty];
}

u16 WildBoss_BoostStat(u16 stat, u16 difficulty)
{
    u32 boosted = (u32)stat * (100 + WildBoss_GetPolicy(difficulty)->statBonusPercent) / 100;
    return boosted > 0xFFFF ? 0xFFFF : boosted;
}

bool8 WildBoss_CanCatch(u16 hp, u16 maxHp, u16 difficulty)
{
    if (hp == 0 || maxHp == 0 || hp > maxHp)
        return FALSE;
    return (u32)hp * 100 <= (u32)maxHp * WildBoss_GetPolicy(difficulty)->catchHpPercent;
}

bool8 WildBoss_ShouldCallAlly(u8 roll, u16 difficulty)
{
    return roll < 100 && roll < WildBoss_GetPolicy(difficulty)->allyCallPercent;
}
