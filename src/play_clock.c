#include "defines.h"
#include "../include/event_data.h"
#include "../include/item_use.h"
#include "../include/script.h"
#include "../include/task.h"
#include "Tables/hoenn_script_flags.h"

/*
    プレイ時間の時計（2026-10-01）

    ユーザー「CFRU の RTC をリアルタイム形式からプレイ時間形式に変更できるようにしたい」「時間変更アイテム（絵は教えテレビ）」
    「プレイ時間形式は 2 時間ごとに昼夜が動くように」→（AskUserQuestion）「1 日＝プレイ 4 時間」「時間変更アイテムで切り替え」
    「最初から持っている」。

    ## 仕組み
      CFRU は時計（gClock・0x3005EF0）を src/rtc.c の RtcCalcLocalTime だけで書き（1 秒に 1 回）、朝昼夜・進化・野生・
      画面の色・日替わりの出来事は全部 gClock を読む。プレイ時間の時計のときは、RTC から作った gClock を PlayClock_Fill で
      上書きする（src/rtc.c）。
      ・ゲームの時刻 = プレイ時間 × 6（1 日 = プレイ 4 時間・昼がおよそ 2 時間・夜が 2 時間）＋ ずらし
      ・日付は 2000 年 1 月 1 日（土曜）から数える。ずらしは前へしか進めない（日替わりの出来事が日付の戻りで止まらないように・
        src/scripting.c の CheckAndSetDailyEvent は過去の日付では何もしない）
      ・プレイ時間は RTC の秒ではなく画面の書き換え 60 回で 1 秒（src/rtc.c の PlayTimeCounter_Update。RTC の秒を待つと、
        時計がプレイ時間から出来ているので互いに待ち合って止まる）

    ## 状態（セーブに残る予約変数・build_hoenn_scripts.RESERVED_VARS）
      HOENN_VAR_CLOCK_MIN（0x5148）… ずらし（分・0〜1439）
      HOENN_VAR_CLOCK_DAYS（0x5149）… 上位 1 ビット＝プレイ時間の時計か・下 15 ビット＝ずらし（日）

    ## 時間変更アイテム（タイムテレビ・ITEM_TIME_TV・絵は教えテレビ）
      assembly/play_clock/play_clock_scripts.s（tools/clock/build_play_clock.py が生成）が callasm で下の関数を呼ぶ
*/

#define CLOCK_MODE_BIT 0x8000
#define CLOCK_DAYS_MASK 0x7FFF
#define CLOCK_SPEED 6                 /* ゲームの時計はプレイ時間の 6 倍 */
#define MINUTES_PER_DAY 1440

void DirectClockUpdate(void);                                 /* src/rtc.c（include/rtc.h と同じ宣言） */

static const u8 sTimeOfDayStart[] = {4, 8, 17, 20};          /* あさ・ひる・ゆうがた・よる（config.h の TIME_*_START と同じ） */
static const u8 sDaysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

bool8 PlayClock_IsOn(void)
{
	return (VarGet(HOENN_VAR_CLOCK_DAYS) & CLOCK_MODE_BIT) != 0;
}

/* プレイ時間 → ゲームの秒（ずらし抜き） */
static u32 PlayClock_GameSeconds(void)
{
	u32 s = (u32) gSaveBlock2->playTimeHours * 3600 + gSaveBlock2->playTimeMinutes * 60 + gSaveBlock2->playTimeSeconds;
	return s * CLOCK_SPEED;
}

static bool8 IsLeapYear(u32 y)
{
	return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

void PlayClock_Fill(void)
{
	u32 gsec = PlayClock_GameSeconds();
	u32 gmin = gsec / 60 + VarGet(HOENN_VAR_CLOCK_MIN);
	u32 days = gmin / MINUTES_PER_DAY + (VarGet(HOENN_VAR_CLOCK_DAYS) & CLOCK_DAYS_MASK);
	u32 mod = gmin % MINUTES_PER_DAY;
	u32 y = 2000, m = 0, left = days;

	while (left >= (IsLeapYear(y) ? 366u : 365u))
	{
		left -= IsLeapYear(y) ? 366 : 365;
		++y;
	}
	for (;;)
	{
		u32 dim = sDaysInMonth[m] + (m == 1 && IsLeapYear(y) ? 1 : 0);
		if (left < dim)
			break;
		left -= dim;
		++m;
	}

	gClock.year = y;
	gClock.month = m + 1;
	gClock.day = left + 1;
	gClock.dayOfWeek = (days + 6) % 7;     /* 2000 年 1 月 1 日は土曜（0 = 日曜） */
	gClock.hour = mod / 60;
	gClock.minute = mod % 60;
	gClock.second = gsec % 60;
}

/* callasm: LASTRESULT = 1（プレイ時間の時計）/ 0（リアルタイム） */
void PlayClock_GetMode(void)
{
	gSpecialVar_LastResult = PlayClock_IsOn();
}

/* callasm: 0x8004 = 1 でプレイ時間の時計・0 でリアルタイムへ。
   日付が大きく飛ぶ（RTC の年 ⇔ 2000 年から）ので、日替わりの記録（群れ・VAR_SWARM_DAILY_EVENT と次の変数）を消して、
   次に時計を読んだときに新しい日として扱わせる */
void PlayClock_SetMode(void)
{
	u16 days = VarGet(HOENN_VAR_CLOCK_DAYS);

	if (Var8004)
		days |= CLOCK_MODE_BIT;
	else
		days &= ~CLOCK_MODE_BIT;
	VarSet(HOENN_VAR_CLOCK_DAYS, days);
	VarSet(VAR_SWARM_DAILY_EVENT, 0);
	VarSet(VAR_SWARM_DAILY_EVENT + 1, 0);
	DirectClockUpdate();
}

/* callasm: 0x8004 = 0 あさ / 1 ひる / 2 ゆうがた / 3 よる。その時間帯の始まりまで時計を**先へ**進める */
void PlayClock_SetTimeOfDay(void)
{
	u32 target, now, delta, m;
	u16 days;

	if (Var8004 >= NELEMS(sTimeOfDayStart) || !PlayClock_IsOn())
		return;
	target = sTimeOfDayStart[Var8004] * 60;
	now = (PlayClock_GameSeconds() / 60 + VarGet(HOENN_VAR_CLOCK_MIN)) % MINUTES_PER_DAY;
	delta = (target + MINUTES_PER_DAY - now) % MINUTES_PER_DAY;
	m = VarGet(HOENN_VAR_CLOCK_MIN) + delta;
	days = VarGet(HOENN_VAR_CLOCK_DAYS);
	days = (days & CLOCK_MODE_BIT) | (((days & CLOCK_DAYS_MASK) + m / MINUTES_PER_DAY) & CLOCK_DAYS_MASK);
	VarSet(HOENN_VAR_CLOCK_MIN, m % MINUTES_PER_DAY);
	VarSet(HOENN_VAR_CLOCK_DAYS, days);
	DirectClockUpdate();
}

/* ---- タイムテレビ（ITEM_TIME_TV）: バッグで「つかう」→ バッグを閉じてスクリプト（src/music_player.c と同じ形）---- */
extern const u8 EventScript_TimeTV[];

static void Task_TimeTVField(u8 taskId)
{
	ScriptContext1_SetupScript(EventScript_TimeTV);
	DestroyTask(taskId);
}

void FieldUseFunc_TimeTV(u8 taskId)
{
	sItemUseOnFieldCB = Task_TimeTVField;
	SetUpItemUseOnFieldCallback(taskId);
}
