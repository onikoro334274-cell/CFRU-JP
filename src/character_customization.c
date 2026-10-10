#include "defines.h"
#include "defines_battle.h"
#include "../include/event_object_movement.h"
#include "../include/field_effect_helpers.h"
#include "../include/field_player_avatar.h"
#include "../include/link.h"
#include "../include/random.h"
#include "../include/sprite.h"
#include "../include/constants/event_object_movement_constants.h"
#include "../include/constants/event_objects.h"
#include "../include/constants/trainers.h"

#include "../include/new/character_customization.h"
#include "../include/new/follow_me.h"
#include "../include/new/frontier.h"
#include "../include/new/multi.h"
#include "../include/new/util.h"
/*
character_customization.c
	functions for altering the player's sprite based on the current sprite/palette selections

tables to edit:
	gOverworldTableSwitcher
	sPlayerAvatarGfxIds

*/

#ifdef UNBOUND
extern const u8 TS_Male_Player_White_Brunette_BlackPal[];
extern const u8 TS_Male_Player_White_Silver_RedPal[];
extern const u8 TS_Male_Player_White_Silver_BlackPal[];
extern const u8 TS_Male_Player_Brown_Dark_RedPal[];
extern const u8 TS_Male_Player_Brown_Dark_BlackPal[];
extern const u8 TS_Male_Player_Brown_Silver_RedPal[];
extern const u8 TS_Male_Player_Brown_Silver_BlackPal[];
extern const u8 BS_Male_Player_White_Brunette_BlackPal[];
extern const u8 BS_Male_Player_White_Silver_RedPal[];
extern const u8 BS_Male_Player_White_Silver_BlackPal[];
extern const u8 BS_Male_Player_Brown_Dark_RedPal[];
extern const u8 BS_Male_Player_Brown_Dark_BlackPal[];
extern const u8 BS_Male_Player_Brown_Silver_RedPal[];
extern const u8 BS_Male_Player_Brown_Silver_BlackPal[];
extern const u8 TS_Female_Player_White_Brunette_PurplePal[];
extern const u8 TS_Female_Player_White_Silver_RedPal[];
extern const u8 TS_Female_Player_White_Silver_PurplePal[];
extern const u8 TS_Female_Player_Brown_Dark_RedPal[];
extern const u8 TS_Female_Player_Brown_Dark_PurplePal[];
extern const u8 TS_Female_Player_Brown_Silver_RedPal[];
extern const u8 TS_Female_Player_Brown_Silver_PurplePal[];
extern const u8 BS_Female_Player_White_Brunette_PurplePal[];
extern const u8 BS_Female_Player_White_Silver_RedPal[];
extern const u8 BS_Female_Player_White_Silver_PurplePal[];
extern const u8 BS_Female_Player_Brown_Dark_RedPal[];
extern const u8 BS_Female_Player_Brown_Dark_PurplePal[];
extern const u8 BS_Female_Player_Brown_Silver_RedPal[];
extern const u8 BS_Female_Player_Brown_Silver_PurplePal[];

struct CharacterCustomizationPaletteSwitch
{
	u16 owNum;
	const u8* frontSpritePal;
	const u8* backSpritePal;
};

static const struct CharacterCustomizationPaletteSwitch sCharacterPalSwitchTable[] =
{
	{262, 		TS_Male_Player_White_Brunette_BlackPal, BS_Male_Player_White_Brunette_BlackPal},
	{271, 		TS_Male_Player_White_Silver_RedPal, BS_Male_Player_White_Silver_RedPal},
	{280, 		TS_Male_Player_White_Silver_BlackPal, BS_Male_Player_White_Silver_BlackPal},
	{289, 		TS_Male_Player_Brown_Dark_RedPal, BS_Male_Player_Brown_Dark_RedPal},
	{298, 		TS_Male_Player_Brown_Dark_BlackPal, BS_Male_Player_Brown_Dark_BlackPal},
	{307, 		TS_Male_Player_Brown_Silver_RedPal, BS_Male_Player_Brown_Silver_RedPal},
	{316, 		TS_Male_Player_Brown_Silver_BlackPal, BS_Male_Player_Brown_Silver_BlackPal},
	{325, 		TS_Female_Player_White_Brunette_PurplePal, BS_Female_Player_White_Brunette_PurplePal},
	{334, 		TS_Female_Player_White_Silver_RedPal, BS_Female_Player_White_Silver_RedPal},
	{343, 		TS_Female_Player_White_Silver_PurplePal, BS_Female_Player_White_Silver_PurplePal},
	{352, 		TS_Female_Player_Brown_Dark_RedPal, BS_Female_Player_Brown_Dark_RedPal},
	{361, 		TS_Female_Player_Brown_Dark_PurplePal, BS_Female_Player_Brown_Dark_PurplePal},
	{370, 		TS_Female_Player_Brown_Silver_RedPal, BS_Female_Player_Brown_Silver_RedPal},
	{379, 		TS_Female_Player_Brown_Silver_PurplePal, BS_Female_Player_Brown_Silver_PurplePal},
	{0xFFFF, 	NULL, NULL},
};
#endif

