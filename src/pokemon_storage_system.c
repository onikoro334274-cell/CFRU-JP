#include "defines.h"
#include "../include/event_data.h"
#include "../include/pokemon_icon.h"
#include "../include/pokemon_storage_system.h"
#include "../include/pokemon_storage_system_internal.h"
#include "../include/string_util.h"
#include "../include/text.h"
#include "../include/constants/flags.h"
#include "../include/constants/vars.h"

#include "../include/new/build_pokemon.h"
#include "../include/new/form_change.h"
#include "../include/new/frontier.h"
#include "../include/new/item.h"
#include "../include/new/pokemon_storage_system.h"
#include "hack_mon_id.h" //[hack] patches/0259
#include "hack_box_shelf.h" //[hack] patches/0196 ボックスの仮想化
/*
pokemon_storage_system.c
	handles pokemon storage expansion and related functions
*/

enum
{
	WALLPAPER_FOREST,
	WALLPAPER_CITY,
	WALLPAPER_DESERT,
	WALLPAPER_SAVANNA,
	WALLPAPER_CRAG,
	WALLPAPER_VOLCANO,
	WALLPAPER_SNOW,
	WALLPAPER_CAVE,
	WALLPAPER_BEACH,
	WALLPAPER_SEAFLOOR,
	WALLPAPER_RIVER,
	WALLPAPER_SKY,
	WALLPAPER_STARS,
	WALLPAPER_POKECENTER,
	WALLPAPER_TILES,
	WALLPAPER_SIMPLE,
	WALLPAPER_SPARE, //[hack] patches/0242 使わない番号（メニューの文 38 番は CFRU の Select）
	WALLPAPER_PEACH, //[hack] patches/0242 もも（src/hack_box_wallpaper.c）
	WALLPAPER_COUNT
};

typedef u8 BoxNameT[9];

#define ORIGINAL_BOX_POKEMON_RAM ((struct CompressedPokemon*) (0x202924C + 4))
#define ORIGINAL_BOX_NAME_RAM ((BoxNameT*) (0x202924C + 0x8344))
#define ORIGINAL_BOX_WALLPAPER_RAM ((u8*) (0x202924C + 0x83C2))

#define BOX_20_RAM ((struct CompressedPokemon*) 0x203CABC)
#define BOX_21_RAM (BOX_20_RAM + 30)
#define BOX_22_RAM (BOX_21_RAM + 30) //Should end at 0x203DFA8
#define BOX_23_RAM ((struct CompressedPokemon*) 0x2026D74)
#define BOX_24_RAM (BOX_23_RAM + 30)
#define BOX_25_RAM ((struct CompressedPokemon*) 0x20245E0)

//#define sPSSData ((struct PokemonStorageSystemData*) *((u32*) 0x20397B0))

#define gDisposableBoxMon ((struct BoxPokemon*) 0x2022B24) //Reuse Battle Buffer A

//[hack] patches/0196: 1〜19 箱・26〜39 箱は棚（src/hack_box_shelf.c）。この表は 20〜25 箱（棚でない箱）だけが使う
struct CompressedPokemon* const sPokemonBoxPtrs[25] =
{
	ORIGINAL_BOX_POKEMON_RAM,				//Box 1
	ORIGINAL_BOX_POKEMON_RAM + (30 * 1),	//Box 2
	ORIGINAL_BOX_POKEMON_RAM + (30 * 2),	//Box 3
	ORIGINAL_BOX_POKEMON_RAM + (30 * 3),	//Box 4
	ORIGINAL_BOX_POKEMON_RAM + (30 * 4),	//Box 5
	ORIGINAL_BOX_POKEMON_RAM + (30 * 5),	//Box 6
	ORIGINAL_BOX_POKEMON_RAM + (30 * 6),	//Box 7
	ORIGINAL_BOX_POKEMON_RAM + (30 * 7),	//Box 8
	ORIGINAL_BOX_POKEMON_RAM + (30 * 8),	//Box 9
	ORIGINAL_BOX_POKEMON_RAM + (30 * 9),	//Box 10
	ORIGINAL_BOX_POKEMON_RAM + (30 * 10),	//Box 11
	ORIGINAL_BOX_POKEMON_RAM + (30 * 11),	//Box 12
	ORIGINAL_BOX_POKEMON_RAM + (30 * 12),	//Box 13
	ORIGINAL_BOX_POKEMON_RAM + (30 * 13),	//Box 14
	ORIGINAL_BOX_POKEMON_RAM + (30 * 14),	//Box 15
	ORIGINAL_BOX_POKEMON_RAM + (30 * 15),	//Box 16
	ORIGINAL_BOX_POKEMON_RAM + (30 * 16),	//Box 17
	ORIGINAL_BOX_POKEMON_RAM + (30 * 17),	//Box 18
	ORIGINAL_BOX_POKEMON_RAM + (30 * 18),	//Box 19
	BOX_20_RAM,								//Box 20
	BOX_21_RAM,								//Box 21
	BOX_22_RAM,								//Box 22
	BOX_23_RAM,								//Box 23
	BOX_24_RAM,								//Box 24
	BOX_25_RAM,								//Box 25
};

