#include "defines.h"
#include "defines_battle.h"
#include "../include/battle.h"
#include "../include/battle_interface.h"
#include "../include/decompress.h"
#include "../include/palette.h"
#include "../include/pokemon.h"
#include "../include/string_util.h"
#include "../include/new/Vanilla_functions_battle.h"
#include "../include/new/battle_util.h"
#include "bw_hpbar_gfx.h"

/*
bw_hpbar.c
    BW 風の HP バー（やりたいこと #17・戦闘画面の名前枠を 6 文字に）。

    元: https://github.com/hbdfreitas/BW-hpbars（参考/BW-hpbars-master）
    移植の経緯と番地の根拠: docs/research/2026-09-19_bw-hpbars-port.md
    記号の対応表: tools/bwhp/symbols.tsv

    ## 素 FRLG との関係

    素の `UpdateHealthboxAttribute`（BPRJ 08049674）の前置きの直後（+0x0C）に
    フックを置き、中身を丸ごとこちらの `BwHpbarDrawElements` に差し替えて、
    素の後始末（+0x22E = 080498A2）へ戻る。`UpdateHpTextInHealthbox`
    （BPRJ 08047B08）は先頭でフックして `BwHpbarUpdateHpNums` に差し替える。
    枠の絵・バーの絵・パレットは `bytereplacement` で**素の置き場に上書き**
    （tools/bwhp/build_bw_hpbar.py が生成）。

    ★ `BwHpbarDrawElements` が 0 を返すと、踏み台（assembly/bwhp）が潰した
      8 バイトをやり直して**素の中身を動かす**。サファリゾーンがこれ。

    ## ★★ 名前は元のコードを使わない（日本語が出ない）

    元の `outlined_font_draw` は英字専用の自前フォント（小文字 0xD5〜0xEE・
    大文字 0xBB〜0xD4・数字・記号）で、**日本語の文字コードは全部空白に落ちる**。
    そこで名前だけは**素の日本語版と同じ描き方**にした:

      - `RenderTextHandleBold`（BPRJ 08005F90。素の JP 版 `UpdateNickInHealthbox`
        が使っているもの）で、1 文字 = 64 バイト（上のタイル＋下のタイル）の
        列を作る。**かなの本体は下のタイル 8x8**、上のタイルには濁点・半濁点の
        タイルを入れる。素と同じ判定。
        ★ ただし素の要素 0x2B/0x2C/0x2D は**素の枠の上辺（行 2-4）と中（行 5-7）
          を含む**ので、そのまま置くと名前の上に暗い帯が出る。BW の枠には
          **点だけ**にした `gBwMarkTiles`（生成物）を置く（段 4・2026-09-19）
      - BW の枠は上 2 段（行 0・行 1）が**透明**なので、上段を行 0、下段を行 1 に
        置けば背景を壊さない（枠の絵を展開して確かめた）
      - **7 列**にした（6 文字＋性別）。素は 6 列。列の増分は素の置き場
        （`barFontGfx + 0x520 + 位置 * 列数 * 64`）に収まる
        （`barFontGfx` は 0x1000 バイト。HP の数字フォントは先頭 0x520 だけ）

    ## ★ HP とレベルの数字も素の日本語フォント（2026-09-19・ユーザー判断）

    元の BW は自前の大きな白い数字だったが「元のほうが見やすい」とのことで、
    素の JP 版 `UpdateHpTextInHealthbox` と同じく `RenderTextHandleBold` で描き、
    **下のタイルだけ**を並べる（素も `barFontGfx[i*64+32]` を写している）。
    ダイマックスで HP が 4 桁になるので **"xxxx/xxxx" の 9 タイル**。
    レベルも同じ描き方で 3 列（段 3・2026-09-19。BW の英字フォントは捨てた）。

    ★ 文字の背景色は**置く場所の絵の色**に合わせる（枠の絵を展開して確かめた）:
      - 名前（行 0・1）とレベルの 3 タイルは絵が **0（透明）** → 背景 0
      - HP の数字（行 3 = 28〜31・88〜92）は絵が **8（濃い帯）** → 背景 8。
        0 にすると帯に穴が開いて、後ろの戦闘の背景が透けた（段 3 の実機）

    ## サファリゾーン: 味方だけ素に任せる（段 3）

    味方の枠が「サファリボール ×N」になる特別な作りで、BW の描き方に合わない
    （2026-09-19 実機で味方側が壊れた）。**相手は BW のまま**描く。
    味方は 0 を返して素の中身に描かせる。そのとき素が展開する味方の枠の絵
    （シート表 [6] = 08C1FABC）は、**素の絵を BW のパレットに合わせて色を
    付け替えたもの**に差し替えてある（tools/bwhp/build_bw_hpbar.py）。
    パレットや相手の絵を戻す細工はしない（段 2 でそれをやったら相手の枠が
    壊れた。要素の絵は BW のものなので素の相手の描き方と合わない）。

    ## ★ ダブルの「HP を数字で」（START で切り替え）— 段 5・2026-09-19

    素 FRLG はダブルの味方の枠で START を押すと HP バーを数字に切り替える
    （`SwapHpBarsWithHpText`・BPRJ 0804804C。CFRU の move_menu.c が呼ぶ）。
    素はバーのスプライト（8 タイル）を消して、その 1〜7 タイル目に 3 桁の数字を
    素の色（文字 1・背景 透明）で描くので、BW の枠では枠の絵（HP の札・バーの枠）
    の上に数字が重なって見えた（実機）。しかも 4 桁は 8 タイルに入らない。
    そこで `SwapHpBarsWithHpText` の入口をフックして丸ごとこちらの
    `BwSwapHpBarsWithHpText` にした（先頭 8 バイトは PUSH/MOV x3 で、戻らないので
    やり直し不要。途中へ飛び込む分岐なし。verify_bw_hpbar.py が見る）:
      - 数字にするとき: バーのスプライトの 8 タイルを 0 で消し、**枠の絵の行 2**
        （HP の札 20〜21・バーの枠 22〜23・48〜51・丸い端 52 ＝ 9 タイル）に
        "xxxx/xxxx" を描く。背景は 8（濃い帯）で、帯の絵（タイル 18）が透明な
        画素（行 0）だけ透明のままにする（数字は下のタイルの行 1〜7 に収まる。
        実機の写真で測った）
      - バーに戻すとき: 枠の絵（LZ・08C1F794）を barFontGfx に展開して 9 タイルを
        戻し、`BwDrawHpBar` でバーを描き直す
      - 戦闘中の HP の変化は素と同じく `UpdateHpTextInHealthbox`（フック済み）
        経由で来るので、`BwHpbarUpdateHpNums` がダブル＋数字モードのときも描く
      - 交代などで枠を全部描き直すとき（`BwHpbarDrawElements`）も、数字モードなら
        数字を描く（素は交代後に何も出なくなる）
    フラグは素と同じ `gBattleSpritesDataPtr->bankData[i].hpNumbersNoBars`
    （RAM）。`MoveBattleBar` はこのフラグが立っていると絵を描かない（素の仕様）ので、
    数値の更新のためだけに呼んでも枠は汚れない。

    ## 元のコードから変えたところ

      - `0x02024284`（BPRE の gPlayerParty）で味方か決めていた →
        `GetBattlerSide` で決める
      - `outlined_font_draw`（BW の英字フォント）→ 捨てた。名前・HP・レベルとも
        素の日本語フォント
      - `0x8253AE4`（BPRE の経験値表）の決め打ち → CFRU の `gExperienceTables`
        と `gBaseStats[].growthRate`（CFRU の間接参照なので DPE でも正しい）
      - `0x203D000` の作業用バッファ → スタック上のバッファ
      - `temp_vars.var_8000 = 0x77`（デバッグの残骸。Var8000 を壊す）→ 削除
      - 絵の配列は `const`（CFRU では書き込み可能な static が使えない・6.13）
      - 相手側の判定に `objects[obj_id].x == 32` を使っていた →
        `GetBattlerSide`。**枠の x = 32 は元どおり残す**（BW の配置）
      - ★★ タイル番号 `u8 tile = tileNum + n` を **u16** に。スプライトのタイルは
        1023 まであるので、後から作った枠（ダブルの相手 2 匹目）で 256 を超えて
        あふれ、レベルが別の場所に描かれていた。元のパッチの既知バグ
        「ダブルで左の相手のレベルが出ない」の正体（2026-09-19 実機）

    ## 状態を持たない

    このファイルは書き込み可能な static を持たない（6.13 節）。
    作業領域は素と同じ `gMonSpritesGfxPtr->barFontGfx` と
    `gDisplayedStringBattle`、そしてスタック。
*/

