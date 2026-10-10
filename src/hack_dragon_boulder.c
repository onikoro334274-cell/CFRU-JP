#include "defines.h"
#include "defines_battle.h"
#include "../include/constants/abilities.h"
#include "../include/new/battle_util.h"

/*
 * りゅうのいわお（ABILITY_DRAGONBOULDER 320・2026-10-07）。
 * ユーザー「ガブリアスZにふゆうの他に技の威力を2倍にする特性と岩タイプの追加とハードロック追加　なれるのは特別なガブリアスだけ」
 *   →（AskUserQuestion）特性 1 は りゅうのよろい のまま、特性 2 を りゅうのいわお（ふゆう＋技の威力 2 倍＋ハードロック）。名前は りゅうのいわお。
 *
 *   - 持つのは特別なガブリアス（クチバのフカマルのタマゴの子・src/hack_gible_egg.c）がガブリアスZ のときの 2 つ目の特性だけ
 *     （src/second_ability.c の SecondAbility_OfBank / SecondAbility_OfMon。2 つ目の特性なので Lv 50 から）
 *   - 2 つ目の特性はふだん「場に出たとき」だけ動くが、これは ずっと効く。CFRU の ふゆう・威力・ハードロックを見ている所に
 *     HackDragonBoulder_Bank / _Mon を足した（patches/0207: battle_util.c・damage_calc.c・set_effect.c）
 *   - ★ 同じ日 ユーザー「ふつうのガブリアスは特性ふゆうのガブリアスZになれる　とくべつなのはとくべつなのに」:
 *     ガブリアスZ にはだれでもなれる（ふつうは ドラゴン単タイプ・ふゆう＝Z-A のまま）。特別なガブリアスがZ のときだけ
 *     2 つ目のタイプを いわ に（HackDragonBoulder_Type2・form_change.c・switching.c・util.c GetMonType・move_menu.c の表示）
 */

u16 SecondAbility_OfBank(u8 bank);
u16 SecondAbility_OfMon(struct Pokemon* mon);
bool8 SecondAbility_IsUnlocked(u8 level);
u16 HackGibleEgg_SpecialAbility(const struct Pokemon* mon, u16 species);

bool8 HackDragonBoulder_Bank(u8 bank)
{
	return SecondAbility_OfBank(bank) == ABILITY_DRAGONBOULDER;
}

bool8 HackDragonBoulder_Mon(struct Pokemon* mon)
{
	return mon != NULL && SecondAbility_IsUnlocked(GetMonData(mon, MON_DATA_LEVEL, NULL))
	    && SecondAbility_OfMon(mon) == ABILITY_DRAGONBOULDER;
}

//特別なガブリアスのガブリアスZ の 2 つ目の特性（ほかは ABILITY_NONE）
u16 HackDragonBoulder_SecondAbility(const struct Pokemon* mon, u16 species)
{
	if (species == SPECIES_GARCHOMP_MEGA_Z && mon != NULL && HackGibleEgg_SpecialAbility(mon, species) != ABILITY_NONE)
		return ABILITY_DRAGONBOULDER;
	return ABILITY_NONE;
}

//2 つ目のタイプ（特別なガブリアスのガブリアスZ は いわ・ほかは type2 のまま）
u8 HackDragonBoulder_Type2(const struct Pokemon* mon, u16 species, u8 type2)
{
	if (species == SPECIES_GARCHOMP_MEGA_Z && mon != NULL && HackGibleEgg_SpecialAbility(mon, species) != ABILITY_NONE)
		return TYPE_ROCK;
	return type2;
}
