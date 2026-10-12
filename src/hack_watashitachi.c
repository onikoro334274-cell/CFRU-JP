#include "defines.h"
#include "../include/event_data.h"
#include "../include/sprite.h"
#include "../include/constants/battle.h"
#include "../include/battle.h"
#include "../include/new/ram_locs_battle.h"
#include "hack_watashitachi.h"

/*
	隠しトレーナー「わたしたち」10 人（#99・2026-10-11 ユーザーの描いたトレーナーメモ「私たちのトレーナーメモ」）。
	  - 話しかけたときの顔: 台本（assembly/hack_watashitachi/watashitachi_scripts.s）が Var8004 に 10 人の何番目かを入れて
	    callasm HackWatashi_ShowFace → メッセージ → closemessage → callasm HackWatashi_HideFace。
	    顔は 64 × 64・4bpp（tools/oritore/watashitachi/build_watashitachi_art.py → assembly/hack_watashitachi/watashi_faces.s）。
	    メッセージの窓（下の 48 画素）の上・左寄りに出す。ユーザー「縮尺難しかったらええ感じでそのまま実装して」
	  - 種族値 +50（2026-10-12 に +20 から）: 10 人とのトレーナー戦の間、相手の手持ちは 6 つとも +50（src/hack_story_partner.c の HackPartner_BaseBonus から聞く）。
	    ポケモンに印は付けない（トレーナー戦の旗・相手の番号 940〜949・相手の手持ちの中か で決める）
	  - 状態はスプライトのタグで探す（6.13: 書き換えられる静的変数は使えない）
	  - 2026-10-12 ユーザー「戦えるようになるのをバトルフロンティアに行けることが条件で」: 10 人の人の表の旗は WATASHI_FLAG_HIDE（0xF0D）。
	    人の表の旗は「立っていれば隠す」なので、フィールドの毎コマ（src/overworld_speedup.c）で
	    「行ける印 WATASHI_FLAG_UNLOCK（0x13EB）が立っていない間は 0xF0D を立てる・立ったら下ろす」に保つ（FlagGet 1 回・ビット 1 つ）。
	    旗が 0 の前のセーブをその人のいる地図で つづきから すると、最初のコマだけ人が湧く（地図を出入りすると消える）
*/

#define FACE_TAG        0x5A70             //顔のタイルとパレットの札（ほかで使っていない番号）
#define FACE_BYTES      (64 * 64 / 2)
#define FACE_X          40                  //スプライトの真ん中（左寄り）
#define FACE_Y          74                  //下端 106＝メッセージの窓（112〜）のすぐ上

extern const u8 gWatashiFaceTiles[];
extern const u16 gWatashiFacePals[];

static const struct OamData sFaceOam =
{
	.affineMode = 0,
	.objMode = 0,
	.shape = 0,        //正方形
	.size = 3,         //64 × 64
	.priority = 0,
};

static const struct SpriteTemplate sFaceTemplate =
{
	.tileTag = FACE_TAG,
	.paletteTag = FACE_TAG,
	.oam = &sFaceOam,
	.anims = gDummySpriteAnimTable,
	.images = NULL,
	.affineAnims = gDummySpriteAffineAnimTable,
	.callback = SpriteCallbackDummy,
};

void HackWatashi_HideFace(void)
{
	u32 i;

	for (i = 0; i < MAX_SPRITES; ++i)
	{
		if (gSprites[i].inUse && gSprites[i].template == &sFaceTemplate)
			DestroySprite(&gSprites[i]);
	}
	FreeSpriteTilesByTag(FACE_TAG);
	FreeSpritePaletteByTag(FACE_TAG);
}

//フィールドの毎コマ: バトルフロンティアへ行けるまで 10 人を隠す
void HackWatashi_FieldTick(void)
{
	if (FlagGet(WATASHI_FLAG_UNLOCK))
		FlagClear(WATASHI_FLAG_HIDE);
	else
		FlagSet(WATASHI_FLAG_HIDE);
}

//callasm: Var8004 番（0〜9）の顔を出す
void HackWatashi_ShowFace(void)
{
	u16 idx = Var8004;
	struct SpriteSheet sheet;
	struct SpritePalette pal;

	HackWatashi_HideFace();
	if (idx >= WATASHI_COUNT)
		return;
	sheet.data = gWatashiFaceTiles + idx * FACE_BYTES;
	sheet.size = FACE_BYTES;
	sheet.tag = FACE_TAG;
	pal.data = gWatashiFacePals + idx * 16;
	pal.tag = FACE_TAG;
	LoadSpriteSheet(&sheet);
	LoadSpritePalette(&pal);
	CreateSprite(&sFaceTemplate, FACE_X, FACE_Y, 0);
}

//10 人とのトレーナー戦の相手の手持ちなら WATASHI_BASE_BONUS（50・src/hack_story_partner.c の HackPartner_BaseBonus から）
u8 HackWatashi_BaseBonus(const struct Pokemon* mon)
{
	if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
		return 0;
	if (gTrainerBattleOpponent_A < WATASHI_TRAINER_FIRST || gTrainerBattleOpponent_A >= WATASHI_TRAINER_FIRST + WATASHI_COUNT)
		return 0;
	if (mon < &gEnemyParty[0] || mon >= &gEnemyParty[PARTY_SIZE])
		return 0;
	return WATASHI_BASE_BONUS;
}
