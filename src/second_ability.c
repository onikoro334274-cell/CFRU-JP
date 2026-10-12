#include "defines.h"
#include "defines_battle.h"
#include "../include/constants/abilities.h"
#include "../include/new/battle_util.h"
#include "../include/new/ability_battle_scripts.h"
#include "../include/constants/region_map_sections.h"
#include "Tables/second_abilities.h"

/*
    2 つ目の特性（場に出たときだけ効く・2026-10-01）

    ユーザー「建物の中とレベルで特性が増える」→（AskUserQuestion）「2 つ目は場に出たときだけ」「全部最終進化とメガ進化に」
    「タイプごとに決める」「Lv 50」。考えは SoulGold（参考/soulgold の innates）から借りた（コードは自前）。

    ★ CFRU の特性は 1 つだけ（ABILITY(bank)）。2 つ目は「場に出たときの効果だけ」を、その特性を名指しして
       AbilityBattleEffects(ABILITYEFFECT_ON_SWITCHIN, bank, 0, 特性, 0) で動かす（4 つ目の引数 special がそのまま
       gLastUsedAbility になる仕組みが元からある）。ポップアップの名前は gNewBS->abilityPopupOverwrite。
    ★ いかく・おどかしは素の台本が「本当の特性が いかく か」を見るので、自前の台本（second_ability_scripts.s）。
    ★ 1 回だけ: gNewBS->secondAbilitySpecies[bank] に出した種族を覚える（入れ替えで 0・ClearSwitchBytes）。
       メガシンカで種族が変わると、メガの 2 つ目の特性がもう 1 回出る。
    ★ 呼ぶ所: 入れ替え（switching.c の SwitchIn_Abilities）・戦闘の始め（battle_start_turn_start.c の
       BTSTART_SWITCH_IN_ABILITIES の 2 周目）・メガシンカ（Mega_SwitchInAbilities の 2 周目）
*/

bool8 Hack_StartIntimidateLike(u8 bank, bool8 atk);

u16 SecondAbility_OfSpecies(u16 species)
{
	u32 lo = 0, hi = SECOND_ABILITY_COUNT;

	while (lo < hi)
	{
		u32 mid = (lo + hi) / 2;
		if (sSecondAbilities[mid][0] == species)
			return sSecondAbilities[mid][1];
		if (sSecondAbilities[mid][0] < species)
			lo = mid + 1;
		else
			hi = mid;
	}
	return ABILITY_NONE;
}

/* ★ 2026-10-03 ユーザー「お母さんのピカチュウだけ」→「マサラタウンで出会ったの条件を」:
   お母さんのピカチュウ（src/kanto_mom_pikachu.c・自宅でもらう）にも 2 つ目の特性。印は持たず、もらったときの記録で見分ける
   ＝ ピカチュウ・出会った場所がマサラタウン（MAPSEC 0x58）・出会ったレベルが 0 でない（タマゴから孵った子でない）・親が主人公。
   前のセーブのピカチュウにもそのまま効く。マサラタウンには草むらもトレーナーもいないので、ほかのピカチュウはまず当たらない */
#define MOM_PIKACHU_SECOND_ABILITY ABILITY_ELECTRICSURGE    /* でんき → エレキメイカー（TYPE_ABILITY と同じ） */

static bool8 IsMomPikachu(u16 species, struct Pokemon* mon)
{
	u32 otId;

	if ((species != SPECIES_PIKACHU && species != SPECIES_PIKACHU_POP_STAR) || mon == NULL)   //★ 2026-10-09 今のママのピカチュウはポップスター・前のセーブはふつうのピカチュウ
		return FALSE;
	if (GetMonData(mon, MON_DATA_MET_LOCATION, NULL) != MAPSEC_PALLET_TOWN || GetMonData(mon, MON_DATA_MET_LEVEL, NULL) == 0)
		return FALSE;
	otId = gSaveBlock2->playerTrainerId[0] | (gSaveBlock2->playerTrainerId[1] << 8)
	     | (gSaveBlock2->playerTrainerId[2] << 16) | ((u32) gSaveBlock2->playerTrainerId[3] << 24);
	return GetMonData(mon, MON_DATA_OT_ID, NULL) == otId;
}