#ifdef EXISTING_OW_TABLE_ADDRESS
	#define gOverworldTableSwitcher ((struct EventObjectGraphicsInfo***) EXISTING_OW_TABLE_ADDRESS)
#elif defined UNBOUND //For Pokemon Unbound
	const struct EventObjectGraphicsInfo** const gOverworldTableSwitcher[] =
	{
		(NPCPtr*) 0x88110E0,
		(NPCPtr*) 0x88B2720,
		(NPCPtr*) 0x88B2B20,
	};

#else //Modify this
	//★ 表 1〜5 = ポケモンの連れ歩きの絵（src/bw_follower_gfx.c・#16。256 件 × 5 = 1280 枠）
	extern const struct EventObjectGraphicsInfo* const gBwFollowerOwTable1[256];
	extern const struct EventObjectGraphicsInfo* const gBwFollowerOwTable2[256];
	extern const struct EventObjectGraphicsInfo* const gBwFollowerOwTable3[256];
	extern const struct EventObjectGraphicsInfo* const gBwFollowerOwTable4[256];
	extern const struct EventObjectGraphicsInfo* const gBwFollowerOwTable5[256];
	//★ 表 6 = BW の主人公の絵（src/bw_player_gfx.c・#20）
	extern const struct EventObjectGraphicsInfo* const gBwPlayerOwTable[256];
	//★ 表 7 = ホウエンの NPC（src/Tables/hoenn_ow_gfx.c・#2 ジム）
	extern const struct EventObjectGraphicsInfo* const gHoennOwTable[256];
	//★ 表 8 = 主人公の歩き・走りを BW の主人公に塗り替えたもの（src/Tables/bw_walk_gfx.c・#20 案 C）
	extern const struct EventObjectGraphicsInfo* const gBwWalkOwTable[256];
	// create 255 OW tables
	const struct EventObjectGraphicsInfo** const gOverworldTableSwitcher[255] =
	{
		(NPCPtr*) 0x8363E38,
		(NPCPtr*) gBwFollowerOwTable1,
		(NPCPtr*) gBwFollowerOwTable2,
		(NPCPtr*) gBwFollowerOwTable3,
		(NPCPtr*) gBwFollowerOwTable4,
		(NPCPtr*) gBwFollowerOwTable5,
		(NPCPtr*) gBwPlayerOwTable,
		(NPCPtr*) gHoennOwTable,
		(NPCPtr*) gBwWalkOwTable,
		// etc...
		// please note that this method makes compatability with OW Manager challenging
	};
#endif

struct PlayerGraphics
{
	u16 graphicsId;
	u8 stateFlag;
};