BoxNameT* const sPokemonBoxNamePtrs[TOTAL_BOXES_COUNT] =
{
	ORIGINAL_BOX_NAME_RAM,		//Box 1
	ORIGINAL_BOX_NAME_RAM + 1,	//Box 2
	ORIGINAL_BOX_NAME_RAM + 2,	//Box 3
	ORIGINAL_BOX_NAME_RAM + 3,	//Box 4
	ORIGINAL_BOX_NAME_RAM + 4,	//Box 5
	ORIGINAL_BOX_NAME_RAM + 5,	//Box 6
	ORIGINAL_BOX_NAME_RAM + 6,	//Box 7
	ORIGINAL_BOX_NAME_RAM + 7,	//Box 8
	ORIGINAL_BOX_NAME_RAM + 8,	//Box 9
	ORIGINAL_BOX_NAME_RAM + 9,	//Box 10
	ORIGINAL_BOX_NAME_RAM + 10,	//Box 11
	ORIGINAL_BOX_NAME_RAM + 11,	//Box 12
	ORIGINAL_BOX_NAME_RAM + 12,	//Box 13
	ORIGINAL_BOX_NAME_RAM + 13,	//Box 14
	ORIGINAL_BOX_NAME_RAM - 1,	//Box 15
	ORIGINAL_BOX_NAME_RAM - 2,	//Box 16
	ORIGINAL_BOX_NAME_RAM - 3,	//Box 17
	ORIGINAL_BOX_NAME_RAM - 4,	//Box 18
	ORIGINAL_BOX_NAME_RAM - 5,	//Box 19
	ORIGINAL_BOX_NAME_RAM - 6,	//Box 20
	ORIGINAL_BOX_NAME_RAM - 7,	//Box 21
	ORIGINAL_BOX_NAME_RAM - 8,	//Box 22
	ORIGINAL_BOX_NAME_RAM - 9,	//Box 23
	ORIGINAL_BOX_NAME_RAM - 10,	//Box 24
	ORIGINAL_BOX_NAME_RAM - 11,	//Box 25
	//[hack] patches/0196 26〜39 箱（PokemonStorage + 0x8134 の空き・棚のページ 0 に入る）
	(BoxNameT*) (0x202924C + 0x8134) + 0,	//Box 26
	(BoxNameT*) (0x202924C + 0x8134) + 1,	//Box 27
	(BoxNameT*) (0x202924C + 0x8134) + 2,	//Box 28
	(BoxNameT*) (0x202924C + 0x8134) + 3,	//Box 29
	(BoxNameT*) (0x202924C + 0x8134) + 4,	//Box 30
	(BoxNameT*) (0x202924C + 0x8134) + 5,	//Box 31
	(BoxNameT*) (0x202924C + 0x8134) + 6,	//Box 32
	(BoxNameT*) (0x202924C + 0x8134) + 7,	//Box 33
	(BoxNameT*) (0x202924C + 0x8134) + 8,	//Box 34
	(BoxNameT*) (0x202924C + 0x8134) + 9,	//Box 35
	(BoxNameT*) (0x202924C + 0x8134) + 10,	//Box 36
	(BoxNameT*) (0x202924C + 0x8134) + 11,	//Box 37
	(BoxNameT*) (0x202924C + 0x8134) + 12,	//Box 38
	(BoxNameT*) (0x202924C + 0x8134) + 13,	//Box 39
	//[hack] patches/0232 40〜50 箱（PokemonStorage + 0x8260・追加のセーブ領域のページ 4 に入る）
	(BoxNameT*) (0x202924C + 0x8260) + 0,	//Box 40
	(BoxNameT*) (0x202924C + 0x8260) + 1,	//Box 41
	(BoxNameT*) (0x202924C + 0x8260) + 2,	//Box 42
	(BoxNameT*) (0x202924C + 0x8260) + 3,	//Box 43
	(BoxNameT*) (0x202924C + 0x8260) + 4,	//Box 44
	(BoxNameT*) (0x202924C + 0x8260) + 5,	//Box 45
	(BoxNameT*) (0x202924C + 0x8260) + 6,	//Box 46
	(BoxNameT*) (0x202924C + 0x8260) + 7,	//Box 47
	(BoxNameT*) (0x202924C + 0x8260) + 8,	//Box 48
	(BoxNameT*) (0x202924C + 0x8260) + 9,	//Box 49
	(BoxNameT*) (0x202924C + 0x8260) + 10,	//Box 50
};

