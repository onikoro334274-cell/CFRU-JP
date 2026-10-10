#include "defines.h"

#include "../include/overworld.h"
#include "../include/pokemon_icon.h"
#include "../include/sprite.h"
#include "../include/constants/species.h"
#include "Tables/hoenn_fly.h"
#include "Tables/hoenn_script_flags.h"
#include "Tables/hoenn_abnormal_weather.h"

/*
hoenn_townmap.c
    タウンマップにホウエンを 5 枚目として足す（やりたいこと #6）。

    ★ 詳細は docs/research/2026-09-16_town-map.md

    ## 考え方（2026-09-16・スクロール方式）

    ホウエンの区分は **28 列**あるが、カーソルが動ける窓は
    **カントーと同じ 22 列（画面タイル 4〜25）**にして、
    **横にスクロール**して全部に届かせる。

    | | |
    |---|---|
    | 区分の座標 | **地図全体での絶対位置** 0〜27 |
    | 画面タイル | `x + 4 - スクロール量` |
    | BG のずらし | **使わない**（`BG0HOFS` は常に 0） |
    | スクロールの範囲 | 0 〜 6 タイル（28 - 22） |

    ★★ **BG をずらすのではなく、タイルマップを書き直す**（2026-09-16 変更）。

    BG をずらす方式だと、**画面の左右 2 列に当たる BG の列がスクロール量で
    変わる**ため、素 FRLG と同じ「左右 2 列を空ける」ができない。
    （素は 30×20 のうち **行 2〜19・列 2〜27 の 26×18** しか使わず、
     上 2 行と左右 2 列を透明にして下地の青を見せている）

    そこで **BG の列 c には元の列 `c + スクロール量` を書く**。
    列 0・1 と 28〜31 は常に透明にする。`BG0HOFS` は 0 のまま。
    スクロール量が変わったら書き直して VRAM に送る
    （`assembly/hoenn/townmap_hook.s` の 9）。

    ★ 切り替えボタンと「おわる」ボタンは BG の固定位置に描く。
      BG をずらさないので**画面上でも動かない**。

    ## ここで差し替えている関数

    | `functionrewrites` の行 | 素の関数 |
    |---|---|
    | `GetMapsecAtExpanded 080C5348 4 1` | 配置表を引く（呼び出し元 13 箇所） |
    | `GetMapsecUnderCursorExpanded 080C4704 0 1` | カーソルの下の区分（層 0） |
    | `GetMapsecUnderCursorLayer1Expanded 080C4764 0 1` | 〃（層 1） |
    | `CopyTownMapTilemapExpanded 080C1EAC 2 0` | 絵を BG に写す＋ボタン＋目隠し |

    最後のものは **5 枚目だけ 32 列**で読む。素は 30 列固定で、
    31・32 列目には先頭のマスを書いていた（区分 26・27 の絵が出ない）。

    ★ 幅は **ここと `tools/rom/build_townmap.py` の `NEW_W`** の 2 箇所にある。
      `verify_townmap.py` が食い違いを弾く。
*/

#define HOENN_TOWNMAP_STRIDE  28    /* 配置表の刻み。地図によらず一定 */
#define HOENN_TOWNMAP_HEIGHT  15
#define HOENN_TOWNMAP_LAYERS  2
#define HOENN_TOWNMAP_COUNT   5     /* カントー + ななしま×3 + ホウエン */
#define HOENN_TOWNMAP_PAGE    4     /* ホウエンは 5 枚目 */
#define HOENN_MAPSEC_NONE     0xC5

/* カーソルが動ける x の上限（この値まで） */
#define HOENN_CURSOR_MAX_X    27
#define KANTO_CURSOR_MAX_X    21
#define HOENN_CURSOR_MAX_Y    14

#define HOENN_TOWNMAP_PLANE (HOENN_TOWNMAP_STRIDE * HOENN_TOWNMAP_HEIGHT)

/* 絵を写すときの列数。5 枚目は 34 列持つ（素は 30 列）
   34 = 画面に出す右端の列 27 + スクロールの最大 6 + 1 */
#define TOWNMAP_PIC_W        30
#define HOENN_PIC_W          34
#define TOWNMAP_PIC_H        20
#define BG_TILEMAP_W         32

/* 素 FRLG の枠（実測）:
     列 2 と列 27（行 2〜19）は `0x200E`＝タイル 14（全画素が色 15 の無地）で
     **白い縦線**。絵はその内側の列 3〜26。
     上 2 行と列 0・1・28〜31 は `0x2000`＝タイル 0（全画素透明）で下地の青。 */
#define TOWNMAP_LINE_L       2
#define TOWNMAP_LINE_R       27
#define TOWNMAP_ART_X1       3
#define TOWNMAP_ART_X2       26
#define TOWNMAP_TOP_ROWS     2
#define TOWNMAP_BLANK        0x2000   /* タイル 0・全画素が透明 */
#define TOWNMAP_LINE         0x200E   /* タイル 14・白い縦線 */

/* 「おわる」ボタン。素 FRLG はカントーの絵に焼き込んである
   （(24,16)=0x30F0 / (24,17)=0x3120 / (24,18)=0x3130）。
   5 枚目は絵を書き直すので、ここで描く。 */
#define TOWNMAP_EXIT_X       24
#define TOWNMAP_EXIT_Y       16
/* ★ ボタンの当たり判定は **素のまま**（配置表のマス 21）。
   切り替えボタンが行 11、「おわる」が行 13。
   ★★ 絵を画面に固定したうえで判定を素のままにできるのは、
     **ボタンの上にいる間だけカーソルの描画からスクロールを外す**
     ようにしたから（assembly/hoenn/townmap_hook.s の HoennTownMapOriginX）。
     カーソルのマス番号が 21 のままなので、判定は 1 箇所も触らなくてよい。 */
#define TOWNMAP_BUTTON_X     21
/* カーソルが今どこにいるか（素の `カーソル+14`）。
   0 = 地図の上 / 1 = 切り替えボタン / 2 = おわるボタン。
   `080C4618` が 3 で割った余りにしており、`080C4644` がこれを見て
   カーソルを (21,11) / (21,13) へ寄せる。 */
#define TOWNMAP_CURSOR_MODE  14

/* スクロール。窓はカントーと同じ 22 列 */
#define HOENN_VIEW_W         22
#define HOENN_MAX_SCROLL     (HOENN_CURSOR_MAX_X + 1 - HOENN_VIEW_W)  /* 6 */
#define CopyBgTilemapToVram  ((void (*)(u8)) (0x080020BC | 1))

/* 素 FRLG の中身。アドレスは実 ROM を逆アセンブルして確認したもの */
#define sTownMapState   (*(u8 **) 0x0203994C)
#define sTownMapCursor  (*(struct TownMapCursor **) 0x0203995C)
#define sPlayerIcon     (*(u8 **) 0x02039960)
#define ICON_SPRITE_X      0x20
/* スプライトの +0x3E の bit2 が「見えない」。
   素の SetPlayerIconInvisibility（080C5508）が触っているのと同じ場所 */
#define ICON_SPRITE_FLAGS  0x3E
#define ICON_INVISIBLE     0x04

#define sFlyMapState    (*(u8 **) 0x02039964)

/* 飛べる場所のアイコン（実 ROM を逆アセンブルして確認）
     [0x02039964] + 0x144 + i*16 ... +0x00 地図番号(u8)
                                     +0x04 スプライト
                                     +0x08 絵のタグ(u16)
                                     +0x0A 色のタグ(u16)
   ★ **件数は 25**。後始末のループが `CMP r5,#0x18`（080C5C28 / 080C5C6C）。
     以前「50 件」と資料に書いていたのは誤り。
     素は 14 個使う。ホウエンは 23 マスあるので、全部の地図ぶんを
     一度に作ると 37 個で溢れる。だから
     **「今見ている地図」のぶんだけ作り、切り替えたら作り直す**。 */
/* ★★ アイコンを作るのは **080C56F0**。保存先は `+0x2D4` から 16 バイト刻み。
     080C579A `MOV r1,#0xB6 / LSL r1,#2` ＝ 0xB6 * 4 = **0x2D8**（スプライト）
     080C57BC `MOV r2,#0xB5 / LSL r2,#2` ＝ 0xB5 * 4 = **0x2D4**（地図番号）

   ★ 以前 `+0x144` だと思っていたのは **別の関数（080C57F8）** の配列。
     `MOV rX,#0xA4 / LSL #1` の形だけを探して、`LSL #2` のこちらを
     見落としていた（CLAUDE.md 6.12 の罠をまた踏んだ）。
     そのため**ずっと空の配列を見ており**、消せない＝増える、
     動かせない＝スクロールについてくる、になっていた（2026-09-17）。 */
#define FLY_ICON_ARRAY   0x2D4
#define FLY_ICON_MAX     25
#define FLY_ICON_MAP     0x00
#define FLY_ICON_SPRITE  0x04
#define FLY_ICON_TILETAG 0x08
#define FLY_ICON_PALTAG  0x0A

/* ★★ 素のアイコン作成は **タグの控え（+0x14C / +0x14E）に何も書かない**
   （080C58C0〜080C5912 に書き込みが無い。控えは 0 のまま）。
   そこを読んで解放すると `FreeSpriteTilesByTag(0)` になり、
   **タグ 0 ＝ カーソル**の絵を解放してしまう。
   2026-09-17 に「そらをとぶのカーソルがフライングアイコンになる」
   という形で出た。**控えではなく、素の付け方から計算する。**

   素の付け方（080C5986）: 絵のタグ = 通し番号 + 10。
   置き場の番号 ＝ 通し番号なので、i + 10 でよい。 */
#define FLY_ICON_TAG_BASE  10

#define TOWNMAP_ICON_PAGE  0x4D12   /* アイコンを作った地図（u8） */
#define TOWNMAP_ICON_SET   0x4D13   /* 一度でも作ったか（u8） */
#define TOWNMAP_REBUILD    0x4D14   /* 作り直し中の地図 + 1（u8） */
#define TOWNMAP_SEEN_N     0x4D15   /* 控えた区分の数（u8） */
#define TOWNMAP_SEEN       0x4D16   /* 控えた区分（u8 × 25） */
/* ★★ 入れ物の中の場所取り（重ならないよう必ずここで確認すること）
     0x4D10  u16  スクロール量（画素）
     0x4D12  u8   アイコンを作った地図
     0x4D13  u8   一度でも作ったか
     0x4D14  u8   作り直し中の地図 + 1
     0x4D15  u8   重複よけに控えた区分の数
     0x4D16  u8   控えた区分 **× 25 = 0x4D16〜0x4D2E**
     0x4D30  s16  アイコンの素の x **× 25 = 0x4D30〜0x4D61**
     0x4D64  u32  アイコンを作ったときの入れ物
     0x4D68  u8   異常気象のアイコンのスプライト番号 + 1（0 なら無し・2026-09-23）
     0x4D69  u8   そのアイコンを作ったときの異常気象の場所（1〜16）
     0x4D6A  u8   そのアイコンのマス x（地図の列）
     0x4D6C  u8   ジョウトの印を作った数 / 0x4D6D〜 スプライト番号 × 17（2026-09-29）
     0x4D80  s16  シンオウの縦のスクロール量（画素）
     0x4D82  u8   320〜の置き場の絵（0 = ジョウト / 1 = シンオウ）
     0x4D83  u8   シンオウの印を作った数 / 0x4D84〜 スプライト番号 × 23
     0x4DA0       6 枚目（シンオウ）の「元データの位置」の印（中身は読まない）
     （確保は 0x4DC0 バイト・bytereplacement 080C10C0 / 080C1134。verify_townmap.py が重なりを見る）

   ★ 2026-09-17 にここを 0x4D18 に置いて **控えた区分の配列と重ねて**
     しまい、区分の番号でポインタが壊れて「アイコンが 0 個に見える」
     不具合になった。画面に出した本数（上段 0 / 下段 18）で判明した。 */
#define TOWNMAP_FLYBASE    0x4D64   /* アイコンを作ったときの入れ物（u32） */
#define TOWNMAP_ICON_BASEX 0x4D30   /* アイコンの素の x（s16 × 25） */

/* 窓の外のアイコンの置き場。画面（240 画素）の外へ逃がす。
   ★ 「見えない」ビットを立てる手は使えない。素が地図を切り替えるたびに
      `SetFlyIconInvisibility`（080C5B44）で出し入れしており、
      そちらに上書きされるため（080C284C で見えるように戻される）。 */
#define FLY_ICON_PARK      (-64)

/* ★★ アイコンの x に足す補正。**0**（2026-09-18）。

   素の `080C58A6` は区分の種類が 2 か 3 のとき `MOV r7,#2` で +2 するので、
   ここも +2 にしていた。ところが**実機で測ると、カーソルの枠と
   アイコンがホウエンだけ 2 画素ずれていた**。

     アイコンの 16×16 枠を 0..15 として、カーソルの枠の範囲
       カントー（素のまま）: x  2..15
       ホウエン（+2 あり）  : x  4..15   ← 2 画素ぶん食い込む

   カーソルは `マス*8 + 36 - スクロール量`（`HoennTownMapOriginX`）なので、
   **アイコンも同じ値にすると揃う**。カントーは
   `HoennFlyIconWantCell` が先頭で返るため素のままで、影響しない。

   ★ もし実機で逆に 2 画素ずれたら、ここを 2 に戻すだけでよい。
     `tools/rom/check_flyicons.py` で測れる。 */
#define FLY_ICON_ADJ       0

/* ★ サイユウシティ。上下 2 マスで飛び先が違う（上＝ポケモンリーグ）ので、
   ここだけはアイコンを 2 個作る。 */
#define MAPSEC_EVER_GRANDE 0x07

/* 素の地図の絵は state + 0x26 から 1200 バイトずつ 4 枚ぶん並ぶ */
#define TOWNMAP_PIC_BASE   0x26
#define TOWNMAP_PIC_SIZE   1200

/* ★ 地図を切り替えた直後は `state[0x479B]` がまだ**古い**ことがある。
   そのせいで「地図が変わったから作り直す」判定が空振りし、
   ななしま側の飛べる場所のアイコンが出なくなっていた
   （2026-09-17 にユーザーが発見）。
   **元データの位置**から地図番号を割り出せば取り違えない。
   戻り -1 は「地図の絵ではない」（そらをとぶの縁飾りなど）。 */

#define DestroySprite         ((void (*)(void *)) (0x08006DFC | 1))
#define FreeSpriteTilesByTag  ((void (*)(u16)) (0x080082C8 | 1))
#define FreeSpritePaletteByTag ((void (*)(u16)) (0x080085AC | 1))
#define CreateFlyDestIcons    ((void (*)(void)) (0x080C5934 | 1))
#define GetSelectedRegionMap  ((u8 (*)(void)) (0x080C2004 | 1))
#define TownMapFlagGet  ((u8 (*)(u16)) (0x0806DEC4 | 1))
#define TownMapFillRect     ((void (*)(u8, u16, u8, u8, u8, u8)) (0x08002454 | 1))
#define TownMapWriteSeq     ((void (*)(u8, u16, u8, u8, u8, u8, u8, s16)) (0x08002590 | 1))