static const struct PlayerGraphics sPlayerAvatarGfxIds[][2] =
{
	//★ #20: 歩き・走りは素のレッド／リーフを BW のトウヤ／トウコに**塗り替えた絵**（表 8・案 C・2026-09-23）。
	//   0x0800 = トウヤ / 0x0801 = トウコ（tools/bwplayer/build_bw_walk.py）。素に戻すときは
	//   EVENT_OBJ_GFX_RED_NORMAL / EVENT_OBJ_GFX_LEAF_NORMAL に戻すだけでよい。
	//   ★ 表 6 の 0x0600（DS の絵を縮めたもの）は 2026-09-20 に取りやめたまま使っていない。
	//★ 2026-09-23: 自転車・なみのり・技を使う・つりざお・VS シーカー・潜水も同じ塗り替え（表 8 の 2〜13 番）。
	//   素に戻すときは各行を EVENT_OBJ_GFX_RED_* / EVENT_OBJ_GFX_LEAF_* に戻すだけでよい。
	[PLAYER_AVATAR_STATE_NORMAL] =     {{0x0800, PLAYER_AVATAR_FLAG_ON_FOOT},          {0x0801, PLAYER_AVATAR_FLAG_ON_FOOT}},
	[PLAYER_AVATAR_STATE_BIKE] =       {{0x0802, PLAYER_AVATAR_FLAG_BIKE},               {0x0803, PLAYER_AVATAR_FLAG_BIKE}},
	[PLAYER_AVATAR_STATE_SURFING] =    {{0x0804, PLAYER_AVATAR_FLAG_SURFING},         {0x0805, PLAYER_AVATAR_FLAG_SURFING}},
	[PLAYER_AVATAR_STATE_FIELD_MOVE] = {{0x0806, PLAYER_AVATAR_FLAG_FIELD_MOVE},   {0x0807, PLAYER_AVATAR_FLAG_FIELD_MOVE}},
	[PLAYER_AVATAR_STATE_FISHING] =    {{0x0808, 0},                                  {0x0809, 0}},
	[PLAYER_AVATAR_STATE_VS_SEEKER] =  {{0x080A, 0},                                {0x080B, 0}},
	[PLAYER_AVATAR_STATE_UNDERWATER] = {{0x080C, PLAYER_AVATAR_FLAG_UNDERWATER},   {0x080D, PLAYER_AVATAR_FLAG_UNDERWATER}},
};

void BwFollowerUpdateFromParty(void); //★ #16（src/bw_follower.c）
u16 HoennBikeModeGfx(u8 gender, u16 normalGfx); //★ 2026-09-24 自転車のモード（src/hoenn_bike.c）

//★ 2026-09-24: マッハ・ダートの絵（表 8 の 16〜19 番）を「自転車の絵」として数える。
//   逆引き（絵 → 状態・性別）は素の表の番号でしか引けないので、通常の自転車の絵に直してから引く
static u16 NormalizeBikeModeGfx(u16 graphicsId)
{
	switch (graphicsId) {
		case 0x0810:
		case 0x0812:
			return sPlayerAvatarGfxIds[PLAYER_AVATAR_STATE_BIKE][MALE].graphicsId;
		case 0x0811:
		case 0x0813:
			return sPlayerAvatarGfxIds[PLAYER_AVATAR_STATE_BIKE][FEMALE].graphicsId;
	}
	return graphicsId;
}

//This file's functions:
#ifdef UNBOUND
static const u8* GetAlternateTrainerSpritePal(void);
#endif

//npc_get_type hack for character customization
//hook at 0805F2C8 via r1
NPCPtr GetEventObjectGraphicsInfo(u16 graphicsId)
{
	u16 newId;
	u8 tableId = (graphicsId >> 8) & 0xFF;	// upper byte
	u8 spriteId = graphicsId & 0xFF;		// lower byte

	//Check runtime changeable OWs
	if (tableId == 0xFF && spriteId <= 0xF)
	{
		//Runtime changeable
		newId = VarGet(VAR_RUNTIME_CHANGEABLE + spriteId);
		tableId = (newId >> 8) & 0xFF;	// upper byte
		spriteId = (newId & 0xFF);		// lower byte
	}
	else
	{
		switch (spriteId) {
			case EVENT_OBJ_GFX_RED_BIKE_VS_SEEKER:
			case EVENT_OBJ_GFX_LEAF_BIKE_VS_SEEKER:
				if (tableId == 0) //Actually the Vs. Seeker sprites
				{
					newId = VarGet(VAR_PLAYER_VS_SEEKER_ON_BIKE);
					if (newId != 0) //Actually set to something different
					{
						tableId = (newId >> 8) & 0xFF;	// upper byte
						spriteId = (newId & 0xFF);		// lower byte
					}
					else //★ #20: 自転車の VS シーカーも BW の主人公の塗り替え（表 8 の 12 / 13 番）
					{
						spriteId = (spriteId == EVENT_OBJ_GFX_RED_BIKE_VS_SEEKER) ? 14 : 15; //★ 2026-09-23: 12/13 は潜水（エメラルドの絵）に分けた
						tableId = 8;
					}
				}
				break;
		}

		#ifndef UNBOUND
		if (spriteId > 239 && tableId == 0)
		{
			newId = VarGetEventObjectGraphicsId(spriteId + 16);
			tableId = (newId >> 8) & 0xFF;	// upper byte
			spriteId = (newId & 0xFF);		// lower byte
		}
		#endif
	}

	NPCPtr spriteAddr;
	#ifndef EXISTING_OW_TABLE_ADDRESS
	if (tableId >= NELEMS(gOverworldTableSwitcher)
	|| gOverworldTableSwitcher[tableId] == 0)
		spriteAddr = gOverworldTableSwitcher[0][spriteId];
	else
	#endif
		spriteAddr = gOverworldTableSwitcher[tableId][spriteId];

	if (spriteAddr == NULL)
		spriteAddr = gOverworldTableSwitcher[0][EVENT_OBJ_GFX_LITTLE_BOY];	// first non-player sprite in first table default

	return spriteAddr;
};

