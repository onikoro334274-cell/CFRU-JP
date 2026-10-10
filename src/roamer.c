#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/overworld.h"
#include "../include/random.h"
#include "../include/region_map.h"
#include "../include/script.h"
#include "../include/pokemon_icon.h"
#include "../include/constants/maps.h"
#include "../include/constants/region_map_sections.h"
#include "../include/constants/species.h"

#include "../include/new/battle_start_turn_start.h"
#include "../include/new/util.h"
#include "../include/new/roamer.h"
#include "../include/new/wild_encounter.h"
#include "Tables/hoenn_roamer_locations.h" //[hack] patches/0093: ラティ兄妹のホウエンの徘徊（tools/rom/build_hoenn_roamers.py）
/*
roamer.c
	handles roaming pokemon and relevant functions/hooks
*/

//TODO: Do Pokedex area screen / Get icons on the region map

enum
{
	MAP_GRP = 0, // map group
	MAP_NUM = 1, // map number
};

//static u8 sLocationHistory[3][2]; //0x203F3A8
//static u8 sRoamerLocation[2] //0x203F3AE

#define ROAMING_MAP_BANK 3
#define NUM_MAPS_IN_SET 7

#ifndef UNBOUND //Modify this

static const u8 sRoamerLocations[][NUM_MAPS_IN_SET] = //0x8466C58 in FR
{
	{MAP_NUM(ROUTE_1),	MAP_NUM(ROUTE_2),	MAP_NUM(ROUTE_21_A),  0xFF,   0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_2),	MAP_NUM(ROUTE_1),	MAP_NUM(ROUTE_3),	MAP_NUM(ROUTE_22),   0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_3),	MAP_NUM(ROUTE_2),	MAP_NUM(ROUTE_4),	0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_4),	MAP_NUM(ROUTE_3),	MAP_NUM(ROUTE_5),	MAP_NUM(ROUTE_9),	MAP_NUM(ROUTE_24),   0xFF, 0xFF},
	{MAP_NUM(ROUTE_5),	MAP_NUM(ROUTE_4),	MAP_NUM(ROUTE_6),	MAP_NUM(ROUTE_7),	MAP_NUM(ROUTE_8),	MAP_NUM(ROUTE_9),	MAP_NUM(ROUTE_24)},
	{MAP_NUM(ROUTE_6),	  MAP_NUM(ROUTE_5),	MAP_NUM(ROUTE_7),	MAP_NUM(ROUTE_8),	MAP_NUM(ROUTE_11),   0xFF, 0xFF},
	{MAP_NUM(ROUTE_7),	  MAP_NUM(ROUTE_5),	MAP_NUM(ROUTE_6),	MAP_NUM(ROUTE_8),	MAP_NUM(ROUTE_16),   0xFF, 0xFF},
	{MAP_NUM(ROUTE_8),	  MAP_NUM(ROUTE_5),	MAP_NUM(ROUTE_6),	MAP_NUM(ROUTE_7),	MAP_NUM(ROUTE_10),   MAP_NUM(ROUTE_12),   0xFF},
	{MAP_NUM(ROUTE_9),	  MAP_NUM(ROUTE_4),	MAP_NUM(ROUTE_5),	MAP_NUM(ROUTE_10),   MAP_NUM(ROUTE_24),   0xFF, 0xFF},
	{MAP_NUM(ROUTE_10),   MAP_NUM(ROUTE_8),	MAP_NUM(ROUTE_9),	MAP_NUM(ROUTE_12),   0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_11),   MAP_NUM(ROUTE_6),	MAP_NUM(ROUTE_12),   0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_12),   MAP_NUM(ROUTE_10),   MAP_NUM(ROUTE_11),   MAP_NUM(ROUTE_13),   0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_13),   MAP_NUM(ROUTE_12),   MAP_NUM(ROUTE_14),   0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_14),   MAP_NUM(ROUTE_13),   MAP_NUM(ROUTE_15),   0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_15),   MAP_NUM(ROUTE_14),   MAP_NUM(ROUTE_18),   MAP_NUM(ROUTE_19),   0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_16),   MAP_NUM(ROUTE_7),	MAP_NUM(ROUTE_17),   0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_17),   MAP_NUM(ROUTE_16),   MAP_NUM(ROUTE_18),   0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_18),   MAP_NUM(ROUTE_15),   MAP_NUM(ROUTE_17),   MAP_NUM(ROUTE_19),   0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_19),   MAP_NUM(ROUTE_15),   MAP_NUM(ROUTE_18),   MAP_NUM(ROUTE_20),   0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_20),   MAP_NUM(ROUTE_19),   MAP_NUM(ROUTE_21_A), 0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_21_A), MAP_NUM(ROUTE_1),	MAP_NUM(ROUTE_20),   0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_22),   MAP_NUM(ROUTE_1),	MAP_NUM(ROUTE_2),	MAP_NUM(ROUTE_23),   0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_23),   MAP_NUM(ROUTE_22),   MAP_NUM(ROUTE_2),	0xFF, 0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_24),   MAP_NUM(ROUTE_4),	MAP_NUM(ROUTE_5),	MAP_NUM(ROUTE_9),	0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_25),   MAP_NUM(ROUTE_24),   MAP_NUM(ROUTE_9),	0xFF, 0xFF, 0xFF, 0xFF},
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
};