/* 状態の入れ物の中の位置（実測） */
#define TOWNMAP_SELECTED   0x479B    /* 今見ている地図番号（u8） */
#define TOWNMAP_PLAYER_MAP 0x479C    /* 主人公がいる地図番号（u8）
                                        ★ 初期化で 0x479B と**同じ値**を
                                          書いている（080C12C4 / 080C12CC）。
                                          素はこれと表示中の地図を比べて
                                          主人公アイコンを隠す（080C2862） */
#define TOWNMAP_PERM       0x4797    /* 権限。[0] が 1 なら切り替えられる */
#define TOWNMAP_BGBUF      0x1796    /* BG の絵の下書き。0x1000 ごとに 3 本 */
#define TOWNMAP_HOENN_SLOT 0x47C0    /* 5 枚目の絵（34×20）の置き場 */
#define TOWNMAP_SCROLL_X   0x4D10    /* スクロール量（画素・s16。ジョウトが見えるカントーは負） */

/* ★ 隠し区分の規則は層ごとに違う（実 ROM を逆アセンブルして確認）
     層 0（080C4704）: 0xAE または 0xBB は フラグ 0x8B5 が立つまで出さない
     層 1（080C4764）: 0x8D          は フラグ 0x844 が立つまで出さない */
#define MAPSEC_HIDDEN_L0_A   0xAE
#define MAPSEC_HIDDEN_L0_B   0xBB
#define FLAG_SHOW_HIDDEN_L0  0x8B5
#define MAPSEC_HIDDEN_L1     0x8D
#define FLAG_SHOW_HIDDEN_L1  0x844

struct TownMapCursor
{
    s16 x;
    s16 y;
};

extern const u8 gExpandedTownMapLayout[];

static void MovePlayerIconForScroll(u8 *state);
static bool8 TownMapOnSinnoh(void);
static bool8 TownMapOnKitakami(void);      /* ★ 2026-10-02 7 枚目 */
static bool8 KitakamiTownMapEnabled(void);

/* ================================================================
    ジョウト（カントーの西・地続き）— #6・2026-09-29
    ★ 詳細は docs/research/2026-09-29_johto-town-map.md

    ユーザー「カントーだけ移してスクロール禁止でジョウト行けるフラグがたったら
    マップスクロールできてジョウトが見えるように」。
      フラグ（JOHTO_FLAG_TOWNMAP = 0x13E2）が立つまで … 素のカントーのまま（スクロールしない）
      立ったら（または config.h の JOHTO_TOWNMAP_DEBUG）… 1 枚目（カントー）が
        西へ 30 マスぶんスクロールしてジョウトが見える

    ★ 座標の約束: 配置表の x はカントーが 0〜21 のまま、ジョウトは -30〜-1。
      カーソルの x もそのまま負になる。カントー側は素の値が 1 つも変わらない。
      スクロール量（入れ物 +0x4D10）は **符号つき**（カントーは -240〜0 画素・ホウエンは 0〜48）。
    ★ 絵は ROM の gJohtoKantoTilemap（60 列）から直接写す（入れ物に置き場を足さない）。
    ★ ジョウトの地名の番号は tools/rom/build_mapsec.py（ホウエンの「すいちゅう」をまとめて空けた）。
    ★★ シンオウを足すとき: 地名の番号の空きはジョウトのあと残り 4 個。
      シンオウ（70〜80 個）は「地方ごとに番号を読み替える」仕組みが別に要る（資料を参照）。
   ================================================================ */
#include "Tables/johto_townmap.h"
#include "../include/new/roamer.h"

#define KANTO_TOWNMAP_PAGE   0
#define SCROLL_PX(state)     (*(s16 *) ((state) + TOWNMAP_SCROLL_X))

static bool8 JohtoTownMapEnabled(void)
{
#ifdef JOHTO_TOWNMAP_DEBUG
    return TRUE;
#else
    return FlagGet(JOHTO_FLAG_TOWNMAP);
#endif
}

/* そのページがスクロールするか。する場合は範囲（マス）を返す */
static bool8 TownMapScrollRange(u8 page, s32 *min, s32 *max)
{
    if (page == HOENN_TOWNMAP_PAGE || page == 5 /* シンオウ（28 列）*/)
    {
        *min = 0;
        *max = HOENN_MAX_SCROLL;
        return TRUE;
    }
    if (page == KANTO_TOWNMAP_PAGE && JohtoTownMapEnabled())
    {
        *min = JOHTO_TM_MIN_X;
        *max = 0;
        return TRUE;
    }
    *min = *max = 0;
    return FALSE;
}

static bool8 TownMapPageScrolls(u8 page)
{
    s32 a, b;

    return TownMapScrollRange(page, &a, &b);
}

/* ジョウトの配置表（x = -30〜-1）と、カントー側に足すマス（28 番道路）。
   見つからなければ 0xFF */
static u8 JohtoMapsecAt(u8 layer, s16 y, s16 x)
{
    u32 i;

    if (y < 0 || y >= JOHTO_TM_LAYOUT_H)
        return 0xFF;
    if (x < 0)
    {
        if (x < JOHTO_TM_MIN_X)
            return HOENN_MAPSEC_NONE;
        return gJohtoTownMapLayout[((u32) layer * JOHTO_TM_LAYOUT_H + (u32) y) * JOHTO_TM_LAYOUT_W
                                   + (u32) (x + JOHTO_TM_LAYOUT_W)];
    }
    if (layer == 0)
        for (i = 0; i < JOHTO_TM_EXTRA_COUNT; i++)
            if (gJohtoTownMapExtraCells[i * 4] == x && gJohtoTownMapExtraCells[i * 4 + 1] == y)
                return gJohtoTownMapExtraCells[i * 4 + 2];
    return 0xFF;
}

/* ジョウトのダンジョンの区分の「種類」（`JohtoDungeonTypeHook`・080C4A5C から）。
   素の GetDungeonMapsecType は知らない区分に 1（道）を返すが、ダンジョン名の色は
   `表[種類 - 2]`（080C1E28〜）で引くので、1 だと**表の 1 つ手前**を読んでしまう。
   ジョウトの区分には 3（まだ行っていない）を返す。0 は「ジョウトではない（素に任せる）」。
   ★ ジョウトの地図ができたら、ここで訪問フラグを見て 2 を返す */
u8 JohtoDungeonMapsecType(u8 mapsec)
{
    u32 i;

    /* シンオウのページ: 番号はシンオウだけのもの。地名があれば 3（まだ行っていない）。
       0xC5（地名なし）は素と同じ 0 を返すので素に任せる */
    if (TownMapOnSinnoh() || TownMapOnKitakami())
        return (mapsec == HOENN_MAPSEC_NONE) ? 0 : 3;

    for (i = 0; i < JOHTO_TM_ICON_COUNT; i++)
        if ((u8) gJohtoDungeonIcons[i * 4 + 3] == mapsec)
            return 3;
    return 0;
}

/* 左へ動ける x の下限（`HoennTownMapLeftHook`・080C440C から）。
   素は `CMP r0,#0 / BLE`（x が 0 なら動かない）。 */
s32 HoennTownMapMinCursorX(void)
{
    u8 *state = sTownMapState;

    if (state != NULL && state[TOWNMAP_SELECTED] == KANTO_TOWNMAP_PAGE && JohtoTownMapEnabled())
        return JOHTO_TM_MIN_X;
    return 0;
}

/* ================================================================
    シンオウ（6 枚目のページ・切り替え画面から選ぶ）— #6・2026-09-29
    ★ 詳細は docs/research/2026-09-29_sinnoh-town-map.md

    ユーザー「シンオウ地方のマップも作成してこっちはエメラルドと同じくマップ切り替えで」
    「こちらもデバッグフラグで」。
      フラグ（SINNOH_FLAG_TOWNMAP = 0x13E1）が立つか config.h の SINNOH_TOWNMAP_DEBUG で、
      切り替え画面に 6 枚目（シンオウ）が出る。

    ★ 28 × 23 マス。横はホウエンと同じ 0〜6 のスクロール、**縦も 0〜8 のスクロール**（窓は 15 行）。
      縦のスクロール量は入れ物 +0x4D80（s16・画素）。原点の y・ボタンの y・下の限界はフックで
      （assembly/hoenn/townmap_hook.s の 19〜）。
    ★★ 地名の番号は**シンオウだけの番号**（0〜77）。空きが 4 個しか無いため（CLAUDE.md 6.44）。
      シンオウのページを見ている間だけ、地名（GetMapName・ダンジョン名）・種類を読み替える。
      「タウンマップが開いていてシンオウのページ」＝入れ物 [0x0203994C] が NULL でなく、見ているページが 5。
      素も閉じているかを同じ入れ物の NULL で見ている（080C5F2A）。
    ★ 絵のタイルはジョウトと同じ置き場（320〜・808〜）。ページを切り替えたときに積み直す（TownMapLoadTiles）。
   ================================================================ */
#include "Tables/sinnoh_townmap.h"
#include "Tables/kitakami_townmap.h"
#include "map_region.h"
#include "region_fly.h"

#define SINNOH_TOWNMAP_PAGE      5
/* ★ 2026-10-02 キタカミ（7 枚目・切り替え画面の右列の 3 枚目）。地図ができるまでは選べない（KitakamiTownMapEnabled） */
#define KITAKAMI_TOWNMAP_PAGE    6
#define JOHTO_TOWNMAP_INITIAL_KANTO 0xFF    /* HoennTownMapInitialPage の戻り値（townmap_hook.s の HoennTownMapInitialPageHook と同じ値） */
#define TOWNMAP_SCROLL_Y         0x4D80   /* s16 縦のスクロール量（画素）。シンオウだけ */
#define TOWNMAP_TILES_NOW        0x4D82   /* u8  320〜の置き場の絵: 0 = ジョウト（開いたときに素と一緒に積む）/ 1 = シンオウ */
#define TOWNMAP_SINNOH_ICON_N    0x4D83   /* u8  シンオウの印を作った数 */
#define TOWNMAP_SINNOH_ICONS     0x4D84   /* u8  スプライト番号 × SINNOH_TM_ICON_COUNT */
#define TOWNMAP_SINNOH_SLOT      0x4DA0   /* 6 枚目の「元データの位置」の印（中身は読まない・見分けるだけ） */
#define TOWNMAP_KITAKAMI_SLOT    0x4DA8   /* ★ 2026-10-02 7 枚目（キタカミ）の印。assembly/hoenn/townmap_hook.s の KITAKAMI_TOWNMAP_SLOT と揃える */
#define SCROLL_PY(state)         (*(s16 *) ((state) + TOWNMAP_SCROLL_Y))
#define SINNOH_VIEW_H            15
#define SINNOH_MAX_SCROLL_Y      (SINNOH_TM_H - SINNOH_VIEW_H)    /* 8 */
#define VANILLA_CURSOR_DOWN_MAX  13       /* 素は y <= 13 なら下へ動ける（080C43DC CMP r0,#0x0D） */
#define sSwitchStateOpen         (*(u8 **) 0x02039950)   /* 切り替え画面の入れ物（開いている間だけ） */
#define TILE_VRAM(n)             ((void *) (0x06000000 + (n) * 32))
/* BIOS の LZ77UnCompVram（素の入口 081C7A8C。自前のビルドでは振り分け役を通る・CLAUDE.md 6.38）。
   絵は min_dist 2 で圧縮してある（VRAM は 16 ビット単位で書くため） */
#define TownMapLZ77UnCompVram    ((void (*)(const void *, void *)) (0x081C7A8C | 1))

extern const u8 *const gExpandedMapSecNames[];

static bool8 SinnohTownMapEnabled(void)
{
#ifdef SINNOH_TOWNMAP_DEBUG
    return TRUE;
#else
    return FlagGet(SINNOH_FLAG_TOWNMAP);
#endif
}

static bool8 TownMapOnSinnoh(void)
{
    u8 *state = sTownMapState;

    return state != NULL && state[TOWNMAP_SELECTED] == SINNOH_TOWNMAP_PAGE;
}

static bool8 TownMapOnKitakami(void)
{
    u8 *state = sTownMapState;

    return state != NULL && state[TOWNMAP_SELECTED] == KITAKAMI_TOWNMAP_PAGE;
}

/* 切り替え画面で選べる最大のページ（`HoennTownMapUnlockHook`・080C20B8 から）。
   0 なら素のまま（素は 0 / 1 / 3 を書く）。ホウエンは 0x5C（ハナダのどうくつ前の人が消える）で 4 */
u8 HoennTownMapMaxPage(void)
{
    if (KitakamiTownMapEnabled())
        return KITAKAMI_TOWNMAP_PAGE;     /* シンオウがまだのときは切り替え画面が飛ばす（SwitchPageEnabled） */
    if (SinnohTownMapEnabled())
        return SINNOH_TOWNMAP_PAGE;
    if (FlagGet(0x5C))
        return HOENN_TOWNMAP_PAGE;
    return 0;
}

/* 縦のスクロール量（画素）。シンオウ以外は 0 に戻して 0。原点の y（フック）から呼ばれる */
s32 HoennTownMapScrollPy(void)
{
    u8 *state = sTownMapState;

    if (state == NULL)
        return 0;
    if (state[TOWNMAP_SELECTED] != SINNOH_TOWNMAP_PAGE)
    {
        SCROLL_PY(state) = 0;
        return 0;
    }
    return SCROLL_PY(state);
}

/* 下へ動ける y の上限（これ以下なら動ける）。`HoennTownMapDownHook`（080C43D8）から */
s32 HoennTownMapMaxCursorY(void)
{
    return TownMapOnSinnoh() ? SINNOH_TM_H - 2 : VANILLA_CURSOR_DOWN_MAX;
}

/* 地名の文字（`GetMapName` 080C5F88・ダンジョン名 080C1DE8 のフックから）。
   シンオウのページではシンオウだけの番号として引く */
const u8 *HoennTownMapNameFor(u32 mapsec)
{
    /* ★ 2026-09-29: タウンマップの外では**今いるマップの地方**で引く（src/map_region.c）。
       タウンマップを開いている間は見ているページで引く（シンオウのページ以外はいつもの表） */
    if (sTownMapState != NULL)
        return MapRegion_GetName((u8) mapsec, TownMapOnSinnoh() ? MAP_REGION_SINNOH
                                             : TownMapOnKitakami() ? MAP_REGION_KITAKAMI : MAP_REGION_KANTO);
    return MapRegion_GetName((u8) mapsec, GetCurrentMapRegion());
}

/* タウンマップを開いたときに最初に見せるページ（`HoennTownMapInitialPageHook`・080C1240 から）。
   0 = 素のまま（カントー・ななしま）/ 4 = ホウエン / 5 = シンオウ /
   JOHTO_TOWNMAP_INITIAL_KANTO = 素の判定を飛ばしてカントー（ジョウトつき）＝フックが 0 にして 080C12BC へ。
   ★ 2026-09-29 までは番号の範囲（0x58 未満か 0xC5 より上＝ホウエン）で決めていたが、
     シンオウの番号とジョウトの番号もその範囲に入るので、**今いるマップの地方**で決める */
