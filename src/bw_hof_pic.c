#include "defines.h"
#include "../include/sprite.h"
#include "Tables/bw_card_pic.h"

/*
    殿堂入りの主人公を「縮めない全身」にする（#20・2026-09-23）

    ユーザー「殿堂入りも同じ要領で設定しておいて」（トレーナーカードと同じく縮めない全身）。

    ★ カードと違って殿堂入りの主人公は**スプライト（64x64）**
      （pokefirered hall_of_fame.c:621 の CreateTrainerPicSprite(135/136, TRUE, 0x78, 0x48, 6, 0xFFFF)）。
      BW の前向きの絵（トウヤ 35x71・トウコ 40x74）は縮めなければ 64 に入らないので、
      **VS と同じく 2 枚に分ける**:
        1 枚目 … 素が作ったスプライト。絵の入れ物（WRAM の images[0].data）を
                 縮めない全身の上 64 行で上書きする
        2 枚目 … 残りの下 32 行（64x32）。毎フレーム 1 枚目の位置を写す
    ★ 絵の入れ物は CreatePicSprite が Alloc したもので、アニメは 0 コマ目だけ
      （gTrainerFrontAnimsPtrTable[0]）。作った直後（最初の AnimateSprites より前）に
      上書きするので、その絵が VRAM へ送られる。
    ★ 2 枚とも 8 画素上げる（pos2.y）。素のまま（中心 y=72）だと足先が y 112 付近になり、
      あとで出る会話の窓にかかる。殿堂入りの処理が動かすのは x だけ（192 まで 1 画素ずつ）。

    入口: 素の Task_Hof_SpawnPlayerPic の中、CreateTrainerPicSprite の直後
    （080F367C・assembly/bwplayer/bw_hof_hook.s）。
*/

#define HOF_LIFT 8                  /* 2 枚とも上げる画素 */
#define HOF_BOTTOM_Y 48             /* 2 枚目の中心 = 1 枚目の中心 + 32 + 16 */

static const struct OamData sHofBottomOam =
{
    .affineMode = ST_OAM_AFFINE_OFF, .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x32), .size = SPRITE_SIZE(64x32), .priority = 0,
};
static const struct SpriteFrameImage sHofBottomMale[] = {{gBwHofPicMaleBottom, BW_HOF_PIC_BOTTOM_SIZE, 0}};
static const struct SpriteFrameImage sHofBottomFemale[] = {{gBwHofPicFemaleBottom, BW_HOF_PIC_BOTTOM_SIZE, 0}};

static void SpriteCB_HofBottom(struct Sprite *sprite)
{
    struct Sprite *parent = &gSprites[sprite->data[0]];
    u32 images = (u16) sprite->data[1] | ((u32) (u16) sprite->data[2] << 16);

    /* 1 枚目が消えた（または別のスプライトに入れ替わった）ら自分も消える */
    if (!parent->inUse || (u32) parent->images != images)
    {
        DestroySprite(sprite);
        return;
    }
    sprite->pos1.x = parent->pos1.x;
    sprite->pos1.y = parent->pos1.y + HOF_BOTTOM_Y;
    sprite->pos2 = parent->pos2;
    sprite->invisible = parent->invisible;
}

void BwHofPlayerPic(u8 spriteId)
{
    struct Sprite *parent;
    struct SpriteTemplate t;
    const u8 *top;
    u8 id;
    u32 images;
    u16 i;

    if (spriteId >= MAX_SPRITES)
        return;
    parent = &gSprites[spriteId];
    if (!parent->inUse || parent->images == NULL)
        return;

    if (gSaveBlock2->playerGender != 0)
    {
        top = gBwHofPicFemaleTop;
        t.images = sHofBottomFemale;
    }
    else
    {
        top = gBwHofPicMaleTop;
        t.images = sHofBottomMale;
    }

    /* 1 枚目: 絵の入れ物（WRAM）を縮めない全身の上 64 行で上書き */
    for (i = 0; i < BW_HOF_PIC_TOP_SIZE; ++i)
        ((u8 *) parent->images[0].data)[i] = top[i];
    parent->pos2.y = -HOF_LIFT;

    /* 2 枚目: 下 32 行。★ 1 枚目より後ろの番号にして、同じコマのうちに位置を写す */
    t.tileTag = 0xFFFF;
    t.paletteTag = 0xFFFF;
    t.oam = &sHofBottomOam;
    t.anims = gDummySpriteAnimTable;
    t.affineAnims = gDummySpriteAffineAnimTable;
    t.callback = SpriteCB_HofBottom;
    id = CreateSpriteAtEnd(&t, parent->pos1.x, parent->pos1.y + HOF_BOTTOM_Y, 0);
    if (id >= MAX_SPRITES)
        return;
    images = (u32) parent->images;
    gSprites[id].oam.paletteNum = parent->oam.paletteNum;
    gSprites[id].oam.priority = parent->oam.priority;
    gSprites[id].subpriority = parent->subpriority;
    gSprites[id].pos2 = parent->pos2;
    gSprites[id].data[0] = spriteId;
    gSprites[id].data[1] = (u16) images;
    gSprites[id].data[2] = (u16) (images >> 16);
}