NPCPtr GetEventObjectGraphicsInfoByEventObj(struct EventObject* eventObj)
{
	return GetEventObjectGraphicsInfo(GetEventObjectGraphicsId(eventObj));
}

static u16 GetCustomGraphicsIdByState(u8 state)
{
	u16 gfxId = 0;

	switch (state) {
		case PLAYER_AVATAR_STATE_NORMAL:
			gfxId = VarGet(VAR_PLAYER_WALKRUN);
			break;
		case PLAYER_AVATAR_STATE_BIKE:
			gfxId = VarGet(VAR_PLAYER_BIKING);
			break;
		case PLAYER_AVATAR_STATE_SURFING:
			gfxId = VarGet(VAR_PLAYER_SURFING);
			break;
		case PLAYER_AVATAR_STATE_FIELD_MOVE: //HM Use
			gfxId = VarGet(VAR_PLAYER_HM_USE);
			break;
		case PLAYER_AVATAR_STATE_VS_SEEKER:
			gfxId = VarGet(VAR_PLAYER_VS_SEEKER);
			break;
		case PLAYER_AVATAR_STATE_FISHING:
			gfxId = VarGet(VAR_PLAYER_FISHING);
			break;
		case PLAYER_AVATAR_STATE_UNDERWATER:
			gfxId = VarGet(VAR_PLAYER_UNDERWATER);
			break;
	}

	return gfxId;
}

u16 GetPlayerAvatarGraphicsIdByStateIdAndGender(u8 state, u8 gender)
{
	u16 graphicsId = GetCustomGraphicsIdByState(state);
	if (graphicsId != 0)
		return graphicsId;

	if (state == PLAYER_AVATAR_STATE_BIKE) //★ 2026-09-24 自転車のモードの絵
		return HoennBikeModeGfx(gender, sPlayerAvatarGfxIds[state][gender].graphicsId);

	return sPlayerAvatarGfxIds[state][gender].graphicsId;
}

u16 GetPlayerAvatarGraphicsIdByStateId(u8 state)
{
	return GetPlayerAvatarGraphicsIdByStateIdAndGender(state, gPlayerAvatar->gender);
}

u8 GetPlayerAvatarStateTransitionByGraphicsId(u16 graphicsId, u8 gender)
{
	graphicsId = NormalizeBikeModeGfx(graphicsId); //★ 2026-09-24
    for (u8 state = 0; state < NELEMS(sPlayerAvatarGfxIds); ++state)
    {
		u16 customGraphicsId = GetCustomGraphicsIdByState(state);
		if (customGraphicsId == graphicsId)
			graphicsId = sPlayerAvatarGfxIds[state][gender].graphicsId;

        if (sPlayerAvatarGfxIds[state][gender].graphicsId == graphicsId)
            return sPlayerAvatarGfxIds[state][gender].stateFlag;
    }

    return PLAYER_AVATAR_FLAG_ON_FOOT;
}