#else //For Pokemon Unbound

static const u8 sRoamerLocations[][NUM_MAPS_IN_SET] =
{
	{MAP_NUM(ROUTE_2),	  MAP_NUM(ROUTE_3),	   MAP_NUM(ROUTE_4),           0xFF,              0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_2),	  MAP_NUM(ROUTE_3),	   MAP_NUM(ROUTE_4),           MAP_NUM(ROUTE_5),  0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_6),    MAP_NUM(ROUTE_7),    MAP_NUM(ROUTE_8),           0xFF,              0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_6),    MAP_NUM(ROUTE_7),    MAP_NUM(ROUTE_9),           MAP_NUM(ROUTE_10), MAP_NUM(AUBURN_WATERWAY_B), 0xFF, 0xFF},
	{MAP_NUM(ROUTE_9),    MAP_NUM(ROUTE_10),   MAP_NUM(AUBURN_WATERWAY_B), 0xFF,              0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_9),    MAP_NUM(ROUTE_10),   MAP_NUM(ROUTE_11),          0xFF,              0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_11),   MAP_NUM(ROUTE_12),   0xFF,                       0xFF,              0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_12),   MAP_NUM(ROUTE_14_B), MAP_NUM(ROUTE_14_C),        MAP_NUM(ROUTE_6),  0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_16),   MAP_NUM(ROUTE_17),   0xFF,                       0xFF,              0xFF, 0xFF, 0xFF},
	{MAP_NUM(ROUTE_18_B), 0xFF,                0xFF,                       0xFF,              0xFF, 0xFF, 0xFF},
	{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
};

#endif

//[hack] patches/0093: 徘徊する場所の表は群で使い分ける（2026-09-29 ユーザー「ホウエンリーグ制覇時にフラグをたててラティ兄妹2匹を徘徊させる」）。
//  群 3（ROAMING_MAP_BANK）= カントーの道路（素の表）・群 43（HOENN_ROAMING_MAP_BANK）= ホウエンの道路（エメラルドの表）。
//  徘徊しているポケモンは自分のいる群（location[MAP_GRP]）の表の中だけを動く
typedef const u8 RoamerLocationSet[NUM_MAPS_IN_SET];

static RoamerLocationSet* GetRoamerLocationTable(u8 group, u32* numSets)
{
	if (group == HOENN_ROAMING_MAP_BANK)
	{
		*numSets = ARRAY_COUNT(sHoennRoamerLocations) - 1; //The last row is the terminator
		return sHoennRoamerLocations;
	}

	*numSets = ARRAY_COUNT(sRoamerLocations) - 1;
	return sRoamerLocations;
}

//This file's functions:
static void CreateInitialRoamerMon(u16 species, u8 level, bool8 allowedOnLand, bool8 allowedOnWater, u8 group);
static bool8 IsRoamerAt(u8 mapGroup, u8 mapNum, u8 id);
static void CreateRoamerMonInstance(u8 id);

void ClearRoamersData(void)
{
	memset(gRoamers, 0, sizeof(struct Roamer) * MAX_NUM_ROAMERS);
}

