#include "defines.h"
#include "bw_follower_internal.h"
#include "../include/sprite.h"
#include "../include/decompress.h"
#include "../include/pokemon.h"
#include "../include/event_object_movement.h"
#include "../include/task.h"
#include "../include/palette.h"
#include "../include/field_player_avatar.h"
#include "../include/sound.h"
#include "../include/script.h"
#include "../include/new/follow_me.h"
#include "../include/fieldmap.h"
#include "../include/constants/metatile_behaviors.h"
#include "../include/new/dynamic_ow_pals.h"

/*
    ポケモンの連れ歩き（#16）— 絵のある種族を全部（1054 種）

    設計は docs/research/2026-09-20_follower-charizard.md /
           docs/research/2026-09-20_follower-all-species.md。要点:

    - **エンジンは CFRU の src/follow_me.c がそのまま使える。** こちらが足すのは
      ①絵（表 1〜5）②手持ちの先頭から `gFollowerState` を組み立てること
      ③地図に実体の無い連れ歩きでも `CreateFollowerAvatar` が使えるようにする細工、の 3 つだけ。
    - 絵の表・パレット表は tools/bwfollow/build_bw_follower.py が作る（src/bw_follower_gfx.c）。
    - **枠（EventObjectTemplate）は地図に無い。** CFRU の `CreateFollowerAvatar` は
      `GetEventObjectTemplateByLocalIdAndMap(gFollowerState.map.id, ...)` を引くので、
      localId が 0xFE（DEFAULT_FOLLOWER_LOCAL_ID）のときだけ**こちらの固定の枠**を返す
      （assembly/bwfollow/bw_follower_hooks.s で素の 0805F61C にフック）。
    - パレットは**札で引かれる**ので、素のパレット表（083691E0・18 件）を写して
      種族ぶん足した表に repoints で向け直している。札は 0x2000 + 通し番号。

    ★★ **絵は圧縮してあり、スプライトシートとして VRAM へ展開する。**
    生のまま入れると 3.43 MB で CFRU の挿入枠（空き 1.53 MB）に入らない。圧縮すると 1.07 MB。
    そのため tileTag を持たせた「シート方式」（sprite->usingSheet == TRUE）になり、

      - コマの番号は**画像の番号ではなくタイルのずれ**（32x32 = 16 / 64x64 = 64 の倍数）
      - 地図を作り直すと VRAM の割り当ては消えるので、**毎回積み直す**
        （`BwFollowerUpdateFromParty` が `InitPlayerAvatar` から呼ばれる）
      - `images` は使われないが、**NULL にしてはいけない**。
        素 FRLG の `ObjectEventSetGraphicsId` は `sprite->images->size` を
        usingSheet を見る**前に**読む。見せかけの 1 件を持たせてある
      - 逆に良いこと: `DestroySprite` はシートのタイルを解放しないので、
        CFRU が自転車などで連れ歩きを作り直しても（`SetFollowerSprite`）絵が消えない

    ★ 書き込み可能な static は使えない（6.13）。状態は全部 gFollowerState（0203B818）に入れる。
*/

/* ★★ **なみのり等の旗を落とすと、その技そのものが使えなくなる**（実機 2026-09-20）。
   CFRU は「連れ歩きが付いて来られないなら技を出させない」という作りで、
   `src/overworld.c`（なみのり 2 箇所・たきのぼり・いわくだき相当・ダイビング 2 箇所）と
   `src/party_menu.c`（手持ち画面の技の欄 8 箇所）が
   `!gFollowerState.inProgress || (flags & FOLLOWER_FLAG_CAN_xxx)` で門番をしている。
   旗が無いと**話しかけても何も起きず、手持ち画面にも技が出ない**。
   ポケモンの連れ歩きは全部の地形に付いて来られる想定なので、4 つとも立てる。 */
#define BW_FOLLOWER_FLAGS (FOLLOWER_FLAG_HAS_RUNNING_FRAMES | FOLLOWER_FLAG_CAN_BIKE \
                         | FOLLOWER_FLAG_CAN_LEAVE_ROUTE | FOLLOWER_FLAG_CAN_SURF \
                         | FOLLOWER_FLAG_CAN_WATERFALL | FOLLOWER_FLAG_CAN_DIVE \
                         | FOLLOWER_FLAG_CAN_ROCK_CLIMB)

extern const struct CompressedSpriteSheet gBwFollowerSheets[];
extern const u16 gBwFollowerCount;
extern struct Follower gFollowerState;

/* ★ 64x64 の小分け表は**素のもの（08367958）をそのまま借りる**。
   一度「優先度を上げて建物の前に出す」案を試したが、**建物の裏へ回ったときに
   連れ歩きが屋根の上を歩いて見える**のでユーザー判断で取りやめた（実機 2026-09-20）。
   仕組みは docs/research/2026-09-20_follower-all-species.md に残してある。 */

/* ---- 話しかけたときのスクリプト ----
   ★ NULL にしてはいけない。CFRU の FollowMe_ScriptHook は、連れ歩きの localId に話しかけると
   `gFollowerState.script` をそのまま返す。NULL だとスクリプトの機械が 0 番地を読み、
   BIOS の読めない場所から出てくるゴミを命令として実行して飛ぶ
   （実機 2026-09-20: Jumped to invalid address）。
   ★ 中身は assembly/bwfollow/bw_follower_hooks.s にある（C では書けない。理由はそちらに）。 */
extern const u8 gBwFollowerScript[];


/* ---- 地図に実体の無い連れ歩きのための枠（フックから返す） ----
   絵の番号は `CreateFollowerAvatar` が `gFollowerState.gfxId` で上書きするので、
   ここの値は使われない（表 1 の 0 番を入れてある）。 */
const struct EventObjectTemplate gBwFollowerTemplate = {
    .localId = DEFAULT_FOLLOWER_LOCAL_ID,   /* 0xFE */
    .graphicsIdLowerByte = 0,
    .graphicsIdUpperByte = BW_FOLLOWER_FIRST_TABLE,
    .x = 0,
    .y = 0,
    .elevation = 3,
    .movementType = 0,                      /* 自分では動かない */
    .movementRangeX = 0,
    .movementRangeY = 0,
    .trainerType = 0,
    .trainerRange_berryTreeId = 0,
    .script = gBwFollowerScript,
    .flagId = 0,
};

/* ============================================================================
   ボールに出し入れする演出（なみのり・そらをとぶ・テレポート・あなをほる）

   ★ きっかけ: CFRU の src/follow_me.c は**なみのりもそらをとぶも見ていない**。
     なみのりは連れ歩きに波乗りの台を付け、そらをとぶは何もしないので取り残される
     （実機 2026-09-20）。pokeemerald-expansion は
     `MovementAction_EnterPokeball` / `ExitPokeball` と `HideFollowerForFieldEffect()`
     で解いている。**見た目と間合いはそれに合わせた。**

   ★ 作りだけは変えてある。expansion は**同じ物体の絵をボールに差し替える**が、
     こちらの絵は**シート方式**（tileTag つき）なので `ObjectEventSetGraphicsId` では
     タイルが入れ替わらない。ボールは**別のスプライト**として出し、
     連れ歩き本体は隠すだけにした。見え方は同じになる。

   expansion の間合い（src/event_object_movement.c）:
     しまう … 16 コマ。11 で白くして横に縮め、7 でボールに変わり、0 で消える
     出す   … 16 コマ。ボールが開き、7 で白いポケモンが横に伸び、3 で元の色に戻る

   ★ 状態は `gFollowerState.locked` を借りている。**CFRU は この欄を 1 度も使っていない**
     （`src/follow_me.c` に `.locked` が 0 箇所。検算が毎回見る）。
     書き換え可能な static は使えない（6.13）ので、空いている欄に置くしかない。
   ============================================================================ */
extern bool8 __attribute__((long_call)) FieldEffectActiveListContains(u8 id);
static void BwFollowerLoadOwnPalette(void);
static void BwFollowerCheckShinyPalette(struct EventObject *obj);
static void Task_BwFollowerBall(u8 taskId);
static void BwFollowerReleaseFromBall(struct EventObject *obj);
extern const struct CompressedSpriteSheet gBwFollowerBallSheet;
extern const u16 *const gBwFollowerBallPal;

/* ★★ `FIELD_MOVE_SHOW_MON` は**フィールド技の「ポケモンを出す」演出そのもの**で、
   いあいぎり・いわくだき・かいりき・なみのり・そらをとぶ・あなをほる・テレポート・
   フラッシュ・あまいかおり・たきのぼり・ダイビング・ロッククライムの**全部**が通る。
   これを見ておけば、主人公がボールを投げるしぐさをする間は必ず引っ込められる。
   そらをとぶ等は演出が終わったあとも続く（鳥に乗る・穴を掘る）ので、個別にも見る。 */
#define BW_FLDEFF_FIELD_MOVE_SHOW_MON 6
#define BW_FLDEFF_FIELD_MOVE_SHOW_MON_INIT 59
#define BW_FLDEFF_USE_FLY 31
#define BW_FLDEFF_USE_DIG 38
#define BW_FLDEFF_USE_FLY_ANCIENT_TOMB 60
#define BW_FLDEFF_USE_TELEPORT 63
/* ★ 2026-09-25: ポケモンセンターの回復の演出（機械にボールを並べる）の間もしまう
   （実機「回復するとき連れ歩きポケモンが出っぱなし」）。素の看護師のスクリプトが
   dofieldeffect / waitfieldeffect FLDEFF_POKECENTER_HEAL で待つので、演出が終わったらまた出てくる */
#define BW_FLDEFF_POKECENTER_HEAL 25