u16 GetPlayerAvatarGraphicsIdByCurrentState(void)
{
	u8 state = 0;
	u8 gender = gPlayerAvatar->gender;
    u8 flags = gPlayerAvatar->flags;

    for (; state < NELEMS(sPlayerAvatarGfxIds); ++state)
    {
        if (sPlayerAvatarGfxIds[state][gender].stateFlag & flags)
		{
			u16 graphicsId = sPlayerAvatarGfxIds[state][gender].graphicsId;
			u16 customGraphicsId = GetCustomGraphicsIdByState(state);
			if (customGraphicsId != 0)
				graphicsId = customGraphicsId;
			else if (state == PLAYER_AVATAR_STATE_BIKE) //★ 2026-09-24 自転車のモードの絵
				graphicsId = HoennBikeModeGfx(gender, graphicsId);

			return graphicsId;
		}
    }

    return sPlayerAvatarGfxIds[0][0].graphicsId;
}

//★ 2026-09-23: 引数を u8 → u16 に。表 8 の絵（0x0800〜）は u8 だと下位 1 バイトに切り詰められ、
//   トウコ（0x0801 → 0x01）がどの行とも一致せず MALE を返していた（メニューや戦闘から戻るたびに男扱い）。
//   素の呼び元（0805E904）は SetPlayerAvatarEventObjectIdAndObjectId ごと hooks で差し替え済みなので通らない。
u8 GetPlayerAvatarGenderByGraphicsId(u16 gfxId)
{
	gfxId = NormalizeBikeModeGfx(gfxId); //★ 2026-09-24
    for (u8 state = 0; state < NELEMS(sPlayerAvatarGfxIds); ++state)
    {
		for (u8 gender = 0; gender < NELEMS(sPlayerAvatarGfxIds[0]); ++gender)
		{
			if (sPlayerAvatarGfxIds[state][gender].graphicsId == gfxId)
				return gender;

			u16 customGraphicsId = GetCustomGraphicsIdByState(state);
			if (customGraphicsId != 0 && customGraphicsId == gfxId)
				return gSaveBlock2->playerGender;
		}
	}

	return MALE;
}

static void SetPlayerAvatarExtraStateTransition(u16 graphicsId, u8 b)
{
    u8 unk = GetPlayerAvatarStateTransitionByGraphicsId(graphicsId, gPlayerAvatar->gender);
	SetPlayerAvatarTransitionFlags(unk | b);
}

u16 GetEventObjectGraphicsId(struct EventObject* eventObj)
{
	u8 lowerByte = eventObj->graphicsIdLowerByte;
	u8 upperByte = eventObj->graphicsIdUpperByte;

	#ifndef EXISTING_OW_TABLE_ADDRESS
	if (upperByte >= NELEMS(gOverworldTableSwitcher)
	&& upperByte != 0xFF) //Dynamic OW table
		return lowerByte;
	#endif

	if (upperByte == 0xFF && lowerByte <= 0xF)
		return VarGet(VAR_RUNTIME_CHANGEABLE + lowerByte); //Runtime changeable

	return lowerByte | (upperByte << 8);
}

void SetPlayerAvatarEventObjectIdAndObjectId(u8 eventObjectId, u8 spriteId)
{
    gPlayerAvatar->eventObjectId = eventObjectId;
    gPlayerAvatar->spriteId = spriteId;
    gPlayerAvatar->gender = GetPlayerAvatarGenderByGraphicsId(GetEventObjectGraphicsId(&gEventObjects[eventObjectId]));
    SetPlayerAvatarExtraStateTransition(GetEventObjectGraphicsId(&gEventObjects[eventObjectId]), 0x20);
}

// load trainer card sprite based on variables
// 	hook at 810c374 via r2
u8 PlayerGenderToFrontTrainerPicId(u8 gender, bool8 modify)
{
	if (modify != TRUE)
		return gender;

	u16 trainerId = VarGet(VAR_TRAINERCARD_MALE + gender);
	if (trainerId == 0)
		trainerId = TRAINER_PIC_PLAYER_M + gender;

	return trainerId;
};