#define BW_TILE_SIZE 32
#define BW_NAME_COLS 7          /* 6 文字 + 性別。素の日本語版は 6 */

/* BW の枠のタイル番号（元の hp_draw.c と同じ。1 スプライト = 8 列） */
#define BW_NAME_COL_PLAYER 1    /* 行 0/1 の列 1〜7（列 0 は矢印の先）*/
#define BW_NAME_COL_OPPONENT 0  /* 行 0/1 の列 0〜6（列 7 は "Lv"）*/
#define BW_LEVEL_TILE_SINGLE_PLAYER 74
#define BW_LEVEL_TILE_OPPONENT 40
#define BW_LEVEL_TILE_DOUBLES_PLAYER 42
/* HP の数字（1 人用・味方）。"xxxx/xxxx" の 9 タイルを行 3 の 28〜31 と
     右のスプライトの 88〜92 に置く（画面内 x ≦ 235）。
     元は 3 桁 7 タイル（29〜）。そのぶん状態異常の印を 1 つ左（26 → 25）へ */
#define BW_HP_NUMS_TILE_PLAYER 28
#define BW_HP_DIGITS 4
#define BW_HP_SLASH 0xBA
#define BW_HP_COLS (BW_HP_DIGITS * 2 + 1)
#define BW_LEVEL_COLS 3
/* 文字の背景色（枠の絵のその場所の色に合わせる。冒頭のコメント参照） */
#define BW_BG_TRANSPARENT 0
#define BW_BG_HP_BAND 8
#define BW_AILMENT_TILE_SINGLE_PLAYER 25
#define BW_AILMENT_TILE_DOUBLE_PLAYER 17
#define BW_AILMENT_TILE_OPPONENT 16
/* ダブルの味方の枠で、HP を数字にするときに使う行 2 のタイル（HP の札〜丸い端） */
#define BW_DOUBLES_BAND_TILE 18                 /* 帯だけの平坦なタイル（透明の形を借りる）*/
#define BW_DOUBLES_PLAYER_GFX ((const void *)0x08C1F794)   /* 2 人用の味方の枠（LZ・BW の絵）*/
#define BW_HEALTHBAR_TILES 8                    /* バーのスプライト（64x8）*/

