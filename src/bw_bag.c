#include "defines.h"
#include "../include/sprite.h"

/*
    B2W2 風バッグ（バッグ・TM ケース・きのみ袋）— 男女でバッグのスプライトの
    パレットを変えるフック。

    元: ROM Hacking Patches Pack の「B2W2 Styled Bag, TM Case and Berry Pouch
    2020s Edition」（作者 Verloren 5・BPRE 用 IPS）。
    経緯と番地の根拠: docs/research/2026-09-19_bw-bag.md

    ## 素 FRLG との関係

    絵（タイル・タイルマップ・パレット・スプライト）は全部 `src/bw_bag_gfx.c`
    （生成物）にあり、素のリテラル語を `repoints` でそちらへ向けるだけ。
    コードを触るのはここ 1 箇所:

      素の DoLoadBagGraphics 相当（BPRJ 08108D44〜）は、バッグのスプライトの
      シートを男女で分けて読む（0839C6EC / 0839C6F4）のに、**パレットは 1 本**
      （0839C6FC = {08D8456C, tag 0x64}）を男女共通で読む。B2W2 のバッグは
      男が青・女が桃で色が違うので、パレットも男女で分ける。

      フック先 08108E44 は
          LDR r0,=0839C6FC / BL LoadCompressedSpritePalette / B 08108E56
      の 8 バイトぴったり（08108E44 % 4 == 0）。ここを踏み台にして
      `BwBagLoadSpritePalette` を呼び、素の続き 08108E56 へ戻る。
      途中へ飛び込む分岐が無いことは tools/bwbag/verify_bw_bag.py が毎回見る。
      元の BPRE パッチも同じ場所（DoLoadBagGraphics+0x100）をフックしていた。

    ## 状態を持たない
    書き込み可能な static は無い（CLAUDE.md 6.13）。
*/

extern const u8 gBwBagSpritePalMaleData[];
extern const u8 gBwBagSpritePalFemaleData[];

#define BW_BAG_SPRITE_PAL_TAG 0x64      /* 素のバッグのスプライトと同じタグ */

/* BPRJ.ld の LoadCompressedSpritePalette（0x0800E5DC）。ヘッダに宣言が無いので直接 */
#define BwLoadCompressedSpritePalette \
    ((void (*)(const struct CompressedSpritePalette *))(0x0800E5DC + 1))

static const struct CompressedSpritePalette sBwBagSpritePalMale =
    {gBwBagSpritePalMaleData, BW_BAG_SPRITE_PAL_TAG};
static const struct CompressedSpritePalette sBwBagSpritePalFemale =
    {gBwBagSpritePalFemaleData, BW_BAG_SPRITE_PAL_TAG};

/* ★ 2026-09-25: 素の BagIsTutorial（BPRJ 0810B8A8・static なので BPRJ.ld に無い）。
   バッグの呼ばれ方（gBagMenuState.location）が 6（じいさんの捕獲の説明）・7〜10（テレビの説明番組）なら真。
   素の DoLoadBagGraphics は説明のときは性別に関係なく**男のバッグ**を出す
   （`BagIsTutorial() || MALE` で絵・`!BagIsTutorial() && FEMALE` で背景のパレット）。
   ここでもそろえる（実機「捕獲チュートリアルのじいさんがバッグを開いたときは男のものを」）。 */
#define BwBagIsTutorial ((bool8 (*)(void))(0x0810B8A8 + 1))

void BwBagLoadSpritePalette(void)
{
    if (gSaveBlock2->playerGender != 0 && !BwBagIsTutorial())
        BwLoadCompressedSpritePalette(&sBwBagSpritePalFemale);
    else
        BwLoadCompressedSpritePalette(&sBwBagSpritePalMale);
}