u8 HoennTownMapInitialPage(void)
{
    switch (GetCurrentMapRegion())
    {
    case MAP_REGION_HOENN:
        return HOENN_TOWNMAP_PAGE;
    case MAP_REGION_SINNOH:
        return SINNOH_TOWNMAP_PAGE;
    case MAP_REGION_KITAKAMI:
        return KITAKAMI_TOWNMAP_PAGE;
    case MAP_REGION_JOHTO:
        /* ★ 2026-09-30 ユーザー「ジョウト地方でタウンマップ開こうとしたら真っ暗になった」:
           素（080C1248）は番号が 0x8E より大きいと、ななしまの 3 行の表（083B862C）から見つかるまで探し続ける。
           ジョウトの番号 0xC9〜0xF7 はその表に無いので表の外を回り続けていた → 素の判定を飛ばしてカントーのページ（ジョウトつき）に */
        return JOHTO_TOWNMAP_INITIAL_KANTO;
    default:
        return 0;
    }
}

/* 主人公の位置を決める表（素の GetPlayerPositionOnRegionMap・080C4E88 / 080C4EEA のフックから）。
   シンオウのマップにいるときはシンオウだけの番号の表（build_sinnoh_townmap.py が配置表から作る） */
extern const u16 gExpandedMapSecSizes[];
extern const u16 gExpandedMapSecPos[];
/* ★ 2026-10-07 ユーザー「ダイビング…洞窟から出た際にマップを開くと 134 ばんすいどうにキャラがいる判定」:
   ホウエンの「すいちゅう」は 1 つの番号にまとめてある（tools/rom/build_mapsec.py）ので、表を番号で引くとまとめ先の位置になる。
   水中の地図（gHoennUnderwaterMapPos・まとめる前の区分の位置と大きさ）にいるときは、その行を指す番地 − 番号 × 4 を返す
   （素は表の頭 + 番号 × 4 を引く・y と高さは +2・番号は gMapHeader の地名＝水中は素のとおり今の地図の見出し） */
struct HoennUnderwaterMapPos
{
    u8 group, num, pad[2];
    u16 pos[2];
    u16 size[2];
};
extern const struct HoennUnderwaterMapPos gHoennUnderwaterMapPos[];

static const struct HoennUnderwaterMapPos *HoennUnderwaterEntry(void)
{
    const struct HoennUnderwaterMapPos *e;

    for (e = gHoennUnderwaterMapPos; e->group != 0xFF; e++)
    {
        if (e->group == (u8) gSaveBlock1->location.mapGroup && e->num == (u8) gSaveBlock1->location.mapNum)
            return e;
    }
    return NULL;
}

const u16 *HoennTownMapSizeTable(void)
{
    u8 r = GetCurrentMapRegion();
    const struct HoennUnderwaterMapPos *e = (r == MAP_REGION_HOENN) ? HoennUnderwaterEntry() : NULL;

    if (e != NULL)
        return e->size - 2 * gMapHeader.regionMapSectionId;
    return (r == MAP_REGION_SINNOH) ? gSinnohMapSecSizes : (r == MAP_REGION_KITAKAMI) ? gKitakamiMapSecSizes : gExpandedMapSecSizes;
}

const u16 *HoennTownMapPosTable(void)
{
    u8 r = GetCurrentMapRegion();
    const struct HoennUnderwaterMapPos *e = (r == MAP_REGION_HOENN) ? HoennUnderwaterEntry() : NULL;

    if (e != NULL)
        return e->pos - 2 * gMapHeader.regionMapSectionId;
    return (r == MAP_REGION_SINNOH) ? gSinnohMapSecPos : (r == MAP_REGION_KITAKAMI) ? gKitakamiMapSecPos : gExpandedMapSecPos;
}

/* 区分の種類（GetMapsecType 080C47C0 のフックの先頭から）。0xFF = いつもどおり（gHoennMapsecKind・素）。
   ★ 2026-09-30 ジョウト・シンオウの町（src/region_fly.c の表）は 2（訪問済み＝飛べる）/ 3（まだ）。
     シンオウのページのほかの地名は「道」（1）。行き先は HoennSetFlyWarpDestination が同じ表から決める
     （表に無い番号で 2 を返すと、そらをとぶでホウエンの番号として飛んでしまう） */
u8 SinnohMapsecType(u8 mapsec)
{
    u8 t;
    u8 region = TownMapOnSinnoh() ? MAP_REGION_SINNOH : TownMapOnKitakami() ? MAP_REGION_KITAKAMI : MAP_REGION_JOHTO;

    if (region == MAP_REGION_JOHTO)
    {
        t = RegionFlyCityType(MAP_REGION_JOHTO, mapsec);
        return t ? t : 0xFF;
    }
    if (mapsec == HOENN_MAPSEC_NONE)
        return 0;
    t = RegionFlyCityType(region, mapsec);
    return t ? t : 1;
}

/* 320〜・808〜の置き場の絵を、見るページに合わせて積み直す（0 = ジョウト / 1 = シンオウ）。
   ★ 開いたときは素の読み込み（080C14E8 のフック）がジョウトを積むので、入れ物の 0（AllocZeroed）= ジョウト */
static void TownMapLoadTiles(u8 *state, u8 want)
{
    if (state[TOWNMAP_TILES_NOW] == want)
        return;
    /* ★ 2026-10-02 2 = キタカミ */
    TownMapLZ77UnCompVram(want == 2 ? gKitakamiTownMapTiles : want ? gSinnohTownMapTiles : gJohtoTownMapTiles,
                          TILE_VRAM(JOHTO_TM_TILE_INDEX));
    TownMapLZ77UnCompVram(want == 2 ? gKitakamiTownMapTiles2 : want ? gSinnohTownMapTiles2 : gJohtoTownMapTiles2,
                          TILE_VRAM(JOHTO_TM_TILE_INDEX2));
    state[TOWNMAP_TILES_NOW] = want;
}

/* 今のページのスクロール量（画素・符号つき）。スクロールしないページでは 0 に戻して 0。
   `HoennTownMapOriginX`（毎フレーム）と `HoennPlayerIconXHook` から呼ばれる。 */
s32 HoennTownMapScrollPx(void)
{
    u8 *state = sTownMapState;

    if (state == NULL)
        return 0;
    if (!TownMapPageScrolls(state[TOWNMAP_SELECTED]))
    {
        SCROLL_PX(state) = 0;
        return 0;
    }
    return SCROLL_PX(state);
}

/* ★★ CFRU では **書き換え可能な static / グローバル変数が使えない**。
   `linker.ld` の出力セクションは `.text` ひとつだけで、`.bss` も
   `COMMON` も **ROM に置かれる**（`.data` は配置規則すら無い）。
   書き込みは ROM 宛てなので黙って捨てられ、読み出しは初期値のまま。
   2026-09-17 にこれで丸一往復むだにした。**状態は必ず入れ物（EWRAM）へ。**

   入れ物の中の割り当て（`AllocZeroed` で 0 になる）:
     0x4D10 u16  スクロール量（画素）
     0x4D12 u8   アイコンを作った地図
     0x4D13 u8   一度でも作ったか
     0x4D14 u8   作り直し中の地図 + 1（0 なら作り直しではない）
     0x4D15 u8   重複よけに控えた区分の数
     0x4D16 u8   控えた区分（25 件）
     0x4D30 s16  アイコンの素の x（25 件・スクロールを引く前） */
/*
    アイコンの置き場（そらをとぶの入れ物）を返す。

    ★★ `[0x02039964]` は**途中で別の領域に差し替わる**ことがある。
      その結果、作ったときと後で読むときで別の入れ物を見てしまい、
      **枠が 1 つも埋まっていないように見えていた**
      （2026-09-17 に画面へ数を出して確定。アイコンは 18 個以上あるのに
      こちらからは 0 個に見えていた）。
      消せない＝増える／動かせない＝スクロールについてくる、が
      これで全部説明できる。

    **だから作った瞬間の値を控えておき、以後はそれを使う。**
    控えが無いときだけ今の値を使う。
*/
/* 本当にスプライトを指しているか。
   ★ 控えた入れ物が解放済みだった場合に、でたらめな値を
     破棄処理へ渡してしまわないための関門。
     素のスプライトの表は 0x020205B8 から 64 個 × 0x44 バイト。 */
#define GSPRITES_ADDR  0x020205B8
#define GSPRITES_SIZE  0x44
#define GSPRITES_COUNT 64

static u8 IsRealSprite(const u8 *p)
{
    u32 off;

    if (p == NULL)
        return 0;
    off = (u32) p - GSPRITES_ADDR;
    if (off >= (u32) (GSPRITES_SIZE * GSPRITES_COUNT))
        return 0;
    return (off % GSPRITES_SIZE) == 0;
}

static u8 *FlyIconBase(u8 *state)
{
    u8 *kept;

    if (state == NULL)
        return NULL;
    kept = *(u8 **) (state + TOWNMAP_FLYBASE);
    return (kept != NULL) ? kept : sFlyMapState;
}

static void HoennFlyIconsApply(u8 *state);
static void HoennFlyIconsRebuild(u8 page);
static void SinnohFlyIconsTick(u8 *state);

u8 GetMapsecAtExpanded(u8 mapIndex, u8 layer, s16 y, s16 x)
{
    u32 plane;

    /* ★ 2026-10-02 キタカミ（7 枚目・22 × 15・値はキタカミだけの番号） */
    if (mapIndex == KITAKAMI_TOWNMAP_PAGE && layer < HOENN_TOWNMAP_LAYERS)
    {
        if (x < 0 || x >= KITAKAMI_TM_W || y < 0 || y >= KITAKAMI_TM_H)
            return HOENN_MAPSEC_NONE;
        return gKitakamiTownMapLayout[((u32) layer * KITAKAMI_TM_H + (u32) y) * KITAKAMI_TM_W + (u32) x];
    }

    /* シンオウ（6 枚目・28 × 23・値はシンオウだけの番号） */
    if (mapIndex == SINNOH_TOWNMAP_PAGE && layer < HOENN_TOWNMAP_LAYERS)
    {
        if (x < 0 || x >= SINNOH_TM_W || y < 0 || y >= SINNOH_TM_H)
            return HOENN_MAPSEC_NONE;
        return gSinnohTownMapLayout[((u32) layer * SINNOH_TM_H + (u32) y) * SINNOH_TM_W + (u32) x];
    }

    if (mapIndex >= HOENN_TOWNMAP_COUNT || layer >= HOENN_TOWNMAP_LAYERS)
        return HOENN_MAPSEC_NONE;

    /* ジョウト（x < 0）とカントー側に足した 28 番道路。フラグが立つまでは素のまま */
    if (mapIndex == KANTO_TOWNMAP_PAGE && JohtoTownMapEnabled())
    {
        u8 j = JohtoMapsecAt(layer, y, x);

        if (j != 0xFF)
            return j;
    }

    if (x < 0 || x >= HOENN_TOWNMAP_STRIDE
     || y < 0 || y >= HOENN_TOWNMAP_HEIGHT)
        return HOENN_MAPSEC_NONE;

    plane = (u32) mapIndex * HOENN_TOWNMAP_LAYERS + layer;

    return gExpandedTownMapLayout[plane * HOENN_TOWNMAP_PLANE
                                  + (u32) y * HOENN_TOWNMAP_STRIDE + x];
}

/* その地図でカーソルが動ける x の上限。★ スクロールの範囲
   （assembly/hoenn/townmap_hook.s の HoennTownMapMaxX と
   HOENN_TM_MAX_SCROLL）と必ず揃えること。 */
u8 GetTownMapMaxCursorX(u8 mapIndex)
{
    return (mapIndex == HOENN_TOWNMAP_PAGE || mapIndex == SINNOH_TOWNMAP_PAGE) ? HOENN_CURSOR_MAX_X
                                                                           : KANTO_CURSOR_MAX_X;
}

static u8 MapsecUnderCursor(u8 layer)
{
    struct TownMapCursor *cursor = sTownMapCursor;
    u8 mapIndex;
    u8 mapsec;

    mapIndex = GetSelectedRegionMap();

    if ((u16) cursor->y > ((mapIndex == SINNOH_TOWNMAP_PAGE) ? SINNOH_TM_H - 1 : HOENN_CURSOR_MAX_Y))
        return HOENN_MAPSEC_NONE;

    if (cursor->x < HoennTownMapMinCursorX() || cursor->x > GetTownMapMaxCursorX(mapIndex))
        return HOENN_MAPSEC_NONE;

    mapsec = GetMapsecAtExpanded(mapIndex, layer, cursor->y, cursor->x);

    /* 素 FRLG と同じ隠し扱い。層で規則が違う */
    if (layer == 0)
    {
        if (mapsec == MAPSEC_HIDDEN_L0_A || mapsec == MAPSEC_HIDDEN_L0_B)
        {
            if (!TownMapFlagGet(FLAG_SHOW_HIDDEN_L0))
                mapsec = HOENN_MAPSEC_NONE;
        }
    }
    else
    {
        if (mapsec == MAPSEC_HIDDEN_L1)
        {
            if (!TownMapFlagGet(FLAG_SHOW_HIDDEN_L1))
                mapsec = HOENN_MAPSEC_NONE;
        }
    }

    return mapsec;
}

u8 GetMapsecUnderCursorExpanded(void)
{
    return MapsecUnderCursor(0);
}

u8 GetMapsecUnderCursorLayer1Expanded(void)
{
    return MapsecUnderCursor(1);
}

/*
    絵を BG の下書きに写す（素は 0x080C1EAC）。

    素がやっていたこと（実 ROM を逆アセンブルして確認）:
      1. 元（30 × 20）を BG の下書き（32 列）へ。
         **31・32 列目には元の先頭のマスを書いていた**
      2. 権限[0] が 1 なら切り替えボタンを BG の (24,14)(24,15)(24,16) に
         3×1 ずつ（最初のタイル 0xF0 / 0x100 / 0x110・パレット 3・増分 1）
      3. 今の地図が 2 でフラグ 0x8B5 が立っていなければ (13,11) を 3×2 塗る
      4. 今の地図が 3 でフラグ 0x8C2 が立っていなければ (21,16) を 3×3 塗る

    変えたのは 1 だけ。**5 枚目は 32 列で読む。**
*/
/* カーソルが窓（22 列）に入るようにスクロール量を決め直して覚える。
   絵の書き直しはしない。戻りはタイル数（符号つき・ジョウトは負）。
   範囲はページごと（ホウエン 0〜6・ジョウトの見えるカントー -30〜0）。 */
