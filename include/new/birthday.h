#pragma once

#include "../global.h"

/* Player birthday, month 1..12 and day 1..31. Invalid registration leaves
 * the previous date intact. February 29 is accepted and matches only on
 * February 29. Unregistered/invalid saves never match.
 * These entry points have no menu, event, or automatic caller. */
bool8 Birthday_Set(u16 month, u16 day);
bool8 Birthday_Get(u8 *month, u8 *day);
void Birthday_Clear(void);
bool8 Birthday_IsOnDate(u16 year, u16 month, u16 day);
bool8 Birthday_IsToday(void);

/* Optional callasm entry points for future scripts; not wired to events.
 * Set: 0x8004=month, 0x8005=day, LASTRESULT=success.
 * Get: LASTRESULT=registered, 0x8004/0x8005=month/day (0/0 if absent).
 * CheckToday: LASTRESULT=match using the currently populated gClock.
 * Clear: erase registration, LASTRESULT=1 if save memory is available. */
void Birthday_SetFromScript(void);
void Birthday_GetForScript(void);
void Birthday_CheckTodayFromScript(void);
void Birthday_ClearFromScript(void);