static void CreateInitialRoamerMon(u16 species, u8 level, bool8 allowedOnLand, bool8 allowedOnWater, u8 group)
{
	int i;
	u32 numSets;
	RoamerLocationSet* table = GetRoamerLocationTable(group, &numSets);
	struct Roamer roamer;

	if (species == 0xFFFF) //FRLGSytle
	{
		switch(GetStarterChoice()) { //0x80CBDB0
			case SPECIES_BULBASAUR:
				species = SPECIES_ENTEI;
				break;
			case SPECIES_CHARMANDER:
				species = SPECIES_SUICUNE;
				break;
			default:
				species = SPECIES_RAIKOU;
		}
	}

	if (IsSpeciesRoaming(species)) //Only one of each species may roam at a time.
	{
		gSpecialVar_LastResult = 0;
		return;
	}

	CreateMon(&gEnemyParty[0], species, level, 0x20, 0, 0, 0, 0);

	#ifdef CREATE_ROAMER_WITH_X_PERFECT_IVS
	{
		u8 numPerfectStats = 0;
		u8 perfect = 31;
		bool8 perfectStats[NUM_STATS] = {0};

		//Count pre-existing perfect stats
		for (int i = 0; i < NUM_STATS; ++i)
		{
			if (GetMonData(&gEnemyParty[0], MON_DATA_HP_IV + i, NULL) >= 31)
			{
				perfectStats[i] = TRUE;
				++numPerfectStats;
			}
		}

		//Add more perfect stats up to max
		while (numPerfectStats < MathMin(CREATE_ROAMER_WITH_X_PERFECT_IVS, NUM_STATS)) //Error prevention
		{
			u8 statId = Random() % NUM_STATS;
			if (!perfectStats[statId]) //Must be unique
			{
				perfectStats[statId] = TRUE;
				++numPerfectStats;
				SetMonData(&gEnemyParty[0], MON_DATA_HP_IV + statId, &perfect);
			}
		}
		
		HealMon(&gEnemyParty[0]);
	}
	#endif

	roamer.level = level;
	roamer.status = 0;
	roamer.ivs = GetMonData(&gEnemyParty[0], MON_DATA_IVS, NULL);
	roamer.personality = gEnemyParty[0].personality;
	roamer.species = species;
	roamer.hp = gEnemyParty[0].hp;
	roamer.canAppearOnLand = allowedOnLand;
	roamer.canAppearOnWater = allowedOnWater;
	roamer.location[MAP_GRP] = group;
	roamer.location[MAP_NUM] = table[Random() % numSets][0];

	for (i = 0; i < MAX_NUM_ROAMERS; ++i)
	{
		if (gRoamers[i].species == SPECIES_NONE) //Fill in empty spot
		{
			gRoamers[i] = roamer;
			break;
		}
	}

	if (i == MAX_NUM_ROAMERS)
		gSpecialVar_LastResult = 0; //Too many roamers
	else
		gSpecialVar_LastResult = 1; //Success
}

// Inputs:
//		Var8000: Species
//		Var8001: Level
//		Var8002: 0x0 = Roamer Not Allowed On Land
//		Var8003: 0x0 = Roamer Not Allowed On Water
// Returns: To LastResult 0 if failed.
/*★★★ [hack] patches/0050 — 3 匹とも徘徊させる（2026-09-23）

  ユーザー要望「カントーを巡回する 3 犬を最初に選んだポケモンではなく
  全部でるようにしてほしい」。

  ★★ **CFRU は最初から複数の徘徊に対応している。**
    `MAX_NUM_ROAMERS` は 10（`SAVE_BLOCK_EXPANSION` が有効なため）で、
    出現（`TryStartRoamerEncounter`）・移動（`RoamersMove`）・
    タウンマップのアイコンまで**全部この数でループしている**。
    **1 匹に絞っていたのはここ 1 か所だけ。**

  ★ `CreateInitialRoamerMon()` は `IsSpeciesRoaming()` で
    **同じ種族の二重登録を自分で弾く**ので、何度呼んでも増えない。
    倒した／捕まえた個体は枠が空くが、この関数を呼び直さない限り
    復活しない（呼び元は下の 2 か所だけ）。

  ★ 起動は素 FRLG の `special InitRoamer` ＝ **ななしま 1 のポケセンで
    セリオがホウエンと通信をつなぐイベントの最後**（1 回きり）。
    そこを既に過ぎているセーブのために、`src/hoenn_steps.c` の
    `HoennRoamerTopUpTick()` が**一度だけ**ここを呼び直す。

  ★ レベル 50・陸/水どちらにも出る、は素のまま。
*/
void sp129_InitRoamer(void)
{
	#ifdef FRLG_ROAMING
		#ifdef FRLG_ROAMING_ALL_BEASTS
			CreateInitialRoamerMon(SPECIES_RAIKOU, 50, TRUE, TRUE, ROAMING_MAP_BANK);
			CreateInitialRoamerMon(SPECIES_ENTEI, 50, TRUE, TRUE, ROAMING_MAP_BANK);
			CreateInitialRoamerMon(SPECIES_SUICUNE, 50, TRUE, TRUE, ROAMING_MAP_BANK);
		#else
			CreateInitialRoamerMon(0xFFFF, 50, TRUE, TRUE, ROAMING_MAP_BANK);
		#endif
	#else
		CreateInitialRoamerMon(Var8000, Var8001, Var8002, Var8003, ROAMING_MAP_BANK);
	#endif
}

