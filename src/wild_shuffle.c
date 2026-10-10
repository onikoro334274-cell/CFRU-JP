#include "defines.h"
#include "../include/event_data.h"
#include "../include/wild_encounter.h"
#include "../include/new/dns.h"
#include "../include/battle_setup.h"

#include "Tables/hoenn_script_flags.h"
#include "hack_profile.h"

/*
wild_shuffle.c
    強化リーグに勝ったあとの野生（2026-10-02）

    ユーザー「強化リーグに勝つとでてくる野生をランダムに他世代のポケモンがでるようにしてほしい
    カントーの強化リーグに勝つとカントーの野生に他世代がでる ホウエンならホウエンの野生に他世代がでる
    簡単にいえば出現ポケモン表の入れ替えって感じ」

    ・入れ替えた表は tools/rom/build_wild_shuffle.py が作る（src/Tables/wild_encounter_tables.c の末尾の WILD SHUFFLE の囲み）。
      {元の表, 入れ替えた表, 地方} の組。元の表は C の表（朝昼夕夜）と、C の昼の表に無い地図の素 ROM の表
    ・LoadProperMonsData（src/wild_encounter.c）が返す表をここで差し替える（遭遇・ずかんナビ・釣り・草むらの判定がすべてそこを通る）
    ・カントー: 強化リーグ（ななしまのあとのチャンピオン戦）の殿堂入り（KANTO_FLAG_WILD_SHUFFLE・src/hoenn_league.c）
      ホウエン: 2 回目の制覇（HOENN_FLAG_ISLANDS_OPEN）
    ・入れ替えるのは朝と夜だけ（2026-10-02 ユーザー「よる朝をランダムに昼夕をもともとで」）。C の昼・夕方の表は組を作っていない。
      素 ROM の表は時間帯の区別が無いので、ここで時間を見る
    ★ 状態は持たない（CLAUDE.md 6.13）。印が立っていなければ何もしない（表を探さない）
    ★ 2026-10-09 ユーザー「カントー全域に最初から他地方ポケモンでるように　時間帯は朝と夜　テーブル何個か作って日付と時間でランダムに」:
      カントーは最初から入れ替える（強化リーグの印は見ない）。カントーの表は HACK_WILD_KANTO_VARIANTS 通りあり、
      日付と朝・夜から 1 つを選ぶ（KantoVariant）。夜の 0〜3 時は前の日の夜として数える（夜の途中で入れ替わらない）
    ★ 2026-10-10 ユーザー「パッチごとにランダムでカントー全域の野生テーブルを切り替えてほしい」（判断「朝・昼・夕・夜 全部」「ホウエンも」）:
      入れ替えるのは朝・昼・夕・夜の全部（素 ROM の表も時間を見ない）。通りは日付と 4 つの時間帯で選ぶ。
      表の中身は配る版ごとに変わる（tools/rom/build_wild_shuffle.py の乱数の種が make_demo_bundle.py の VERSION）
*/

#define HACK_WILD_KANTO_VARIANTS 4      //tools/rom/build_wild_shuffle.py の KANTO_VARIANTS と同じ数

struct HackWildShufflePair { const struct WildPokemonInfo* orig; const struct WildPokemonInfo* shuffled; u8 region; u8 variant; };
extern const struct HackWildShufflePair gHackWildShufflePairs[];
extern const u16 gHackWildShufflePairCount;

#define WILD_SHUFFLE_REGION_KANTO 0
#define WILD_SHUFFLE_REGION_HOENN 1

//★ 2026-10-05 ユーザー「昔のセーブでも強化リーグのチャンピオンに勝った記録があれば入れ替えが効くように」:
//  印 0x12FF は 2026-10-02 から殿堂入りで立てる。それより前に強化リーグに勝ったセーブは印が無いので、記録で見る。
//  ★ チャンピオンの再戦（739〜741）の記録は殿堂入りで消える（pokefirered data/scripts/hall_of_fame.inc 33〜35）ので使えない
//    （実機のセーブで 3 つとも 0 だった）。消されないワタルの 2 回目（TRAINER_ELITE_FOUR_LANCE_2 738）に勝った記録で見る
#define TRAINER_ELITE_FOUR_LANCE_2 738

static __attribute__((unused)) bool8 KantoStrongLeagueCleared(void)   //★ 2026-10-09 カントーは最初から入れ替えるので使わない（戻すときのために残す）
{
	return FlagGet(KANTO_FLAG_WILD_SHUFFLE) || HasTrainerBeenFought(TRAINER_ELITE_FOUR_LANCE_2);
}

static u32 PreviousWildDate(u32 year, u32 month, u32 day)
{
	static const u8 monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	u32 value = year * 372u + month * 31u + day;
	u32 previousDays;
	if (day != 1 || month < 2 || month > 12)
		return value - 1;
	previousDays = monthDays[month - 2];
	if (month == 3 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))
		++previousDays;
	return year * 372u + (month - 1) * 31u + previousDays;
}

//日付と時間帯（朝 0・昼 1・夕 2・夜 3）→ 通りの番号（0〜HACK_WILD_KANTO_VARIANTS-1）
static u8 KantoVariant(void)
{
	u32 day = gClock.year * 372u + gClock.month * 31u + gClock.day;
	u32 slot = IsNightTime() ? 3u : IsEvening() ? 2u : IsMorning() ? 0u : 1u;
	u32 x;

	if (IsNightTime() && gClock.hour < TIME_MORNING_START)
		day = PreviousWildDate(gClock.year, gClock.month, gClock.day);
	x = day * 4u + slot;
	x = (x ^ 0x5A17u) * 0x9E3779B1u;
	x ^= x >> 15;
	x *= 0x2C1B3C6Du;
	x ^= x >> 13;
	return x % HACK_WILD_KANTO_VARIANTS;
}