//Declaring this as "static" makes it not compile properly for some reason.
u8* const sPokemonBoxWallpaperPtrs[TOTAL_BOXES_COUNT] =
{
	ORIGINAL_BOX_WALLPAPER_RAM,							//Box 1
	ORIGINAL_BOX_WALLPAPER_RAM + 1,						//Box 2
	ORIGINAL_BOX_WALLPAPER_RAM + 2,						//Box 3
	ORIGINAL_BOX_WALLPAPER_RAM + 3,						//Box 4
	ORIGINAL_BOX_WALLPAPER_RAM + 4,						//Box 5
	ORIGINAL_BOX_WALLPAPER_RAM + 5,						//Box 6
	ORIGINAL_BOX_WALLPAPER_RAM + 6,						//Box 7
	ORIGINAL_BOX_WALLPAPER_RAM + 7,						//Box 8
	ORIGINAL_BOX_WALLPAPER_RAM + 8,						//Box 9
	ORIGINAL_BOX_WALLPAPER_RAM + 9,						//Box 10
	ORIGINAL_BOX_WALLPAPER_RAM + 10,					//Box 11
	ORIGINAL_BOX_WALLPAPER_RAM + 11,					//Box 12
	ORIGINAL_BOX_WALLPAPER_RAM + 12,					//Box 13
	ORIGINAL_BOX_WALLPAPER_RAM + 13,					//Box 14
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)),		//Box 15
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 1,	//Box 16
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 2,	//Box 17
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 3,	//Box 18
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 4,	//Box 19
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 5,	//Box 20
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 6,	//Box 21
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 7,	//Box 22
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 8,	//Box 23
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 9,	//Box 24
	(u8*) (ORIGINAL_BOX_POKEMON_RAM + (30 * 19)) + 10,	//Box 25
	//[hack] patches/0196 26〜39 箱（PokemonStorage + 0x81B2）
	(u8*) (0x202924C + 0x81B2) + 0,	//Box 26
	(u8*) (0x202924C + 0x81B2) + 1,	//Box 27
	(u8*) (0x202924C + 0x81B2) + 2,	//Box 28
	(u8*) (0x202924C + 0x81B2) + 3,	//Box 29
	(u8*) (0x202924C + 0x81B2) + 4,	//Box 30
	(u8*) (0x202924C + 0x81B2) + 5,	//Box 31
	(u8*) (0x202924C + 0x81B2) + 6,	//Box 32
	(u8*) (0x202924C + 0x81B2) + 7,	//Box 33
	(u8*) (0x202924C + 0x81B2) + 8,	//Box 34
	(u8*) (0x202924C + 0x81B2) + 9,	//Box 35
	(u8*) (0x202924C + 0x81B2) + 10,	//Box 36
	(u8*) (0x202924C + 0x81B2) + 11,	//Box 37
	(u8*) (0x202924C + 0x81B2) + 12,	//Box 38
	(u8*) (0x202924C + 0x81B2) + 13,	//Box 39
	//[hack] patches/0232 40〜50 箱（PokemonStorage + 0x82C3）
	(u8*) (0x202924C + 0x82C3) + 0,	//Box 40
	(u8*) (0x202924C + 0x82C3) + 1,	//Box 41
	(u8*) (0x202924C + 0x82C3) + 2,	//Box 42
	(u8*) (0x202924C + 0x82C3) + 3,	//Box 43
	(u8*) (0x202924C + 0x82C3) + 4,	//Box 44
	(u8*) (0x202924C + 0x82C3) + 5,	//Box 45
	(u8*) (0x202924C + 0x82C3) + 6,	//Box 46
	(u8*) (0x202924C + 0x82C3) + 7,	//Box 47
	(u8*) (0x202924C + 0x82C3) + 8,	//Box 48
	(u8*) (0x202924C + 0x82C3) + 9,	//Box 49
	(u8*) (0x202924C + 0x82C3) + 10,	//Box 50
};