#define BW_BALL_TILE_TAG 0x4F11             /* src/bw_follower_gfx.c と同じ値 */
#define BW_BALL_FRAMES 5
#define BW_BALL_TILES_PER_FRAME 8           /* 16x32 = 2x4 タイル */
#define BW_BALL_ANIM_COUNT 8                /* 使うのは 0〜4。こちらしか動かさないので 8 で足りる */
#define BW_BALL_DURATION 16

/* gFollowerState.locked を状態に借りる */
#define BW_BALL_STATE (gFollowerState.locked)
#define BW_BALL_OUT 0                       /* 出ている */
#define BW_BALL_IN 1                        /* ボールの中（こちらが隠した） */
/* ★★ 出す前に待つコマ数。なみのりを降りると CFRU の `PrepareFollowerDismountSurf` が
   ROM のフック（0805C9FC）から呼ばれ、`inProgress` が立っていると `createSurfBlob = GET_OFF`
   を書く。すると次の一歩で `SetSurfDismount()` が走り、**連れ歩きに「台から跳び降りる」
   動きを命じる**。台はもう無いので跳ねたまま浮いて見える（実機 2026-09-20）。
   そのフックが通り過ぎるまで待ってから作り直す。数は `BW_BALL_STATE` 自身で数える
   （1 = ボールの中 / 2〜 = 待っている途中）。 */
#define BW_BALL_WAIT 20

/* 白一色（白く光らせる演出用） */
static const u16 sBwFollowerWhitePal[16] = {
    0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
    0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
};

/* ボールのコマ（値はタイルのずれ） */
#define BALL_ANIM(name, f) \
    static const union AnimCmd name[] = { ANIMCMD_FRAME((f) * BW_BALL_TILES_PER_FRAME, 16), ANIMCMD_FRAME((f) * BW_BALL_TILES_PER_FRAME, 16), \
        ANIMCMD_FRAME((f) * BW_BALL_TILES_PER_FRAME, 16), ANIMCMD_FRAME((f) * BW_BALL_TILES_PER_FRAME, 16), ANIMCMD_JUMP(0) }   /* FACE と同じ（2026-10-07） */
BALL_ANIM(sAnimBall0, 0);
BALL_ANIM(sAnimBall1, 1);
BALL_ANIM(sAnimBall2, 2);
BALL_ANIM(sAnimBall3, 3);
BALL_ANIM(sAnimBall4, 4);

static const union AnimCmd *const sBallAnims[BW_BALL_ANIM_COUNT] = {
    sAnimBall0, sAnimBall1, sAnimBall2, sAnimBall3, sAnimBall4,
    sAnimBall0, sAnimBall0, sAnimBall0,
};

static const struct SpriteTemplate sBallTemplate = {
    .tileTag = BW_BALL_TILE_TAG,
    .paletteTag = 0xFFFF,                   /* パレットはこちらで置き場へ積む */
    .oam = (const struct OamData *)0x08367798,      /* 素の 16x32 の OAM（形 2・大きさ 2） */
    .anims = sBallAnims,
    .images = NULL,                         /* シート方式なので読まれない */
    .affineAnims = (const union AffineAnimCmd *const *)0x081F11F0,
    .callback = SpriteCallbackDummy,
};

/* ★ 横だけ伸び縮みさせる。値は expansion の sAffineAnim_Pokeball* と同じ。
   右向きが左右反転で作られている種族は、アフィンを入れると反転の指定が効かなくなるので
   **負の倍率**で反転させる（expansion も同じ手）。 */
static const union AffineAnimCmd sAffine_BallExit[] = {
    AFFINEANIMCMD_FRAME(0x40, 0x100, 0, 0), AFFINEANIMCMD_FRAME(0x80, 0x100, 0, 0),
    AFFINEANIMCMD_FRAME(0xC0, 0x100, 0, 0), AFFINEANIMCMD_FRAME(0x100, 0x100, 0, 0),
    AFFINEANIMCMD_END,
};
static const union AffineAnimCmd sAffine_BallExitEast[] = {
    AFFINEANIMCMD_FRAME(0xFFC0, 0x100, 0, 0), AFFINEANIMCMD_FRAME(0xFF80, 0x100, 0, 0),
    AFFINEANIMCMD_FRAME(0xFF40, 0x100, 0, 0), AFFINEANIMCMD_FRAME(0xFF00, 0x100, 0, 0),
    AFFINEANIMCMD_END,
};
static const union AffineAnimCmd sAffine_BallEnter[] = {
    AFFINEANIMCMD_FRAME(0x100, 0x100, 0, 0), AFFINEANIMCMD_FRAME(0xC0, 0x100, 0, 0),
    AFFINEANIMCMD_FRAME(0x80, 0x100, 0, 0), AFFINEANIMCMD_FRAME(0x40, 0x100, 0, 0),
    AFFINEANIMCMD_END,
};
static const union AffineAnimCmd sAffine_BallEnterEast[] = {
    AFFINEANIMCMD_FRAME(0xFF00, 0x100, 0, 0), AFFINEANIMCMD_FRAME(0xFF40, 0x100, 0, 0),
    AFFINEANIMCMD_FRAME(0xFF80, 0x100, 0, 0), AFFINEANIMCMD_FRAME(0xFFC0, 0x100, 0, 0),
    AFFINEANIMCMD_END,
};
static const union AffineAnimCmd *const sBallAffineAnims[] = {
    sAffine_BallExit, sAffine_BallExitEast, sAffine_BallEnter, sAffine_BallEnterEast,
};

#define tTimer data[0]
#define tBallSprite data[1]
#define tAffine data[2]
#define tEnter data[3]

/* ---- 出してよい場所か ------------------------------------------------
   ★★ **HGSS は地図ごとに 3 段階**を持っている（`MapHeader.followMode`）:
       禁止 / 大きいポケモンだけ禁止 / 許可。
       pokeheartgold の全 540 枚を数えた結果（2026-09-20）:

         屋内(INTERIOR) 326 枚 -> 大きいのだけ禁止 292 / 一切禁止 34
         洞窟(CAVE)     116 枚 -> 許可 87 / 大きいのだけ禁止 27 / 一切禁止 2
         道路・街       94 枚 -> ほぼ許可

       つまり「**屋内では大きいポケモンを出さない**」が本体で、
       それとは別に「一切出さない地図」が 42 枚ある。ここではその両方を入れる。
       ★ ポケモンリーグは HGSS でも**出る**（四天王もワタルの部屋も「大きいのだけ禁止」）。
         リーグ関連で一切禁止なのは**殿堂入りの部屋だけ**。 */

/* 一切出さない地図 */
#define BW_GROUP_LINK            0      /* コロシアム・交換・記録・ユニオンルーム（5 枚とも）*/
#define BW_GROUP_DUNGEONS        1
#define BW_MAP_HALL_OF_FAME     80      /*   群 1 の 80 = 殿堂入りの部屋（HGSS も禁止）*/
#define BW_GROUP_SPECIAL         2
#define BW_MAP_TRAINER_TOWER_LO  1      /*   群 2 の 1〜11 = トレーナータワー（バトル施設）*/
#define BW_MAP_TRAINER_TOWER_HI 11
#define BW_GROUP_HOENN_SPECIAL  69      /* ホウエンの特別地区 */
#define BW_MAP_FRONTIER_LO       4      /*   群 69 の 4〜55 = バトルフロンティア */
#define BW_MAP_FRONTIER_HI      55

/* ★★ シートの大きさで 64x64 を見分ける。**「3072 より大きい」では駄目。**
   32x32 は 6 コマ(3072) と **8 コマ(4096・左右が別の種族)** の 2 通りあるので、
   3072 を敷居にすると 8 コマの 32x32 まで「大きい」と誤判定する
   （2026-09-20 に検算が捕まえた）。64x64 は 6 コマ x 2048 = 12288 の 1 通りだけ。 */
#define BW_SHEET_SIZE_64     12288

static bool8 BwFollowerBannedHere(void)
{
    u8 group = gSaveBlock1->location.mapGroup;
    u8 num = gSaveBlock1->location.mapNum;

    if (group == BW_GROUP_LINK)
        return TRUE;
    /* ★ 2026-09-23: 水中（ダイビング）では出さない。CFRU の follow_me.c は水中で
       FOLLOWER_SPRITE_INDEX_UNDERWATER（人の NPC 用の水中の絵）へ差し替えるが、
       ポケモンの連れ歩きにはその絵が無い。なみのりと同じくボールの中にいる扱い。
       ★ 素 FRLG には水中の地図が無く、ホウエンの海中 14 枚だけが種別 5 */
    if (gMapHeader.mapType == MAP_TYPE_UNDERWATER)
        return TRUE;
    if (group == BW_GROUP_DUNGEONS && num == BW_MAP_HALL_OF_FAME)
        return TRUE;
    if (group == BW_GROUP_SPECIAL
     && num >= BW_MAP_TRAINER_TOWER_LO && num <= BW_MAP_TRAINER_TOWER_HI)
        return TRUE;
    if (group == BW_GROUP_HOENN_SPECIAL
     && num >= BW_MAP_FRONTIER_LO && num <= BW_MAP_FRONTIER_HI)
        return TRUE;

    return FALSE;
}

/* 屋内では 64x64 の種族を出さない（HGSS の「大きいのだけ禁止」）。
   ★ これは残タスクの「64x64 が建物に食われる」への本家と同じ答えでもある。 */