/* 素 FRLG（BPRJ）の関数と絵。番地の根拠は docs/research/2026-09-19_bw-hpbars-port.md */
#define BwRenderTextHandleBold \
    ((u8 (*)(u8 *, u8, const u8 *, s32, s32, s32, s32, s32))(0x08005F90 + 1))
#define BwCheckBattleTypeGhost \
    ((bool8 (*)(struct Pokemon *, u8))(0x08043EB0 + 1))
#define BW_OBJ_VRAM ((u8 *)0x06010000)

static const u8 sBwDoublesHpTiles[BW_HP_COLS] = {20, 21, 22, 23, 48, 49, 50, 51, 52};

/* 素の JP 版 UpdateNickInHealthbox が名前の列を置く場所 */
#define BW_NAME_SCRATCH 0x520

/* 濁点・半濁点（gBwMarkTiles の添字。素の要素 0x2B/0x2C/0x2D から点だけ抜いたもの） */
#define BW_MARK_NONE 0
#define BW_MARK_DAKUTEN 1
#define BW_MARK_HANDAKUTEN 2


static u8 *BwTile(u8 spriteId, u16 tile)
{
    /* ★ u16。tileNum + tile は 255 を超える */
    return BW_OBJ_VRAM + (gSprites[spriteId].oam.tileNum + tile) * BW_TILE_SIZE;
}

static void BwCopyTiles(u8 *dst, const u8 *src, u32 nTiles)
{
    Memcpy(dst, src, nTiles * BW_TILE_SIZE);
}

/* 文字列の色指定（BW の枠のパレット）: 文字 2（白）/ 影 1（濃い灰）/ 背景 bg */
static u8 *BwTextColorPrefix(u8 *p, u8 bg)
{
    *p++ = 0xFC; *p++ = 0x01; *p++ = 2;
    *p++ = 0xFC; *p++ = 0x03; *p++ = 1;
    *p++ = 0xFC; *p++ = 0x02; *p++ = bg;
    return p;
}

/* ---- HP・レベルの数字（素の日本語フォント）-------------------------------
   `RenderTextHandleBold` で描いた列の**下のタイル**だけを並べる。
   置き場は素の JP 版と同じ `barFontGfx` の先頭（HP の数字フォントを潰すが、
   バーを描く前に `load_gfxc_health_bar` で展開し直すので素と同じ）。
   ★ 右寄せは `ConvertIntToDecimalStringN` の右寄せ（0x00 = 空白）でよい。 */

static void BwHpTextToTiles(u8 spriteId, const u8 *digits, u8 nCols, u16 firstTile, u8 bg)
{
    u8 *buf = gDisplayedStringBattle;
    u8 *p = BwTextColorPrefix(buf, bg);
    u8 *tiles = gMonSpritesGfxPtr->barFontGfx;
    u8 k;

    StringCopy(p, digits);
    BwRenderTextHandleBold(tiles, 0, buf, 0, 0, 0, 0, 0);
    for (k = 0; k < nCols; ++k)
    {
        u16 tile = firstTile + k;
        /* "xxxx/" の 5 タイルで行 3 の右端（31）を使い切り、続きは
           右のスプライトの行 3（88〜）。64x64 のスプライト = 8 列 x 8 行なので
           31 の次は 31 + 57 = 88 */
        if (tile > 31 && firstTile <= 31)
            tile += 64 - 8;
        BwCopyTiles(BwTile(spriteId, tile),
                    tiles + k * 2 * BW_TILE_SIZE + BW_TILE_SIZE, 1);
    }
}

static void BwDrawHpNums(struct Pokemon *mon, u8 spriteId)
{
    u16 maxHp = GetMonData(mon, MON_DATA_MAX_HP, NULL);
    u16 curHp = GetMonData(mon, MON_DATA_HP, NULL);
    u8 text[BW_HP_COLS + 1];

    ConvertIntToDecimalStringN(text, curHp, STR_CONV_MODE_RIGHT_ALIGN, BW_HP_DIGITS);
    text[BW_HP_DIGITS] = BW_HP_SLASH;
    ConvertIntToDecimalStringN(text + BW_HP_DIGITS + 1, maxHp,
                               STR_CONV_MODE_RIGHT_ALIGN, BW_HP_DIGITS);
    BwHpTextToTiles(spriteId, text, BW_HP_COLS, BW_HP_NUMS_TILE_PLAYER, BW_BG_HP_BAND);
}

/* ---- ダブルの味方: HP を数字で（START で切り替え）----------------------- */

static bool8 BwDoublesHpNumsOn(u8 battlerId)
{
    return gBattleSpritesDataPtr->bankData[battlerId].hpNumbersNoBars;
}

/* 行 2 の 9 タイルに "xxxx/xxxx" を描く。背景 8。帯の絵（タイル 18）が透明な画素は
   透明のまま（行 0）。数字は下のタイルの行 1〜7 に収まる（実機の写真で測った） */