static s32 ComputeTownMapScroll(u8 *state, u8 page)
{
    s32 x, lo, hi;

    if (!TownMapScrollRange(page, &lo, &hi))
    {
        SCROLL_PX(state) = 0;
        return 0;
    }

    /* ★ カーソルはまだ無いことがある。
       絵を描くのは 080C137E で、カーソルを作る 080C172C より**先**。
       素の作りではカーソルを見ないので問題にならなかった。
       前回のカーソル（解放済みメモリ）を読まないよう、
       無いときはスクロールしない。正しい値はカーソルが出来た直後に
       `HoennTownMapOriginHook2` から決め直す。 */
    if (sTownMapCursor == NULL)
    {
        SCROLL_PX(state) = 0;
        return 0;
    }

    /* ★ ボタンの上にいる間はスクロールを動かさない（2026-09-18）。
       素はカーソルを (21,11) / (21,13) に**毎フレーム寄せ直す**ので、
       右を押すと x が一瞬 22 になり、ここが拾うと地図だけが滑っていく。
       ボタンは画面に固定した部品なので、乗っている間は止めておく。 */
    if (((const u8 *) sTownMapCursor)[TOWNMAP_CURSOR_MODE] != 0)
        return (SCROLL_PX(state) >> 3);

    x = sTownMapCursor->x;
    s32 s = (SCROLL_PX(state) >> 3);

    if (x < s)
        s = x;
    else if (x > s + (HOENN_VIEW_W - 1))
        s = x - (HOENN_VIEW_W - 1);

    if (s < lo)
        s = lo;
    if (s > hi)
        s = hi;

    SCROLL_PX(state) = (s16) (s * 8);
    return s;
}

/* 縦のスクロール（シンオウだけ・窓 15 行・0〜8）。作りは横と同じ */
static s32 ComputeTownMapScrollY(u8 *state, u8 page)
{
    s32 y, s;

    if (page != SINNOH_TOWNMAP_PAGE || sTownMapCursor == NULL)
    {
        SCROLL_PY(state) = 0;
        return 0;
    }
    if (((const u8 *) sTownMapCursor)[TOWNMAP_CURSOR_MODE] != 0)
        return SCROLL_PY(state) >> 3;       /* ボタンの上にいる間は動かさない（横と同じ） */
    y = sTownMapCursor->y;
    s = SCROLL_PY(state) >> 3;
    if (y < s)
        s = y;
    else if (y > s + (SINNOH_VIEW_H - 1))
        s = y - (SINNOH_VIEW_H - 1);
    if (s < 0)
        s = 0;
    if (s > SINNOH_MAX_SCROLL_Y)
        s = SINNOH_MAX_SCROLL_Y;
    SCROLL_PY(state) = (s16) (s * 8);
    return s;
}

/* 元データの位置から「今どの地図を描いているか」を割り出す。
   素の絵は state + 0x26 から 1200 バイトずつ 4 枚、5 枚目は +0x47C0。
   戻り -1 は「地図の絵ではない」（そらをとぶの縁飾りなど）。 */
static s32 PageFromSrc(u8 *state, const u16 *src)
{
    u32 off;

    if ((const u8 *) src == state + TOWNMAP_HOENN_SLOT)
        return HOENN_TOWNMAP_PAGE;
    if ((const u8 *) src == state + TOWNMAP_SINNOH_SLOT)
        return SINNOH_TOWNMAP_PAGE;
    if ((const u8 *) src == state + TOWNMAP_KITAKAMI_SLOT)
        return KITAKAMI_TOWNMAP_PAGE;

    off = (u32) ((const u8 *) src - state);
    if (off >= TOWNMAP_PIC_BASE
        && off < TOWNMAP_PIC_BASE + TOWNMAP_PIC_SIZE * HOENN_TOWNMAP_PAGE
        && ((off - TOWNMAP_PIC_BASE) % TOWNMAP_PIC_SIZE) == 0)
        return (s32) ((off - TOWNMAP_PIC_BASE) / TOWNMAP_PIC_SIZE);

    return -1;
}

void CopyTownMapTilemapExpanded(u8 bg, const u16 *src)
{
    u8 *state = sTownMapState;
    u16 *dest = (u16 *) (state + TOWNMAP_BGBUF + (u32) bg * 0x1000);
    u8 page = state[TOWNMAP_SELECTED];
    u32 x, y;
    s32 sx = 0;
    /* ★★ ボタンの絵は **画面の列 24 に固定**（2026-09-18）。

       以前は `24 - スクロール量` に置いていた。当たり判定が
       カーソルのマス (21,11) / (21,13) なので、絵をマスに合わせるのが
       一番変更が小さかったため。ただしそれだと
       **スクロールするとボタンが地図の真ん中まで流れてくる**。

       → **絵を固定し、当たり判定のマスのほうを窓の右端へ動かす**。
         マスの判定は素が `080C46EC`（カーソルの x）を呼んだ直後に
         `CMP r0,#0x15` で見ているので、**その呼び出しを
         「画面での列番号（x − スクロール量）」に差し替える**と
         素の比較がそのまま効く（`assembly/hoenn/townmap_hook.s` の 15）。

       ★ 固定に戻したので、以前ここにあった
         「そらをとぶの縁飾りを写すときだけ列 24 に焼き付く」問題も
         消える（どの呼び出しでも同じ列に書くため）。 */
    u32 btnX = TOWNMAP_EXIT_X;
    /* ★ 5 枚目かどうかは「地図番号」ではなく「元データの位置」で決める。
       同じ関数が「そらをとぶ」の縁飾り（+0x12E6・30 列）も写すため。 */
    u8 hoenn = ((const u8 *) src == state + TOWNMAP_HOENN_SLOT);
    /* ★ ジョウトが見えるカントー: 元データ（入れ物の 1 枚目）の代わりに ROM のつないだ絵（60 列）を写す */
    u8 johto = (!hoenn && PageFromSrc(state, src) == KANTO_TOWNMAP_PAGE && JohtoTownMapEnabled());
    /* ★ シンオウ（6 枚目）: 元データの位置は印だけ（中身は無い）。ROM の絵（34 × 28）から縦横にずらして写す */
    u8 sinnoh = ((const u8 *) src == state + TOWNMAP_SINNOH_SLOT);
    /* ★ 2026-10-02 キタカミ（7 枚目）: 印だけ。ROM の絵（30 × 20・画面そのまま・スクロールしない）を写す */
    u8 kitakami = ((const u8 *) src == state + TOWNMAP_KITAKAMI_SLOT);

    if (kitakami)
    {
        TownMapLoadTiles(state, 2);
        for (y = 0; y < TOWNMAP_PIC_H; y++)
        {
            for (x = 0; x < BG_TILEMAP_W; x++)
            {
                u16 v;

                if (y < TOWNMAP_TOP_ROWS)
                    v = TOWNMAP_BLANK;
                else if (x == TOWNMAP_LINE_L || x == TOWNMAP_LINE_R)
                    v = TOWNMAP_LINE;
                else if (x < TOWNMAP_ART_X1 || x > TOWNMAP_ART_X2 || x >= KITAKAMI_TM_ART_W || y >= KITAKAMI_TM_ART_H)
                    v = TOWNMAP_BLANK;
                else
                    v = gKitakamiTownMapTilemap[y * KITAKAMI_TM_ART_W + x];
                dest[y * BG_TILEMAP_W + x] = v;
            }
        }
        TownMapWriteSeq(0, 0x0F0, btnX, TOWNMAP_EXIT_Y, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x120, btnX, TOWNMAP_EXIT_Y + 1, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x130, btnX, TOWNMAP_EXIT_Y + 2, 3, 1, 3, 1);
    }
    else if (sinnoh)
    {
        s32 sy;

        TownMapLoadTiles(state, 1);
        sx = ComputeTownMapScroll(state, SINNOH_TOWNMAP_PAGE);    /* 0〜6 */
        sy = ComputeTownMapScrollY(state, SINNOH_TOWNMAP_PAGE);   /* 0〜8 */
        for (y = 0; y < TOWNMAP_PIC_H; y++)
        {
            for (x = 0; x < BG_TILEMAP_W; x++)
            {
                u32 c = x + (u32) sx;
                u32 r = y + (u32) sy;
                u16 v;

                if (y < TOWNMAP_TOP_ROWS)
                    v = TOWNMAP_BLANK;
                else if (x == TOWNMAP_LINE_L || x == TOWNMAP_LINE_R)
                    v = TOWNMAP_LINE;
                else if (x < TOWNMAP_ART_X1 || x > TOWNMAP_ART_X2
                      || c >= SINNOH_TM_ART_W || r >= SINNOH_TM_ART_H)
                    v = TOWNMAP_BLANK;
                else
                    v = gSinnohTownMapTilemap[r * SINNOH_TM_ART_W + c];
                dest[y * BG_TILEMAP_W + x] = v;
            }
        }
        TownMapWriteSeq(0, 0x0F0, btnX, TOWNMAP_EXIT_Y, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x120, btnX, TOWNMAP_EXIT_Y + 1, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x130, btnX, TOWNMAP_EXIT_Y + 2, 3, 1, 3, 1);
    }
    else if (johto)
    {
        TownMapLoadTiles(state, 0);
        sx = ComputeTownMapScroll(state, KANTO_TOWNMAP_PAGE);   /* -30〜0 */

        for (y = 0; y < TOWNMAP_PIC_H; y++)
        {
            for (x = 0; x < BG_TILEMAP_W; x++)
            {
                s32 c = (s32) x + JOHTO_TM_SRC_OFS + sx;
                u16 v;

                if (y < TOWNMAP_TOP_ROWS)
                    v = TOWNMAP_BLANK;              /* 上の青い帯 */
                else if (x == TOWNMAP_LINE_L || x == TOWNMAP_LINE_R)
                    v = TOWNMAP_LINE;               /* 左右の白い縦線 */
                else if (x < TOWNMAP_ART_X1 || x > TOWNMAP_ART_X2
                      || c < 0 || c >= JOHTO_TM_SRC_W)
                    v = TOWNMAP_BLANK;              /* 両脇の余白 */
                else
                    v = gJohtoKantoTilemap[y * JOHTO_TM_SRC_W + c];

                dest[y * BG_TILEMAP_W + x] = v;
            }
        }

        /* 「おわる」ボタン。つないだ絵からは焼き込みを消してある（流れるため）ので、
           ホウエンと同じく画面の列 24 に固定して描く */
        TownMapWriteSeq(0, 0x0F0, btnX, TOWNMAP_EXIT_Y, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x120, btnX, TOWNMAP_EXIT_Y + 1, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x130, btnX, TOWNMAP_EXIT_Y + 2, 3, 1, 3, 1);
    }
    else if (hoenn)
    {
        /* スクロールした分だけ元をずらして読む。左右 2 列は空ける。
           ★ ここで決め直すので、地図を開いた直後や切り替えた直後に
              カーソルが窓の外にいても正しい位置から描かれる。 */
        sx = ComputeTownMapScroll(state, HOENN_TOWNMAP_PAGE);

        for (y = 0; y < TOWNMAP_PIC_H; y++)
        {
            for (x = 0; x < BG_TILEMAP_W; x++)
            {
                u32 c = x + (u32) sx;
                u16 v;

                if (y < TOWNMAP_TOP_ROWS)
                    v = TOWNMAP_BLANK;              /* 上の青い帯 */
                else if (x == TOWNMAP_LINE_L || x == TOWNMAP_LINE_R)
                    v = TOWNMAP_LINE;               /* 左右の白い縦線 */
                else if (x < TOWNMAP_ART_X1 || x > TOWNMAP_ART_X2
                      || c >= HOENN_PIC_W)
                    v = TOWNMAP_BLANK;              /* 両脇の余白 */
                else
                    v = src[y * HOENN_PIC_W + c];

                dest[y * BG_TILEMAP_W + x] = v;
            }
        }

        /* 「おわる」ボタン（素はカントーの絵に焼き込んである）。
           ★ 画面の列 24 に固定。当たり判定のマスのほうが
              スクロールに合わせて動く。 */
        TownMapWriteSeq(0, 0x0F0, btnX, TOWNMAP_EXIT_Y, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x120, btnX, TOWNMAP_EXIT_Y + 1, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x130, btnX, TOWNMAP_EXIT_Y + 2, 3, 1, 3, 1);
    }
    else
    {
        for (y = 0; y < TOWNMAP_PIC_H; y++)
        {
            for (x = 0; x < BG_TILEMAP_W; x++)
            {
                dest[y * BG_TILEMAP_W + x] =
                    (x < TOWNMAP_PIC_W) ? src[y * TOWNMAP_PIC_W + x]
                                        : src[0];
            }
        }
    }

    if (state[TOWNMAP_PERM] == 1)
    {
        TownMapWriteSeq(0, 0x0F0, btnX, 14, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x100, btnX, 15, 3, 1, 3, 1);
        TownMapWriteSeq(0, 0x110, btnX, 16, 3, 1, 3, 1);
    }

    if (page == 2 && !TownMapFlagGet(0x8B5))
        TownMapFillRect(0, 3, 13, 11, 3, 2);

    if (page == 3 && !TownMapFlagGet(0x8C2))
        TownMapFillRect(0, 3, 21, 16, 3, 3);

    /* 地図を切り替えたらアイコンを作り直す（25 件しか置けないため）。
       ★ 一度も作っていないうちは何もしない。最初の 1 回は素が作る
          （絵を描くのは 080C137E で、アイコンを作る 080C564C より先）。 */

    /* ★ 作り直しは **ここ（絵を描いたとき）だけ**で判断する。
       地図は `state[0x479B]` ではなく **元データの位置**から割り出す。
       切り替えた直後は `0x479B` がまだ古いため。
       ★★ 判断する場所を 2 つ作らないこと。以前は毎フレームの側でも
          判断しており、互いに違う地図を指して毎フレーム作り直しを
          繰り返し、VRAM を壊した（2026-09-17）。 */
    {
        s32 shown = PageFromSrc(state, src);

        if (shown >= 0 && state[TOWNMAP_ICON_SET]
            && state[TOWNMAP_ICON_PAGE] != (u8) shown)
            HoennFlyIconsRebuild((u8) shown);
    }
    HoennFlyIconsApply(state);

    /* 絵を描き直したらアイコンも合わせる（地図を切り替えたときなど）。
       ★ アイコンがまだ無いときは何もしない。開いた直後は
          `HoennPlayerIconXHook` が生成の時点で正しい位置に置く。 */
    MovePlayerIconForScroll(state);
}

/*
    飛べる場所のアイコンをスクロールに合わせて動かし、窓の外なら隠す。

    ★ アイコンは作られた時点の位置（マス * 8 + 36）で固定され、
      あとは誰も動かさない。主人公アイコンと同じ作り。
*/
static void HoennFlyIconsApply(u8 *state)
{
    u8 *base = FlyIconBase(state);
    s16 *basex = (s16 *) (state + TOWNMAP_ICON_BASEX);
    s32 scroll = 0;
    /* ★ ホウエンと、ジョウトが見えるカントー（2026-09-29）でスクロールする */
    u8 hoenn = TownMapPageScrolls(state[TOWNMAP_SELECTED]);
    u32 i;

    if (base == NULL)
        return;

    if (hoenn)
        scroll = SCROLL_PX(state);

    for (i = 0; i < FLY_ICON_MAX; i++)
    {
        u8 *e = base + FLY_ICON_ARRAY + i * 16;
        u8 *spr = *(u8 **) (e + FLY_ICON_SPRITE);
        s32 x;

        if (!IsRealSprite(spr))
            continue;
        /* まだ素の x を控えていない（0）ものは触らない。
           本物の素の x は必ず 36 以上になる。 */
        if (basex[i] == 0)
            continue;

        /* ★ 素の x（スクロールを引く前）から**毎回作り直す**。
           差分を足し込む方式にすると、取りこぼした回のぶんだけ
           ずれが積み上がる。 */
        x = basex[i] - scroll;
        /* ★ 窓の中かは **マス番号**で見る。
           画素で `36〜204` と見ていたら、素の +2 の補正のぶんだけ
           右端のマス 27（サイユウシティ）が 2 画素はみ出した扱いになり、
           そこだけアイコンが出なかった（2026-09-17）。 */
        if (hoenn)
        {
            s32 cell = (basex[i] - 36 - FLY_ICON_ADJ) >> 3;
            s32 first = scroll >> 3;   /* 8 の倍数なので算術シフトで割り算と同じ */

            if (cell < first || cell > first + (HOENN_VIEW_W - 1))
                x = FLY_ICON_PARK;
        }
        *(s16 *) (spr + ICON_SPRITE_X) = (s16) x;
    }
}