static bool8 BwFollowerTooBigHere(u16 gfxId)
{
    if (gMapHeader.mapType != MAP_TYPE_INDOOR
     && gMapHeader.mapType != MAP_TYPE_SECRET_BASE)
        return FALSE;

    if (BwFollowerIndexOf(gfxId) >= gBwFollowerCount)
        return FALSE;   /* Moémon の連れ歩きは 32x32 */
    return gBwFollowerSheets[BwFollowerIndexOf(gfxId)].size >= BW_SHEET_SIZE_64;
}

static u16 BwFollowerWantedGfxId(void)
{
    struct Pokemon *mon = BwFollowerLeadMon();
    u16 gfxId;

    if (mon == NULL)
        return 0;

    /* ★ HGSS も「最初の生きている子」を選んだあとで絵があるか見る。無ければ連れ歩かない */
    gfxId = BwFollowerGfxIdForMon(mon);
    if (gfxId == 0 || !BwFollowerValidIndex(BwFollowerIndexOf(gfxId)))
        return 0;

    /* ★ 場所の制限はここで見る。**呼び元 3 箇所すべてに効く。**
       出ている最中に禁止の地図へ入ったら、呼び元が**ボールへしまう**動きをしてくれる。 */
    if (BwFollowerBannedHere() || BwFollowerTooBigHere(gfxId))
        return 0;

    return gfxId;
}

/* タイルを積み直す。**地図を作り直すと割り当ては消えるので毎回呼ぶ**
   ★ 積めなかったら FALSE。64x64 は 384 タイル要るので、遊んでいる最中だと
   VRAM の空きが足りないことがある。そのまま出すと**絵がゴミになる**ので出さない。 */
/* ★★ メニューなどで画面を作り直されるとタイルの割り当てが消える。
   こちらの BW 風スタートメニューは `ResetSpriteData()` と `FreeAllSpritePalettes()` を呼ぶので、
   戻ってきたとき連れ歩きの**シートの札が登録されていない**（`GetSpriteTileStartByTag` が 0xFFFF）。
   素の物体の作り直しはシートを積み直してくれないので、**ゴミのタイルが出る**
   （実機 2026-09-20「メニューを開くと連れ歩きの絵が壊れる」）。
   毎コマ見て、消えていたら積み直して貼り直す。パレットも一緒に戻す。 */
/* 積み直したタイルの先頭を、いま出ているスプライトに貼り直す */
static void BwFollowerRebindSheet(struct EventObject *obj)
{
    struct Sprite *sprite = &gSprites[obj->spriteId];

    sprite->sheetTileStart = GetSpriteTileStartByTag(BW_FOLLOWER_TILE_TAG);
    SetSpriteSheetFrameTileNum(sprite);
    BwFollowerLoadOwnPalette();
}

static void BwFollowerRepairSheet(struct EventObject *obj)
{
    if (GetSpriteTileStartByTag(BW_FOLLOWER_TILE_TAG) != 0xFFFF)
        return;     /* 積んである */

    if (!BwFollowerLoadSheet(gFollowerState.gfxId))
        return;     /* VRAM が足りない。次のコマでやり直す */

    BwFollowerRebindSheet(obj);
}

/* ★ inProgress は見ない。ボールの中では FALSE にしてあるため。
   ★ objId が 0（＝主人公の物体）のときは必ず弾く。**ここを踏むと主人公を消す。** */
static struct EventObject *BwFollowerObject(void)
{
    struct EventObject *obj;

    if (gFollowerState.map.id != DEFAULT_FOLLOWER_LOCAL_ID)
        return NULL;
    if (gFollowerState.objId == 0 || gFollowerState.objId >= EVENT_OBJECTS_COUNT)
        return NULL;

    obj = &gEventObjects[gFollowerState.objId];
    /* ★ 2026-10-03 ユーザー「ニビの青年に案内されると、連れ歩きが置き去り → ジムの西に謎の女が現れて付いてくる」:
       置き去りの連れ歩きは画面の外で素の RemoveObjectEventsOutsideView に消され、空いた枠に町の人が入る。
       CFRU は objId しか覚えていないので、その町の人を連れ歩きとして動かしていた → localId も確かめる */
    return (obj->active && obj->localId == DEFAULT_FOLLOWER_LOCAL_ID) ? obj : NULL;
}

/* ★ 2026-10-03 ユーザー「ついていくことはできない？」（ニビの青年の案内のように、スクリプトが主人公だけを歩かせる場面）:
   CFRU の FollowMe は主人公が自分で歩いた一歩でしか連れ歩きを動かさない。スクリプトの最中は、主人公が 1 マス動いたら
   連れ歩きを主人公が直前にいたマス（previousCoords）へ歩かせる。連れ歩きがまだ歩いている途中・離れすぎ（主人公の方が速い）なら
   何もしない（スクリプトが終わってから BwFollowerPlaceBehindPlayer で戻す） */
static void BwFollowerFollowScriptedStep(struct EventObject *obj)
{
    struct EventObject *player = &gEventObjects[gPlayerAvatar->eventObjectId];
    s16 dx = player->previousCoords.x - obj->currentCoords.x;
    s16 dy = player->previousCoords.y - obj->currentCoords.y;
    u8 dir;

    if (player->previousCoords.x == player->currentCoords.x && player->previousCoords.y == player->currentCoords.y)
        return;                                 /* 主人公は動いていない */
    if (dx == 0 && dy == 0)
        return;                                 /* もう直前のマスにいる */
    if (obj->singleMovementActive || (obj->heldMovementActive && !obj->heldMovementFinished))
        return;                                 /* 連れ歩きが歩いている途中 */
    if (dx == 1 && dy == 0)       dir = DIR_EAST;
    else if (dx == -1 && dy == 0) dir = DIR_WEST;
    else if (dx == 0 && dy == 1)  dir = DIR_SOUTH;
    else if (dx == 0 && dy == -1) dir = DIR_NORTH;
    else
        return;                                 /* 隣でない（追いつけていない） */

    EventObjectClearHeldMovement(obj);
    EventObjectSetHeldMovement(obj, GetWalkNormalMovementAction(dir));
}

/* ★ 2026-10-03 ユーザーの画面（おつきみやまの化石の場面）「イベント時に連れ歩きポケモンが人物と重なる　かさならないように
   ポケモンがうごくようにできる？」: スクリプトの applymovement は人どうしのぶつかりを見ないので、歩いてきた人が連れ歩きのマスに
   重なる。スクリプトの最中に、主人公・連れ歩き以外の人が連れ歩きと同じマスにいたら（歩き出した時点で currentCoords は行き先）、
   となりの空いているマス（素の GetCollisionAtCoords が 0＝壁・段差・人がいない）へ早足でよける。
   ★ よける向きは 下・左・右・上 の順。主人公のマスへは入らない（人がいるので当たり判定で外れる） */
static void BwFollowerAvoidOverlap(struct EventObject *obj)
{
    static const u8 sDirs[] = {DIR_SOUTH, DIR_WEST, DIR_EAST, DIR_NORTH};
    bool8 overlap = FALSE;
    u32 i;

    if (obj->singleMovementActive || (obj->heldMovementActive && !obj->heldMovementFinished))
        return;                                 /* 歩いている途中 */
    for (i = 0; i < EVENT_OBJECTS_COUNT; ++i)
    {
        struct EventObject *o = &gEventObjects[i];

        if (!o->active || o == obj || o->isPlayer || o->invisible)
            continue;
        if (o->currentCoords.x == obj->currentCoords.x && o->currentCoords.y == obj->currentCoords.y)
        {
            overlap = TRUE;
            break;
        }
    }
    if (!overlap)
        return;

    for (i = 0; i < ARRAY_COUNT(sDirs); ++i)
    {
        s16 x = obj->currentCoords.x;
        s16 y = obj->currentCoords.y;

        MoveCoords(sDirs[i], &x, &y);
        if (GetCollisionAtCoords(obj, x, y, sDirs[i]) == 0)
        {
            EventObjectClearHeldMovement(obj);
            EventObjectSetHeldMovement(obj, GetWalkFastMovementAction(sDirs[i]));
            return;
        }
    }
}

/* ★ 2026-10-03 画面の外へ出ても連れ歩きは消さない。素の RemoveObjectEventIfOutsideView（0805E630・functionrewrites で置き換え）と
   同じ判定（左 = x - 2・右 = x + 17・上 = y・下 = y + 16。今の位置も最初の位置も外なら消す）で、連れ歩きだけは消さない。
   ユーザーの動画（ニビの青年の案内）: 置き去りの連れ歩きがカメラの移動で消され、主人公がそこへ戻るまで出てこなかった */
static bool8 BwInsideView(struct Coords16 c, s16 left, s16 right, s16 top, s16 bottom)
{
    return c.x >= left && c.x <= right && c.y >= top && c.y <= bottom;
}

/* ★ 2026-10-04 ユーザーの動画「オリジンフォルムのディアルガとパルキアの連れ歩きポケモンがバトル後にグラフィックがくずれる」:
   素の SetObjectEventDynamicGraphicsId（0805EBB8・戦闘から戻って物体を作り直すときなどに通る）は、絵の番号の下位バイトが 0xF0 以上なら
   上位バイト（CFRU の表の番号）を見ずに「変数で決める絵」と見て、下位バイトを VAR_OBJ_GFX_ID_x の値に書き換えていた。
   連れ歩きの絵は表 1〜5 なので、通し番号の下位バイトが 0xF0〜0xFF の種族（759 オリジンディアルガ・760 オリジンパルキアなど）だけ
   別の絵（32x32）の形になり、64x64 のタイルを切れ端で出していた → 変数で決めるのは表 0 だけ（functionrewrites） */