static void BwDoublesHpTextToTiles(u8 spriteId, const u8 *digits, u8 nCols, u8 firstIdx)
{
    u8 *buf = gDisplayedStringBattle;
    u8 *p = BwTextColorPrefix(buf, BW_BG_HP_BAND);
    u8 *tiles = gMonSpritesGfxPtr->barFontGfx;
    const u8 *band = BwTile(spriteId, BW_DOUBLES_BAND_TILE);
    u8 work[BW_TILE_SIZE];
    u8 k, i;

    StringCopy(p, digits);
    BwRenderTextHandleBold(tiles, 0, buf, 0, 0, 0, 0, 0);
    for (k = 0; k < nCols; ++k)
    {
        const u8 *src = tiles + k * 2 * BW_TILE_SIZE + BW_TILE_SIZE;
        for (i = 0; i < BW_TILE_SIZE; ++i)
        {
            u8 b = src[i], m = band[i];
            work[i] = ((m & 0x0F) ? (b & 0x0F) : 0) | ((m & 0xF0) ? (b & 0xF0) : 0);
        }
        BwCopyTiles(BwTile(spriteId, sBwDoublesHpTiles[firstIdx + k]), work, 1);
    }
}

/* バーのスプライトを消して、数字を描く（数字モードの入口） */
static void BwDoublesShowHpNums(struct Pokemon *mon, u8 spriteId)
{
    u8 barSpriteId = gSprites[spriteId].data[5];
    u32 *bar = (u32 *)BwTile(barSpriteId, 0);
    u16 maxHp = GetMonData(mon, MON_DATA_MAX_HP, NULL);
    u16 curHp = GetMonData(mon, MON_DATA_HP, NULL);
    u8 text[BW_HP_COLS + 1];
    u8 i;

    for (i = 0; i < BW_HEALTHBAR_TILES * BW_TILE_SIZE / 4; ++i)     /* VRAM は 32 ビットで */
        bar[i] = 0;
    ConvertIntToDecimalStringN(text, curHp, STR_CONV_MODE_RIGHT_ALIGN, BW_HP_DIGITS);
    text[BW_HP_DIGITS] = BW_HP_SLASH;
    ConvertIntToDecimalStringN(text + BW_HP_DIGITS + 1, maxHp,
                               STR_CONV_MODE_RIGHT_ALIGN, BW_HP_DIGITS);
    BwDoublesHpTextToTiles(spriteId, text, BW_HP_COLS, 0);
}

/* 枠の絵の 9 タイルを LZ から戻す（バーに戻すとき）。barFontGfx を作業に使う */
static void BwDoublesRestoreBandArt(u8 spriteId)
{
    u8 *gfx = gMonSpritesGfxPtr->barFontGfx;
    u8 k;

    LZ77UnCompWram(BW_DOUBLES_PLAYER_GFX, gfx);      /* 0x800 バイト（入れ物は 0x1000）*/
    for (k = 0; k < BW_HP_COLS; ++k)
        BwCopyTiles(BwTile(spriteId, sBwDoublesHpTiles[k]),
                    gfx + sBwDoublesHpTiles[k] * BW_TILE_SIZE, 1);
}

/* UpdateHpTextInHealthbox(u8 healthboxSpriteId, s16 value, u8 maxOrCurrent) の差し替え */
void BwHpbarUpdateHpNums(u8 spriteId, u16 newHp, u8 maxOrCurrent)
{
    u8 battlerId = gSprites[spriteId].data[6];
    u8 text[BW_HP_COLS + 1];

    if (gBattleTypeFlags & BATTLE_TYPE_SAFARI)
        return;
    if (GetBattlerSide(battlerId) != B_SIDE_PLAYER)
        return;
    ConvertIntToDecimalStringN(text, newHp, STR_CONV_MODE_RIGHT_ALIGN, BW_HP_DIGITS);
    if (IsDoubleBattle())
    {
        /* 数字モードのときだけ。今の HP は "xxxx/"（5 列）、最大 HP は 6 列目から */
        if (!BwDoublesHpNumsOn(battlerId))
            return;
        if (maxOrCurrent)
            BwDoublesHpTextToTiles(spriteId, text, BW_HP_DIGITS, BW_HP_DIGITS + 1);
        else
        {
            text[BW_HP_DIGITS] = BW_HP_SLASH;
            text[BW_HP_DIGITS + 1] = EOS;
            BwDoublesHpTextToTiles(spriteId, text, BW_HP_DIGITS + 1, 0);
        }
        return;
    }
    /* 今の HP は 28〜31、最大 HP は「/」（右のスプライトの 88）の次から。
       ★ 2026-09-19 実機で「/」が消えたのは、3 桁の枠に 4 桁を書いて
         隣のタイルまで潰していたため */
    if (maxOrCurrent)
        BwHpTextToTiles(spriteId, text, BW_HP_DIGITS,
                        BW_HP_NUMS_TILE_PLAYER + BW_HP_DIGITS + 1, BW_BG_HP_BAND);
    else
        BwHpTextToTiles(spriteId, text, BW_HP_DIGITS, BW_HP_NUMS_TILE_PLAYER,
                        BW_BG_HP_BAND);
}

/* ---- 名前（★日本語。素の JP 版 UpdateNickInHealthbox と同じ描き方）---- */

