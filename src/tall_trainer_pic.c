#include "defines.h"
#include "defines_battle.h"
#include "../include/decompress.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "Tables/tall_trainer_pics.h"
#include "../include/new/Vanilla_functions.h"

/*
    戦闘のトレーナーの絵を「縮めない全身」で出す（2026-10-01）

    ユーザー「通信対戦時の主人公の絵をトレーナーカードのような形にしておいて」
    「あれヒョウタこの方法使えないか？」→「戦闘の絵を縮めない全身に」。
    殿堂入り（src/bw_hof_pic.c）と同じく 64 x 96 を 2 枚に分ける:
      1 枚目 … 素が作るトレーナーのスプライト（64 x 64）。VRAM へ送る前の入れ物を上 64 行で上書きする
      2 枚目 … 下 32 行（64 x 32）。毎フレーム 1 枚目の位置を写す（1 枚目より後ろの番号＝同じコマのうちに写せる）
    どの絵をそうするかは gTallTrainerPics（tools/rom/build_tall_trainer_pics.py）。
    その絵は座標の表の「大きさ」を 10 にしてある（tools/rom/build_hoenn_trainer_pics.py の TALL_COORDS）ので、
    1 枚目の上端が画面の 0 行目に来て、下 32 行が足もとまで届く。

    ★ 2026-10-01 ユーザー実機「ヒョウタのドット絵足が消えてた」→（AskUserQuestion）「2 枚方式のまま直す」:
      2 枚目は images 方式（作るときに OBJ の VRAM を 32 タイル取りに行く）だった。戦闘の VRAM は混んでいるので
      取れずに作れなかったと推測（実機で確かめていない）。→ 1 枚目の絵を読み込むときに、同じ札で 96 タイル（上 64 行＋下 32 行）を
      まとめて読み込み、2 枚目はその札の 64 タイル目から先を指す（作るときに VRAM を取りに行かない）。
      96 タイルが取れなかったときは、今までどおり 64 タイル（足なし）で読む。札を放すと（素の FreeSpriteTilesByTag）両方消える。

    入口: 素の DecompressTrainerFrontPic（08033E38）を functionrewrites で丸ごと置き換え。
      呼ぶのは相手のトレーナーを出すとき（CFRU の OpponentHandleDrawTrainerPic / OpponentHandleTrainerSlide）と、
      通信対戦の相手を出すとき（素の LinkOpponentHandleDrawTrainerPic）。中身は素と同じ
      （絵を入れ物に展開 → スプライトの絵として読み込み → パレット）で、背の高い絵だけ間に上書きを挟む。
    2 枚目はスプライトができてから作るので、タスクが待つ。見分けは
      「gBattlerSpriteIds の先が使用中・affineParam が絵の番号（CFRU も素もトレーナーのスプライトに入れる）・
        タイルがその絵の札の場所」。
      ポケモンのスプライトはタイルが別なので取り違えない。
*/

#define DecompressPicFromTable_ ((void (*)(const struct CompressedSpriteSheet *, void *, s32)) (0x0800E640 | 1))  /* 素 FRLG（DecompressTrainerFrontPic の最初の BL）*/

#define TALL_BOTTOM_DY 48           /* 2 枚目の中心 = 1 枚目の中心 + 32 + 16 */
#define TALL_BOTTOM_TILE_OFS 64     /* 2 枚目は 1 枚目の札の 64 タイル目から（上 64 行 = 64 タイル） */
#define TALL_SHEET_SIZE (TALL_TRAINER_PIC_TOP_SIZE + 0x400)   /* 上 64 行＋下 32 行 = 96 タイル */

enum { tBattler, tPicId, tBottom };

static const struct OamData sTallBottomOam =
{
    .affineMode = ST_OAM_AFFINE_OFF, .objMode = ST_OAM_OBJ_NORMAL,
    .shape = SPRITE_SHAPE(64x32), .size = SPRITE_SIZE(64x32), .priority = 0,
};

static const struct TallTrainerPic *FindTallTrainerPic(u16 picId)
{
    const struct TallTrainerPic *t;

    for (t = gTallTrainerPics; t->picId != 0xFFFF; ++t)
    {
        if (t->picId == picId)
            return t;
    }
    return NULL;
}

/* 1 枚目（トレーナーのスプライト）として今も生きているか */
static bool8 IsTallTrainerSprite(u8 spriteId, u16 picId)
{
    struct Sprite *s;

    if (spriteId >= MAX_SPRITES)
        return FALSE;
    s = &gSprites[spriteId];
    return s->inUse && s->oam.affineParam == picId
        && s->oam.tileNum == GetSpriteTileStartByTag(gTrainerFrontPicTable[picId].tag);
}

static void SpriteCB_TallBottom(struct Sprite *sprite)
{
    struct Sprite *parent = &gSprites[sprite->data[0]];

    if (!IsTallTrainerSprite(sprite->data[0], sprite->data[1]))
    {
        DestroySprite(sprite);
        return;
    }
    sprite->pos1.x = parent->pos1.x;
    sprite->pos1.y = parent->pos1.y + TALL_BOTTOM_DY;
    sprite->pos2 = parent->pos2;
    sprite->invisible = parent->invisible;
    sprite->oam.paletteNum = parent->oam.paletteNum;
    sprite->oam.priority = parent->oam.priority;
    sprite->oam.tileNum = parent->oam.tileNum + TALL_BOTTOM_TILE_OFS;   /* 1 枚目の札の後ろ 32 タイル（毎コマ合わせる） */
}