#define gTempTeamBackup ((struct CompressedPokemon*) 0x203E118)

//This file's functions:
void CreateBoxMonFromCompressedMon(struct BoxPokemon* boxMon, struct CompressedPokemon* compMon);
#define HACK_MARK_OFF 0x10   //[hack] patches/0286 hyperTraining（手持ち・縮めた形とも 0x10）
#define HACK_MARK_BIT 0x80
void CreateCompressedMonFromBoxMon(struct BoxPokemon* boxMon, struct CompressedPokemon* compMon);
struct CompressedPokemon* GetCompressedMonPtr(u8 boxId, u8 boxPosition);

//[hack] patches/0196 ボックスの仮想化: 読むときは 1 匹の写し（棚の箱が入れ物に無ければフラッシュから直に）・書くときは入れ物（汚れの印）
static void ReadMonAt(u8 boxId, u8 boxPosition, struct CompressedPokemon* out)
{
	if (BoxShelf_ShelfIndex(boxId) >= 0)
		BoxShelf_ReadMon(boxId, boxPosition, out);
	else
		*out = sPokemonBoxPtrs[boxId][boxPosition];
}

static struct CompressedPokemon* MonPtrForWrite(u8 boxId, u8 boxPosition)
{
	struct CompressedPokemon* box = BoxShelf_BoxMons(boxId, TRUE);
	return (box != NULL) ? &box[boxPosition] : &sPokemonBoxPtrs[boxId][boxPosition];
}

u32 GetBoxMonDataAt(u8 boxId, u8 boxPosition, s32 request)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		if (request == MON_DATA_SPECIES)
		{
			//Save time since checking if a slot is empty is very common
			struct CompressedPokemon comp;
			ReadMonAt(boxId, boxPosition, &comp);
			return comp.substruct0.species;
		}
		else
		{
			struct BoxPokemon mon;
			struct CompressedPokemon comp;
			ReadMonAt(boxId, boxPosition, &comp);
			CreateBoxMonFromCompressedMon(&mon, &comp);
			return GetBoxMonData(&mon, request, NULL);
		}
	}
	else
		return 0;
}

void SetBoxMonDataAt(u8 boxId, u8 boxPosition, s32 request, const void* value)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		struct Pokemon mon; //Not Box Mon just in case stats are modified by hold item form change and it corrupts data
		struct CompressedPokemon* comp = MonPtrForWrite(boxId, boxPosition);
		CreateBoxMonFromCompressedMon((struct BoxPokemon*) &mon, comp); //Create temporary mon
		SetBoxMonData((struct BoxPokemon*) &mon, request, value);
		CreateCompressedMonFromBoxMon((struct BoxPokemon*) &mon, comp); //Copy new data back
	}
}

void GetBoxMonNickAt(u8 boxId, u8 boxPosition, u8* dst)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		struct BoxPokemon mon;
		struct CompressedPokemon comp;
		ReadMonAt(boxId, boxPosition, &comp);
		CreateBoxMonFromCompressedMon(&mon, &comp);
		GetBoxMonData(&mon, MON_DATA_NICKNAME, dst);
	}
	else
		*dst = EOS;
}

