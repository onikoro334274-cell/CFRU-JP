#pragma once
#include "../global.h"

/* Policy for random wild bosses. Spawn selection and battle lifecycle are
 * separate from these helpers. Percentages use integer arithmetic. */
struct WildBossPolicy
{
    u16 statBonusPercent;
    u8 allyCallPercent;
    u8 catchHpPercent;
};

const struct WildBossPolicy* WildBoss_GetPolicy(u16 difficulty);
u16 WildBoss_BoostStat(u16 stat, u16 difficulty);
bool8 WildBoss_CanCatch(u16 hp, u16 maxHp, u16 difficulty);
/* roll must be uniformly sampled in 0..99; invalid rolls return FALSE. */
bool8 WildBoss_ShouldCallAlly(u8 roll, u16 difficulty);