void BwFollowerSetDynamicGraphicsId(struct EventObject *obj)
{
    if (obj->graphicsIdUpperByte == 0 && obj->graphicsIdLowerByte >= 0xF0)
        obj->graphicsIdLowerByte = VarGetEventObjectGraphicsId(obj->graphicsIdLowerByte - 0xF0);
}

void BwFollowerRemoveIfOutsideView(struct EventObject *obj)
{
    s16 left = gSaveBlock1->pos.x - 2;
    s16 right = gSaveBlock1->pos.x + 17;
    s16 top = gSaveBlock1->pos.y;
    s16 bottom = gSaveBlock1->pos.y + 16;

    if (BwInsideView(obj->currentCoords, left, right, top, bottom)
     || BwInsideView(obj->initialCoords, left, right, top, bottom))
        return;
    if (gFollowerState.inProgress && gFollowerState.map.id == DEFAULT_FOLLOWER_LOCAL_ID
     && gFollowerState.objId < EVENT_OBJECTS_COUNT && obj == &gEventObjects[gFollowerState.objId]
     && obj->localId == DEFAULT_FOLLOWER_LOCAL_ID)
        return;
    RemoveEventObject(obj);
}

/* 主人公も連れ歩きも止まっていて、スクリプトも動いていない（置き直してよい） */
static bool8 BwFollowerCanRepositionNow(struct EventObject *obj)
{
    if (ScriptContext2_IsEnabled())
        return FALSE;
    if (gPlayerAvatar->tileTransitionState != T_NOT_MOVING || gPlayerAvatar->runningState != NOT_MOVING)
        return FALSE;
    if (obj != NULL && (obj->singleMovementActive || (obj->heldMovementActive && !obj->heldMovementFinished)))
        return FALSE;
    return TRUE;
}

/* ★★ 2026-10-03 ユーザーの画面「コノヨザルの連れ歩きパレットがおかしい」（体が桃色・ふちが水色）:
   連れ歩きは色を**置き場 10 に決め打ち**で書いていたが、CFRU は物体の色を**札ごとに空いている置き場へ**割り振る
   （src/dynamic_ow_pals.c の sPalRefs・6.36）。連れ歩きの絵そのものは割り振られた置き場 X を指し、
   こちらの書き込みは 10 へ行く。メニューから戻ったとき・入れ替えのときなどに X が別の人の色になっても直らず、
   連れ歩きがその人の色で出ていた（逆に 10 を使っている人の色を連れ歩きの色で潰すこともあった）。
   → 主人公（src/bw_player_pal.c）と同じく、**札で置き場を引き（無ければ CFRU に割り振らせ）、絵をそこへ向ける**。
     ボールの絵・白く光る色も同じ置き場を使う（演出の間は連れ歩きとボールだけがその置き場を使う） */
static u8 BwFollowerPalSlot(void)
{
    u16 tag = BW_PAL_TAG_BASE + BwFollowerIndexOf(gFollowerState.gfxId);
    u8 slot = FindPalRef(PalTypeNPC, tag);

    if (slot == 0xFF)
        slot = FindOrLoadNPCPalette(tag);   /* 色も積まれる・数を 1 つ（物体を消すとき CFRU が減らす） */
    if (slot >= 16)
        slot = BW_FOLLOWER_PAL_SLOT;        /* 置き場が足りないときだけ元のやり方 */
    return slot;
}

/* 連れ歩きの絵が、札の置き場を指すようにする（毎コマ・CFRU の数は増やさない） */
static void BwFollowerFixPaletteSlot(struct EventObject *obj)
{
    struct Sprite *sprite = &gSprites[obj->spriteId];
    u8 slot;

    if (!BwFollowerValidIndex(BwFollowerIndexOf(gFollowerState.gfxId)) || FuncIsActiveTask(Task_BwFollowerBall))
        return;
    slot = BwFollowerPalSlot();
    if (sprite->oam.paletteNum != slot)
    {
        sprite->oam.paletteNum = slot;
        PatchObjectPalette(BW_PAL_TAG_BASE + BwFollowerIndexOf(gFollowerState.gfxId), slot);
    }
}

static void BwFollowerLoadOwnPalette(void)
{
    struct EventObject *obj = BwFollowerObject();
    u8 slot = BwFollowerPalSlot();

    PatchObjectPalette(BW_PAL_TAG_BASE + BwFollowerIndexOf(gFollowerState.gfxId), slot);
    if (obj != NULL)
        gSprites[obj->spriteId].oam.paletteNum = slot;
}

static void BwFollowerAffineOn(struct Sprite *sprite, u8 anim)
{
    sprite->affineAnims = sBallAffineAnims;
    sprite->oam.affineMode = 1;             /* ST_OAM_AFFINE_NORMAL（大きさは変えない） */
    InitSpriteAffineAnim(sprite);
    StartSpriteAffineAnim(sprite, anim);
}

static void BwFollowerAffineOff(struct Sprite *sprite)
{
    sprite->affineAnimEnded = TRUE;
    FreeSpriteOamMatrix(sprite);
    sprite->oam.affineMode = 0;             /* ST_OAM_AFFINE_OFF */
}

static u8 BwFollowerMakeBall(struct Sprite *follower, u8 frame)
{
    u8 id;
    struct Sprite *ball;

    FreeSpriteTilesByTag(BW_BALL_TILE_TAG);
    LoadCompressedSpriteSheetUsingHeap(&gBwFollowerBallSheet);
    if (GetSpriteTileStartByTag(BW_BALL_TILE_TAG) == 0xFFFF)
        return 0xFF;
    LoadPalette(gBwFollowerBallPal, 0x100 + follower->oam.paletteNum * 16, 32);

    /* ボールの足元を連れ歩きの足元に合わせる（16x32 なので中心は下端の 16 画素上） */
    id = CreateSprite(&sBallTemplate, follower->pos1.x,
                      follower->pos1.y - follower->centerToCornerVecY - 16, follower->subpriority);
    if (id >= MAX_SPRITES)
        return 0xFF;

    ball = &gSprites[id];
    ball->oam.paletteNum = follower->oam.paletteNum;   /* 連れ歩きと同じ置き場（演出の間だけボールの色） */
    ball->oam.priority = follower->oam.priority;
    ball->coordOffsetEnabled = TRUE;
    StartSpriteAnim(ball, frame);
    return id;
}

static void BwFollowerKillBall(struct Task *task)
{
    if (task->tBallSprite != 0xFF)
    {
        DestroySprite(&gSprites[task->tBallSprite]);
        task->tBallSprite = 0xFF;
    }
    FreeSpriteTilesByTag(BW_BALL_TILE_TAG);
}

static void Task_BwFollowerBall(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    struct EventObject *obj = BwFollowerObject();
    struct Sprite *follower;
    u8 t;

    if (obj == NULL)
    {
        BwFollowerKillBall(task);
        DestroyTask(taskId);
        return;
    }

    follower = &gSprites[obj->spriteId];
    t = --task->tTimer;

    if (task->tBallSprite != 0xFF)
    {
        /* 途中で主人公が動いても置いて行かれないように、毎コマ位置を合わせる */
        gSprites[task->tBallSprite].pos1.x = follower->pos1.x;
        gSprites[task->tBallSprite].pos1.y = follower->pos1.y - follower->centerToCornerVecY - 16;
    }

    if (!task->tEnter)
        gFollowerState.createSurfBlob = 0;  /* 作り直した個体に台は無い。演出の間だけ押さえる */

    if (task->tEnter)
    {
        /* しまう */
        if (t == 11)
        {
            LoadPalette(sBwFollowerWhitePal, 0x100 + follower->oam.paletteNum * 16, 32);
            BwFollowerAffineOn(follower, 2 + (task->tAffine & 1));
        }
        else if (t == 7)
        {
            BwFollowerAffineOff(follower);
            follower->pos2.x = 0;           /* 水へ跳びかけていた残りを消す */
            follower->pos2.y = 0;
            obj->invisible = TRUE;
            task->tBallSprite = BwFollowerMakeBall(follower, 4);
            PlaySE(0x10);                   /* SE_POKE_BALL_CLOSE */
        }
        else if (t < 7 && t >= 2 && task->tBallSprite != 0xFF)
        {
            StartSpriteAnim(&gSprites[task->tBallSprite], t - 2);
        }
        else if (t == 0)
        {
            BwFollowerKillBall(task);
            BwFollowerLoadOwnPalette();
            /* ★★ **物体ごと消して CFRU の管理から外す。**
               隠すだけだと「見えない壁」が残るうえ、なみのりの状態機械
               （createSurfBlob）が中途半端に動いて主人公を巻き込む。
               expansion も `RemoveFollowingPokemon()` で物体ごと消している。 */
            /* ★ 波乗りの台が付いていたら一緒に片づける。
               残すと CFRU の降りる処理が「すでに無い台」を触りに行く */
            if (gFollowerState.createSurfBlob != 0 && obj->fieldEffectSpriteId != 0)
            {
                DestroySprite(&gSprites[obj->fieldEffectSpriteId]);
                obj->fieldEffectSpriteId = 0;
            }
            DestroySprite(&gSprites[obj->spriteId]);
            RemoveEventObject(obj);
            gFollowerState.createSurfBlob = 0;
            gFollowerState.inProgress = FALSE;      /* 始めに落としてあるが念のため */
            BW_BALL_STATE = BW_BALL_IN;
            DestroyTask(taskId);
        }
    }
    else
    {
        /* 出す */
        if (t == 15)
        {
            task->tBallSprite = BwFollowerMakeBall(follower, 0);
            PlaySE(0x0F);                   /* SE_POKE_BALL_OPEN */
        }
        else if (t >= 11 && t <= 14 && task->tBallSprite != 0xFF)
        {
            StartSpriteAnim(&gSprites[task->tBallSprite], 15 - t);
        }
        else if (t == 9)
        {
            BwFollowerKillBall(task);
            obj->invisible = FALSE;
            LoadPalette(sBwFollowerWhitePal, 0x100 + follower->oam.paletteNum * 16, 32);
            BwFollowerAffineOn(follower, task->tAffine & 1);
        }
        else if (t == 5)
        {
            BwFollowerAffineOff(follower);
            BwFollowerLoadOwnPalette();
        }
        else if (t == 0)
        {
            follower->pos2.x = 0;           /* 跳ねの残りよけ（念のためもう一度） */
            follower->pos2.y = 0;
            BwFollowerLoadOwnPalette();
            BW_BALL_STATE = BW_BALL_OUT;
            DestroyTask(taskId);
        }
    }
}