void SetBoxMonNickAt(u8 boxId, u8 boxPosition, const u8* nick)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		struct BoxPokemon mon;
		struct CompressedPokemon* comp = MonPtrForWrite(boxId, boxPosition);
		CreateBoxMonFromCompressedMon(&mon, comp);
		SetBoxMonData(&mon, MON_DATA_NICKNAME, nick);
		CreateCompressedMonFromBoxMon(&mon, comp); //Copy new data back
	}
}

u32 GetAndCopyBoxMonDataAt(u8 boxId, u8 boxPosition, s32 request, void* dst)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		struct BoxPokemon mon;
		struct CompressedPokemon comp;
		ReadMonAt(boxId, boxPosition, &comp);
		CreateBoxMonFromCompressedMon(&mon, &comp);
		return GetBoxMonData(&mon, request, dst);
	}
	else
		return 0;
}

void SetBoxMonAt(u8 boxId, u8 boxPosition, struct BoxPokemon* src)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		#ifdef BOX_PRESET_TEAMS
		if (boxId != BOX_PRESET_TEAMS) //Don't revert in Battle Box
		#endif
			HoopaShayminPCRevertCheck((struct Pokemon*) src, FALSE); //Don't recalc stats for a box mon

		struct CompressedPokemon mon;
		CreateCompressedMonFromBoxMon(src, &mon);
		*MonPtrForWrite(boxId, boxPosition) = mon;
	}
}

void CopyBoxMonAt(u8 boxId, u8 boxPosition, struct CompressedPokemon* dst)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
		ReadMonAt(boxId, boxPosition, dst);
}

void CreateBoxMonAt(u8 boxId, u8 boxPosition, u16 species, u8 level, u8 fixedIV, u8 hasFixedPersonality, u32 personality, u8 otIDType, u32 otID)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		struct BoxPokemon boxMon;
		CreateBoxMon(&boxMon,
					 species,
					 level,
					 fixedIV,
					 hasFixedPersonality, personality,
					 otIDType, otID);

		struct CompressedPokemon compMon;
		CreateCompressedMonFromBoxMon(&boxMon, &compMon);
		*MonPtrForWrite(boxId, boxPosition) = compMon;
	}
}

void ZeroBoxMonAt(u8 boxId, u8 boxPosition)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		//[hack] patches/0196 もう空なら書かない（はじめからの全部の箱を空にする所で、棚の箱を汚さない）
		struct CompressedPokemon comp;
		const u8* b = (const u8*) &comp;
		u32 i;
		ReadMonAt(boxId, boxPosition, &comp);
		for (i = 0; i < sizeof(comp) && b[i] == 0; ++i);
		if (i < sizeof(comp))
			memset(MonPtrForWrite(boxId, boxPosition), 0, sizeof(struct CompressedPokemon));
	}
}

void BoxMonAtToMon(u8 boxId, u8 boxPosition, struct Pokemon* dst)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		struct BoxPokemon mon;
		struct CompressedPokemon comp;
		ReadMonAt(boxId, boxPosition, &comp);
		CreateBoxMonFromCompressedMon(&mon, &comp);
		BoxMonToMon(&mon, dst);
	}
}

void CompressedMonToMon(struct CompressedPokemon* compMon, struct Pokemon* dst)
{
	struct BoxPokemon mon;
	CreateBoxMonFromCompressedMon(&mon, compMon);
	BoxMonToMon(&mon, dst);
}

struct BoxPokemon* GetBoxedMonPtr(u8 boxId, u8 boxPosition)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		struct CompressedPokemon compMon;
		ReadMonAt(boxId, boxPosition, &compMon);
		CreateBoxMonFromCompressedMon(gDisposableBoxMon, &compMon); //Use a static location in memory
		return gDisposableBoxMon;
	}
	else
		return NULL;
}