static u8 BwMarkTileFor(u8 c)
{
    /* 素の JP 版と同じ判定（逆アセンブル 08048E36〜）:
         0x37〜0x4A / 0x87〜0x9A -> 濁点、0x4B〜0x4F / 0x9B〜0x9F -> 半濁点 */
    if ((u8)(c - 0x37) <= 0x13 || (u8)(c - 0x87) <= 0x13)
        return BW_MARK_DAKUTEN;
    if ((u8)(c - 0x4B) <= 0x04 || (u8)(c - 0x9B) <= 0x04)
        return BW_MARK_HANDAKUTEN;
    return BW_MARK_NONE;
}

static void BwDrawName(struct Pokemon *mon, u8 spriteId, u8 battlerId, u8 firstCol)
{
    u8 *buf = gDisplayedStringBattle;
    u8 *p = BwTextColorPrefix(buf, BW_BG_TRANSPARENT);
    u8 nick[POKEMON_NAME_LENGTH + 1];
    u8 glyphs[BW_NAME_COLS];
    u8 *tiles;
    u16 species;
    u8 gender;
    u8 n = 0;
    u8 k;

    /* ★ 2026-10-04 ユーザー「ゾロアークが相手のときイリュージョンで登場してもゾロアークと表記される」（キクコ戦）:
       素の名前の描き方は CFRU のフック（UpdateNickForIllusionHook 08048CCC）で化けている先のポケモンに差し替わるが、
       BW の枠はここで描くので通っていなかった → 名前と性別は化けている先（GetIllusionPartyData・化けていなければ本人） */
    if (battlerId < gBattlersCount)
        mon = GetIllusionPartyData(battlerId);

    GetMonData(mon, MON_DATA_NICKNAME, nick);
    StringGetEnd10(nick);
    for (k = 0; nick[k] != EOS && n < BW_NAME_COLS - 1; ++k)
    {
        *p++ = nick[k];
        glyphs[n++] = nick[k];
    }

    gender = GetMonGender(mon);
    species = GetMonData(mon, MON_DATA_SPECIES, NULL);
    if ((species == SPECIES_NIDORAN_F || species == SPECIES_NIDORAN_M)
     && StringCompare(nick, gSpeciesNames[species]) == 0)
        gender = MON_GENDERLESS;
    if (BwCheckBattleTypeGhost(mon, battlerId))
        gender = MON_GENDERLESS;

    if (gender == MON_MALE)
    {
        *p++ = 0xFC; *p++ = 0x01; *p++ = 0x0B;      /* 素の JP 版と同じ色 */
        *p++ = 0xB5;
        glyphs[n++] = 0xB5;
    }
    else if (gender == MON_FEMALE)
    {
        *p++ = 0xFC; *p++ = 0x01; *p++ = 0x0A;
        *p++ = 0xB6;
        glyphs[n++] = 0xB6;
    }
    while (n < BW_NAME_COLS)        /* 前の名前が残らないよう空白で埋める */
    {
        *p++ = 0x00;
        glyphs[n++] = 0x00;
    }
    *p = EOS;

    tiles = gMonSpritesGfxPtr->barFontGfx + BW_NAME_SCRATCH
          + GetBattlerPosition(battlerId) * (BW_NAME_COLS * 2 * BW_TILE_SIZE);
    BwRenderTextHandleBold(tiles, 0, buf, 0, 0, 0, 0, 0);

    for (k = 0; k < BW_NAME_COLS; ++k)
    {
        /* 上のタイル = 濁点・半濁点 / 下のタイル = かなの本体。
           ★ ROM の要素 0x2B〜 は素の枠の上辺を含む（サファリの味方の枠を素が
             描くときに要る）ので、BW の枠には点だけの gBwMarkTiles を置く */
        BwCopyTiles(tiles + k * 2 * BW_TILE_SIZE, gBwMarkTiles[BwMarkTileFor(glyphs[k])], 1);
        BwCopyTiles(BwTile(spriteId, firstCol + k), tiles + k * 2 * BW_TILE_SIZE, 1);
        BwCopyTiles(BwTile(spriteId, 8 + firstCol + k),
                    tiles + k * 2 * BW_TILE_SIZE + BW_TILE_SIZE, 1);
    }
}

/* ---- レベル（素の日本語フォント・左寄せ 3 列）/ バー / 状態異常 ------------ */

static void BwDrawLevel(struct Pokemon *mon, u8 spriteId, u16 tileNum)
{
    u8 level = GetMonData(mon, MON_DATA_LEVEL, NULL);
    u8 text[BW_LEVEL_COLS + 1];
    u8 n;

    ConvertIntToDecimalStringN(text, level, STR_CONV_MODE_LEFT_ALIGN, BW_LEVEL_COLS);
    for (n = 0; text[n] != EOS; ++n)
        ;
    for (; n < BW_LEVEL_COLS; ++n)      /* 余りは空白（0x00）で埋めて前の桁を消す */
        text[n] = 0x00;
    text[BW_LEVEL_COLS] = EOS;
    /* レベルの 3 タイルは枠の絵が透明（0）なので背景も 0 */
    BwHpTextToTiles(spriteId, text, BW_LEVEL_COLS, tileNum, BW_BG_TRANSPARENT);
}