/* ★★ 2026-10-03 ユーザー「ポケセンで…回復してもらった後、ボールから出てくるタイミングでメニュー開くとポケモンが爆発四散します」。
   BW のスタートメニューは全画面（ResetSpriteData）なのに、ボールの演出のタスクはメニューの間も動き続け、
   メニューの絵や戻ったあと作り直した絵を、古い番号のまま伸び縮み（affine）させていた。
   → メニューを開く直前（src/bw_start_menu.c の BwShowStartMenu）に、演出を最後まで一気に進めて終わらせる
     （同じタスクの処理をコマの数だけ続けて呼ぶ＝しまう／出すの終わりの形は普段と同じ）。 */
void BwFollower_FinishBallNow(void)
{
    u8 taskId = FindTaskIdByFunc(Task_BwFollowerBall);
    u8 guard;

    for (guard = 0; taskId != 0xFF && guard < 64; ++guard)
    {
        Task_BwFollowerBall(taskId);
        if (!FuncIsActiveTask(Task_BwFollowerBall))
            break;
    }
}

/* ★★ ボールから出すとき、連れ歩きを**主人公のうしろのマス**に置き直す（2026-09-25）。

   実機「回復が終わったあと出てくる場所がおかしい（右下）。移動すればうしろに瞬間移動する」。
   CFRU の `CreateFollowerAvatar()`（src/follow_me.c）は
       TrySpawnEventObjectTemplate(&clone, mapNum, mapGroup, clone.x, clone.y)
   と、**最後の 2 つ（素ではカメラのずれ）に主人公のマスの座標（-7）を渡している**。
   素 FRLG の TrySetupObjectEventSprite は `currentCoords + cameraX` から絵の位置を出すので、
   絵が「主人公の座標 × 16 画素」だけ右下にずれる（マスそのものは主人公の位置で正しい）。
   ワープ直後は見えない状態で作り、一歩目で CFRU が置き直す（warpEnd）ので見えなかった。
   ボールから出すときはすぐ見せるので、ずれが見える。外の地図では画面外に出ていたはず（推測）。

   ここでは `MoveEventObjectToMapCoords`（絵の位置も正しく計算し直す）で置き直す。
   ・うしろのマスが通れる → そこへ置き、主人公と同じ向きにし、warpEnd を下ろす
     （次の一歩から CFRU の普通のついて来かた。DetermineFollowerDirection がうしろから追う）
   ・通れない → 主人公と同じマスに置き直すだけ（warpEnd は立てたまま。今までどおり一歩目で追いつく）
   ★ 当たり判定は素の GetCollisionAtCoords（回転門の hooks も通る）。人・壁・段差を見る。 */
static void BwFollowerPlaceBehindPlayer(bool8 ontoPlayerIfBlocked)
{
    struct EventObject *player = &gEventObjects[gPlayerAvatar->eventObjectId];
    struct EventObject *obj = BwFollowerObject();
    u8 back;
    s16 x, y;

    if (obj == NULL)
        return;

    switch (player->facingDirection)
    {
    case DIR_NORTH: back = DIR_SOUTH; break;
    case DIR_SOUTH: back = DIR_NORTH; break;
    case DIR_WEST:  back = DIR_EAST;  break;
    case DIR_EAST:  back = DIR_WEST;  break;
    default:        back = DIR_NORTH; break;
    }

    x = player->currentCoords.x;
    y = player->currentCoords.y;
    MoveCoords(back, &x, &y);

    /* ★ 2026-10-04 置き直したら「次の一歩で跳ぶ」の残り（CFRU の delayedState）は捨てる（残すと置き直した所から 2 マス跳んで
       主人公に重なった・ユーザーの動画「連れ歩きポケモンの挙動がおかしい」） */
    if (GetCollisionAtCoords(obj, x, y, back) == 0)
    {
        MoveEventObjectToMapCoords(obj, x, y);
        EventObjectTurn(obj, player->facingDirection);
        gFollowerState.warpEnd = FALSE;
        gFollowerState.delayedState = 0;
    }
    else if (ontoPlayerIfBlocked)
    {
        MoveEventObjectToMapCoords(obj, player->currentCoords.x, player->currentCoords.y);
        gFollowerState.delayedState = 0;
    }
    /* ★ 2026-10-04 遠くの連れ歩きの置き直しで うしろが通れない（段差を跳び降りた直後は うしろが段差）ときは動かさない。
       前は主人公と同じマスへ動かしていて、主人公の下に隠れて見えなくなった（ユーザーの動画） */
}

/* ★ 2026-10-03 ワープのあと見えないまま待っている連れ歩きを、主人公が止まっているうちに出す。
   ユーザーの動画（ポケモンセンターから出たあと）: v0.9 は BwFollowerPlaceBehindPlayer を使っていたので、
   うしろが通れないと主人公と同じマスへ動かしたまま見えないままにしていた → 一歩目で CFRU の「ドアから出てくる」演出
   （Task_FollowerOutOfDoor・連れ歩きの今の位置から出る）が違う所から始まり、横や斜めを歩いた。ドアの印も残っていた。
   ・ドアから出た（comeOutDoorStairs == 1）: 主人公が出てきたマス（previousCoords・隣）に置いて見せる（ドアの中から出てきた形）
   ・ほか: うしろのマスが通れるときだけ置いて見せる
   ・どちらも置けなければ何もしない（今までどおり一歩目で CFRU が出す） */
/* ★ 2026-10-08 ドアから出たときに横のマスで出せるか: マス目が通れて、ほかの物（人・主人公）がいない。
   GetCollisionAtCoords は見えないままの連れ歩きの高さで判定して、通れる床でも「通れない」になった（mGBA） */
bool8 __attribute__((long_call)) MapGridIsImpassableAt(s16 x, s16 y);

static bool8 BwFollowerTileFree(struct EventObject *self, s16 x, s16 y)
{
    u32 i;

    if (MapGridIsImpassableAt(x, y))
        return FALSE;
    for (i = 0; i < EVENT_OBJECTS_COUNT; i++)
    {
        struct EventObject *o = &gEventObjects[i];
        if (o->active && o != self && o->currentCoords.x == x && o->currentCoords.y == y)
            return FALSE;
    }
    return TRUE;
}

/* 主人公の横（向きに対して左・右）の空いているマスを *x, *y に。無ければ FALSE */
static bool8 BwFollowerSideTile(struct EventObject *obj, struct EventObject *player, s16 *x, s16 *y)
{
    u8 sides[2];
    u32 i;

    switch (player->facingDirection)
    {
    case DIR_NORTH:
    case DIR_SOUTH: sides[0] = DIR_WEST;  sides[1] = DIR_EAST;  break;
    default:        sides[0] = DIR_NORTH; sides[1] = DIR_SOUTH; break;
    }
    for (i = 0; i < 2; i++)
    {
        s16 sx = player->currentCoords.x, sy = player->currentCoords.y;
        MoveCoords(sides[i], &sx, &sy);
        if (BwFollowerTileFree(obj, sx, sy))
        {
            *x = sx;
            *y = sy;
            return TRUE;
        }
    }
    return FALSE;
}

