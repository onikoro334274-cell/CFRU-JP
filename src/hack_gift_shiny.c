#include "defines.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/random.h"
#include "../include/constants/flags.h"
#include "../include/constants/species.h"
#include "../include/constants/maps.h"

/*
 * 最初のポケモン（オーキド研究所の 3 匹）とお母さんのピカチュウは 1/16 で色違い（2026-10-03）。
 * ユーザー「最初にもらえるポケモンの処理だけ色違いでやすいようにできる？ピカチュウも」→ 1/16。
 *   - 最初のポケモン: 素のスクリプトは setflag FLAG_SYS_POKEMON_GET → givemon（pokefirered
 *     PalletTown_ProfessorOaksLab/scripts.inc）。CFRU の CheckShinyMon（src/build_pokemon.c・patches/0134）が
 *     HackGiftShiny_IsStarterRoll を聞く。研究所で「旗が立っていて手持ちが空」のときだけ。
 *     ライバルのポケモンは手持ちができてから作られるので当たらない
 *   - ピカチュウ: src/kanto_mom_pikachu.c が CreateMon のあとに HackGiftShiny_Roll を聞いて ForceMonShiny
 *   色違いにするのは CFRU の ForceMonShiny（性格・性別は元のまま。色違いを通常色に戻す処理ではない・6.5）
 */

#define GIFT_SHINY_ODDS 16

bool8 HackGiftShiny_Roll(void)
{
	return (Random() % GIFT_SHINY_ODDS) == 0;
}

bool8 HackGiftShiny_IsStarterRoll(void)
{
	/* Empty-party creation can also occur elsewhere (e.g. temporary teams).
	 * Keep the starter bonus confined to the original gift location. */
	if (gSaveBlock1 == NULL || !MAP_IS(PALLET_TOWN_PROFESSOR_OAKS_LAB))
		return FALSE;
	if (!FlagGet(FLAG_SYS_POKEMON_GET))
		return FALSE;
	if (GetMonData(&gPlayerParty[0], MON_DATA_SPECIES, NULL) != SPECIES_NONE)
		return FALSE;
	return HackGiftShiny_Roll();
}