struct CompressedPokemon* GetCompressedMonPtr(u8 boxId, u8 boxPosition)
{
	if (boxId < TOTAL_BOXES_COUNT && boxPosition < IN_BOX_COUNT)
	{
		//[hack] patches/0196 ポインタを渡す（ステータス画面がこの先を 58 バイトずつ足して読む）→ 棚の箱は入れ物に読み込む（今の箱は追い出さない）
		struct CompressedPokemon* box = BoxShelf_BoxMons(boxId, FALSE);
		return (box != NULL) ? &box[boxPosition] : &sPokemonBoxPtrs[boxId][boxPosition];
	}
	else
		return NULL;
}

void CreateBoxMonFromCompressedMon(struct BoxPokemon* boxMon, struct CompressedPokemon* compMon)
{
	Memset(boxMon, 0, sizeof(struct BoxPokemon)); //Clear garbage data

	Memcpy(boxMon, compMon, 0x1C); //Personality to markings
	Memcpy(&boxMon->substruct0, &compMon->substruct0, sizeof(struct CompressedPokemonSubstruct0));
	Memcpy(&boxMon->substruct2, &compMon->hpEv, NUM_STATS); //Copy EVs
	Memcpy(&boxMon->substruct3, &compMon->pokerus, 8); //Copy some of substruct misc

	boxMon->substruct1.moves[0] = compMon->move1;
	boxMon->substruct1.moves[1] = compMon->move2;
	boxMon->substruct1.moves[2] = compMon->move3;
	boxMon->substruct1.moves[3] = compMon->move4;

	for (int i = 0; i < MAX_MON_MOVES; ++i) //Give PP
		boxMon->substruct1.pp[i] = CalculatePPWithBonus(boxMon->substruct1.moves[i], boxMon->substruct0.ppBonuses, i);

	//[hack] patches/0259 #70 ポケモン自身の表 ID・裏 ID: 縮めた形の空きビット 32 本 → コンディションの 4 バイト（src/hack_mon_id.c）
	HackMonId_ClearSpare(boxMon);
	HackMonId_Set(boxMon, HackMonId_UnpackComp(compMon));

	//[hack] patches/0286 うんめいの印（obedient・クチバの特別なフカマル・お母さんのピカチュウ）: 縮めた形には欄が無く、預けると消えていた。
	//  縮めるとき すごいとっくんの欄（0x10・使うのは bit0〜5）の bit7 に入れたものを戻す
	if (((u8*) boxMon)[HACK_MARK_OFF] & HACK_MARK_BIT)
	{
		((u8*) boxMon)[HACK_MARK_OFF] &= ~HACK_MARK_BIT;
		boxMon->substruct3.obedient = TRUE;
	}
}

void CreateCompressedMonFromBoxMon(struct BoxPokemon* boxMon, struct CompressedPokemon* compMon)
{
	Memset(compMon, 0, sizeof(struct CompressedPokemon)); //Clear garbage data

	Memcpy(compMon, boxMon, 0x1C); //Personality to markings
	Memcpy(&compMon->substruct0, &boxMon->substruct0, sizeof(struct CompressedPokemonSubstruct0));
	Memcpy(&compMon->hpEv, &boxMon->substruct2, NUM_STATS); //Copy EVs
	Memcpy(&compMon->pokerus, &boxMon->substruct3, 8); //Copy some of substruct misc

	compMon->move1 = boxMon->substruct1.moves[0];
	compMon->move2 = boxMon->substruct1.moves[1];
	compMon->move3 = boxMon->substruct1.moves[2];
	compMon->move4 = boxMon->substruct1.moves[3];

	//[hack] patches/0259 #70 ポケモン自身の表 ID・裏 ID: まだ無ければ今の色違いのまま付けて、空きビット 32 本へ（src/hack_mon_id.c）
	HackMonId_Ensure(boxMon);
	HackMonId_PackComp(compMon, HackMonId_Get(boxMon));

	//[hack] patches/0286 うんめいの印を すごいとっくんの欄の bit7 へ（上の CreateBoxMonFromCompressedMon で戻す）
	((u8*) compMon)[HACK_MARK_OFF] &= ~HACK_MARK_BIT;
	if (boxMon->substruct3.obedient)
		((u8*) compMon)[HACK_MARK_OFF] |= HACK_MARK_BIT;
}