//[hack] patches/0093: ホウエンを徘徊させる（ラティ兄妹・src/hoenn_league.c の HoennLeague_OnHallOfFame から）。
//陸でも水でも出る（エメラルドと同じ）。同じ種族が徘徊中なら何もしない（CreateInitialRoamerMon が弾く）
void CreateHoennRoamer(u16 species, u8 level)
{
	CreateInitialRoamerMon(species, level, TRUE, TRUE, HOENN_ROAMING_MAP_BANK);
}

void UpdateLocationHistoryForRoamers(void)
{
	for (int i = 0; i < MAX_NUM_ROAMERS; ++i)
	{
		struct Roamer* roamer = &gRoamers[i];

		if (roamer->species != SPECIES_NONE)
		{
			roamer->locationHistory[2][MAP_GRP] = roamer->locationHistory[1][MAP_GRP];
			roamer->locationHistory[2][MAP_NUM] = roamer->locationHistory[1][MAP_NUM];

			roamer->locationHistory[1][MAP_GRP] = roamer->locationHistory[0][MAP_GRP];
			roamer->locationHistory[1][MAP_NUM] = roamer->locationHistory[0][MAP_NUM];

			roamer->locationHistory[0][MAP_GRP] = gSaveBlock1->location.mapGroup;
			roamer->locationHistory[0][MAP_NUM] = gSaveBlock1->location.mapNum;
		}
	}
}

void RoamersMoveToOtherLocationSet(void)
{
	u8 mapNum = 0;

	for (int i = 0; i < MAX_NUM_ROAMERS; ++i)
	{
		struct Roamer* roamer = &gRoamers[i];

		if (roamer->species != SPECIES_NONE)
		{
			//[hack] patches/0093: 群は自分のいる群のまま（ホウエンの徘徊をカントーへ移さない）
			u32 numSets;
			RoamerLocationSet* table = GetRoamerLocationTable(roamer->location[MAP_GRP], &numSets);

			while (TRUE)
			{
				mapNum = table[Random() % numSets][0];
				if (roamer->location[MAP_NUM] != mapNum)
				{
					roamer->location[MAP_NUM] = mapNum;
					break;
				}
			}
		}
	}
}

void RoamersMove(void)
{
	u32 locSet;

	if ((Random() % 16) == 0)
	{
		RoamersMoveToOtherLocationSet();
	}
	else
	{
		for (int i = 0; i < MAX_NUM_ROAMERS; ++i)
		{
			struct Roamer* roamer = &gRoamers[i];

			if (roamer->species != SPECIES_NONE)
			{
				//[hack] patches/0093: 表は自分のいる群のもの。★ 探し始めを 1 匹ごとに 0 に戻す
				//（素の CFRU は locSet を関数の頭で 1 回だけ 0 にしていて、2 匹目以降は 1 匹目が止まった所から探した）
				u32 numSets;
				RoamerLocationSet* table = GetRoamerLocationTable(roamer->location[MAP_GRP], &numSets);

				locSet = 0;
				while (locSet < numSets)
				{
					if (roamer->location[MAP_NUM] == table[locSet][0])
					{
						u8 mapNum;
						while (TRUE)
						{
							mapNum = table[locSet][Random() % NUM_MAPS_IN_SET];
							if (!(roamer->locationHistory[2][MAP_GRP] == roamer->location[MAP_GRP] && roamer->locationHistory[2][MAP_NUM] == mapNum) && mapNum != 0xFF)
								break;
						}
						roamer->location[MAP_NUM] = mapNum;
						break;
					}
					locSet++;
				}

				//[hack] patches/0093: 居場所が表に無い（2026-09-29 の表の誤りで 0xFF になったセーブ）なら、表の道路へ置き直す
				if (locSet >= numSets)
					roamer->location[MAP_NUM] = table[Random() % numSets][0];
			}
		}
	}
}