void InitPlayerAvatar(s16 x, s16 y, u8 direction, u8 gender)
{
	u8 eventObjectId;
	struct EventObject* eventObject;
	struct EventObjectTemplate playerEventObjTemplate = {0};
	u16 graphicsId = GetPlayerAvatarGraphicsIdByStateIdAndGender(PLAYER_AVATAR_STATE_NORMAL, gender);

	playerEventObjTemplate.localId = EVENT_OBJ_ID_PLAYER;
	playerEventObjTemplate.graphicsIdLowerByte = graphicsId & 0xFF;
	playerEventObjTemplate.graphicsIdUpperByte = graphicsId >> 8;
	playerEventObjTemplate.x = x - 7;
	playerEventObjTemplate.y = y - 7;
	playerEventObjTemplate.movementType = MOVEMENT_TYPE_PLAYER;

	eventObjectId = SpawnSpecialEventObject(&playerEventObjTemplate);
	eventObject = &gEventObjects[eventObjectId];
	eventObject->isPlayer = 1;
	eventObject->warpArrowSpriteId = CreateWarpArrowSprite();
	EventObjectTurn(eventObject, direction);
	ClearPlayerAvatarInfo();

	gPlayerAvatar->runningState = NOT_MOVING;
	gPlayerAvatar->tileTransitionState = T_NOT_MOVING;
	gPlayerAvatar->eventObjectId = eventObjectId;
	gPlayerAvatar->spriteId = eventObject->spriteId;
	gPlayerAvatar->gender = gender;
	SetPlayerAvatarStateMask(PLAYER_AVATAR_FLAG_FIELD_MOVE | PLAYER_AVATAR_FLAG_ON_FOOT);
	BwFollowerUpdateFromParty(); //★ #16: 手持ちの先頭から連れ歩きを組み立てる（src/bw_follower.c）
	CreateFollowerAvatar();
}

void PlayerHandleDrawTrainerPic(void)
{
	s16 xPos, yPos;
	u32 trainerPicId = GetBackspriteId();

	if (gBattleTypeFlags & (BATTLE_TYPE_MULTI | BATTLE_TYPE_INGAME_PARTNER))
	{
		if ((GetBattlerPosition(gActiveBattler) & BIT_FLANK) != B_FLANK_LEFT) // Second mon, on the right.
			xPos = 90;
		else // First mon, on the left.
			xPos = 32;

		yPos = (8 - gTrainerBackPicCoords[trainerPicId].coords) * 4 + 80;
	}

	else
	{
		xPos = 80;
		yPos = (8 - gTrainerBackPicCoords[trainerPicId].coords) * 4 + 80;
	}

	LoadTrainerBackPal(trainerPicId, gActiveBattler);
	SetMultiuseSpriteTemplateToTrainerBack(trainerPicId, GetBattlerPosition(gActiveBattler));
	gBattlerSpriteIds[gActiveBattler] = CreateSprite(&gMultiuseSpriteTemplate[0], xPos, yPos, GetBattlerSpriteSubpriority(gActiveBattler));

	if (IS_DOUBLE_BATTLE)
		gSprites[gBattlerSpriteIds[gActiveBattler]].oam.priority = 0; //So it appears above enemy healthbars
	
	gSprites[gBattlerSpriteIds[gActiveBattler]].oam.paletteNum = gActiveBattler;
	gSprites[gBattlerSpriteIds[gActiveBattler]].pos2.x = 240;
	gSprites[gBattlerSpriteIds[gActiveBattler]].data[0] = -3; //-2; //Speed scrolling in
	gSprites[gBattlerSpriteIds[gActiveBattler]].callback = SpriteCB_TrainerSlideIn; //sub_805D7AC in Emerald

	gBattlerControllerFuncs[gActiveBattler] = Player_CompleteOnBattlerSpriteCallbackDummy;
}

void PlayerHandleTrainerSlide(void)
{
	u32 trainerPicId = GetBackspriteId();

	LoadTrainerBackPal(trainerPicId, gActiveBattler);
	SetMultiuseSpriteTemplateToTrainerBack(trainerPicId, GetBattlerPosition(gActiveBattler));
	gBattlerSpriteIds[gActiveBattler] = CreateSprite(&gMultiuseSpriteTemplate[0], 80, (8 - gTrainerBackPicCoords[trainerPicId].coords) * 4 + 80, 30);

	if (IS_DOUBLE_BATTLE)
		gSprites[gBattlerSpriteIds[gActiveBattler]].oam.priority = 0;

	gSprites[gBattlerSpriteIds[gActiveBattler]].oam.paletteNum = gActiveBattler;
	gSprites[gBattlerSpriteIds[gActiveBattler]].pos2.x = -96;
	gSprites[gBattlerSpriteIds[gActiveBattler]].data[0] = 2;
	gSprites[gBattlerSpriteIds[gActiveBattler]].callback = SpriteCB_TrainerSlideIn;

	gBattlerControllerFuncs[gActiveBattler] = Player_CompleteOnBattlerSpriteCallbackDummy2;
}