/*
    地図を切り替えたらアイコンを作り直す。

    ★ 25 件しか置けないので「今見ている地図」のぶんだけ作る。
      後始末は素の終了処理（080C5C42〜080C5C62）と同じ順序でやる。
*/
/*
    飛べる場所のアイコンを全部消す。
    `HoennFlyIconClearHook`（080C5940 ＝ 作成関数の入口）から呼ばれる。

    ★★ **作成は必ず空の状態から始める。**
      素は「一度作ったら作り直さない」前提なので、消す処理を
      こちらの作り直しだけに任せていたら、地図を切り替えるたびに
      アイコンが積み上がった（2026-09-17 にユーザーの画面で
      8 個 → 22 個に増えているのを確認）。
      25 件しか置けないので溢れて位置も壊れる。
      **入口で必ず消す**ようにすれば、誰が何度作っても積み上がらない。

    消し方は素の終了処理（080C5C42〜080C5C62）と同じ順序。
*/
void HoennFlyIconsClear(void)
{
    u8 *state = sTownMapState;
    u8 *base = FlyIconBase(state);
    u32 i;

    if (base == NULL)
        return;

    for (i = 0; i < FLY_ICON_MAX; i++)
    {
        u8 *e = base + FLY_ICON_ARRAY + i * 16;
        u8 *spr = *(u8 **) (e + FLY_ICON_SPRITE);

        if (!IsRealSprite(spr))
        {
            *(u8 **) (e + FLY_ICON_SPRITE) = NULL;
            continue;
        }
        DestroySprite(spr);
        /* ★ 控え（0 のまま）ではなく素の付け方から計算する。
           0 を渡すとカーソルの絵を解放してしまう。 */
        FreeSpriteTilesByTag((u16) (i + FLY_ICON_TAG_BASE));
        /* ★ 色は解放しない。全部のアイコンで同じタグを使い回しており、
           素の `LoadSpritePalette` は同じタグなら積み直さない。
           解放すると次に積むとき別の場所に移り、他の絵が化ける。 */
        *(u8 **) (e + FLY_ICON_SPRITE) = NULL;
        e[FLY_ICON_MAP] = 0;
    }
    if (state != NULL)
        for (i = 0; i < FLY_ICON_MAX; i++)
            *(s16 *) (state + TOWNMAP_ICON_BASEX + i * 2) = 0;
}

static void HoennFlyIconsRebuild(u8 page)
{
    u8 *state = sTownMapState;

    if (state == NULL)
        return;

    /* ★ 作り直しの相手は **元データから割り出した地図**。
       state[0x479B] はまだ古いことがあるので、ここで先に入れておく。
       0 は「作り直しではない」なので +1 して入れる。
       古いアイコンは CreateFlyDestIcons の入口（HoennFlyIconsClear）が消す。 */
    state[TOWNMAP_REBUILD] = (u8) (page + 1);
    CreateFlyDestIcons();
    state[TOWNMAP_REBUILD] = 0;

    /* ★★ 作れなかったときでも、ここで必ず揃えておく。
       素の作成関数は入口で権限（`state[0x479A]`）を見ており、
       立っていなければ**何もせずに帰る**。
       その場合 `HoennFlyIconWantMap` が呼ばれず `ICON_PAGE` が
       古いままになるので、**毎フレーム作り直しを繰り返して**
       消しては作りを延々と続けてしまう
       （2026-09-17 にこれでカーソルの絵まで巻き込んだ）。 */
    state[TOWNMAP_ICON_PAGE] = page;
}

/*
    素のアイコン作成ループ（080C5934）から、地図 1 枚ごとに呼ばれる。
    戻り 0 ならその地図は飛ばす。
    `assembly/hoenn/townmap_hook.s` の `HoennFlyIconMapHook`（080C5950）。
*/
u8 HoennFlyIconWantMap(u8 map)
{
    u8 *state = sTownMapState;
    u8 want;

    if (state == NULL)
        return 1;                       /* 素の動きに任せる */

    want = state[TOWNMAP_REBUILD] ? (u8) (state[TOWNMAP_REBUILD] - 1)
                                  : state[TOWNMAP_SELECTED];
    if (want != map)
        return 0;                       /* 見ていない地図は作らない */

    /* ★ 作った瞬間の入れ物を控える（後で消す・動かすときに使う） */
    *(u8 **) (state + TOWNMAP_FLYBASE) = sFlyMapState;
    state[TOWNMAP_SEEN_N] = 0;
    state[TOWNMAP_ICON_PAGE] = map;
    state[TOWNMAP_ICON_SET] = 1;
    return 1;
}

/*
    マス 1 つごとに「アイコンを作るか」を決める。
    `HoennFlyIconCellHook`（080C5980）から呼ばれる。

    ★ ホウエンには **2 マスにまたがる街が 6 つ**ある
      （ミナモ・カナズミ・カイナ・トクサネ・キンセツ・サイユウ）。
      素のループはマスごとに作るので 2 個並んでしまう。
      **サイユウ以外は 1 個でよい**（ユーザー方針 2026-09-17）。
      サイユウは上下で飛び先が違う（上＝ポケモンリーグ）ので 2 個のまま。
*/
u8 HoennFlyIconWantCell(u8 map, s16 y, s16 x, u8 slot)
{
    u8 *state = sTownMapState;
    u8 sec;
    u32 i;

    if (state == NULL)
        return 1;
    /* ★ ジョウトが見えるカントーはスクロールするので、素の x を控える（2026-09-29）。
       素のカントーのアイコンは街（種類 2）なので素の式は マス * 8 + 36 + 2（080C58A6）。
       重複よけはしない（素のカントーは 1 マスずつ）。 */
    if (map == KANTO_TOWNMAP_PAGE && JohtoTownMapEnabled())
    {
        /* ★ 2026-09-30 +2 をやめた: 素の CreateFlyIconSprite（080C56F0）は マス * 8 + 36 で +2 しない
           （+2 は印の CreateDungeonIconSprite 080C58A6 のほう）。ホウエンと同じ FLY_ICON_ADJ */
        if (slot < FLY_ICON_MAX)
            *(s16 *) (state + TOWNMAP_ICON_BASEX + slot * 2) = (s16) (x * 8 + 36 + FLY_ICON_ADJ);
        return 1;
    }
    if (map != HOENN_TOWNMAP_PAGE)
        return 1;               /* 素の 4 枚はもともと 1 マスずつ。
                                   スクロールしないので控えも要らない */

    sec = GetMapsecAtExpanded(map, 0, y, x);
    if (sec != MAPSEC_EVER_GRANDE)
    {
        for (i = 0; i < state[TOWNMAP_SEEN_N]; i++)
            if (state[TOWNMAP_SEEN + i] == sec)
                return 0;       /* もう作ってある */
        if (state[TOWNMAP_SEEN_N] < FLY_ICON_MAX)
            state[TOWNMAP_SEEN + state[TOWNMAP_SEEN_N]++] = sec;
    }

    /* ★★ 素の x は **作る前にここで確定値を控える**。
       以前は作ったあとスプライトから読み取っていたが、
       控えられていないアイコンを `HoennFlyIconsApply` は素通しするので、
       **動かない・枠からはみ出す**という形で出ていた（2026-09-17）。
       素の式（080C58A8〜）は マス * 8 + 36 で、
       区分の種類が 2 か 3 のときさらに +2 される（080C58A6）。
       ここへ来る時点で種類は 2 なので +2 で確定。 */
    if (slot < FLY_ICON_MAX)
        *(s16 *) (state + TOWNMAP_ICON_BASEX + slot * 2) =
            (s16) (x * 8 + 36 + FLY_ICON_ADJ);
    return 1;
}

/*
    アイコンを作り終えたところで位置を合わせる。
    `HoennFlyIconDoneHook`（080C59CC）から呼ばれる。
*/
/*
    毎フレーム、飛べる場所のアイコンを地図に貼り直す。
    `HoennTownMapOriginX`（カーソルの位置を出すたび）から呼ばれる。

    ★ なぜ毎フレームか
      絵を描き直したときだけ貼り直す作りにしていたが、
      そらをとぶでは効かなかった（呼ばれる条件を追い切れなかった）。
      処理はスプライト 25 個ぶんの足し算だけで軽いので、
      **いつ呼ばれるかに依存しない形**にした。
      同じ値を書くだけなので何度呼んでも構わない。
*/
/*
    ★ うみ／りくのどうくつ（異常気象）の場所をタウンマップに出す（#2・2026-09-23）

    ユーザー「マップにでるようにしてほしい」。こちらは気象研究所の研究員を置かず
    **自動で**異常気象を起こす（`src/hoenn_abnormal_weather.c`）ので、場所を教える人がいない。
    代わりにホウエンのページの、その道路の区分の上にカイオーガ（海）／グラードン（陸）の
    ポケモンアイコンを出す（CFRU の徘徊のアイコン `CreateTownMapRoamerSprites` と同じ作り方）。

    ・位置は**その道路の区分が地図の上で占めるマスの真ん中**。区分は地図ヘッダ +0x14
      （`regionMapSectionId`）で、配置表（`GetMapsecAtExpanded`）と同じ番号
    ・x はカーソルや主人公アイコンと同じ `マス * 8 + 36 - スクロール量`、
      y は CFRU の徘徊のアイコンと同じ `マス * 8 + 36 - 8`（アイコンの絵が下寄りのため）
    ・ホウエン以外のページと窓の外では隠す（主人公アイコンと同じ・6.30 節の回り込みも防ぐ）
    ・毎フレーム（`HoennFlyIconsTick`）から呼ぶ。作るのは 1 回だけで、以後は置き直すだけ
    ★ 状態は入れ物（`AllocZeroed` の EWRAM）の 0x4D68〜。書き換え可能な static は使えない（6.13）
*/
#define TOWNMAP_ABN_SPRITE 0x4D68
#define TOWNMAP_ABN_LOC    0x4D69
#define TOWNMAP_ABN_CX     0x4D6A
#define HOENN_TM_MAP_W     28       /* ホウエンの配置表の列（0〜27） */
#define HOENN_TM_MAP_H     20

/* 控えたスプライトがまだ自分のアイコンか（画面の作り直しで別物に使い回されていないか） */
static struct Sprite *HoennAbnIconSprite(u8 *state)
{
    u8 id = state[TOWNMAP_ABN_SPRITE];
    struct Sprite *sp;

    if (id == 0)
        return NULL;
    sp = &gSprites[id - 1];
    if (!sp->inUse || sp->callback != SpriteCB_PokeIcon)
    {
        state[TOWNMAP_ABN_SPRITE] = 0;
        return NULL;
    }
    return sp;
}

static void HoennAbnIconTick(u8 *state)
{
    u16 loc = VarGet(HOENN_VAR_ABNORMAL_WEATHER_LOCATION);
    struct Sprite *sp = HoennAbnIconSprite(state);
    u8 hoenn = (state[TOWNMAP_SELECTED] == HOENN_TOWNMAP_PAGE);
    s32 sx, cx;

    /* 異常気象が終わった・別の場所に変わった → 消す */
    if (sp != NULL && (loc == 0 || loc != state[TOWNMAP_ABN_LOC]))
    {
        DestroyMonIcon(sp);
        state[TOWNMAP_ABN_SPRITE] = 0;
        sp = NULL;
    }
    if (loc == 0 || loc > HOENN_ABN_COUNT)
        return;

    if (sp == NULL)
    {
        const struct MapHeader *hdr;
        u8 sec;
        s32 x, y, n = 0, sumx = 0, sumy = 0;
        u8 id;

        if (!hoenn)
            return;         /* ホウエンのページを開いたときに作る */
        hdr = Overworld_GetMapHeaderByGroupAndId(sHoennAbnRoutes[loc - 1][0],
                                                 sHoennAbnRoutes[loc - 1][1]);
        if (hdr == NULL)
            return;
        sec = hdr->regionMapSectionId;
        for (y = 0; y < HOENN_TM_MAP_H; ++y)
        {
            for (x = 0; x < HOENN_TM_MAP_W; ++x)
            {
                if (GetMapsecAtExpanded(HOENN_TOWNMAP_PAGE, 0, (s16) y, (s16) x) == sec)
                {
                    sumx += x;
                    sumy += y;
                    ++n;
                }
            }
        }
        if (n == 0)
            return;         /* 地図の上に無い区分（起きないはず） */
        LoadMonIconPalettes();
        id = CreateMonIcon(loc >= HOENN_ABN_MARINE_START ? SPECIES_KYOGRE : SPECIES_GROUDON,
                           SpriteCB_PokeIcon, 0, (s16) ((sumy / n) * 8 + 36 - 8), 0, 0, FALSE);
        if (id >= MAX_SPRITES)
            return;
        sp = &gSprites[id];
        sp->oam.priority = 1;
        state[TOWNMAP_ABN_SPRITE] = id + 1;
        state[TOWNMAP_ABN_LOC] = (u8) loc;
        state[TOWNMAP_ABN_CX] = (u8) (sumx / n);
    }

    /* 置き直す（スクロールに合わせる）・ホウエン以外と窓の外では隠す */
    cx = state[TOWNMAP_ABN_CX];
    sx = hoenn ? ((SCROLL_PX(state) >> 3)) : 0;
    sp->pos1.x = (s16) (cx * 8 + 36 - sx * 8);
    sp->invisible = (!hoenn || cx < sx || cx > sx + (HOENN_VIEW_W - 1));
}

/* ================================================================
    ジョウトが見えるカントーの、スクロールに付いてこないスプライト（2026-09-29）

    素はダンジョンの印・CFRU の徘徊の印を**作るときに 1 回だけ**置いて動かさない。
    そこで **pos2.x（表示のときだけ足されるずれ）** で動かす。pos1.x は素の値のまま
    なので、マスの番号は `(pos1.x - 36) >> 3` でいつでも分かる（町のマスの +2 は切り捨てで消える）。
      窓の中 … pos2.x = -スクロール量
      窓の外 … 画面の右の外（x = 300）へ逃がす（隠すビットは素が書き換えるので使わない）
    ★ 素の入れ物の中（実 ROM を逆アセンブルして確認・080C5C30〜）:
       [0x02039964] + 0x144 + i*16 … 地図番号（u8）/ + 0x148 … スプライト（25 件）

    ジョウトの印は素の入れ物（25 件・素で 20 件使用）に入らないので**自前で作る**。
    絵は素のカントーの印と同じタグ（同じ絵）を借りる。コマ 0（小さな点 = まだ行っていない）。
    ★ 状態は入れ物（EWRAM）の 0x4D6C〜（6.13・書き換え可能な static は使えない）:
       0x4D6C u8 作った数（0 = まだ）/ 0x4D6D〜 u8 スプライト番号 × JOHTO_TM_ICON_COUNT
   ================================================================ */
