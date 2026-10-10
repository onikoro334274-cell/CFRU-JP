#include "defines.h"
#include "../include/data2.h"
#include "../include/event_data.h"
#include "../include/new/catching.h"
#include "../include/pokemon.h"
#include "../include/pokedex.h"
#include "../include/random.h"
#include "../include/string_util.h"
#include "../include/constants/species.h"
#include "hack_mon_id.h"

/*
 * ★ 2026-10-10 ユーザー「バトルフロンティアショップ ... もえもんのかせきを」→「復元すると確定でmoemonドット絵が手に入る」「1 つの道具でランダム」
 *
 * Moemonかせき（ITEM_MOEMON_FOSSIL 0x34B・バトルフロンティアのショップ）を グレンの研究所の かせきの研究員（assembly/kanto/kanto_moemon_fossil.s・
 * repoints 083807D4）が その場で よみがえらせる。化石ポケモン 15 種のどれか・Lv 5（素の化石と同じ）・裏 ID を Moémon の当たり
 * （HackMonId_MakeMoemon・色違いは今のまま）。15 種とも Moémon の絵があり、絵は性別を問わない（tools/sprites の表で確かめた・2026-10-10）。
 * 見えるのは「せってい」の Moemon が だす のとき（オミット版は絵が無いので ふつうの化石ポケモン）。
 * LASTRESULT: 0 手持ち / 1 パソコン / 2 どちらもいっぱい（道具は減らさない）。gStringVar1 = 種族名
 */

static const u16 sMoemonFossilSpecies[] =
{
	SPECIES_OMANYTE, SPECIES_KABUTO, SPECIES_AERODACTYL, SPECIES_LILEEP, SPECIES_ANORITH,
	SPECIES_CRANIDOS, SPECIES_SHIELDON, SPECIES_TIRTOUGA, SPECIES_ARCHEN, SPECIES_TYRUNT,
	SPECIES_AMAURA, SPECIES_DRACOZOLT, SPECIES_ARCTOZOLT, SPECIES_DRACOVISH, SPECIES_ARCTOVISH,
};

#define MOEMON_FOSSIL_LEVEL 5

static u32 RandomFossilIndex(void)
{
	u32 value;
	const u32 count = ARRAY_COUNT(sMoemonFossilSpecies);
	const u32 limit = 0x10000u - 0x10000u % count;
	do { value = Random(); } while (value >= limit);
	return value % count;
}

void HackMoemonFossil_Revive(void)
{
	struct Pokemon mon;
	u16 species = sMoemonFossilSpecies[RandomFossilIndex()];
	u8 result;

	CreateMon(&mon, species, MOEMON_FOSSIL_LEVEL, 32, FALSE, 0, OT_ID_PLAYER_ID, 0);
	HackMonId_MakeMoemon((struct BoxPokemon*) &mon);
	result = GiveMonToPlayer(&mon);
	gSpecialVar_LastResult = result;
	StringCopy(gStringVar1, gSpeciesNames[species]);
	if (result < 2)
	{
		HandleSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_SEEN, mon.personality);
		HandleSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_CAUGHT, mon.personality);
	}
}
