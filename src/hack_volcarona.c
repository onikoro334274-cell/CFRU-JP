#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/constants/abilities.h"
#include "../include/constants/moves.h"
#include "../include/constants/species.h"
#include "../include/constants/hold_effects.h"
#include "../include/battle.h"
#include "../include/new/build_pokemon.h"
#include "../include/new/catching.h"
#include "../include/new/learn_move.h"
#include "../include/new/battle_util.h"
#include "../include/new/util.h"
#include "hack_volcarona.h"

/*
 * 特別なウルガモス（#95・2026-10-11）。詳しくは hack_volcarona.h と docs/research/2026-10-11_volcarona-sun-avatar.md。
 * 状態は持たない（6.13）: 特別かどうかは毎回ポケモンの中身（種族＋印＋親）で決める。
 */

bool8 HackMark_IsPlayersMarkedMon(const struct Pokemon* mon);   //src/hack_gible_egg.c
u16 SecondAbility_OfBank(u8 bank);                               //src/second_ability.c（Lv50 未満・特性が封じられていれば NONE）

bool8 HackVolcarona_IsSpecial(const struct Pokemon* mon)
{
	if (mon == NULL)
		return FALSE;
	if (GetMonData((struct Pokemon*) mon, MON_DATA_SPECIES, NULL) != SPECIES_VOLCARONA)
		return FALSE;
	return HackMark_IsPlayersMarkedMon(mon);
}

u16 HackVolcarona_SpecialAbility(const struct Pokemon* mon, u16 species)
{
	if (species == SPECIES_VOLCARONA && HackVolcarona_IsSpecial(mon))
		return ABILITY_SUN_AVATAR;
	return ABILITY_NONE;
}

u16 HackVolcarona_SecondAbility(u16 species, const struct Pokemon* mon)
{
	if (species == SPECIES_VOLCARONA && HackVolcarona_IsSpecial(mon))
		return ABILITY_SUN_BUTTERFLY;
	return ABILITY_NONE;
}

//晴れのとき 2 つ目の特性が働いているか（Lv50 から・ばんのうがさ を持っていれば効かない・ほかの特性に写らない＝ability_tables.s）
static bool8 ButterflyOn(u8 bank)
{
	if (!BATTLER_ALIVE(bank) || SecondAbility_OfBank(bank) != ABILITY_SUN_BUTTERFLY)
		return FALSE;
	if (!WEATHER_HAS_EFFECT || !(gBattleWeather & WEATHER_SUN_ANY))
		return FALSE;
	return ITEM_EFFECT(bank) != ITEM_EFFECT_UTILITY_UMBRELLA;
}

bool8 HackVolcarona_ButterflyBoost(u8 bank)
{
	return ButterflyOn(bank);
}

u32 HackVolcarona_BoostSpeed(u8 bank, u32 speed)
{
	return ButterflyOn(bank) ? (speed * 15) / 10 : speed;
}

//にほんばれ などの 5 ターンの晴れを、場に 2 つ目が働く子がいる間は減らさない（特性の晴れは INFINITE_WEATHER で元から永続）
bool8 HackVolcarona_HoldsSun(void)
{
	u8 i;

	for (i = 0; i < gBattlersCount; ++i)
	{
		if (ButterflyOn(i))
			return TRUE;
	}
	return FALSE;
}

//callasm（assembly/hack_volcarona/volcarona_scripts.s）: Lv50・6V・プレシャスボール・技 4 つ・印 → 手持ちかボックスへ。LASTRESULT は GiveMonToPlayer の値
void HackVolcarona_Give(void)
{
	struct Pokemon mon;
	static const u16 moves[MAX_MON_MOVES] = {MOVE_ENBUCHO, MOVE_QUIVERDANCE, MOVE_BUGBUZZ, MOVE_HEATWAVE};
	u8 mark = TRUE;
	u8 ball = BALL_TYPE_CHERISH_BALL;
	u8 i;

	CreateMon(&mon, SPECIES_VOLCARONA, VOLCARONA_GIFT_LEVEL, 31, FALSE, 0, OT_ID_PLAYER_ID, 0);   //個体値はぜんぶ 31
	for (i = 0; i < MAX_MON_MOVES; ++i)
		SetMonMoveSlot(&mon, moves[i], i);
	SetMonData(&mon, MON_DATA_OBEDIENCE, &mark);     //特別な個体の印（HackMark_IsPlayersMarkedMon）
	SetMonData(&mon, MON_DATA_POKEBALL, &ball);
	CalculateMonStats(&mon);
	SetMonPokedexFlags(&mon);
	gSpecialVar_LastResult = GiveMonToPlayer(&mon);
}