/* ★ 2026-10-03 トキワシティの人（v0.7 だけの期間限定・2026-10-03 に外した・docs/research/2026-10-03_pikachu-ability-npc.md）が付けた印
   （MON_DATA_OBEDIENCE）。もらった子は外したあとも特性を持ち続けるので、ここは残す。
   素の FRLG がこの印を見るのはミュウ・デオキシスが言うことを聞くかとユニオンルームの掲示板だけ */
bool8 HackMark_IsPlayersMarkedMon(const struct Pokemon* mon); //src/hack_gible_egg.c（★ 2026-10-05 フロンティアの相手も印が付いているので、親が主人公・施設の外だけ）

static bool8 HasPikachuAbilityMark(u16 species, struct Pokemon* mon)
{
	return species == SPECIES_PIKACHU && mon != NULL && HackMark_IsPlayersMarkedMon(mon);
}

static u16 MomPikachuSecondAbility(u16 species, struct Pokemon* mon)
{
	return (IsMomPikachu(species, mon) || HasPikachuAbilityMark(species, mon)) ? MOM_PIKACHU_SECOND_ABILITY : ABILITY_NONE;
}


u16 HackDragonBoulder_SecondAbility(const struct Pokemon* mon, u16 species);
u16 HackVolcarona_SecondAbility(u16 species, const struct Pokemon* mon);   //src/hack_volcarona.c（2026-10-11 特別なウルガモスの 2 つ目 たいようのちょう・#95） //src/hack_dragon_boulder.c（2026-10-07 特別なガブリアスZ → りゅうのいわお）

/* 1 匹の 2 つ目の特性（レベルは見ない・ステータス画面と AI 用） */
u16 SecondAbility_OfMon(struct Pokemon* mon)
{
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	u16 ability = HackDragonBoulder_SecondAbility(mon, species);

	if (ability != ABILITY_NONE)
		return ability;
	ability = SecondAbility_OfSpecies(species);

	if (ability == ABILITY_NONE)
		ability = MomPikachuSecondAbility(species, mon);
	if (ability == ABILITY_NONE)
		ability = HackVolcarona_SecondAbility(species, mon);
	return ability;
}

/* ★ 2026-10-03 ステータス画面（src/bw_summary_screen.c）用: そのレベルで 2 つ目の特性が働くか */
bool8 SecondAbility_IsUnlocked(u8 level)
{
	return level >= SECOND_ABILITY_UNLOCK_LEVEL;
}

/* この場のポケモンの 2 つ目の特性（無ければ ABILITY_NONE） */
u16 SecondAbility_OfBank(u8 bank)
{
	u16 ability;

	if (gBattleMons[bank].level < SECOND_ABILITY_UNLOCK_LEVEL || IsAbilitySuppressed(bank))
		return ABILITY_NONE;
	ability = HackDragonBoulder_SecondAbility(SIDE(bank) == B_SIDE_PLAYER ? GetBankPartyData(bank) : NULL, SPECIES(bank));
	if (ability == ABILITY_NONE)
		ability = SecondAbility_OfSpecies(SPECIES(bank));
	if (ability == ABILITY_NONE)
		ability = MomPikachuSecondAbility(SPECIES(bank), SIDE(bank) == B_SIDE_PLAYER ? GetBankPartyData(bank) : NULL);
	if (ability == ABILITY_NONE)
		ability = HackVolcarona_SecondAbility(SPECIES(bank), GetBankPartyData(bank));   //★ 相手側の個体にも効く（通信対戦）
	if (ability == ABILITY(bank))
		return ABILITY_NONE;
	return ability;
}

/* ★ 2026-10-03 ユーザーの動画（ライチュウ Lv58・エレキメイカー）「フィールド系の特性が発動しない」「次に特性が出たポケモンの
   札がエレキメイカーになる」: AbilityBattleEffects に名指しで渡すとフィールド系だけ台本が走らない（原因は未特定・
   docs/research/2026-10-01_second-ability.md）。フィールドは ability_battle_effects.c の TryActivateTerrainAbility と同じ手順を
   ここで踏む（いかく系と同じく自前で台本を積む） */