#define DUNGEON_ICON_MAP     0x144
#define DUNGEON_ICON_SPRITE  0x148
/* 素の印の絵・色のタグ（080C5A40 付近の CreateDungeonIconSprite の呼び出し: 通し番号 + 35 / 10）。
   ★ 入れ物の +0x14C / +0x14E（tileTag / palTag）は素が書かないので 0 のまま。使わない */
#define DUNGEON_ICON_TAG_BASE 35
#define DUNGEON_ICON_PAL_TAG  10
#define DUNGEON_ICON_MAX     25
#define TOWNMAP_JOHTO_ICON_N  0x4D6C
#define TOWNMAP_JOHTO_ICONS   0x4D6D
#define ICON_PARK_X          300

/* 窓の中なら pos2.x をスクロールに合わせ、外なら画面の外へ */
static void ScrollStaticIcon2(struct Sprite *sp, s32 scrollPx, s32 scrollPy, s32 viewH)
{
    s32 cell = (sp->pos1.x - 36) >> 3;
    s32 row = (sp->pos1.y - 36) >> 3;
    s32 first = scrollPx >> 3;
    s32 top = scrollPy >> 3;

    sp->pos2.y = (s16) -scrollPy;
    if (cell < first || cell > first + (HOENN_VIEW_W - 1) || row < top || row > top + (viewH - 1))
        sp->pos2.x = (s16) (ICON_PARK_X - sp->pos1.x);
    else
        sp->pos2.x = (s16) -scrollPx;
}

static void ScrollStaticIcon(struct Sprite *sp, s32 scrollPx)
{
    ScrollStaticIcon2(sp, scrollPx, 0, HOENN_TOWNMAP_HEIGHT);
}

static bool8 IsTownMapRoamerIcon(const struct Sprite *sp)
{
    u32 j;

    if (!sp->inUse || sp->callback != SpriteCB_PokeIcon)
        return FALSE;
    for (j = 0; j < MAX_NUM_ROAMERS; ++j)
        if (gRoamers[j].species != SPECIES_NONE
         && (const void *) sp->images == (const void *) GetMonIconTiles(GetIconSpecies(gRoamers[j].species, gRoamers[j].personality), FALSE))
            return TRUE;
    return FALSE;
}

/* 素のカントーの印（地図番号 0）の中から 1 つ（見本にする） */
static struct Sprite *KantoDungeonIconSample(u8 *base)
{
    u32 i;

    for (i = 0; i < DUNGEON_ICON_MAX; i++)
    {
        u8 *e = base + i * 16;
        struct Sprite *sp = *(struct Sprite **) (e + DUNGEON_ICON_SPRITE);

        if (e[DUNGEON_ICON_MAP] == KANTO_TOWNMAP_PAGE && IsRealSprite((u8 *) sp))
            return sp;
    }
    return NULL;
}

/* 自前のダンジョンの印を作る（ジョウト・シンオウ共用）。
   list は {x, y, 町のマスなら 1, 番号} × count。スプライト番号は state[slots + i]、作った数は state[nOfs] */
static void TownMapIconsCreate(u8 *state, u8 *base, const s8 *list, u32 count, u32 slots, u32 nOfs)
{
    struct Sprite *sample = KantoDungeonIconSample(base);
    struct SpriteTemplate t;
    u32 i;

    if (sample == NULL)
        return;
    /* ★★ タグは入れ物（+0x14C / +0x14E）から読まない。素は**そこに何も書かない**（0 のまま）ので、
       読むとタグ 0 ＝ カーソルの絵（白い角）を借りてしまう（2026-09-29 実機で発覚・
       そらをとぶの印でも同じ罠を踏んでいる → FLY_ICON_TAG_BASE の注記）。
       素の付け方（CreateDungeonIcons: 絵のタグ = 通し番号 + 35・色のタグ = 10）から計算する */
    t.tileTag = 0xFFFF;
    t.paletteTag = DUNGEON_ICON_PAL_TAG;
    for (i = 0; i < DUNGEON_ICON_MAX; i++)
    {
        u8 *e = base + i * 16;

        if (*(struct Sprite **) (e + DUNGEON_ICON_SPRITE) == sample)
        {
            t.tileTag = (u16) (i + DUNGEON_ICON_TAG_BASE);
            break;
        }
    }
    if (IndexOfSpriteTileTag(t.tileTag) == 0xFF)
        return;
    t.oam = &sample->oam;
    t.anims = gDummySpriteAnimTable;
    t.images = NULL;
    t.affineAnims = gDummySpriteAffineAnimTable;
    t.callback = SpriteCallbackDummy;

    for (i = 0; i < count; i++)
    {
        s32 x = list[i * 4];
        s32 y = list[i * 4 + 1];
        s32 off = list[i * 4 + 2] ? 2 : 0;   /* 町のマスは右下へ 2 画素（素と同じ） */
        u8 id = CreateSprite(&t, (s16) (x * 8 + 36 + off), (s16) (y * 8 + 36 + off), 3);

        state[slots + i] = id;
        if (id < MAX_SPRITES)
            gSprites[id].invisible = TRUE;
    }
    state[nOfs] = (u8) count;
}

static void TownMapIconsFree(u8 *state, u32 slots, u32 nOfs)
{
    u32 i;

    for (i = 0; i < state[nOfs]; i++)
    {
        u8 id = state[slots + i];

        if (id < MAX_SPRITES && gSprites[id].inUse && gSprites[id].callback == SpriteCallbackDummy)
            DestroySprite(&gSprites[id]);
    }
    state[nOfs] = 0;
}

/* タウンマップを閉じるとき（素の FreeMapIcons の終わり・`JohtoTownMapFreeIconsHook` 080C5C70）。
   素が印の絵のタグを解放した直後なので、借りていた印も必ずここで消す */
void JohtoTownMapFreeIcons(void)
{
    u8 *state = sTownMapState;

    if (state == NULL)
        return;
    TownMapIconsFree(state, TOWNMAP_JOHTO_ICONS, TOWNMAP_JOHTO_ICON_N);
    TownMapIconsFree(state, TOWNMAP_SINNOH_ICONS, TOWNMAP_SINNOH_ICON_N);
}

/* シンオウの印（6 枚目）。見えるのはシンオウのページを見ていて、切り替え画面が開いていないときだけ
   （素は切り替え画面を開くと印を全部隠す・080C2ACC 付近 SetDungeonIconInvisibility(0xFF, …, TRUE)。
    素のカントーの印はシンオウのページでは素が隠しているので、見本にはできない） */
static void SinnohTownMapIconsTick(u8 *state, u8 *base)
{
    u8 on = (state[TOWNMAP_SELECTED] == SINNOH_TOWNMAP_PAGE);
    u32 i;

    if (!SinnohTownMapEnabled())
        return;
    if (state[TOWNMAP_SINNOH_ICON_N] == 0)
    {
        if (!on || KantoDungeonIconSample(base) == NULL)
            return;
        TownMapIconsCreate(state, base, (const s8 *) gSinnohDungeonIcons, SINNOH_TM_ICON_COUNT,
                           TOWNMAP_SINNOH_ICONS, TOWNMAP_SINNOH_ICON_N);
    }
    for (i = 0; i < state[TOWNMAP_SINNOH_ICON_N]; i++)
    {
        u8 id = state[TOWNMAP_SINNOH_ICONS + i];
        struct Sprite *sp;

        if (id >= MAX_SPRITES)
            continue;
        sp = &gSprites[id];
        if (!sp->inUse)
            continue;
        ScrollStaticIcon2(sp, on ? SCROLL_PX(state) : 0, on ? SCROLL_PY(state) : 0, SINNOH_VIEW_H);
        sp->invisible = (!on || sSwitchStateOpen != NULL);
    }
}

/* 徘徊の印（CFRU の CreateTownMapRoamerSprites が作る・カントーのページとホウエンのページ）。
   スクロールするページ（ジョウトが見えるカントー・ホウエン）ではスクロールに合わせ、窓の外は隠す。
   ★ 2026-09-29: ラティ兄妹がホウエンを徘徊するようになったので、ジョウトの有無によらず毎フレーム見る */
static void TownMapRoamerIconsTick(u8 *state)
{
    u8 page = state[TOWNMAP_SELECTED];
    u8 scrolls = TownMapPageScrolls(page);
    s32 scroll = scrolls ? SCROLL_PX(state) : 0;
    u32 i;

    for (i = 0; i < MAX_SPRITES; i++)
    {
        struct Sprite *sp = &gSprites[i];

        if (!IsTownMapRoamerIcon(sp))
            continue;
        if (scrolls)
            ScrollStaticIcon(sp, scroll);
        else
            sp->pos2.x = 0;
    }
}

static void JohtoTownMapIconsTick(u8 *state)
{
    u8 *base = FlyIconBase(state);
    u8 kanto = (state[TOWNMAP_SELECTED] == KANTO_TOWNMAP_PAGE);
    s32 scroll = kanto ? SCROLL_PX(state) : 0;
    struct Sprite *sample;
    u32 i;

    if (base == NULL || !JohtoTownMapEnabled())
        return;

    /* 素のカントーの印・徘徊の印 */
    for (i = 0; i < DUNGEON_ICON_MAX; i++)
    {
        u8 *e = base + i * 16;
        struct Sprite *sp = *(struct Sprite **) (e + DUNGEON_ICON_SPRITE);

        if (e[DUNGEON_ICON_MAP] == KANTO_TOWNMAP_PAGE && IsRealSprite((u8 *) sp))
            ScrollStaticIcon(sp, scroll);
    }

    /* ジョウトの印。見えるかどうかは素のカントーの印に合わせる
       （素が地図の切り替え・ダンジョンの見取り図などで出し入れしている） */
    sample = KantoDungeonIconSample(base);
    if (state[TOWNMAP_JOHTO_ICON_N] == 0)
    {
        if (sample == NULL)
            return;
        TownMapIconsCreate(state, base, gJohtoDungeonIcons, JOHTO_TM_ICON_COUNT,
                           TOWNMAP_JOHTO_ICONS, TOWNMAP_JOHTO_ICON_N);
    }
    for (i = 0; i < state[TOWNMAP_JOHTO_ICON_N]; i++)
    {
        u8 id = state[TOWNMAP_JOHTO_ICONS + i];
        struct Sprite *sp;

        if (id >= MAX_SPRITES)
            continue;
        sp = &gSprites[id];
        if (!sp->inUse)
            continue;
        ScrollStaticIcon(sp, scroll);
        sp->invisible = (!kanto || sample == NULL || sample->invisible);
    }
}

void HoennFlyIconsTick(void)
{
    u8 *state = sTownMapState;

    if (state == NULL)
        return;

    /* ★ ここでは作り直さない（判断は絵を描くときの 1 か所だけ）。
       位置を合わせるだけ。 */
    HoennFlyIconsApply(state);
    HoennAbnIconTick(state);
    TownMapRoamerIconsTick(state);
    JohtoTownMapIconsTick(state);
    /* ★ 2026-10-02 どのページでも毎フレーム（シンオウだけにしていたので、他の地方を決定した直後に
       シンオウの位置のまま印が出ていた。カーソルを動かすまで直らなかった） */
    MovePlayerIconForScroll(state);
    if (FlyIconBase(state) != NULL)
        SinnohTownMapIconsTick(state, FlyIconBase(state));
    SinnohFlyIconsTick(state);
}

/* ★ 2026-09-30 ジョウト（カントーのページ・x が負）・シンオウ（6 枚目）の飛べる町の印。
   素のループ（080C5934）は地図 0〜4・列 0〜27 しか回らないので、作り終えたところ（HoennFlyIconsPlace）で
   素の CreateFlyIconSprite（080C56F0・印の置き場 = 通し番号・絵のタグ = 通し番号 + 10・色のタグ 10）を自分で呼ぶ。
   ジョウトは素の x を控えて HoennFlyIconsApply に横のスクロールを任せる。
   シンオウは控えず（0）、HoennFlyIconsTick が縦横のスクロールを pos2 で合わせる（地図の印と同じ ScrollStaticIcon2） */
#define GetRegionMapPermission ((u8 (*)(u8)) (0x080C1FE8 | 1))
#define CreateFlyIconSprite    ((void (*)(u8, u8, u16, u16, u8, u8)) (0x080C56F0 | 1))
#define MAPPERM_HAS_FLY_DESTINATIONS 3

static void RegionFlyIconsCreate(u8 *state)
{
    u8 *base;
    u8 page, region, n = 0;
    u32 i;

    if (state == NULL || !GetRegionMapPermission(MAPPERM_HAS_FLY_DESTINATIONS))
        return;
    page = state[TOWNMAP_REBUILD] ? (u8) (state[TOWNMAP_REBUILD] - 1) : state[TOWNMAP_SELECTED];
    if (page == KANTO_TOWNMAP_PAGE && JohtoTownMapEnabled())
        region = MAP_REGION_JOHTO;
    else if (page == SINNOH_TOWNMAP_PAGE && SinnohTownMapEnabled())
        region = MAP_REGION_SINNOH;
    else if (page == KITAKAMI_TOWNMAP_PAGE && KitakamiTownMapEnabled())
        region = MAP_REGION_KITAKAMI;
    else
        return;
    base = sFlyMapState;
    if (base == NULL)
        return;
    while (n < FLY_ICON_MAX && IsRealSprite(*(u8 **) (base + FLY_ICON_ARRAY + n * 16 + FLY_ICON_SPRITE)))
        n++;
    for (i = 0; i < gRegionFlyCityCount[0] && n < FLY_ICON_MAX; i++)
    {
        const struct RegionFlyCity *c = &gRegionFlyCities[i];

        if (c->region != region || !RegionFlyCityVisited(c))
            continue;
        CreateFlyIconSprite(page, n, (u16) (s16) c->iconX, (u16) (s16) c->iconY, (u8) (n + FLY_ICON_TAG_BASE), FLY_ICON_TAG_BASE);
        if (region == MAP_REGION_JOHTO)
            *(s16 *) (state + TOWNMAP_ICON_BASEX + n * 2) = (s16) (c->iconX * 8 + 36 + FLY_ICON_ADJ);
        n++;
    }
    if (region == MAP_REGION_SINNOH || region == MAP_REGION_KITAKAMI)
    {
        /* 素のループはシンオウ・キタカミのページで HoennFlyIconWantMap を呼ばないので、ここで揃える */
        *(u8 **) (state + TOWNMAP_FLYBASE) = base;
        state[TOWNMAP_ICON_PAGE] = page;
        state[TOWNMAP_ICON_SET] = 1;
    }
}