static bool8 IsRoamerAt(u8 mapGroup, u8 mapNum, u8 id)
{
	struct Roamer* roamer = &gRoamers[id];

	return roamer->species != SPECIES_NONE && mapGroup == roamer->location[MAP_GRP] && mapNum == roamer->location[MAP_NUM];
}

static void CreateRoamerMonInstance(u8 id)
{
	struct Pokemon* mon = &gEnemyParty[0];
	struct Roamer* roamer = &gRoamers[id];

	ZeroEnemyPartyMons();
	CreateMonWithIVsPersonality(mon, roamer->species, roamer->level, roamer->ivs, roamer->personality);
	mon->condition = roamer->status;
	mon->hp = roamer->hp;
}

bool8 TryStartRoamerEncounter(u8 environment)
{
	for (int i = 0; i < MAX_NUM_ROAMERS; ++i)
	{
		if (IsRoamerAt(gSaveBlock1->location.mapGroup, gSaveBlock1->location.mapNum, i) && (Random() % 4) == 0)
		{
			switch (environment) {
				case ENCOUNTER_TYPE_LAND:
					if (!gRoamers[i].canAppearOnLand)
						continue;
					break;
				case ENCOUNTER_TYPE_WATER:
					if (!gRoamers[i].canAppearOnWater)
						continue;
					break;
			}

			CreateRoamerMonInstance(i);
			gLastSelectedRoamer = i;
			return TRUE;
		}
	}

	return FALSE;
}

void UpdateFoughtRoamerHPStatus(struct Pokemon* mon)
{
	gRoamers[gLastSelectedRoamer].hp = mon->hp;
	gRoamers[gLastSelectedRoamer].status = mon->condition;

	RoamersMoveToOtherLocationSet();
}

void SetFoughtRoamerInactive(void)
{
	memset(&gRoamers[gLastSelectedRoamer], 0, sizeof(struct Roamer));
}

//HOOK
void GetRoamerLocation(u8* mapGroup, u8* mapNum, u8 id)
{
	*mapGroup = gRoamers[id].location[MAP_GRP];
	*mapNum = gRoamers[id].location[MAP_NUM];
}

bool8 IsSpeciesRoaming(u16 species)
{
	for (int i = 0; i < MAX_NUM_ROAMERS; ++i)
	{
		if (gRoamers[i].species == species)
			return TRUE;
	}

	return FALSE;
}

void GetMapGroupAndMapNumOfRoamer(u16 species, u8* mapGroup, u8* mapNum)
{
	for (int i = 0; i < MAX_NUM_ROAMERS; ++i)
	{
		if (gRoamers[i].species == species)
		{
			*mapGroup = gRoamers[i].location[MAP_GRP];
			*mapNum = gRoamers[i].location[MAP_NUM];
		}
	}
}

void BattleSetup_StartRoamerBattle(void)
{
	ScriptContext2_Enable();
	FreezeEventObjects();
	StopPlayerAvatar();
	gMain.savedCallback = CB2_EndWildBattle;
	gBattleTypeFlags = BATTLE_TYPE_ROAMER;
	CreateBattleStartTask(GetWildBattleTransition(), GetMUS_ForBattle());
	IncrementGameStat(GAME_STAT_TOTAL_BATTLES);
	IncrementGameStat(GAME_STAT_WILD_BATTLES);
}


/*static const union AffineAnimCmd sSpriteAffineAnim_RegionMapRoamer[] =
{
	AFFINEANIMCMD_FRAME(-128, -128, 0, 1), //Half size
	AFFINEANIMCMD_END,
};

static const union AffineAnimCmd* const sSpriteAffineAnimTable_RegionMapRoamer[] =
{
	sSpriteAffineAnim_RegionMapRoamer,
};*/