static bool8 IsOurBottom(s16 id, u8 parent)
{
    return id >= 0 && id < MAX_SPRITES && gSprites[id].inUse
        && gSprites[id].callback == SpriteCB_TallBottom && gSprites[id].data[0] == parent;
}

static void Task_TallTrainerBottom(u8 taskId)
{
    s16 *data = gTasks[taskId].data;
    const struct TallTrainerPic *t;
    struct SpriteTemplate tpl;
    u8 parent, id;

    if (!gMain.inBattle)
    {
        DestroyTask(taskId);
        return;
    }
    parent = gBattlerSpriteIds[data[tBattler]];
    if (!IsTallTrainerSprite(parent, data[tPicId]) || IsOurBottom(data[tBottom], parent))
        return;             /* まだ出ていない・もう消えた（2 枚目は自分で消える）・もう付いている */

    t = FindTallTrainerPic(data[tPicId]);
    if (t == NULL)
        return;
    if (data[tBottom] == -2)
        return;                     /* 96 タイルが取れなかった（足なし） */
    tpl.tileTag = gTrainerFrontPicTable[data[tPicId]].tag;   /* 1 枚目と同じ札（VRAM を新しく取らない） */
    tpl.paletteTag = 0xFFFF;
    tpl.oam = &sTallBottomOam;
    tpl.anims = gDummySpriteAnimTable;
    tpl.images = NULL;
    tpl.affineAnims = gDummySpriteAffineAnimTable;
    tpl.callback = SpriteCB_TallBottom;
    id = CreateSpriteAtEnd(&tpl, gSprites[parent].pos1.x, gSprites[parent].pos1.y + TALL_BOTTOM_DY, gSprites[parent].subpriority);
    if (id >= MAX_SPRITES)
        return;
    gSprites[id].data[0] = parent;
    gSprites[id].data[1] = data[tPicId];
    gSprites[id].pos2 = gSprites[parent].pos2;
    gSprites[id].invisible = gSprites[parent].invisible;
    gSprites[id].oam.paletteNum = gSprites[parent].oam.paletteNum;
    gSprites[id].oam.priority = gSprites[parent].oam.priority;
    gSprites[id].oam.tileNum = gSprites[parent].oam.tileNum + TALL_BOTTOM_TILE_OFS;
    data[tBottom] = id;
}

static void StartTallTrainerBottom(u8 battler, u16 picId, bool8 loaded)
{
    u8 i;

    for (i = 0; i < NUM_TASKS; ++i)
    {
        if (gTasks[i].isActive && gTasks[i].func == Task_TallTrainerBottom && gTasks[i].data[tBattler] == battler)
        {
            gTasks[i].data[tPicId] = picId;
            gTasks[i].data[tBottom] = loaded ? -1 : -2;
            return;
        }
    }
    i = CreateTask(Task_TallTrainerBottom, 0);
    if (i >= NUM_TASKS)
        return;
    gTasks[i].data[tBattler] = battler;
    gTasks[i].data[tPicId] = picId;
    gTasks[i].data[tBottom] = loaded ? -1 : -2;
}

/* 素の DecompressTrainerFrontPic（08033E38）の代わり。背の高い絵以外は素と同じ */
void TallTrainer_DecompressTrainerFrontPic(u16 picId, u8 battler)
{
    u8 position = GetBattlerPosition(battler);
    const struct TallTrainerPic *t = FindTallTrainerPic(picId);
    struct SpriteSheet sheet;
    u32 i;

    DecompressPicFromTable_(&gTrainerFrontPicTable[picId], gMonSpritesGfxPtr->sprites[position], 0);
    if (t != NULL)
    {
        for (i = 0; i < TALL_TRAINER_PIC_TOP_SIZE; ++i)
            ((u8 *) gMonSpritesGfxPtr->sprites[position])[i] = t->top[i];
    }
    sheet.data = gMonSpritesGfxPtr->sprites[position];
    sheet.size = gTrainerFrontPicTable[picId].size;
    sheet.tag = gTrainerFrontPicTable[picId].tag;
    if (t != NULL)
    {
        /* 上 64 行と下 32 行を続けて 96 タイルで読む（2 枚目は同じ札の 64 タイル目から） */
        u8 *buf = Malloc(TALL_SHEET_SIZE);
        bool8 loaded = FALSE;

        if (buf != NULL)
        {
            for (i = 0; i < TALL_TRAINER_PIC_TOP_SIZE; ++i)
                buf[i] = ((u8 *) gMonSpritesGfxPtr->sprites[position])[i];
            for (i = 0; i < 0x400; ++i)
                buf[TALL_TRAINER_PIC_TOP_SIZE + i] = ((const u8 *) t->bottom[0].data)[i];
            sheet.data = buf;
            sheet.size = TALL_SHEET_SIZE;
            loaded = (LoadSpriteSheet(&sheet) != 0xFFFF);   /* 読み込みは同期（すぐ VRAM へ写す）なので放してよい */
            Free(buf);
        }
        if (!loaded)
        {
            sheet.data = gMonSpritesGfxPtr->sprites[position];
            sheet.size = gTrainerFrontPicTable[picId].size;
            LoadSpriteSheet(&sheet);
        }
        LoadCompressedSpritePaletteUsingHeap(&gTrainerFrontPicPaletteTable[picId]);
        StartTallTrainerBottom(battler, picId, loaded);
        return;
    }
    LoadSpriteSheet(&sheet);
    LoadCompressedSpritePaletteUsingHeap(&gTrainerFrontPicPaletteTable[picId]);
}
