#include "defines.h"
#include "../include/battle.h"
#include "../include/new/ram_locs_battle.h"
#include "../include/constants/species.h"
#include "../include/constants/trainer_classes.h"
#include "../include/new/dynamax.h"
#include "../include/new/frontier.h"
#include "map_region.h"
#include "Tables/hoenn_battle_music.h"

/*
 * ホウエンの地図の戦闘ではエメラルドの戦闘の曲（2026-10-09 ユーザー「ホウエントレーナーのBGMはホウエンの流れるようにしてほしい」
 * 「ROM拡張下から行けるでしょ」）。曲は tools/bgm/build_hoenn_bgm.py の EXTRA_SONGS がエメラルドから移し、番号は
 * tools/bgm/build_hoenn_battle_music.py が src/Tables/hoenn_battle_music.h に書く。
 * CFRU の GetMUS_ForBattle（src/battle_start_turn_start.c）の頭から呼ぶ（patches/0229）。0 を返せば今までどおり。
 * - 通信対戦・フロンティアの施設（ブレーンの曲は CFRU の表）・レイドは今までどおり
 * - トレーナー: ホウエンの人のクラス（素 FRLG の RS のクラス・tools/rom/build_hoenn_trainers.py）で分ける
 * - 野生: カイオーガ・グラードン／レックウザ／レジ系／ミュウは専用の曲、ほかは野生の曲
 */
#define CLASS_AQUA_LEADER   2
#define CLASS_TEAM_AQUA     3
#define CLASS_RS_ELITE_FOUR 23
#define CLASS_RS_LEADER     24
#define CLASS_RS_CHAMPION   30
#define CLASS_RIVAL         44
#define CLASS_MAGMA_LEADER  47
#define CLASS_TEAM_MAGMA    48
#define CLASS_AQUA_ADMIN    55
#define CLASS_MAGMA_ADMIN   56

static u16 HoennTrainerMusic(u8 trainerClass)
{
	switch (trainerClass)
	{
		case CLASS_AQUA_LEADER:
		case CLASS_MAGMA_LEADER:
			return HOENN_MUS_VS_AQUA_MAGMA_LEADER;
		case CLASS_TEAM_AQUA:
		case CLASS_TEAM_MAGMA:
		case CLASS_AQUA_ADMIN:
		case CLASS_MAGMA_ADMIN:
			return HOENN_MUS_VS_AQUA_MAGMA;
		case CLASS_RS_LEADER:
			return HOENN_MUS_VS_GYM_LEADER;
		case CLASS_RS_ELITE_FOUR:
			return HOENN_MUS_VS_ELITE_FOUR;
		case CLASS_RS_CHAMPION:
			return HOENN_MUS_VS_CHAMPION;
		case CLASS_RIVAL:
			return HOENN_MUS_VS_RIVAL;
	}
	return HOENN_MUS_VS_TRAINER;
}

u16 HackHoenn_BattleMusic(void)
{
	u16 species;

	if (GetCurrentMapRegion() != MAP_REGION_HOENN)
		return 0;
	if (gBattleTypeFlags & BATTLE_TYPE_LINK)
		return 0;

	if (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
	{
		if ((gBattleTypeFlags & BATTLE_TYPE_FRONTIER) || IsFrontierTrainerId(gTrainerBattleOpponent_A))
			return 0;
		return HoennTrainerMusic(gTrainers[gTrainerBattleOpponent_A].trainerClass);
	}

	if (IsRaidBattle())
		return 0;
	species = gEnemyParty[0].species;
	switch (species)
	{
		case SPECIES_KYOGRE:
		case SPECIES_GROUDON:
			return HOENN_MUS_VS_KYOGRE_GROUDON;
		case SPECIES_RAYQUAZA:
			return HOENN_MUS_VS_RAYQUAZA;
		case SPECIES_REGIROCK:
		case SPECIES_REGICE:
		case SPECIES_REGISTEEL:
		case SPECIES_REGIGIGAS:
		case SPECIES_REGIELEKI:
		case SPECIES_REGIDRAGO:
			return HOENN_MUS_VS_REGI;
		case SPECIES_MEW:
			return HOENN_MUS_VS_MEW;
	}
	return HOENN_MUS_VS_WILD;
}