/* シンオウのページの飛べる町の印を縦横のスクロールに合わせる（毎フレーム・HoennFlyIconsTick から） */
static void SinnohFlyIconsTick(u8 *state)
{
    u8 *base = FlyIconBase(state);
    u8 on = (state[TOWNMAP_SELECTED] == SINNOH_TOWNMAP_PAGE);
    u32 i;

    if (base == NULL)
        return;
    for (i = 0; i < FLY_ICON_MAX; i++)
    {
        u8 *e = base + FLY_ICON_ARRAY + i * 16;
        u8 *spr = *(u8 **) (e + FLY_ICON_SPRITE);

        if (e[FLY_ICON_MAP] != SINNOH_TOWNMAP_PAGE || !IsRealSprite(spr))
            continue;
        ScrollStaticIcon2((struct Sprite *) spr, on ? SCROLL_PX(state) : 0, on ? SCROLL_PY(state) : 0, SINNOH_VIEW_H);
    }
}

void HoennFlyIconsPlace(void)
{
    u8 *state = sTownMapState;

    RegionFlyIconsCreate(state);

    /* ★ 素の x は `HoennFlyIconWantCell` が作る前に控えている。
       ここでは位置を合わせるだけ。 */
    if (state != NULL)
        HoennFlyIconsApply(state);
}

/*
    主人公アイコンをスクロールに合わせて置き直す。

    ★ 素 FRLG はアイコンを**作るときに一度だけ**置いて、あとは動かさない。
      位置の式は `080C54A0` の `(x << 19 + 0x240000) >> 16` ＝ **x * 8 + 36**
      （`LSL #3` / `ADD #0x24` の形ではないので、
      その並びを探す走査では見つからない。2026-09-17 に取りこぼした）。
      スプライトのコールバックは `0x08007188` ＝ `BX lr` の空関数で、
      毎フレーム位置を直す処理もない。
      そのためスクロールすると**地図だけが動き、アイコンは画面に
      貼りついたまま**になり、指している街からずれていく
      （2026-09-17 にユーザーが発見）。

    入れ物（実 ROM を逆アセンブルして確認）:
      `[0x02039960]` -> +0x00 x(s16) / +0x02 y(s16) / +0x04 スプライト
      スプライトの +0x20 が画面の x（`struct Sprite` の pos1.x）

    カーソルと同じ `x * 8 + 36 - スクロール量` に置き直す。
    ホウエン以外の地図ではスクロール量を 0 として扱うので、
    切り替えて戻ったときに素の位置へ戻る。
*/

static void MovePlayerIconForScroll(u8 *state)
{
    u8 *icon = sPlayerIcon;
    u8 *sprite;
    s32 x;
    s32 sx = 0;
    u8 hoenn = TownMapPageScrolls(state[TOWNMAP_SELECTED]);   /* ホウエン・ジョウトが見えるカントー */
    u8 visible;

    if (icon == NULL)
        return;
    sprite = *(u8 **) (icon + 4);
    if (sprite == NULL)
        return;

    x = *(s16 *) icon;
    if (hoenn)
        sx = (SCROLL_PX(state) >> 3);

    *(s16 *) (sprite + ICON_SPRITE_X) = (s16) (x * 8 + 36 - sx * 8);
    /* ★ シンオウは縦にもスクロールする（2026-09-29）。y は +0x02（マス）・スプライトの +0x22 */
    if (state[TOWNMAP_SELECTED] == SINNOH_TOWNMAP_PAGE)
    {
        s32 y = *(s16 *) (icon + 2);
        s32 sy = SCROLL_PY(state) >> 3;

        *(s16 *) (sprite + ICON_SPRITE_X + 2) = (s16) (y * 8 + 36 - sy * 8);
        if (y < sy || y > sy + (SINNOH_VIEW_H - 1))
            x = -1000;                    /* 下の「窓の中か」で隠す */
    }
    else
    {
        /* ★ 2026-10-02 ユーザー「シンオウ地方でマップ開いて他地方を見ると主人公の表示位置がバグる」:
           シンオウのページで縦にずらした y が、ほかのページで戻っていなかった → 素の位置（y * 8 + 36）へ戻す */
        *(s16 *) (sprite + ICON_SPRITE_X + 2) = (s16) (*(s16 *) (icon + 2) * 8 + 36);
    }

    /* ★ 窓の外にいるときは隠す。

       素は 22 列の地図しか想定していないので、窓から外れた位置に
       スプライトを置くという状況が無く、隠す処理も無い。
       スクロールを入れると**枠の外にはみ出して描かれてしまう**
       （2026-09-17 にユーザーが発見）。

       ★ 素の条件（表示中の地図 == 主人公のいる地図・`080C2862`）を
         消してはいけない。主人公がカントーにいるときホウエンの地図に
         出ないのはこの条件のおかげで、実機で確認済み。
         だから**素の条件に「窓の中か」を足す**形にする。

       カーソルが動ける範囲は [スクロール量, スクロール量 + 21] なので、
       アイコンも同じ範囲なら窓の中にいる。 */
    visible = (state[TOWNMAP_SELECTED] == state[TOWNMAP_PLAYER_MAP]);
    if (visible && hoenn && (x < sx || x > sx + (HOENN_VIEW_W - 1)))
        visible = 0;

    if (visible)
        sprite[ICON_SPRITE_FLAGS] &= (u8) ~ICON_INVISIBLE;
    else
    {
        sprite[ICON_SPRITE_FLAGS] |= ICON_INVISIBLE;
        /* ★ 2026-10-02 素は地図を開く・切り替えるアニメの途中で主人公の印を**無条件に表示**にする
           （pokefirered src/region_map.c の SetPlayerIconInvisibility(FALSE)・ページの見比べなし）。
           旗だけで隠すと、そのフレームにシンオウの位置のまま他の地方の地図に出る → 画面の外へも動かす */
        *(s16 *) (sprite + ICON_SPRITE_X) = (s16) ICON_PARK_X;
    }
}

/*
    カーソルが動いたあとにスクロール量を決め直し、
    変わっていれば絵を書き直して VRAM に送る。
    `assembly/hoenn/townmap_hook.s` の `HoennTownMapFollowHook`
    （フック先 `080C457E`）から呼ばれる。
*/
void HoennTownMapScrollUpdate(void)
{
    u8 *state = sTownMapState;
    u16 before, beforeY;

    u8 page = state[TOWNMAP_SELECTED];

    if (!TownMapPageScrolls(page))
    {
        /* カントー（ジョウトが見えないとき）・ななしまに戻ったら素の位置（x * 8 + 36）へ戻す */
        MovePlayerIconForScroll(state);
        return;
    }

    before = (u16) SCROLL_PX(state);
    beforeY = (u16) SCROLL_PY(state);
    ComputeTownMapScroll(state, page);
    ComputeTownMapScrollY(state, page);      /* シンオウだけ（ほかは 0） */
    MovePlayerIconForScroll(state);

    if ((u16) SCROLL_PX(state) == before && (u16) SCROLL_PY(state) == beforeY)
        return;

    /* ★ 元データの位置でどの絵かが決まる（ホウエン＝5 枚目の置き場・シンオウ＝印・カントー＝1 枚目） */
    CopyTownMapTilemapExpanded(0, (page == HOENN_TOWNMAP_PAGE)
        ? (const u16 *) (state + TOWNMAP_HOENN_SLOT)
        : (page == SINNOH_TOWNMAP_PAGE) ? (const u16 *) (state + TOWNMAP_SINNOH_SLOT)
        : (page == KITAKAMI_TOWNMAP_PAGE) ? (const u16 *) (state + TOWNMAP_KITAKAMI_SLOT)
        : (const u16 *) (state + TOWNMAP_PIC_BASE));
    CopyBgTilemapToVram(0);
}

/* ================================================================
    地図の切り替え画面を 2 列 5 枚にする（やりたいこと #6）

    ★ 詳細は docs/research/2026-09-17_switch-map-menu.md

    素 FRLG の切り替え画面は**縦 1 列**しか想定していない。
    ホウエンを足して 5 項目にすると、素の並べ方では
    24 + 32 × 5 = 184 画素となり画面（160）からはみ出す。

    ## 素の作り（実 ROM を逆アセンブルして確認）

    入れ物は `[0x02039950]`（`AllocZeroed(0x1CE0)`・`080C2062`）。

    | 場所 | 中身 |
    |---|---|
    | `+0x1CCA` | u8  選んでいる項目 |
    | `+0x1CCC` | u8  選べる最大の項目番号（`HoennTownMapUnlockHook` が 4 にする）|
    | `+0x1CCE` | u16 **一覧の開始行（タイル）** |
    | `+0x1CD6` | u16 選択枠の上端（画素）|
    | `+0x18B0 + 1036*i` | 左右の矢印スプライト |
    | `+0x18B8 + 1036*i` | 〃 の x（`080C20C0` / `080C20C6` で 0x58 / 0x98）|

    ★★ **枠の y も矢印の y も、すべて `(選択 × 4 + 開始行) × 8` という
      1 つの式**（`080C2678` / `080C29DC`）から出している。
      だから **`+0x1CCE` を選択のたびに入れ替えるだけで縦位置は全部追従する。**

      左列（項目 0〜2）… 開始行 = 4        → y = 32 / 64 / 96
      右列（項目 3〜4）… 開始行 = 4 - 12   → y = 32 / 64
      （u16 なので -8 は 0xFFF8。`(12 + 0xFFF8) << 3` の下位 16 ビットが 32）

    ## 横位置だけは定数が焼き込んである

    | 場所 | 素 | 用途 |
    |---|---|---|
    | `080C2580` / `080C266E` | `MOV r1,#0x48` | 選択枠の左端 |
    | `080C25B6` / `080C26A6` | `MOV r1,#0xA8` | 選択枠の右端 |

    ★ 同じ計算が **2 つの関数**にある（`080C2574` = 開いたとき /
      `080C265C` = 毎フレーム）。リテラル（`0x1CD4` / `0x1CD8`）を
      ROM 全体で走査して数え上げた。命令の形では探していない
      （CLAUDE.md 6.12）。

    ★ ついでに分かったこと: **`[+0x1CD4]` と `[+0x1CD8]` は誰も読まない**
      （書いているのが上の 4 箇所だけで、読む側が ROM 全体に無い）。
      効いているのは同じ値を積んだスタック上の窓の矩形だけなので、
      フックは r1 を作り直すだけでよく、書き戻しは省いている。
   ================================================================ */

#define SW_COUNT        7       /* 項目数（カントー + ななしま×3 + ホウエン + シンオウ + キタカミ・2026-10-02）*/
#define SW_LEFT_ROWS    4       /* 左列に置く枚数（カントー・ななしま×3）。残り（ホウエン・シンオウ・キタカミ）が右列 */
#define SW_COL_X_L      2       /* tools/rom/build_switchmenu.py の COL_X（パネルの左端のタイル列） */
#define SW_COL_X_R      16
#define SW_PANEL_W      12
#define SW_TM_W         30      /* 入れ物の中のタイルマップの幅 */
#define SW_PANEL_H      4       /* パネル 1 枚の高さ（タイル）*/
#define SW_ROW_Y        4       /* 一番上のパネルの行（タイル）*/
#define SW_COL_DX       56      /* 素（タイル列 9）からのずれ（画素）*/
#define SW_MAX_VANILLA  3       /* これより大きければホウエンが入っている */
#define SW_VANILLA_BASE 3       /* 素の 4 枚のときの開始行（080C2138）*/

/* ★ この 4 つは tools/rom/build_switchmenu.py と同じ値でなければならない。
     `COL_X = (2, 16)` / `ROW_Y = 4` / `COL_ROWS = (3, 2)` / `PANEL_H = 4`。
     タイル列 2 は画素 16、素は列 9 ＝ 画素 72 なので ずれは -56。
     タイル列 16 は画素 128 なので +56。
     `verify_townmap.py` が食い違いを弾く。 */

#define sSwitchState    (*(u8 **) 0x02039950)
#define SW_SEL          0x1CCA
#define SW_MAX          0x1CCC
#define SW_BASE         0x1CCE
#define SW_RECT_TOP     0x1CD6
#define SW_SPR_PTR      0x18B0
#define SW_SPR_X        0x18B8
#define SW_SPR_STRIDE   1036
#define SW_TILEMAP      0x1000      /* 入れ物の中のタイルマップの置き場 */
#define SW_EDGE_L       0x48
#define SW_EDGE_R       0xA8
#define SW_SPRITE_Y     0x22        /* struct Sprite の pos1.y */

/* 素の素材 */
#define FR_SWITCH_GFX   ((const void *) 0x083B7108)
#define FR_SWITCH_MAP4  ((const void *) 0x083B7AA4)

/* 押されたキー（gMain のくり返し込み。080C26CA が読んでいるのと同じ場所）*/
#define SW_KEYS         (*(u16 *) (0x03003130 + 46))
#define KEY_RIGHT       0x10
#define KEY_LEFT        0x20
#define KEY_UP          0x40
#define KEY_DOWN        0x80
#define SE_SELECT       0xF5

#define PlaySE          ((void (*)(u16)) (0x08071A70 | 1))
#define LZ77UnCompWram  ((void (*)(const void *, void *)) (0x081C7A90 | 1))

extern const u8 gHoennSwitchMenuTiles[];
extern const u8 gHoennSwitchMenuTilemap[];    /* 7 枚とも入れたもの（2026-10-02）。選べない地方は HoennSwitchMenuInit が空白にする */

/* 上下左右で次に行く項目。0xFF は「動かない」（2026-10-02 2 列 × 4 段）。
     左列 = 0,1,2,3（カントー・ななしま×3）/ 右列 = 4,5,6（ホウエン・シンオウ・キタカミ）
   ★ 行き先が選べない地方なら、同じ向きにもう 1 つ進む（SwitchMenuNext）。右へ動くときは右列の上へ寄せる */
static const u8 sSwitchUp[SW_COUNT]    = {0xFF, 0,    1,    2,    0xFF, 4,    5};
static const u8 sSwitchDown[SW_COUNT]  = {1,    2,    3,    0xFF, 5,    6,    0xFF};
static const u8 sSwitchLeft[SW_COUNT]  = {0xFF, 0xFF, 0xFF, 0xFF, 0,    1,    2};
static const u8 sSwitchRight[SW_COUNT] = {4,    5,    6,    6,    0xFF, 0xFF, 0xFF};

static bool8 KitakamiTownMapEnabled(void)
{
#ifdef KITAKAMI_TOWNMAP_DEBUG
    return TRUE;
#else
    /* キタカミのさとに一度行ったら（そらをとぶの訪問の旗・tools/johto/johto_ids.json の VISIT_KITAKAMI） */
    const struct RegionFlyCity *c = RegionFlyFindCity(MAP_REGION_KITAKAMI, 0);
    return c != NULL && FlagGet(c->flag);
#endif
}

/* その地方のパネルを選べるか（選べる最大 `+0x1CCC` 以下で、シンオウ・キタカミは各自の印）。
   ★ 2026-10-02 シンオウはまだでもキタカミは選べる、という組み合わせがあるので「最大まで全部」ではない */
static bool8 SwitchPageEnabled(const u8 *state, u8 page)
{
    if (page >= SW_COUNT || page > state[SW_MAX])
        return FALSE;
    if (page == SINNOH_TOWNMAP_PAGE)
        return SinnohTownMapEnabled();
    if (page == KITAKAMI_TOWNMAP_PAGE)
        return KitakamiTownMapEnabled();
    return TRUE;
}