u8* GetBoxNamePtr(u8 boxId)
{
	if (boxId < TOTAL_BOXES_COUNT)
		return (u8*) sPokemonBoxNamePtrs[boxId];
	else
		return NULL;
}

u8 GetBoxWallpaper(u8 boxId)
{
	if (boxId < TOTAL_BOXES_COUNT)
		return *sPokemonBoxWallpaperPtrs[boxId];
	else
		return 0;
}

void SetBoxWallpaper(u8 boxId, u8 wallpaperId)
{
	if (boxId < TOTAL_BOXES_COUNT && wallpaperId < WALLPAPER_COUNT)
		*(sPokemonBoxWallpaperPtrs[boxId]) = wallpaperId;
}

s8 sub_8091130(u8 boxId)
{
	u8 i;
	u8 currentBox = StorageGetCurrentBox();

	for (i = 0; currentBox != boxId; i++)
	{
		currentBox++;
		if (currentBox >= TOTAL_BOXES_COUNT)
			currentBox = 0;
	}

	return (i < TOTAL_BOXES_COUNT / 2) ? 1 : -1;
}

s16 sub_808B884(struct CompressedPokemon *boxMons, u8 currIndex, u8 maxIndex, u8 arg3)
{
	s16 i;
	s16 adder = -1;

	if (arg3 < 2)
		adder = 1;

	if (arg3 == 1 || arg3 == 3)
	{
		for (i = (s8)currIndex + adder; i >= 0 && i <= maxIndex; i += adder)
		{
			if (boxMons[i].substruct0.species != SPECIES_NONE)
				return i;
		}
	}
	else
	{
		for (i = (s8) currIndex + adder; i >= 0 && i <= maxIndex; i += adder)
		{
			struct BoxPokemon boxMon;
			CreateBoxMonFromCompressedMon(&boxMon, &boxMons[i]);
			if (boxMon.substruct0.species != SPECIES_NONE
			&& !GetBoxMonData(&boxMon, MON_DATA_IS_EGG, NULL))
				return i;
		}
	}

	return -1;
}

u8 SendMonToPC(struct Pokemon* mon)
{
	s32 boxNo, boxPos;

	SetPCBoxToSendMon(VarGet(VAR_PC_BOX_TO_SEND_MON));

	boxNo = StorageGetCurrentBox();

	do
	{
		for (boxPos = 0; boxPos < IN_BOX_COUNT && BoxShelf_CanTouchBox(boxNo); boxPos++) //[hack] patches/0196 次のレポートに収まらない箱は「いっぱい」と同じ
		{
			struct CompressedPokemon checking;
			ReadMonAt(boxNo, boxPos, &checking); //[hack] patches/0196 空きを探すだけでは入れ物を動かさない
			if (checking.substruct0.species == SPECIES_NONE)
			{
				CreateCompressedMonFromBoxMon((struct BoxPokemon*) mon, MonPtrForWrite(boxNo, boxPos));
				gSpecialVar_MonBoxId = boxNo;
				gSpecialVar_MonBoxPos = boxPos;
				if (GetPCBoxToSendMon() != boxNo)
					FlagClear(FLAG_SHOWN_BOX_WAS_FULL_MESSAGE);
				VarSet(VAR_PC_BOX_TO_SEND_MON, boxNo);
				return MON_GIVEN_TO_PC;
			}
		}

		boxNo++;
		if (boxNo == TOTAL_BOXES_COUNT)
			boxNo = 0;

	} while (boxNo != StorageGetCurrentBox());

	return MON_CANT_GIVE;
}


bool8 SendMonToBoxPos(struct Pokemon* mon, u8 boxNo, u8 boxPos)
{
	if (boxNo >= TOTAL_BOXES_COUNT || boxPos >= IN_BOX_COUNT)
		return TRUE;

	struct CompressedPokemon* checkingMon = MonPtrForWrite(boxNo, boxPos); //[hack] patches/0196
	CreateCompressedMonFromBoxMon((struct BoxPokemon*) mon, checkingMon);
	gSpecialVar_MonBoxId = boxNo;
	gSpecialVar_MonBoxPos = boxPos;
	if (GetPCBoxToSendMon() != boxNo)
		FlagClear(FLAG_SHOWN_BOX_WAS_FULL_MESSAGE);
	VarSet(VAR_PC_BOX_TO_SEND_MON, boxNo);
	return FALSE;

}