static void BwDrawHpBar(u8 battlerId, u8 spriteId, struct Pokemon *mon)
{
    u16 maxHp, curHp;

    load_gfxc_health_bar(0);        /* 引数は素でも未使用 */
    maxHp = GetMonData(mon, MON_DATA_MAX_HP, NULL);
    curHp = GetMonData(mon, MON_DATA_HP, NULL);
    SetBattleBarStruct(battlerId, spriteId, maxHp, curHp, 0);
    MoveBattleBar(battlerId, spriteId, HEALTH_BAR, 0);

    if (GetBattlerSide(battlerId) == B_SIDE_PLAYER && !IsDoubleBattle())
    {
        u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
        u8 level = GetMonData(mon, MON_DATA_LEVEL, NULL);
        u32 totalExp = GetMonData(mon, MON_DATA_EXP, NULL);
        u8 growth = gBaseStats[species].growthRate;
        u32 expHave = gExperienceTables[growth][level];
        u32 expNeeded = gExperienceTables[growth][level + 1];

        SetBattleBarStruct(battlerId, spriteId, expNeeded - expHave, totalExp - expHave, 0);
        MoveBattleBar(battlerId, spriteId, EXP_BAR, 0);
    }
}

/* SwapHpBarsWithHpText（BPRJ 0804804C）の差し替え。素と同じ条件
   （枠が止まっている・味方・ダブル・サファリでない）で切り替える */
void BwSwapHpBarsWithHpText(void)
{
    u8 i;

    for (i = 0; i < gBattlersCount; ++i)
    {
        u8 spriteId = gHealthboxSpriteIds[i];
        struct Pokemon *mon = &gPlayerParty[gBattlerPartyIndexes[i]];

        if (gSprites[spriteId].callback != SpriteCallbackDummy)
            continue;
        if (GetBattlerSide(i) != B_SIDE_PLAYER || !IsDoubleBattle())
            continue;
        if (gBattleTypeFlags & BATTLE_TYPE_SAFARI)
            continue;

        gBattleSpritesDataPtr->bankData[i].hpNumbersNoBars ^= 1;
        if (BwDoublesHpNumsOn(i))
            BwDoublesShowHpNums(mon, spriteId);
        else
        {
            BwDoublesRestoreBandArt(spriteId);
            BwDrawHpBar(i, spriteId, mon);
        }
    }
}

static void BwDrawAilment(struct Pokemon *mon, u8 spriteId, u16 tileNum, bool8 small)
{
    const u8 *src = small ? gBwAilmentDoubles : gBwAilmentSingle;
    u8 *dst = BwTile(spriteId, tileNum);
    u32 status = GetMonData(mon, MON_DATA_STATUS, NULL);
    u8 slot;

    if (status & STATUS1_TOXIC_POISON)  slot = 5;
    else if (status & STATUS1_POISON)   slot = 0;
    else if (status & STATUS1_PARALYSIS) slot = 1;
    else if (status & STATUS1_SLEEP)    slot = 2;
    else if (status & STATUS1_FREEZE)   slot = 3;
    else if (status & STATUS1_BURN)     slot = 4;
    else                                slot = 6;      /* 消す */
    BwCopyTiles(dst, src + slot * 3 * BW_TILE_SIZE, 3);
}

/* ★ 2026-10-03 ユーザー「野生で捕獲したポケモンが捕獲マークがでていない」:
   素の TryAddPokeballIconToHealthbox（pokefirered battle_interface.c）は、捕まえたことのある種族の野生の相手に、
   状態異常の札が無いときだけボールの印を出す（HP バーの絵の 9 枚目）。BW の枠は全部描き直すので消えていた。
   → 相手の札の場所（札が無いとき）の 1 枚目にボールを重ねる（印の絵は gBwCaughtBallTile・BW の色に付け替えたもの）。
   条件は素と同じ（トレーナー戦・おじいさんの見本・ゴースト（シルフスコープ無し）は出さない） */
#define BW_FLAG_GET_CAUGHT 1

/* ★ 2026-10-10 ユーザー「BW系これになおして」→「全部そちらに置き換える」（bw-reloaded の caught_marker.c と同じ作り）:
   印は札の場所に重ねず、相手のレベルの数字の右（枠の左上 + 88〜103・+ 8〜15 段）に 16×8 の小さなスプライトで出す。
   左の 8×8 がボール（捕まえたことのある種族）・右の 8×8 が色違いの星（捕まえていなくても）。
   出すのは野生の相手だけ（トレーナー戦・おじいさんの見本・ゴースト（シルフスコープ無し）は出さない＝前と同じ）。
   色違いは IsMonShiny（ポケモン自身の ID・#70）で見る（bw-reloaded は素の式）。
   スプライトは毎フレーム、枠の位置・パレット・見え方に合わせ、条件が外れたら自分で消える。状態は data[0]＝枠のスプライト・data[1]＝場所 */
