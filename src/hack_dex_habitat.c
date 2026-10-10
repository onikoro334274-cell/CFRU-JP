#include "defines.h"
#include "../include/pokemon.h"

/*
 * 捕まえたときの図鑑の画面がコラッタになる（2026-10-10・ユーザー「特定Noのポケモンを初ゲットした際に図鑑画面がコラッタになる」）。
 *
 * 素の DexScreen_RegisterMonToPokedex（081076B8）は DexScreen_LookUpCategoryBySpecies で「すみか」の表からその種族のページを探し、
 * 見つからないと何も入れずに進む＝草原の 1 ページ目（コラッタ）を開く（pokefirered src/pokedex_screen.c 3260 行）。
 * 戦闘の displaydexinfo は姿違いの種族の番号のまま渡すので（メガ・キョダイ・テラ・フォルムは CFRU の
 * atkF1_trysetcaughtmondexflags が先に戻す）、アンノーンの B・性別の姿・ヒスイの姿などは表に無くてコラッタになる。
 *   ・表に無い基本の姿（第 9 世代・ヒスイの進化 127 種）は tools/dex/build_habitat_extra.py が DPE の表に足す
 *   ・ここ: 表に無い種族は、同じ全国図鑑の番号の基本の姿（NationalPokedexNumToSpecies・DPE）に読み替える
 * 入口の差し込みは assembly/hack_dex/dex_hooks.s（hooks 081076B8）。見た・捕まえたの印は全国図鑑の番号で付くので、
 * 基本の姿に読み替えても同じ番号に付く。
 *
 * 表の場所は DPE が決める（CFRU からは名前で引けない）。素の DexScreen_LookUpCategoryBySpecies のリテラル 081073E0 を
 * DPE の repointall が gHabitatPtrsTable に向け直しているので、その語を毎回読む。すみかは 9 つ（素の gDexCategories と同じ）。
 */

struct HackHabitatPage
{
	const u16 *mons;
	u32 numMons;
};

struct HackHabitat
{
	const struct HackHabitatPage *page;
	u32 numPages;
};

#define DEX_HABITATS (*(const struct HackHabitat * const *) 0x081073E0)
#define DEX_HABITAT_COUNT 9

static bool8 IsInHabitatTable(u16 species)
{
	const struct HackHabitat *habitats = DEX_HABITATS;
	u32 i, j, k;

	for (i = 0; i < DEX_HABITAT_COUNT; i++)
	{
		for (j = 0; j < habitats[i].numPages; j++)
		{
			const struct HackHabitatPage *page = &habitats[i].page[j];

			for (k = 0; k < page->numMons; k++)
			{
				if (page->mons[k] == species)
					return TRUE;
			}
		}
	}
	return FALSE;
}

u16 HackDex_HabitatSpecies(u16 species)
{
	u16 base;

	if (IsInHabitatTable(species))
		return species;
	base = NationalPokedexNumToSpecies(SpeciesToNationalPokedexNum(species));
	if (base != SPECIES_NONE && IsInHabitatTable(base))
		return base;
	return species;
}