u32 SummaryScreenBoxMonMultiplier(u8 index)
{
	return index * sizeof(struct CompressedPokemon);
}

void BackupPartyToTempTeam(u8 firstId, u8 numPokes)
{
	struct CompressedPokemon* backup = gTempTeamBackup;

	for (int i = 0; i < numPokes; ++i)
		CreateCompressedMonFromBoxMon((struct BoxPokemon*) &gPlayerParty[firstId + i], &backup[firstId + i]);
}

void RestorePartyFromTempTeam(u8 firstId, u8 numPokes)
{
	struct CompressedPokemon* backup = gTempTeamBackup;

	for (int i = 0; i < numPokes; ++i)
	{
		Memset(&gPlayerParty[firstId + i], 0, sizeof(struct Pokemon)); //Wipe old data - mainly for wiping struct data that's not included in Box mon *IMPORTANT
		CreateBoxMonFromCompressedMon((struct BoxPokemon*) &gPlayerParty[firstId + i], &backup[firstId + i]);
		CalculateMonStats(&gPlayerParty[firstId + i]);
	}
}

u16 __attribute__((long_call)) PSS_TryLoadMonIconTiles(u16 species);
void __attribute__((long_call)) LoadCursorMonSprite(void);
void __attribute__((long_call)) RefreshCursorMonData(void);
void PlaceBoxMonIcon(u8 boxId, u8 position)
{
    if (boxId >= TOTAL_BOXES_COUNT) //Party mon
    {
        gPSSData->partySprites[position] = gPSSData->movingMonSprite;
        gPSSData->partySprites[position]->oam.priority = 1;
        gPSSData->partySprites[position]->subpriority = 12;
    }
    else
    {
		u16 species = GetBoxMonDataAt(boxId, position, MON_DATA_SPECIES2);
	
		#if (defined SPECIES_HOOPA || defined SPECIES_SHAYMIN)
		//Try an instant sprite change for post placing Hoopa-Unbound or Shaymin-Sky in the PC
		if (species == SPECIES_HOOPA || species == SPECIES_SHAYMIN)
		{
			u16 tileNum = PSS_TryLoadMonIconTiles(species); //Gets the tile number of the mon icon
			if (tileNum != 0xFFFF)
			{
				//Update mon Icon
				gPSSData->movingMonSprite->oam.tileNum = tileNum;
				gPSSData->movingMonSprite->oam.paletteNum = IndexOfSpritePaletteTag(0xDAC0 + GetMonIconPaletteIndexFromSpecies(species));
			}

			//Update front sprite
			u32 otId = GetBoxMonDataAt(boxId, position, MON_DATA_OT_ID);
			gPSSData->cursorMonSpecies = species;
			gPSSData->cursorMonPalette = GetMonSpritePalFromSpeciesAndPersonality(gPSSData->cursorMonSpecies, otId, gPSSData->cursorMonPersonality);
			RefreshCursorMonData();
		}
		#endif

        gPSSData->boxMonsSprites[position] = gPSSData->movingMonSprite;
        gPSSData->boxMonsSprites[position]->oam.priority = 2;
        gPSSData->boxMonsSprites[position]->subpriority = 19 - (position % IN_BOX_ROWS);
    }

    gPSSData->movingMonSprite->callback = SpriteCallbackDummy;
    gPSSData->movingMonSprite = NULL;
}

void FixItemNameInPokemonStorageSystem(void)
{
	const u8* name = ItemId_GetName(gPSSData->cursorMonItem);
	u8 length = StringLength(name);

	StringCopyPadded(gPSSData->cursorMonTexts[3], name, CHAR_SPACE, 14);
	if (length >= 14) //Too long to look nice
	{
		gPSSData->cursorMonTexts[3][12] = CHAR_ELLIPSIS; //End with trailing ...
		gPSSData->cursorMonTexts[3][13] = EOS;
	}
}