static const u8 sBwShinyStarTile[32] =   /* 8×8 の星（色 1＝枠の暗い色・色 9＝金・bw-reloaded の build_caught.py の形） */
{
    0x00, 0x10, 0x01, 0x00,  0x00, 0x91, 0x19, 0x00,  0x10, 0x99, 0x99, 0x01,  0x91, 0x99, 0x99, 0x19,
    0x91, 0x99, 0x99, 0x19,  0x10, 0x99, 0x99, 0x01,  0x00, 0x91, 0x19, 0x00,  0x00, 0x10, 0x01, 0x00,
};
/* ★ 2026-10-11 ユーザー「色違いマークをこれにしてほしい」（pokemon_Start_-_Battle_Window_-_Shiny_Star.png・8×8・黒の縁と #E8F800）。
   枠のパレットに黒と #E8F800 が無いので、印は自分のパレット（枠のパレットの 14 を黒・15 を #E8F800 にしたもの・札 BW_MARKER_PAL_TAG）を使う。
   パレットの枠が空いていなければ前と同じく枠のパレットと上の金の星（sBwShinyStarTile） */
#define BW_MARKER_PAL_TAG 0x5A1C
static const u8 sBwShinyStarTileOwn[32] =   /* 色 14＝黒・15＝#E8F800（画像の画素のまま） */
{
    0x00, 0xEE, 0x0E, 0x00,  0xEE, 0xFE, 0xEE, 0x0E,  0xFE, 0xFF, 0xFF, 0x0E,  0xEE, 0xFF, 0xEF, 0x00,
    0xE0, 0xEF, 0xEF, 0x00,  0xFE, 0x0E, 0xFE, 0x0E,  0xEE, 0x00, 0xE0, 0x0E,  0x00, 0x00, 0x00, 0x00,
};
static const u16 sBwMarkerPal[16] =   /* BW の枠のパレット（ROM の 08C11B84）の 14・15 だけ差し替え */
{
    0x0000, 0x2108, 0x7FFF, 0x6B5A, 0x1613, 0x5AD6, 0x318C, 0x1084,
    0x18C6, 0x029F, 0x4A7F, 0x7713, 0x173A, 0x5D97, 0x0000, 0x03FD,
};
static const struct SpritePalette sBwMarkerSpritePal = {sBwMarkerPal, BW_MARKER_PAL_TAG};
static void BwCaughtMarkerFollow(struct Sprite *marker);

static void BwCaughtMarkerDestroy(struct Sprite *marker)   //消して、ほかに印が無ければパレットも返す
{
    u8 i;

    DestroySprite(marker);
    for (i = 0; i < MAX_SPRITES; ++i)
        if (gSprites[i].inUse && gSprites[i].callback == BwCaughtMarkerFollow)
            return;
    FreeSpritePaletteByTag(BW_MARKER_PAL_TAG);
}
static const struct OamData sBwMarkerOam = {.shape = SPRITE_SHAPE(16x8), .size = SPRITE_SIZE(16x8)};
static const union AnimCmd sBwMarkerAnim[] = {ANIMCMD_FRAME(0, 1), ANIMCMD_END};
static const union AnimCmd *const sBwMarkerAnims[] = {sBwMarkerAnim};
static const struct SpriteFrameImage sBwMarkerImages[] = {{.data = gBwCaughtBallTile, .size = 64}};   //64 バイト（中身は毎フレーム書く）
static const struct SpriteTemplate sBwMarkerTemplate =
{
    .tileTag = 0xFFFF, .paletteTag = 0xFFFF, .oam = &sBwMarkerOam, .anims = sBwMarkerAnims,
    .images = sBwMarkerImages, .affineAnims = gDummySpriteAffineAnimTable, .callback = BwCaughtMarkerFollow,
};

static u8 BwCaughtMarkerFlags(u8 battlerId, struct Pokemon *mon)   //bit0 = ボール・bit1 = 星
{
    u8 flags;

    if (battlerId >= gBattlersCount || GetBattlerSide(battlerId) != B_SIDE_OPPONENT)
        return 0;
    if (gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_OLD_MAN | BATTLE_TYPE_LINK))
        return 0;
    if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE || BwCheckBattleTypeGhost(mon, battlerId))
        return 0;
    flags = GetSetPokedexFlag(SpeciesToNationalPokedexNum(GetMonData(mon, MON_DATA_SPECIES, NULL)), BW_FLAG_GET_CAUGHT) ? 1 : 0;
    if (IsMonShiny(mon))
        flags |= 2;
    return flags;
}

static void BwCaughtMarkerFollow(struct Sprite *marker)
{
    u8 parentId = marker->data[0], battlerId = marker->data[1], flags, t, i, ownPal;
    struct Sprite *parent;
    u16 *out;

    if (parentId >= MAX_SPRITES || battlerId >= gBattlersCount || gBattlerPartyIndexes[battlerId] >= PARTY_SIZE)
    {
        BwCaughtMarkerDestroy(marker);
        return;
    }
    parent = &gSprites[parentId];
    if (!parent->inUse || gHealthboxSpriteIds[battlerId] != parentId || parent->data[6] != battlerId
     || !(flags = BwCaughtMarkerFlags(battlerId, &gEnemyParty[gBattlerPartyIndexes[battlerId]])))
    {
        BwCaughtMarkerDestroy(marker);
        return;
    }
    ownPal = IndexOfSpritePaletteTag(BW_MARKER_PAL_TAG);
    marker->pos1.x = parent->pos1.x + parent->pos2.x + parent->centerToCornerVecX + 96;
    marker->pos1.y = parent->pos1.y + parent->pos2.y + parent->centerToCornerVecY + 12;
    out = (u16 *) (OBJ_VRAM0 + marker->oam.tileNum * TILE_SIZE_4BPP);
    for (t = 0; t < 2; ++t)
    {
        const u16 *in = (const u16 *) (t == 0 ? gBwCaughtBallTile : ownPal < 16 ? sBwShinyStarTileOwn : sBwShinyStarTile);
        for (i = 0; i < 16; ++i)
            out[t * 16 + i] = (flags & (1 << t)) ? in[i] : 0;
    }
    marker->oam.paletteNum = ownPal < 16 ? ownPal : parent->oam.paletteNum;
    marker->oam.priority = parent->oam.priority;
    marker->invisible = parent->invisible;
    marker->coordOffsetEnabled = parent->coordOffsetEnabled;
}