static void BwFollowerAppearAfterWarp(struct EventObject *obj)
{
    struct EventObject *player = &gEventObjects[gPlayerAvatar->eventObjectId];
    s16 x, y;

    if (gFollowerState.comeOutDoorStairs == 1)
    {
        s16 dx = player->previousCoords.x - player->currentCoords.x;
        s16 dy = player->previousCoords.y - player->currentCoords.y;
        /* ★ 2026-10-08 ユーザー「つれあるきがドアでたときにドアとかさなる」: ドアのマスで出すと絵がドアと家の壁に重なる。
           まず主人公の左・右のマス（主人公の向きに対して横・通れるほう）で出す。どちらも通れなければ今までどおりドアのマス */
        if (BwFollowerSideTile(obj, player, &x, &y))
            goto PLACE;

        if ((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) == 1)
        {
            x = player->previousCoords.x;       /* 出てきたマス（ドア） */
            y = player->previousCoords.y;
        }
        else
        {
            /* ★ 2026-10-04 止まったあと（previousCoords が今の所と同じになった）も、うしろ（ドアの側）のマスで出す。
               前はここで帰っていて、一歩目まで CFRU の「ドアから歩いて出てくる」を待っていた（ユーザー「ボールからでてくるように」） */
            u8 back;

            switch (player->facingDirection)
            {
            case DIR_NORTH: back = DIR_SOUTH; break;
            case DIR_WEST:  back = DIR_EAST;  break;
            case DIR_EAST:  back = DIR_WEST;  break;
            default:        back = DIR_NORTH; break;
            }
            x = player->currentCoords.x;
            y = player->currentCoords.y;
            MoveCoords(back, &x, &y);
        }
    }
    else
    {
        u8 back;

        switch (player->facingDirection)
        {
        case DIR_NORTH: back = DIR_SOUTH; break;
        case DIR_SOUTH: back = DIR_NORTH; break;
        case DIR_WEST:  back = DIR_EAST;  break;
        case DIR_EAST:  back = DIR_WEST;  break;
        default:        back = DIR_NORTH; break;
        }
        x = player->currentCoords.x;
        y = player->currentCoords.y;
        MoveCoords(back, &x, &y);
        /* ★ 2026-10-08 うしろがドア（ドアから出た直後でまだ合図が立っていないとき・mGBA でこちらを通っていた）なら横のマスを先に */
        if (MapGridGetMetatileBehaviorAt(x, y) == MB_WARP_DOOR && BwFollowerSideTile(obj, player, &x, &y))
            goto PLACE;
        if (GetCollisionAtCoords(obj, x, y, back) != 0)
            return;
    }

PLACE:
    MoveEventObjectToMapCoords(obj, x, y);
    EventObjectTurn(obj, player->facingDirection);
    gFollowerState.warpEnd = FALSE;
    gFollowerState.delayedState = 0;
    /* ★ 2026-10-04 ユーザー「ドアからでたときやポケセンから出た時の動きを…ボールからでてくるようにしてほしい」:
       ドア（ポケセンも）から出たときは、主人公のうしろのマスで見えないまま置いて、ボールから出す演出（Task_BwFollowerBall の「出す」）。
       ほかのワープ（階段・洞窟の出入り口）は今までどおり その場で見せる */
    if (gFollowerState.comeOutDoorStairs == 1)
    {
        gFollowerState.comeOutDoorStairs = FALSE;
        BwFollowerReleaseFromBall(obj);
        return;
    }
    gFollowerState.comeOutDoorStairs = FALSE;
    obj->invisible = FALSE;
}

/* ★ 2026-10-04 今ある（見えない）連れ歩きを、その場でボールから出す（BwFollowerStartBall(FALSE) の演出の部分だけ・物体は作り直さない） */
/* Start either ball transition, with one allocation/failure policy. */
static void BwFollowerBeginBallAnimation(struct EventObject *obj, bool8 enter)
{
    struct Sprite *follower = &gSprites[obj->spriteId];
    u8 taskId;

    /* BPRJ CreateTask returns 0 when full, which aliases a live task.
     * Check capacity before calling it; also accept 0xFF from other builds. */
    taskId = GetTaskCount() < NUM_TASKS ? CreateTask(Task_BwFollowerBall, 1) : 0xFF;
    if (taskId >= NUM_TASKS)
    {
        /* No task slot: finish without an animation and preserve live tasks. */
        if (enter)
        {
            DestroySprite(follower);
            RemoveEventObject(obj);
            BW_BALL_STATE = BW_BALL_IN;
        }
        else
        {
            obj->invisible = FALSE;
            BwFollowerLoadOwnPalette();
            BW_BALL_STATE = BW_BALL_OUT;
        }
        return;
    }
    gTasks[taskId].tTimer = BW_BALL_DURATION;
    gTasks[taskId].tBallSprite = 0xFF;
    gTasks[taskId].tEnter = enter;
    gTasks[taskId].tAffine = (obj->facingDirection == DIR_EAST
                              && follower->anims[3]->frame.hFlip) ? 1 : 0;
}

static void BwFollowerReleaseFromBall(struct EventObject *obj)
{
    struct Sprite *follower = &gSprites[obj->spriteId];

    obj->invisible = TRUE;
    follower->pos2.x = 0;
    follower->pos2.y = 0;
    obj->hasShadow = FALSE;
    BwFollowerBeginBallAnimation(obj, FALSE);
}

static void BwFollowerStartBall(bool8 enter)
{
    struct EventObject *obj;
    struct Sprite *follower;

    if (enter)
    {
        /* ★★ **しまい始めた時点で CFRU の管理から外す。**
           そうしないと、次の一歩で `SetSurfJump()` が走って連れ歩きが
           波乗りの台に乗せられてしまう（乗ってから消えることになる）。
           合図も消しておく（立ったままだと次に出したとき台を作られる）。 */
        gFollowerState.inProgress = FALSE;
        gFollowerState.createSurfBlob = 0;
    }
    else
    {
        /* 出すときは物体から作り直す（しまうときに消してあるので） */
        gFollowerState.inProgress = TRUE;
        gFollowerState.warpEnd = TRUE;          /* 主人公のいる所に置き直させる */
        gFollowerState.createSurfBlob = 0;
        gFollowerState.comeOutDoorStairs = FALSE;
        gFollowerState.currentSprite = 0;
        CreateFollowerAvatar();
        if (!gFollowerState.inProgress)
        {
            BW_BALL_STATE = BW_BALL_OUT;        /* 物体の空きが無かった。演出はあきらめる */
            return;
        }
        BwFollowerPlaceBehindPlayer(TRUE);
        gFollowerState.delayedState = 0;
    }

    obj = BwFollowerObject();
    if (obj == NULL)
        return;

    follower = &gSprites[obj->spriteId];
    if (!enter)
    {
        /* ★ 跳ねの残り（浮き上がり）と影を消しておく。
           `pos2.y` は跳ぶ動きと波乗りの台が書き込む欄で、スプライトを消しても
           作り直した先に残ることがある */
        follower->pos2.x = 0;
        follower->pos2.y = 0;
        obj->hasShadow = FALSE;
    }
    BwFollowerBeginBallAnimation(obj, enter);
}

/* ---- 入れ替えはその場で差し替える（HGSS と同じ・ボールは使わない） ----
   ★ HGSS 本家は `MapObject_SetSpriteID()` で**物体を消さずに絵だけ**入れ替える
   （`参考/pokeheartgold` の `FollowMon_ChangeMon()`）。こちらはシート方式なので
   タイル・コマ表・OAM の形・パレットを全部貼り替えることになるが、
   **CFRU が自転車の乗り降りでまったく同じことをしている**（`SetFollowerSprite`）ので、
   その道をそのまま借りる:

     `gFollowerState.currentSprite` を「いまと違う値」にして `FollowMe_HandleSprite()` を呼ぶと、
     CFRU が 物体を作り直し → 位置と向きを戻す ところまでやってくれる。

   ★ タイルは**作り直す前に**積む（`CreateSprite` が札から先頭のタイルを引くため）。 */
static void BwFollowerSwapNow(struct EventObject *obj, u16 want)
{
    if (!BwFollowerLoadSheet(want))
    {
        /* ★ 積めなかった。**古い絵の割り当ても外れている**ので、その場で元に戻す。
           戻さないと 1 コマだけゴミが出て、次のコマでまた外して……とちらつく。 */
        if (BwFollowerLoadSheet(gFollowerState.gfxId))
            BwFollowerRebindSheet(obj);
        return;
    }

    gFollowerState.gfxId = want;
    gFollowerState.currentSprite = 0xFF;        /* 「いまと違う」ことにして作り直させる */
    FollowMe_HandleSprite();
    BwFollowerLoadOwnPalette();
}

/* ---- 毎コマ呼ばれる。しまう／出すのきっかけを見る ----
   src/hoenn_steps.c の HoennStepTick から。 */
/* ★ 2026-10-07 隠す演出（ひでんわざのポケモン・そらをとぶ・テレポート・あなをほる・ポケセンの回復）が動いているか。
   前は素の FieldEffectActiveListContains（08083418・32 個の表を毎回はじめから見る）を 7 回呼んでいて、毎コマ約 7,000 サイクル
   （デモ版の報告「処理落ち」）→ 同じ表（03000FC8・空き 0xFF）を 1 回だけ見る */
#define BW_FIELD_EFFECT_ACTIVE_LIST ((const u8 *) 0x03000FC8)
static bool8 BwFollowerCutsceneFieldEffectActive(void)
{
    u32 i;

    for (i = 0; i < 32; ++i)
    {
        switch (BW_FIELD_EFFECT_ACTIVE_LIST[i])
        {
        case BW_FLDEFF_FIELD_MOVE_SHOW_MON:
        case BW_FLDEFF_FIELD_MOVE_SHOW_MON_INIT:
        case BW_FLDEFF_USE_FLY:
        case BW_FLDEFF_USE_FLY_ANCIENT_TOMB:
        case BW_FLDEFF_USE_TELEPORT:
        case BW_FLDEFF_USE_DIG:
        case BW_FLDEFF_POKECENTER_HEAL:
            return TRUE;
        }
    }
    return FALSE;
}