static u8 SwitchMenuIsTwoColumn(const u8 *state)
{
    return state[SW_MAX] > SW_MAX_VANILLA;
}

/* 選んでいる項目に合わせて開始行を入れ替える。
   これ 1 つで枠も矢印も縦位置が決まる。 */
static void SwitchMenuSetBase(u8 *state, u8 sel)
{
    *(u16 *) (state + SW_BASE) = (sel < SW_LEFT_ROWS)
        ? (u16) SW_ROW_Y
        : (u16) (SW_ROW_Y - SW_LEFT_ROWS * SW_PANEL_H);
}

/*
    一覧の絵とタイルマップを読み込む（素は 080C2130〜080C2150）。
    `sel` は開いたときの項目番号（素は r6 に持っている）。
*/
void HoennSwitchMenuInit(u8 sel)
{
    u8 *state = sSwitchState;

    if (state == NULL)
        return;

    if (SwitchMenuIsTwoColumn(state))
    {
        u32 p, tx, ty;

        SwitchMenuSetBase(state, sel);
        LZ77UnCompWram(gHoennSwitchMenuTilemap, state + SW_TILEMAP);
        LZ77UnCompWram(gHoennSwitchMenuTiles, state);
        /* 選べない地方のパネルを空白にする（タイル 0 = 透明） */
        for (p = 0; p < SW_COUNT; p++)
        {
            u16 *tm = (u16 *) (state + SW_TILEMAP);
            u32 col = (p < SW_LEFT_ROWS) ? SW_COL_X_L : SW_COL_X_R;
            u32 row = SW_ROW_Y + ((p < SW_LEFT_ROWS) ? p : p - SW_LEFT_ROWS) * SW_PANEL_H;

            if (SwitchPageEnabled(state, (u8) p))
                continue;
            for (ty = 0; ty < SW_PANEL_H; ty++)
                for (tx = 0; tx < SW_PANEL_W; tx++)
                    tm[(row + ty) * SW_TM_W + col + tx] = 0;
        }
    }
    else
    {
        *(u16 *) (state + SW_BASE) = SW_VANILLA_BASE;
        LZ77UnCompWram(FR_SWITCH_MAP4, state + SW_TILEMAP);
        LZ77UnCompWram(FR_SWITCH_GFX, state);
    }
}

/*
    選択枠の左端・右端（画素）。`right` が 0 なら左。
    素の 4 枚のときは素と同じ値を返す。
*/
u16 HoennSwitchMenuEdgeX(u8 right)
{
    u8 *state = sSwitchState;
    s32 x = right ? SW_EDGE_R : SW_EDGE_L;

    if (state != NULL && SwitchMenuIsTwoColumn(state))
        x += (state[SW_SEL] < SW_LEFT_ROWS) ? -SW_COL_DX : SW_COL_DX;

    return (u16) x;
}

/*
    上下左右で選択を動かす（素の 080C26C8〜080C2724 の代わり）。
    戻り 1 なら「動いた」＝呼び出し元が地図を描き直す。
*/
u8 HoennSwitchMenuMove(void)
{
    u8 *state = sSwitchState;
    u16 keys;
    u8 sel, max, next;

    if (state == NULL)
        return 0;

    keys = SW_KEYS;
    sel = state[SW_SEL];
    max = state[SW_MAX];
    next = 0xFF;

    if (!SwitchMenuIsTwoColumn(state) || sel >= SW_COUNT)
    {
        /* 素と同じ 1 列。上下だけ動く */
        if ((keys & KEY_UP) && sel != 0)
            next = (u8) (sel - 1);
        else if ((keys & KEY_DOWN) && sel < max)
            next = (u8) (sel + 1);
    }
    else
    {
        const u8 *tab = (keys & KEY_UP) ? sSwitchUp : (keys & KEY_DOWN) ? sSwitchDown
                      : (keys & KEY_LEFT) ? sSwitchLeft : (keys & KEY_RIGHT) ? sSwitchRight : NULL;

        if (tab != NULL)
        {
            next = tab[sel];
            /* 選べない地方は同じ向きにもう 1 つ。右へ動いて右列で見つからなければ、右列の上へ（ホウエン側へ）寄せる */
            while (next != 0xFF && !SwitchPageEnabled(state, next))
                next = (tab == sSwitchRight) ? sSwitchUp[next] : tab[next];
            if (tab == sSwitchRight && next != 0xFF && next < SW_LEFT_ROWS)
                next = 0xFF;
        }
    }

    if (next == 0xFF || !SwitchPageEnabled(state, next))
        return 0;

    PlaySE(SE_SELECT);
    state[SW_SEL] = next;
    /* ★ 2026-10-07 開始行を入れ替えるのは 2 列のときだけ。素の 1 列は項目の数で決まった開始行
       （2 枚 6・3 枚 4・4 枚 3・080C20F0〜080C2138）のまま動かさない。前はここで 1 列でも 4 にしていて、
       トライパスだけのとき（2 枚）にカーソルを動かすと枠と窓が 2 行上へずれた（デモ版の報告） */
    if (SwitchMenuIsTwoColumn(state))
        SwitchMenuSetBase(state, next);
    return 1;
}

/*
    左右の矢印スプライトを選択に合わせて置き直す
    （素のコールバック 080C289C の代わり）。

    ★ 素は y しか直していない。2 列にすると x も変わるので足す。
      どちらの矢印かは **入れ物に控えてあるスプライトのポインタ**で見分ける
      （`+0x18B0` が 0 番、`+0x18B0 + 1036` が 1 番。`080C2A10` が書く）。
      素の x は `+0x18B8 + 1036*i` に入っていて、誰も書き換えない。
*/
void HoennSwitchMenuArrowMove(u8 *sprite)
{
    u8 *state = sSwitchState;

    if (state == NULL || sprite == NULL)
        return;

    *(s16 *) (sprite + SW_SPRITE_Y) =
        (s16) (*(u16 *) (state + SW_RECT_TOP) + 16);

    if (SwitchMenuIsTwoColumn(state))
    {
        u32 i = (*(u8 **) (state + SW_SPR_PTR) == sprite) ? 0 : 1;
        s32 x = *(s16 *) (state + SW_SPR_X + i * SW_SPR_STRIDE);

        *(s16 *) (sprite + ICON_SPRITE_X) = (s16)
            (x + ((state[SW_SEL] < SW_LEFT_ROWS) ? -SW_COL_DX : SW_COL_DX));
    }
}

/*
    ボタンを画面に固定するための 3 本（2026-09-18・作り直し）。

    ★★ 素 FRLG のボタンは**配置表のマス (21,11) / (21,13) そのもの**で、
      十字キーで歩いて乗れる。だから「寄せられたとき」だけ細工しても
      歩いて乗った場合に効かない（2026-09-18 に実機で判明）。
      **当たり判定のマスのほうを窓の右端（21 + スクロール量）へ動かす。**

    ★★ 判定は **9 箇所**あり、形が 3 種類ある。**全部押さえること。**

    | 形 | 場所 | 使うもの |
    |---|---|---|
    | `BL 080C46EC` → `CMP r0,#0x15` | `080C1678` `080C16A6` `080C188C` `080C18C0` `080C627E` | `HoennTownMapCursorScreenX` |
    | `LDR r1,[カーソル,#0]` で **x と y を 32 ビットまとめて** → `0x000B0015` / `0x000D0015` と比較 | `080C4428` `080C443A` | `HoennTownMapCursorPacked` |
    | マス番号 21 を直接書く | `080C465C` `080C469C` | `HoennTownMapButtonCellX` |

    ★ 2 番目の形を取りこぼして「ボタンは付いてくるが A が効かない」に
      なった。**即値の走査では絶対に見つからない**（CLAUDE.md 6.12）。

    カントー・ななしまはスクロール量 0 なので、素とまったく同じ値になる。
*/
static s32 TownMapScrollCells(u8 *state)
{
    /* ★ ジョウトが見えるカントーでは負（2026-09-29）。ボタンのマスは 21 + これ */
    if (state == NULL || !TownMapPageScrolls(state[TOWNMAP_SELECTED]))
        return 0;
    return (SCROLL_PX(state) >> 3);
}

/* カーソルの「画面での列」。素の `080C46EC` ＋ ゼロ拡張の代わり。 */
u16 HoennTownMapCursorScreenX(void)
{
    struct TownMapCursor *cursor = sTownMapCursor;

    if (cursor == NULL)
        return 0;

    return (u16) (cursor->x - TownMapScrollCells(sTownMapState));
}

/* カーソルの「画面での行」（シンオウの縦スクロールを引く・2026-09-29）。
   素の `080C46F8`（カーソルの y）＋ゼロ拡張の代わり。ボタンの判定 5 か所・選択枠の y から呼ばれる */
u16 HoennTownMapCursorScreenY(void)
{
    struct TownMapCursor *cursor = sTownMapCursor;
    u8 *state = sTownMapState;

    if (cursor == NULL)
        return 0;
    if (state == NULL || state[TOWNMAP_SELECTED] != SINNOH_TOWNMAP_PAGE)
        return (u16) cursor->y;
    return (u16) (cursor->y - (SCROLL_PY(state) >> 3));
}

/* ボタンの**配置表での**行（素は 11 / 13 を直接書く・080C465C / 080C469C のフックから）。
   シンオウは縦にスクロールするので行が動く */
u16 HoennTownMapButtonCellY(u32 row)
{
    u8 *state = sTownMapState;

    if (state == NULL || state[TOWNMAP_SELECTED] != SINNOH_TOWNMAP_PAGE)
        return (u16) row;
    return (u16) (row + (SCROLL_PY(state) >> 3));
}

/* x と y をまとめた形（素が `LDR r1,[カーソル,#0]` で読むのと同じ並び）。
   x だけ「画面での列」に直す。 */
u32 HoennTownMapCursorPacked(void)
{
    struct TownMapCursor *cursor = sTownMapCursor;

    if (cursor == NULL)
        return 0;

    return ((u32) HoennTownMapCursorScreenY() << 16)
         | (u32) HoennTownMapCursorScreenX();
}

/* ボタンのある**配置表での**列。窓の右端なのでスクロールで動く。
   カーソルをボタンへ寄せるときに使う（素は 21 を直接書いていた）。 */
u16 HoennTownMapButtonCellX(void)
{
    return (u16) (TOWNMAP_BUTTON_X + TownMapScrollCells(sTownMapState));
}


/* ------------------------------------------------------------------
   サイユウシティ: 上のマスならポケモンリーグ、下のマスなら街へ飛ぶ
   ------------------------------------------------------------------

   ★★★ 2026-09-22 にユーザーが実機で発見:
     「サイユウシティのリーグに到着しているのに
       フライングアイコンで飛ぶとポケモンセンター側に飛ぶ」

   エメラルドは区分 `MAPSEC_EVER_GRANDE_CITY` が**地図の 2 マス**に
   またがっていて、**カーソルがどちらのマスに居るか**で飛び先を変える:

     pokeemerald/src/region_map.c:2007
       FlagGet(FLAG_LANDMARK_POKEMON_LEAGUE) && posWithinMapSec == 0
         ? HEAL_LOCATION_EVER_GRANDE_CITY_POKEMON_LEAGUE
         : HEAL_LOCATION_EVER_GRANDE_CITY

   `posWithinMapSec` は「同じ区分のマスが自分より**前**に何個あるか」
   （`GetPositionOfCursorWithinMapSec` が逆順に数える）なので、
   **0 ＝ 上のマス**。こちらの配置表はエメラルドの 28x15 をそのまま
   写しているので、**上のマスは行 HOENN_EVER_GRANDE_LEAGUE_Y**。

   ★ 素 FRLG は「区分 1 つに飛び先 1 つ」しか持てない
     （`gExpandedMapSecFly` は 1 件 3 バイト）。そこで
     `SetFlyWarpDestination`(080C6460) を丸ごと置き換え、
     **サイユウのときだけ回復地点の索引を差し替える**。

   ★ 潰すのは 20 バイト。この関数は 84 バイトあり、入口より後ろに
     BL の飛び先も分岐の飛び先も無いことを確かめてある
     （CLAUDE.md 6.22 / `verify_townmap.py` が毎回見る）。

   ★ 番号は 1 つも手で書いていない。
     `Tables/hoenn_fly.h` を `build_hoenn_fly.py` が、
     `Tables/hoenn_script_flags.h` を `build_hoenn_scripts.py` が出す。
*/

/* 素 FRLG の中身（BPRJ.ld に名前が無いので番地で呼ぶ）。
   `SetFlyWarpDestination` を逆アセンブルして拾った。 */
#define SetWarpDestinationToHealLocation \
    ((void (*)(u8)) (0x08054D2C | 1))
#define SetUsedFlyQuestLogEvent \
    ((void (*)(const u8 *)) (0x08125580 | 1))
#define SetWarpDestinationToMapWarp \
    ((void (*)(s8, s8, s8)) (0x08054C88 | 1))

extern const u8 gExpandedMapSecFly[];
/* BPRJ.ld に番地がある（CLAUDE.md 6.29）。宣言だけこちらで用意する */
void __attribute__((long_call)) ReturnToFieldFromFlyMapSelect(void);

/*
    `functionrewrites` の `SetFlyWarpDestination 080C6460 1 0`。

    素の中身（080C6460〜080C64B2）をそのまま写したうえで、
    サイユウシティのときだけ索引を差し替える。
    ★ 素は `mapsec - 0x58` で引いていたが、その引き算は
      `bytereplacement`（080C6488 を 0 に）で無効にしてあるので、
      **区分をそのまま添字に使う**。
*/
void HoennSetFlyWarpDestination(u16 mapsec)
{
    const u8 *e = &gExpandedMapSecFly[mapsec * 3];
    u8 heal = e[2];
    struct TownMapCursor *cursor;

    /* ★ 2026-09-30 ジョウト（カントーのページ）・シンオウ（6 枚目・シンオウだけの番号）の町は src/region_fly.c の表から */
    if (RegionFlySetWarp(TownMapOnSinnoh() ? MAP_REGION_SINNOH : TownMapOnKitakami() ? MAP_REGION_KITAKAMI : MAP_REGION_JOHTO,
                         (u8) mapsec))
    {
        ReturnToFieldFromFlyMapSelect();
        return;
    }

    if (mapsec == HOENN_MAPSEC_EVER_GRANDE
     && heal == HOENN_HEAL_EVER_GRANDE_CITY
     && FlagGet(HOENN_FLAG_LANDMARK_POKEMON_LEAGUE))
    {
        cursor = sTownMapCursor;
        if (cursor != NULL && cursor->y == HOENN_EVER_GRANDE_LEAGUE_Y)
            heal = HOENN_HEAL_EVER_GRANDE_LEAGUE;
    }

    if (heal != 0)
    {
        SetWarpDestinationToHealLocation(heal);
        SetUsedFlyQuestLogEvent(e);
    }
    else
    {
        SetWarpDestinationToMapWarp((s8) e[0], (s8) e[1], -1);
    }
    ReturnToFieldFromFlyMapSelect();
}