static bool8 TryStartTerrain(u8 bank, u8 terrain, u8 anim, const u8* string)
{
	if (gBattleTypeFlags & BATTLE_TYPE_BATTLE_CIRCUS && gBattleCircusFlags & BATTLE_CIRCUS_TERRAIN)
		return FALSE; //Can't be removed
	if (gTerrainType == terrain)
		return FALSE;

	gTerrainType = terrain;
	#ifdef INFINITE_TERRAIN
		gNewBS->TerrainTimer = 0;
	#else
		gNewBS->TerrainTimer = (ITEM_EFFECT(bank) == ITEM_EFFECT_TERRAIN_EXTENDER) ? 8 : 5;
	#endif
	gBattleStringLoader = string;
	gBattleScripting.animArg1 = anim;
	gBattleScripting.bank = bank;
	*((u32*) gSeedHelper) = (u32) gBattleStringLoader; //Backup String
	BattleScriptPushCursorAndCallback(BattleScript_TerrainFromAbility);
	return TRUE;
}

/* 場に出たとき（1 回だけ）。効果が出て台本が始まったら TRUE */
bool8 SecondAbility_TrySwitchIn(u8 bank)
{
	u16 species = SPECIES(bank);
	u16 ability;
	u32 done;
	bool8 effect;

	if (!BATTLER_ALIVE(bank) || species == SPECIES_NONE || gNewBS->secondAbilitySpecies[bank] == species)
		return FALSE;
	gNewBS->secondAbilitySpecies[bank] = species;

	ability = SecondAbility_OfBank(bank);
	if (ability == ABILITY_NONE)
		return FALSE;

	gNewBS->abilityPopupOverwrite = ability;
	switch (ability)
	{
		case ABILITY_INTIMIDATE:
			effect = Hack_StartIntimidateLike(bank, TRUE);
			break;
		case ABILITY_SCARE:
			effect = Hack_StartIntimidateLike(bank, FALSE);
			break;
		case ABILITY_MIWAKUNOUROKO:   //★ 2026-10-11 ミワクノウロコ（ツキノウロコ・いかく と同じ）
			effect = Hack_StartIntimidateLike(bank, TRUE);
			break;
		case ABILITY_ELECTRICSURGE:
			effect = TryStartTerrain(bank, ELECTRIC_TERRAIN, B_ANIM_ELECTRIC_SURGE, ElectricTerrainSetString);
			break;
		case ABILITY_GRASSYSURGE:
			effect = TryStartTerrain(bank, GRASSY_TERRAIN, B_ANIM_GRASSY_SURGE, GrassyTerrainSetString);
			break;
		case ABILITY_MISTYSURGE:
			effect = TryStartTerrain(bank, MISTY_TERRAIN, B_ANIM_MISTY_SURGE, MistyTerrainSetString);
			break;
		case ABILITY_PSYCHICSURGE:
			effect = TryStartTerrain(bank, PSYCHIC_TERRAIN, B_ANIM_PSYCHIC_SURGE, PsychicTerrainSetString);
			break;
		default:
			/* 1 つ目の特性で「場に出たときの特性は済んだ」印が立っていると AbilityBattleEffects が何もしないので、呼ぶ間だけ外す */
			done = gStatuses3[bank] & STATUS3_SWITCH_IN_ABILITY_DONE;
			gStatuses3[bank] &= ~STATUS3_SWITCH_IN_ABILITY_DONE;
			effect = AbilityBattleEffects(ABILITYEFFECT_ON_SWITCHIN, bank, 0, ability, 0);
			/* 印は呼ぶ前のとおりに戻す（AbilityBattleEffects が立てたままだと、あとの段で動く 1 つ目の特性が飛ばされる） */
			gStatuses3[bank] = (gStatuses3[bank] & ~STATUS3_SWITCH_IN_ABILITY_DONE) | done;
			break;
	}

	if (!effect)
		gNewBS->abilityPopupOverwrite = 0;
	return effect;
}