void BwFollowerHideForFieldEffect(void)
{
    struct EventObject *obj;
    bool8 surfing;
    bool8 cutscene;
    bool8 hide;

    /* ★ 2026-10-07 ユーザー「グレンタウンの1の島イベントで連れ歩きポケモンがうごかない」: CFRU の FollowMe（src/follow_me.c 229 行）は
       スクリプトの最中は主人公の歩きについていかない（マサキと船へ歩く場面などで置いていかれた）。CFRU の設定
       FLAG_FOLLOWER_IGNORE_ACTIVE_SCRIPT（0x926・2 つの config.h）が立っていればついていくので、いつも立てておく */
    if (!FlagGet(FLAG_FOLLOWER_IGNORE_ACTIVE_SCRIPT))
        FlagSet(FLAG_FOLLOWER_IGNORE_ACTIVE_SCRIPT);

    if (FuncIsActiveTask(Task_BwFollowerBall))
        return;     /* 演出中 */

    /* ★ `createSurfBlob` は CFRU が**なみのりを使った瞬間**に立てる合図
       （`FollowMe_FollowerToWater()`・ROM のフック 080866E4）。
       ここで引っ込めると、連れ歩きが波乗りの台に乗る前にボールへ戻せる。
       主人公が「ボールから出す」しぐさをする前に、先にしまう形になる。 */
    surfing = (TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_SURFING) != 0)
           || (gFollowerState.createSurfBlob != 0);
    cutscene = BwFollowerCutsceneFieldEffectActive();
    /* ★ 2026-09-24: 自転車に乗っている間もボールの中（ユーザー要望「じてんしゃだしたら連れ歩きなしで」）。
       CFRU の MoveOnBike（src/overworld.c）は連れ歩きが出ていると**ターボを使わない**ので、
       マッハ（src/hoenn_bike.c）が速くならなかった。降りて止まったら、なみのりと同じ道で出てくる */
    hide = surfing || cutscene || (TestPlayerAvatarFlags(PLAYER_AVATAR_FLAG_BIKE) != 0);

    if (BW_BALL_STATE != BW_BALL_OUT)
    {
        u16 want;

        /* ★ 主人公が歩ける状態になるまで待つ（なみのりを降りる演出の最中に割り込まない） */
        if (hide || gPlayerAvatar->preventStep)
        {
            BW_BALL_STATE = BW_BALL_IN;     /* 数え直し */
            return;
        }

        if (BW_BALL_STATE < BW_BALL_IN + BW_BALL_WAIT)
        {
            ++BW_BALL_STATE;                /* CFRU の降りる処理が通り過ぎるのを待つ */
            return;
        }

        /* ★★ **主人公が止まるまで出さない。**
           歩いている最中に作り直すと、CFRU の `FollowMe()` が
           「主人公と同じ動き」を連れ歩きに命じる。なみのりを降りた直後は
           その動きが**台から跳び降りる跳ね**なので、出てきた瞬間から浮いて見える
           （実機 2026-09-20「降りてすぐ移動すると浮遊、少し待つと浮遊しない」）。
           止まってから出せば、素のワープ後と同じ道を通るので跳ねない。 */
        if (gPlayerAvatar->tileTransitionState != T_NOT_MOVING
         || gPlayerAvatar->runningState != NOT_MOVING)
            return;

        want = BwFollowerWantedGfxId();
        if (want == 0)
            return;                         /* いまの先頭は出せない（タマゴ・瀕死・絵が無い）。中で待つ */

        /* ★ ここで先頭を見直すので、**ボールに入っている間に入れ替えても付いて来る**。
           ★★ ただし**同じ種族なら積み直さない**。64x64 は 384 タイル要るので、
           遊んでいる最中に積み直すと VRAM の空きが足りずに絵が化けることがある。 */
        if (want != gFollowerState.gfxId)
        {
            if (!BwFollowerLoadSheet(want))
                return;                     /* VRAM が足りない。次のコマでやり直す */
            gFollowerState.gfxId = want;
        }
        BwFollowerStartBall(FALSE);
        return;
    }

    /* 出ている */
    if (!gFollowerState.inProgress)
        return;
    obj = BwFollowerObject();
    if (obj == NULL)
    {
        /* ★ 2026-10-03 連れ歩きの物体が無くなった（画面の外で消された・枠を町の人に取られた）。
           スクリプトが終わって止まったら、ボールから出し直す（取られた枠の町の人には触らない） */
        if (gFollowerState.map.id == DEFAULT_FOLLOWER_LOCAL_ID && !hide && BwFollowerCanRepositionNow(NULL)
         && BwFollowerWantedGfxId() == gFollowerState.gfxId)
            BwFollowerStartBall(FALSE);
        return;
    }

    BwFollowerRepairSheet(obj);     /* メニューから戻った直後などの貼り直し */

    /* ★ 2026-10-08 ユーザー「つれあるきがドアでたときにドアとかさなる」: CFRU の FollowMe_WarpSetEnd（src/follow_me.c）は
       ドアから出たとき連れ歩きをドアのマスに置くが、こちらでは見えたままで、ドアと家の壁に重なっていた（mGBA）。
       ドアから出てきた合図（warpEnd・comeOutDoorStairs == 1）が立っているあいだは隠す → 下の「見えないまま待っている」に入り、
       止まっていれば BwFollowerAppearAfterWarp が主人公の横のマスでボールから出す */
    /* ★ 2026-10-08 ユーザー「ドアから出た瞬間にいるんだよね」（写真）: 主人公がドアから歩いて出てくる途中は、まだ合図が立っておらず
       ドアのマスに見えていた（主人公の座標はもう下のマス）。→ ワープの終わり（warpEnd）が立っているあいだは、いつも隠す。
       見せるのは CFRU（一歩目で warpEnd を下ろしてから見せる・ドアなら Task_FollowerOutOfDoor）か、下の BwFollowerAppearAfterWarp */
    if (gFollowerState.warpEnd && !obj->invisible)
        obj->invisible = TRUE;

    if (obj->invisible)
    {
        /* まだ姿を見せていない（ワープ直後など）。
           ★ 2026-10-02 ユーザー「ドアでたところで先頭のポケモン入れ替えると入れ替える前のポケモンがドアからでてくる」:
             ドアを出た直後は一歩目までドアの中で待っている（CFRU の warpEnd・comeOutDoorStairs）。ここで帰っていたので、
             その間に入れ替えると古い絵のまま出てきた → 待っている間でも絵だけ差し替える。
             SetFollowerSprite（src/follow_me.c）は物体を作り直すとき見えない状態ごと写すので、姿は出ない。
             連れ歩けない子（want == 0）になったときはボールの演出を出さず、出てくるときの処理に任せる */
        /* ★ 2026-10-04 ワープのあと見えないまま待っている間は「次の一歩で跳ぶ」の残りを捨てる（同じ地図へのワープでも・
           段差を跳んだ直後のワープで残り、ワープ先の 2 歩目で 2 マス跳んで主人公に重なった） */
        if (gFollowerState.warpEnd)
            gFollowerState.delayedState = 0;
        if (gFollowerState.warpEnd && !hide)
        {
            u16 want = BwFollowerWantedGfxId();

            if (want != 0 && want != gFollowerState.gfxId
             && gPlayerAvatar->tileTransitionState == T_NOT_MOVING
             && gPlayerAvatar->runningState == NOT_MOVING)
            {
                BwFollowerSwapNow(obj, want);
                return;
            }
            BwFollowerCheckShinyPalette(obj);   /* 同じ種族の色違いと入れ替えた */

            /* ★ 2026-10-03 ユーザーの動画「連れ歩きポケモンの更新がメニューとじたときにも必要そう」:
               ワープのあと一歩も歩かないと、連れ歩きは見えないまま（話しかけると先頭の名前は出る）で、
               メニューで先頭を替えて閉じても出てこなかった。主人公が止まっていて、スクリプト・暗転・
               ほかの演出が無ければ、うしろへ置いて見せる（うしろが通れないときは今までどおり一歩目で） */
            if (want != 0 && want == gFollowerState.gfxId && BwFollowerCanRepositionNow(obj)
             && !gPlayerAvatar->preventStep && !gPaletteFade->active)
                BwFollowerAppearAfterWarp(obj);
        }
        return;
    }

    /* ★ 2026-10-03 スクリプトで主人公だけ動かされて（ニビの青年の案内など）置き去りになった。
       止まったら主人公のうしろへ置き直す（離れたままだと CFRU の FollowMe が離れた所で同じ向きに歩かせる） */
    if (!hide && ScriptContext2_IsEnabled())
    {
        BwFollowerFollowScriptedStep(obj);      /* スクリプトで主人公が歩かされている間も付いていく */
        BwFollowerAvoidOverlap(obj);            /* スクリプトで歩いてきた人と重なったらよける */
    }

    if (!hide && BwFollowerCanRepositionNow(NULL))     /* 遠くの連れ歩きが動きの途中でも（主人公が止まっていれば）置き直す */
    {
        struct EventObject *player = &gEventObjects[gPlayerAvatar->eventObjectId];
        s16 dx = obj->currentCoords.x - player->currentCoords.x;
        s16 dy = obj->currentCoords.y - player->currentCoords.y;

        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        /* ★ 2026-10-04 段差を跳び降りたあとは CFRU が「次の一歩で跳ぶ」を持って 2 マスうしろで待っている（正しい形）。
           そこで置き直すと跳ぶ予定と食い違い、次の一歩で主人公に重なった → そのときは置き直さない */
        if (dx + dy > 1 && !(gFollowerState.delayedState != 0 && dx + dy == 2))
        {
            EventObjectClearHeldMovement(obj);   /* 置き去りの所で受けていた動きを捨てる */
            BwFollowerPlaceBehindPlayer(FALSE);
        }
    }

    BwFollowerFixPaletteSlot(obj);      /* 色の置き場を札の置き場に（2026-10-03） */
    BwFollowerCheckShinyPalette(obj);   /* 同じ種族の色違いと入れ替えた */

    if (!hide)
    {
        u16 want = BwFollowerWantedGfxId();

        if (want == gFollowerState.gfxId)
            return;                             /* 変わっていない */

        /* 歩いている最中には作り直さない（跳ねを写す。実機 2026-09-20） */
        if (gPlayerAvatar->tileTransitionState != T_NOT_MOVING
         || gPlayerAvatar->runningState != NOT_MOVING)
            return;

        if (want == 0)
            BwFollowerStartBall(TRUE);          /* 連れ歩けなくなった → ボールへしまう */
        else
            BwFollowerSwapNow(obj, want);       /* 入れ替え → その場で差し替え（HGSS と同じ） */
        return;
    }

    /* ★★ なみのりに**乗る**演出の最中は手を出さない。
       CFRU は乗るときに `preventStep` を立てて「連れ歩きが跳ねて台に乗る」のを待っている。
       その途中で物体を消すと待ちが終わらず**固まる**（実機 2026-09-20 の固まりはこの形）。
       そらをとぶ等は演出そのものが `preventStep` を立てるので、そちらは待たない。 */
    if (surfing && !cutscene && gPlayerAvatar->preventStep)
        return;

    BwFollowerStartBall(TRUE);
}