struct HackWildShuffleIndex { u16 map; u16 start; u16 count; };
extern const struct HackWildShuffleIndex gHackWildShuffleIndex[];
extern const u16 gHackWildShuffleIndexCount;

//対応表の [start, end) から元の表 info を探す。見つかれば TRUE（*out = 入れ替えた表か、地方が対象でなければ元の表）。前の探し方と同じ決まり
static bool8 FindShuffled(const struct WildPokemonInfo* info, u8 variant, bool8 kanto, bool8 hoenn, u32 start, u32 end,
                          const struct WildPokemonInfo** out)
{
	for (u32 i = start; i < end; ++i)
	{
		if (gHackWildShufflePairs[i].orig == info)
		{
			u8 region = gHackWildShufflePairs[i].region;
			if ((region == WILD_SHUFFLE_REGION_KANTO && kanto) || (region == WILD_SHUFFLE_REGION_HOENN && hoenn))
			{
				if (gHackWildShufflePairs[i].variant == variant)
				{
					*out = gHackWildShufflePairs[i].shuffled;
					return TRUE;
				}
				continue;               //同じ元の表の通りは並んでいる
			}
			*out = info;
			return TRUE;
		}
	}
	return FALSE;
}

#ifdef HACK_PROFILE
//計測用のビルドだけ: 1 回の時間を走査線の数で（0x0203F800 + 4 * 370 から 呼んだ回数・合計・最大・最後に見つけた位置）
static const struct WildPokemonInfo* HackWild_ShuffleInner(const struct WildPokemonInfo* info);
const struct WildPokemonInfo* HackWild_Shuffle(const struct WildPokemonInfo* info)
{
	u32* p = (u32*) 0x0203F800;
	u32 t0 = HackProfile_Now();
	const struct WildPokemonInfo* r = HackWild_ShuffleInner(info);
	u32 t1 = HackProfile_Now();
	u32 lines = t1 - t0;
	p[370]++;
	p[371] += lines;
	if (lines > p[372])
		p[372] = lines;
	return r;
}
static const struct WildPokemonInfo* HackWild_ShuffleInner(const struct WildPokemonInfo* info)
#else
const struct WildPokemonInfo* HackWild_Shuffle(const struct WildPokemonInfo* info)
#endif
{
	bool8 kanto, hoenn;

	if (info == NULL)
		return NULL;
	//★ 2026-10-10 せいそくスイッチ（src/hack_habitat_switch.c）を入れていれば、カントー・ホウエンとも元の生息地
	if (FlagGet(HACK_FLAG_HABITAT_ORIGINAL))
		return info;

	kanto = TRUE;                      //★ 2026-10-09 カントーは最初から（もとは KantoStrongLeagueCleared()）
	hoenn = TRUE;                       //★ 2026-10-09 ユーザー「ホウエンもそんな感じでテーブルおねがい」: ホウエンも最初から・同じ 4 通り（もとは FlagGet(HOENN_FLAG_ISLANDS_OPEN)）
#ifdef WILD_SHUFFLE_DEBUG
	/* ★ 2026-10-02 ユーザー「kanto = FlagGet(KANTO_FLAG_WILD_SHUFFLE);のままで」「デバッグフラグでおねがい」:
	   デバッグ（2 つの config.h・デモ版では切る）なら、強化リーグに勝っていなくてもカントー・ホウエンとも入れ替える */
	kanto = hoenn = TRUE;
#endif
	if (!kanto && !hoenn)
		return info;

	u8 variant = KantoVariant();
	const struct WildPokemonInfo* found;

	//★ 2026-10-10 重さ（patches/0281）: 前は 6,008 件を毎回頭から探して 1 回平均 51 本（1 コマ 228 本）・草むらの 1 歩で 2〜3 回呼ばれ
	//  1 歩で 100〜150 本＝草むらで 2 割のコマが処理落ちしていた。目次（地図 → 対応表の範囲・地図の順・tools/rom/build_wild_shuffle.py）を
	//  二分探索して今いる地図の範囲だけ探す。見つからなければ（ほかの地図の表を引かれたとき）今までどおり全部を探す＝結果は前と同じ
	u16 map = ((u8) gSaveBlock1->location.mapGroup << 8) | (u8) gSaveBlock1->location.mapNum;
	u32 lo = 0, hi = gHackWildShuffleIndexCount;
	while (lo < hi)
	{
		u32 mid = (lo + hi) / 2;
		if (gHackWildShuffleIndex[mid].map < map)
			lo = mid + 1;
		else
			hi = mid;
	}
	if (lo < gHackWildShuffleIndexCount && gHackWildShuffleIndex[lo].map == map
	&& FindShuffled(info, variant, kanto, hoenn, gHackWildShuffleIndex[lo].start,
	                gHackWildShuffleIndex[lo].start + gHackWildShuffleIndex[lo].count, &found))
		return found;

	if (FindShuffled(info, variant, kanto, hoenn, 0, gHackWildShufflePairCount, &found))
		return found;
	return info;
}
