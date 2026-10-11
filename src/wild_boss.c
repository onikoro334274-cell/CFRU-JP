#include "defines.h"
#include "hack_difficulty.h"
#include "../include/new/wild_boss.h"

static const struct WildBossPolicy sPolicies[DIFFICULTY_COUNT] =
{
    [DIFFICULTY_EASY] = {10, 10, 90, 1, 1},
    [DIFFICULTY_MEDIUM] = {30, 30, 60, 20, 2},
    [DIFFICULTY_NORMAL] = {50, 50, 40, 25, 2},
    [DIFFICULTY_HARD] = {100, 70, 20, 30, 3},
    [DIFFICULTY_MOMO_ICE] = {300, 100, 5, 40, 3},
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

bool8 WildBoss_ShouldSpawn(u8 roll, u16 difficulty)
{
    return roll < 100 && roll < WildBoss_GetPolicy(difficulty)->spawnPercent;
}

bool8 WildBoss_CanCallAlly(u8 roll, u8 successfulCalls, u16 difficulty)
{
    return successfulCalls < WildBoss_GetPolicy(difficulty)->allyCallLimit
        && WildBoss_ShouldCallAlly(roll, difficulty);
}