#undef tTimer
#undef tBallSprite
#undef tAffine
#undef tEnter

static bool8 BwPalEquals(const u16 *a, const u16 *b)
{
    u32 k;

    for (k = 0; k < 16; ++k)
    {
        if (a[k] != b[k])
            return FALSE;
    }
    return TRUE;
}

/* 同じ種族で色違いとそうでない子を入れ替えたとき（絵の番号が同じなので積み直されない）、
   置き場の色が**もう一方の色そのまま**なら積み直す。
   ★ どちらとも合わない（霧・回想の色調・ほかの NPC が置き場を使っている など）ときは触らない
   ★ 色がどちらかと合うかを先に見て、合ったときだけ手持ちを調べる（毎コマ呼ばれるので） */
static void BwFollowerCheckShinyPalette(struct EventObject *obj)
{
    u16 index = BwFollowerIndexOf(gFollowerState.gfxId);
    u8 slot = gSprites[obj->spriteId].oam.paletteNum;
    const u16 *cur = &gPlttBufferUnfaded[0x100 + slot * 16];
    bool8 isNormal, isShiny;

    if (!BwFollowerValidIndex(index) || gPaletteFade->active)
        return;
    isNormal = BwPalEquals(cur, gBwFollowerOwPalTable[BwFollowerPalPos(index, FALSE)].data);
    isShiny = BwPalEquals(cur, gBwFollowerOwPalTable[BwFollowerPalPos(index, TRUE)].data);
    if (isNormal == isShiny)
        return;     /* どちらでもない・色違いが通常と同じ色 */
    if (BwFollowerLeadShinyFor(index) ? isNormal : isShiny)
        PatchObjectPalette(BW_PAL_TAG_BASE + index, slot);
}

/* ---- 手持ちの先頭から連れ歩きを組み立てる ----
   character_customization.c の InitPlayerAvatar（地図を作り直すたびに通る）から、
   CFRU の CreateFollowerAvatar() の直前に呼ばれる（patches/0022）。 */
void BwFollowerUpdateFromParty(void)
{
    u16 gfxId;

    if (gFollowerState.inProgress && gFollowerState.map.id != DEFAULT_FOLLOWER_LOCAL_ID)
        return;     /* NPC の連れ歩き（sp0D1）が動いている。そちらを優先する */

    gfxId = BwFollowerWantedGfxId();
    if (gfxId == 0)
    {
        gFollowerState.inProgress = FALSE;      /* 絵の無い種族・タマゴ・瀕死は出さない */
        BW_BALL_STATE = BW_BALL_OUT;
        return;
    }

    /* ★ タイルは VRAM に積む。地図を作り直すと割り当ては消えているので**毎回**積み直す。
       ここは InitPlayerAvatar の中＝スプライトを全部消したあとなので、
       先に FreeSpriteTilesByTag を呼んでも誰も使っていない（積んでいなければ何もしない）。 */
    if (!BwFollowerLoadSheet(gfxId))
    {
        gFollowerState.inProgress = FALSE;      /* 絵を積めなかった。出さない（ゴミを出すより良い） */
        BW_BALL_STATE = BW_BALL_OUT;
        return;
    }
    BW_BALL_STATE = BW_BALL_OUT;                /* 地図を作り直したら出ている状態から */

    if (gFollowerState.inProgress && gFollowerState.gfxId == gfxId)
    {
        /* ★ 同じポケモンが続いているだけ。**状態は作り直さない。**
           ここで全部入れ直すと、ワープの途中で CFRU が立てた warpEnd や comeOutDoorStairs
           （ドアから出てくる合図）を消してしまい、連れ歩きが出て来られなくなる。 */
        /* ★★★ **地図が変わったら「ドアから出てくる」の合図は捨てる**
           （2026-09-22・ユーザーの写真から。再現はしていない）。

           CFRU の `FollowMe()`（`src/follow_me.c:235`）は

               if (動いた && warpEnd) {
                   if (comeOutDoorStairs == 1) {
                       Task_FollowerOutOfDoor を作る（ドアの音が鳴る）
                       goto RESET;      ← **主人公の所へ動かさない**
                   }
                   ...
                   MoveEventObjectToMapCoords(follower, 主人公の x, y);

           という形で、**ドアから出る道に入ると連れ歩きを主人公の所へ運ばない**。
           だから合図が古いまま残っていると

             ・歩くたびにドアの音が鳴る
             ・連れ歩きが**離れた所**に置き去りになる

           の 2 つが同時に出る（ユーザーの写真がまさにこれ。主人公はジムの前、
           連れ歩きは画面の下の柵の所にいた）。

           合図は建物から出る演出の最後（`follow_me.c:1093`）で立つので、
           **その地図の中でだけ**正しい。1 歩も歩かずに そらをとぶ などで
           別の地図へ移ると、次の地図で古い合図が効いてしまう。
           ここ（地図を作り直す所）で地図が変わっていたら落とす。
           ★ ふつうのドアの出入りでは、この関数のあとに合図が立つので消えない。 */
        if (gFollowerState.map.number != gSaveBlock1->location.mapNum
         || gFollowerState.map.group != gSaveBlock1->location.mapGroup)
            gFollowerState.comeOutDoorStairs = FALSE;
        /* ★ 2026-10-04 ワープした（warpEnd）・地図が変わったら「次の一歩で跳ぶ」の残りを捨てる。段差を跳んだ直後にワープすると残り、
           ワープ先の 2 歩目で連れ歩きが 2 マス跳んで主人公に重なった（mGBA で再現） */
        if (gFollowerState.warpEnd
         || gFollowerState.map.number != gSaveBlock1->location.mapNum
         || gFollowerState.map.group != gSaveBlock1->location.mapGroup)
            gFollowerState.delayedState = 0;
        gFollowerState.map.number = gSaveBlock1->location.mapNum;
        gFollowerState.map.group = gSaveBlock1->location.mapGroup;
        gFollowerState.flags = BW_FOLLOWER_FLAGS;   /* 旗の中身を変えたときに取り残さない */
        return;
    }

    gFollowerState.inProgress = TRUE;
    gFollowerState.gfxId = gfxId;
    gFollowerState.map.id = DEFAULT_FOLLOWER_LOCAL_ID;
    gFollowerState.map.number = gSaveBlock1->location.mapNum;
    gFollowerState.map.group = gSaveBlock1->location.mapGroup;
    gFollowerState.script = gBwFollowerScript;
    gFollowerState.flag = 0;
    gFollowerState.flags = BW_FOLLOWER_FLAGS;
    gFollowerState.createSurfBlob = 0;
    gFollowerState.comeOutDoorStairs = FALSE;
    gFollowerState.warpEnd = TRUE;              /* 最初の一歩で出てくる（ドアの演出は通さない） */
    BW_BALL_STATE = BW_BALL_OUT;
    gFollowerState.delayedState = 0;
    gFollowerState.currentSprite = 0;
}

/* ★★ 2026-10-03 ショップで連れ歩きの絵がモンスターボールの絵に化けた（ユーザーの動画）。
   素のショップ（CB2_InitBuyMenu）は ResetSpriteData でタイルの割り当てを全部消してから、
   地図の人を AddPseudoEventObject（0805E238・pokefirered の CreateObjectGraphicsSprite）で描き直す。
   連れ歩きはシート方式なので札 BW_FOLLOWER_TILE_TAG が無いまま作られ、素の CreateSpriteAt が
   GetSpriteTileStartByTag の 0xFFFF をそのまま先頭にして、VRAM の関係ない所（品物のアイコン）を描いていた。
   素と同じ処理（逆アセンブルで確かめた: 0805E1A8 → 0805ED70 → CreateSprite → SetSubspriteTables・subspriteMode 2）に、
   「連れ歩きのシートが積まれていなければ先に積む・積めなければ作らない」を足して丸ごと差し替える（functionrewrites）。
   連れ歩き以外の絵では素と同じ動き */
#define BwFol_LoadEventObjectPalette ((void (*)(u16))(0x0805ED70 | 1))

u8 BwAddPseudoEventObject(u16 graphicsId, void (*callback)(struct Sprite *), s16 x, s16 y, u8 subpriority)
{
    struct SpriteTemplate spriteTemplate;
    const struct SubspriteTable *subspriteTables = NULL;
    u8 spriteId;

    MakeObjectTemplateFromEventObjectGraphicsInfo(graphicsId, callback, &spriteTemplate, &subspriteTables);
    if (spriteTemplate.paletteTag != 0xFFFF)
        BwFol_LoadEventObjectPalette(spriteTemplate.paletteTag);

    if (spriteTemplate.tileTag == BW_FOLLOWER_TILE_TAG
     && GetSpriteTileStartByTag(BW_FOLLOWER_TILE_TAG) == 0xFFFF
     && !BwFollowerLoadSheet(graphicsId))
        return MAX_SPRITES;

    spriteId = CreateSprite(&spriteTemplate, x, y, subpriority);
    if (spriteId != MAX_SPRITES && subspriteTables != NULL)
    {
        SetSubspriteTables(&gSprites[spriteId], subspriteTables);
        gSprites[spriteId].subspriteMode = 2;   /* SUBSPRITES_IGNORE_PRIORITY */
    }
    return spriteId;
}