static void BwCaughtMarkerSync(u8 spriteId, struct Pokemon *mon, u8 battlerId)
{
    u8 i, found = MAX_SPRITES;

    for (i = 0; i < MAX_SPRITES; ++i)
    {
        if (gSprites[i].inUse && gSprites[i].callback == BwCaughtMarkerFollow && gSprites[i].data[0] == spriteId)
        {
            found = i;
            break;
        }
    }
    if (!BwCaughtMarkerFlags(battlerId, mon))
    {
        if (found < MAX_SPRITES)
            BwCaughtMarkerDestroy(&gSprites[found]);
        return;
    }
    if (found == MAX_SPRITES)
    {
        LoadSpritePalette(&sBwMarkerSpritePal);   //枠が空いていなければ 0xFF＝枠のパレットで出す
        found = CreateSprite(&sBwMarkerTemplate, 0, 0, gSprites[spriteId].subpriority);
        if (found >= MAX_SPRITES)
            return;   //スプライトがいっぱい: 出さない
        gSprites[found].data[0] = spriteId;
    }
    gSprites[found].data[1] = battlerId;
    BwCaughtMarkerFollow(&gSprites[found]);
}

/* UpdateHealthboxAttribute(u8 healthboxSpriteId, struct Pokemon *mon, u8 elementId)
   の中身の差し替え。元と同じく elementId は見ずに全部描き直す。
   戻り値 0 = 描かなかった（踏み台が素の中身へ戻す）。 */
bool8 BwHpbarDrawElements(u8 spriteId, struct Pokemon *mon, u8 elementId)
{
    u8 battlerId = gSprites[spriteId].data[6];
    bool8 player = (GetBattlerSide(battlerId) == B_SIDE_PLAYER);

    (void)elementId;
    /* サファリゾーンの味方（サファリボール ×N の枠）だけ素に任せる。
       絵は色を付け替えた素の絵（build_bw_hpbar.py）、パレットは BW のまま。
       相手は下の 1 人用の相手と同じ BW の描き方 */
    if ((gBattleTypeFlags & BATTLE_TYPE_SAFARI) && player)
        return FALSE;

    if (!IsDoubleBattle())
    {
        if (player)
        {
            BwDrawName(mon, spriteId, battlerId, BW_NAME_COL_PLAYER);
            BwDrawLevel(mon, spriteId, BW_LEVEL_TILE_SINGLE_PLAYER);
            BwDrawHpBar(battlerId, spriteId, mon);
            BwDrawHpNums(mon, spriteId);        /* バーのあと（barFontGfx を使い回す）*/
            BwDrawAilment(mon, spriteId, BW_AILMENT_TILE_SINGLE_PLAYER, FALSE);
        }
        else
        {
            gSprites[spriteId].pos1.x = BW_TILE_SIZE;   /* 元の BW と同じ置き場 */
            BwDrawName(mon, spriteId, battlerId, BW_NAME_COL_OPPONENT);
            BwDrawLevel(mon, spriteId, BW_LEVEL_TILE_OPPONENT);
            BwDrawHpBar(battlerId, spriteId, mon);
            BwDrawAilment(mon, spriteId, BW_AILMENT_TILE_OPPONENT, TRUE);
            BwCaughtMarkerSync(spriteId, mon, battlerId);
        }
    }
    else
    {
        if (player)
        {
            BwDrawName(mon, spriteId, battlerId, BW_NAME_COL_PLAYER);
            BwDrawLevel(mon, spriteId, BW_LEVEL_TILE_DOUBLES_PLAYER);
            BwDrawHpBar(battlerId, spriteId, mon);      /* 数字モードでも値の設定は要る */
            if (BwDoublesHpNumsOn(battlerId))
                BwDoublesShowHpNums(mon, spriteId);     /* 素は交代後に何も出なくなる */
            BwDrawAilment(mon, spriteId, BW_AILMENT_TILE_DOUBLE_PLAYER, TRUE);
        }
        else
        {
            gSprites[spriteId].pos1.x = BW_TILE_SIZE;
            BwDrawName(mon, spriteId, battlerId, BW_NAME_COL_OPPONENT);
            BwDrawLevel(mon, spriteId, BW_LEVEL_TILE_OPPONENT);
            BwDrawHpBar(battlerId, spriteId, mon);
            BwDrawAilment(mon, spriteId, BW_AILMENT_TILE_OPPONENT, TRUE);
            BwCaughtMarkerSync(spriteId, mon, battlerId);
        }
    }
    return TRUE;
}