u16 GetBackspriteId(void)
{
	u16 trainerPicId;

	if (gBattleTypeFlags & BATTLE_TYPE_LINK)
	{
		if ((gLinkPlayers[GetMultiplayerId()].version & 0xFF) == VERSION_FIRE_RED
		|| (gLinkPlayers[GetMultiplayerId()].version & 0xFF) == VERSION_LEAF_GREEN)
			trainerPicId = gLinkPlayers[GetMultiplayerId()].gender;
		else
			trainerPicId = gLinkPlayers[GetMultiplayerId()].gender + BACK_PIC_BRENDAN;
	}
	else if (gBattleTypeFlags & BATTLE_TYPE_INGAME_PARTNER && gActiveBattler == 2)
	{
		trainerPicId = LoadPartnerBackspriteIndex();
	}
	else if (IsAIControlledBattle())
	{
		trainerPicId = LoadPartnerBackspriteIndex(); //The trainer's backsprite for the Battle Sands is stored in the multi partner var
	}
	else
	{
		if (VarGet(VAR_BACKSPRITE_SWITCH))
			trainerPicId = VarGet(VAR_BACKSPRITE_SWITCH);
		else
			trainerPicId = gSaveBlock2->playerGender;
	}

	return trainerPicId;
}

void LoadTrainerBackPal(u16 trainerPicId, u8 paletteNum)
{
	#ifdef UNBOUND
	//Changes the skin tones of the player character in Unbound
		u16 owNum = VarGet(VAR_PLAYER_WALKRUN);

		if (VarGet(VAR_PLAYER_WALKRUN) && gActiveBattler == 0 && (trainerPicId == TRAINER_BACK_PIC_RED || trainerPicId == TRAINER_BACK_PIC_LEAF))
		{
			for (int i = 0; sCharacterPalSwitchTable[i].owNum != 0xFFFF; ++i)
			{
				if (sCharacterPalSwitchTable[i].owNum == owNum)
				{
					LoadCompressedPalette(sCharacterPalSwitchTable[i].backSpritePal, 0x100 + paletteNum * 16, 32);
					break;
				}
			}
		}
		else
			DecompressTrainerBackPic(trainerPicId, paletteNum);
	#else
		DecompressTrainerBackPic(trainerPicId, paletteNum);
	#endif
}

void TryUpdateTrainerPicPalTrainerCard(u16 trainerPicId, u16 palOffset)
{
	LoadCompressedPalette(GetTrainerSpritePal(trainerPicId), palOffset * 0x10, 0x20);
}

const u8* GetTrainerSpritePal(u16 trainerPicId)
{
	#ifdef UNBOUND
	const u8* palette;
	if (VarGet(VAR_PLAYER_WALKRUN) != 0)
	{
		if ((gSaveBlock2->playerGender == FEMALE && VarGet(VAR_TRAINERCARD_FEMALE) == 0)
		||  (gSaveBlock2->playerGender != FEMALE && VarGet(VAR_TRAINERCARD_MALE) == 0))
		{
			if (trainerPicId == TRAINER_PIC_PLAYER_M || trainerPicId == TRAINER_PIC_PLAYER_F)
			{
				if ((palette = GetAlternateTrainerSpritePal()) != NULL)
					return palette;
			}
		}
	}
	#endif

	return gTrainerFrontPicPaletteTable[trainerPicId].data;
}

#ifdef UNBOUND
static const u8* GetAlternateTrainerSpritePal(void)
{
	u16 owNum = VarGet(VAR_PLAYER_WALKRUN);

	for (int i = 0; sCharacterPalSwitchTable[i].owNum != 0xFFFF; ++i)
	{
		if (sCharacterPalSwitchTable[i].owNum == owNum)
			return sCharacterPalSwitchTable[i].frontSpritePal;
	}

	return NULL;
}
#endif