extern const u16 sMapSectionTopLeftCorners[][2];
extern const u16 sMapSectionDimensions[][2];
//[hack] patches/0093: 見ているページの地方にいる徘徊だけを出す（カントーのページ = 群 3・ホウエンのページ = 群 43）。
//位置は拡張した地名の表（assembly/hoenn/mapsec.s・番号そのままで引く・カントーの行は素と同じ値）。
//素の表（sMapSectionTopLeftCorners）を番号 - 0x58 で引くと、ホウエンの番号では表の外を読む（patches/0092 の守りを置き換えた）。
//★ ホウエンのページは横にスクロールするので、印は src/hoenn_townmap.c がスクロールに合わせて動かす
extern const u16 gExpandedMapSecPos[][2];
extern const u16 gExpandedMapSecSizes[][2];
#define HOENN_TOWN_MAP_PAGE 4

void CreateTownMapRoamerSprites(void)
{
	u8 page = GetSelectedRegionMap();
	u8 wantGroup;

	if (page == 0)
		wantGroup = ROAMING_MAP_BANK;
	else if (page == HOENN_TOWN_MAP_PAGE)
		wantGroup = HOENN_ROAMING_MAP_BANK;
	else
		return; //No roamers on the Sevii / Sinnoh pages

	for (int i = 0; i < MAX_NUM_ROAMERS; ++i)
	{
		//FlagSet(FLAG_SYS_SEVII_MAP_123); //For debugging
		if (gRoamers[i].species != SPECIES_NONE && gRoamers[i].location[MAP_GRP] == wantGroup)
		{
			LoadMonIconPalettes();
			u8 mapGroup = gRoamers[i].location[MAP_GRP];
			u8 mapNum = gRoamers[i].location[MAP_NUM];
			if (mapNum == 0xFF) //居場所が表の外（2026-09-29 の表の誤りのセーブ・次に区切りをまたげば直る）
				continue;
			const struct MapHeader* mapHeader = Overworld_GetMapHeaderByGroupAndId(mapGroup, mapNum);
			u8 mapsec = mapHeader->regionMapSectionId;

			if (mapsec == MAPSEC_NONE || mapsec > 0xFC) //The expanded tables have 0x00-0xFC
				continue;

			s16 x = 8 * gExpandedMapSecPos[mapsec][0];
			s16 y = 8 * gExpandedMapSecPos[mapsec][1];

			if (gExpandedMapSecSizes[mapsec][0] > 1)
				x += 4 * gExpandedMapSecSizes[mapsec][0];
			if (gExpandedMapSecSizes[mapsec][1] > 1)
				y += 4 * gExpandedMapSecSizes[mapsec][1];

			u8 spriteId = CreateMonIcon(gRoamers[i].species, SpriteCB_PokeIcon, x + 36, y + 36 - 8, 0, gRoamers[i].personality, FALSE);
			if (spriteId < MAX_SPRITES)
			{
				struct Sprite* sprite = &gSprites[spriteId];
			
				sprite->oam.priority = 2;
				sprite->invisible = TRUE;
				/*sprite->oam.affineMode = ST_OAM_AFFINE_NORMAL;
				sprite->affineAnims = sSpriteAffineAnimTable_RegionMapRoamer;
				CalcCenterToCornerVec(sprite, sprite->oam.shape, sprite->oam.size, sprite->oam.affineMode);
				InitSpriteAffineAnim(sprite);*/
			}
		}
	}
}

void DestroyTownMapRoamerSprites(void)
{
	u32 i, j;
	
	for (i = 0; i < MAX_SPRITES; ++i)
	{
		for (j = 0; j < MAX_NUM_ROAMERS; ++j)
		{
			if (gRoamers[j].species != SPECIES_NONE
			&&  (void*) gSprites[i].images == (void*) GetMonIconTiles(GetIconSpecies(gRoamers[j].species, gRoamers[j].personality), FALSE))
			{
				FreeSpriteOamMatrix(&gSprites[i]);
				DestroyMonIcon(&gSprites[i]);
			}
		}
	}
}

void HideOrShowTownMapRoamerSprites(bool8 invisible)
{
	u32 i, j;

	for (i = 0; i < MAX_SPRITES; ++i)
	{
		if (gSprites[i].inUse)
		{
			for (j = 0; j < MAX_NUM_ROAMERS; ++j)
			{
				if (gRoamers[j].species != SPECIES_NONE
				&&  (void*) gSprites[i].images == (void*) GetMonIconTiles(GetIconSpecies(gRoamers[j].species, gRoamers[j].personality), FALSE))
				{
					gSprites[i].invisible = invisible;
				}
			}
		}
	}
}
