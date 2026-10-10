#include "defines.h"
#include "../include/decompress.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "../include/constants/species.h"

/*
 * はじめのオーキド博士の話で見せるポケモンを ニドラン♀ → フカマル に（2026-10-08 ユーザー
 * 「最初のオーキド博士が出すポケモンをニドランからフカマルにしてほしい」）。
 * 素（pokefirered src/oak_speech.c の CreateNidoranFSprite・日本語版 08131740）を丸ごと置き換える（hooks・r1）。
 * 鳴き声は assembly/hack_oak_intro/oak_intro_hooks.s。
 * ★ DPE の絵と色の表は ROM の頭の 08000128（正面）・08000134（色違いの色）から引く（DPE が付け替える語）。
 * ★ 2026-10-08 ユーザー「フカマル色違いで後位置調整してほしい少し上2ドットくらいかな」: 色違いの色・y を 96 → 94。
 *   色違いの表の札は種族の番号と違う → 札は通常の色の表（08000130）のものを使う（型の札 = 種族）
 *   CFRU のコードに表の番地を書くと素の表（412 匹）のまま＝フカマルが無い
 */
#define INTRO_SPECIES SPECIES_GIBLE

#define DecompressPicFromTable ((void (*)(const struct CompressedSpriteSheet*, void*, s32)) (0x0800E640 | 1))
#define MonSpritesGfxManager_GetSpritePtr ((void* (*)(u8)) (0x080443C4 | 1))
#define SetMultiuseSpriteTemplateToPokemon ((void (*)(u16, u8)) (0x0803EF40 | 1))
#define gMultiuseSpriteTemplateOak ((const struct SpriteTemplate*) 0x0202443C)

void HackOakIntro_CreateMonSprite(u8 taskId)
{
	const struct CompressedSpriteSheet* front = *(const struct CompressedSpriteSheet* const*) 0x08000128;
	const struct CompressedSpritePalette* pal = *(const struct CompressedSpritePalette* const*) 0x08000130;
	const struct CompressedSpritePalette* shiny = *(const struct CompressedSpritePalette* const*) 0x08000134;
	struct CompressedSpritePalette shinyPal;
	u8 spriteId;

	DecompressPicFromTable(&front[INTRO_SPECIES], MonSpritesGfxManager_GetSpritePtr(0), INTRO_SPECIES);
	shinyPal = shiny[INTRO_SPECIES];
	shinyPal.tag = pal[INTRO_SPECIES].tag;
	LoadCompressedSpritePaletteUsingHeap(&shinyPal);
	SetMultiuseSpriteTemplateToPokemon(INTRO_SPECIES, 0);
	spriteId = CreateSprite(gMultiuseSpriteTemplateOak, 96, 94, 1);
	gSprites[spriteId].callback = SpriteCallbackDummy;
	gSprites[spriteId].oam.priority = 1;
	gSprites[spriteId].invisible = TRUE;
	gTasks[taskId].data[4] = spriteId;      //素の tNidoranFSpriteId（+0x10 に strh）
}
