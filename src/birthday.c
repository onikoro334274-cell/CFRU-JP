#include "defines.h"
#include "../include/new/birthday.h"
#include "../include/event_data.h"
#include <stddef.h>

#define BIRTHDAY_SIGNATURE 0x31445942u /* BYD1: identifies this save format */

/* Keep the native sector length and every following field unchanged. */
_Static_assert(sizeof(struct PlayerBirthdaySaveData) == 8, "birthday save size");
_Static_assert(offsetof(struct SaveBlock2, playerBirthday) == 0xF18, "birthday save offset");
_Static_assert(offsetof(struct SaveBlock2, encryptionKey) == 0xF20, "save encryption key offset");
_Static_assert(sizeof(struct SaveBlock2) == 0xF24, "native save sector size");

static const u8 sBirthdayMonthLengths[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

static bool8 Birthday_IsValidMonthDay(u16 month, u16 day)
{
	return month >= 1 && month <= 12 && day >= 1 && day <= sBirthdayMonthLengths[month - 1];
}

bool8 Birthday_Set(u16 month, u16 day)
{
	struct PlayerBirthdaySaveData *saved;
	u16 packed;

	if (gSaveBlock2 == NULL || !Birthday_IsValidMonthDay(month, day))
		return FALSE;
	saved = &gSaveBlock2->playerBirthday;
	packed = (month << 8) | day;
	saved->signature = 0;
	saved->monthDay = packed;
	saved->monthDayComplement = (u16)~packed;
	saved->signature = BIRTHDAY_SIGNATURE;
	return TRUE;
}

bool8 Birthday_Get(u8 *month, u8 *day)
{
	const struct PlayerBirthdaySaveData *saved;
	u16 m, d;

	if (gSaveBlock2 == NULL)
		return FALSE;
	saved = &gSaveBlock2->playerBirthday;
	if (saved->signature != BIRTHDAY_SIGNATURE
	 || saved->monthDayComplement != (u16)~saved->monthDay)
		return FALSE;
	m = saved->monthDay >> 8;
	d = saved->monthDay & 0xFF;
	if (!Birthday_IsValidMonthDay(m, d))
		return FALSE;
	if (month != NULL)
		*month = m;
	if (day != NULL)
		*day = d;
	return TRUE;
}

void Birthday_Clear(void)
{
	if (gSaveBlock2 != NULL)
	{
		gSaveBlock2->playerBirthday.signature = 0;
		gSaveBlock2->playerBirthday.monthDay = 0;
		gSaveBlock2->playerBirthday.monthDayComplement = 0;
	}
}

bool8 Birthday_IsOnDate(u16 year, u16 month, u16 day)
{
	u8 birthdayMonth, birthdayDay;

	if (!Birthday_IsValidMonthDay(month, day))
		return FALSE;
	if (month == 2 && day == 29 && !(year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
		return FALSE;
	return Birthday_Get(&birthdayMonth, &birthdayDay) && birthdayMonth == month && birthdayDay == day;
}

bool8 Birthday_IsToday(void)
{
	return Birthday_IsOnDate(gClock.year, gClock.month, gClock.day);
}

void Birthday_SetFromScript(void)
{
	gSpecialVar_LastResult = Birthday_Set(Var8004, Var8005);
}

void Birthday_GetForScript(void)
{
	u8 month = 0, day = 0;
	gSpecialVar_LastResult = Birthday_Get(&month, &day);
	Var8004 = month;
	Var8005 = day;
}

void Birthday_CheckTodayFromScript(void)
{
	gSpecialVar_LastResult = Birthday_IsToday();
}

void Birthday_ClearFromScript(void)
{
	Birthday_Clear();
	gSpecialVar_LastResult = gSaveBlock2 != NULL;
}
