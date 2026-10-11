#include "defines.h"
#include "bw_follower_internal.h"
#include "../include/sprite.h"

extern const u8 gBwFollowerPalBlob[];

/* ---- 見せかけの images（シート方式では中身は読まれない。大きさだけ意味がある） ---- */
const struct SpriteFrameImage gBwFollowerDummyImages32[] = { { gBwFollowerPalBlob, 512, 0 } };
const struct SpriteFrameImage gBwFollowerDummyImages64[] = { { gBwFollowerPalBlob, 2048, 0 } };

/* ---- コマ表 ----
   コマの並びは expansion と同じ: 0 下 / 1 下歩き / 2 上 / 3 上歩き / 4 横 / 5 横歩き。
   8 コマのものは 6 / 7 が右向き（_Asym）。**値はタイルのずれ**。

   ★★ **20 個では足りない。40 個いる。**（実機 2026-09-20 のクラッシュの原因）
   素の FRLG は 0〜19 が「向き・歩き・速い・もっと速い・いちばん速い」だが、そのうしろにも
   **走る 20〜23 / 回る 24〜27 / その場で足踏み 28 / 自転車 20〜39** がある
   （pokefirered `include/constants/event_object_movement.h`）。
   連れ歩きは `FOLLOWER_FLAG_HAS_RUNNING_FRAMES` を立てているので **20〜23 を必ず要求される**。
   表が短いと配列の外を読み、その中身を「コマの命令」とみなして
   **命令の種類で関数の表を引く**ため、でたらめな番地へ飛ぶ（Jumped to invalid address）。 */
#define BW_FOLLOWER_ANIM_COUNT 40

/* ★ 2026-10-07 ユーザー「トキワジムリセット」「移動床にバグが」（オートラン無効で回転して滑る床に乗るとリセット）:
   素の SetStepAnimHandleAlternation（event_object_movement.c）はアニメの番号だけ替え、命令の位置（animCmdIndex）は前のアニメのまま
   SeekSpriteAnim する。素の絵は sStepAnimTables で位置を直すが、この表は載っていないので直されない。歩きの途中（位置 2・3）から
   向きのアニメ（命令 2 つ）に替わると表の外を命令として読み、関数の表の外（0 番地）を呼んでリセットしていた。
   → 向きのアニメも歩きのアニメと同じ 4 コマ＋JUMP(0)（同じ絵を 4 回）にして、どの位置から再開しても表の中に収める */
#define FACE(name, tile, flip) \
    static const union AnimCmd name[] = { \
        ANIMCMD_FRAME(tile, 16, .hFlip = flip), ANIMCMD_FRAME(tile, 16, .hFlip = flip), \
        ANIMCMD_FRAME(tile, 16, .hFlip = flip), ANIMCMD_FRAME(tile, 16, .hFlip = flip), ANIMCMD_JUMP(0) }
#define WALK(name, a, b, dur, flip) \
    static const union AnimCmd name[] = { \
        ANIMCMD_FRAME(a, dur, .hFlip = flip), ANIMCMD_FRAME(b, dur, .hFlip = flip), \
        ANIMCMD_FRAME(b, dur, .hFlip = flip), ANIMCMD_FRAME(a, dur, .hFlip = flip), ANIMCMD_JUMP(0) }

/* 32x32: 1 コマ 16 タイル */
FACE(sAnim32_FaceSouth, 0, FALSE);
FACE(sAnim32_FaceNorth, 32, FALSE);
FACE(sAnim32_FaceWest, 64, FALSE);
FACE(sAnim32_FaceEast, 64, TRUE);
FACE(sAnim32_FaceEastAsym, 96, FALSE);
WALK(sAnim32_GoSouth, 0, 16, 8, FALSE);
WALK(sAnim32_GoNorth, 32, 48, 8, FALSE);
WALK(sAnim32_GoWest, 64, 80, 8, FALSE);
WALK(sAnim32_GoEast, 64, 80, 8, TRUE);
WALK(sAnim32_GoEastAsym, 96, 112, 8, FALSE);
WALK(sAnim32_FastSouth, 0, 16, 6, FALSE);
WALK(sAnim32_FastNorth, 32, 48, 6, FALSE);
WALK(sAnim32_FastWest, 64, 80, 6, FALSE);
WALK(sAnim32_FastEast, 64, 80, 6, TRUE);
WALK(sAnim32_FastEastAsym, 96, 112, 6, FALSE);
WALK(sAnim32_FasterSouth, 0, 16, 4, FALSE);
WALK(sAnim32_FasterNorth, 32, 48, 4, FALSE);
WALK(sAnim32_FasterWest, 64, 80, 4, FALSE);
WALK(sAnim32_FasterEast, 64, 80, 4, TRUE);
WALK(sAnim32_FasterEastAsym, 96, 112, 4, FALSE);
WALK(sAnim32_FastestSouth, 0, 16, 3, FALSE);
WALK(sAnim32_FastestNorth, 32, 48, 3, FALSE);
WALK(sAnim32_FastestWest, 64, 80, 3, FALSE);
WALK(sAnim32_FastestEast, 64, 80, 3, TRUE);
WALK(sAnim32_FastestEastAsym, 96, 112, 3, FALSE);

/* 64x64: 1 コマ 64 タイル */
FACE(sAnim64_FaceSouth, 0, FALSE);
FACE(sAnim64_FaceNorth, 128, FALSE);
FACE(sAnim64_FaceWest, 256, FALSE);
FACE(sAnim64_FaceEast, 256, TRUE);
WALK(sAnim64_GoSouth, 0, 64, 8, FALSE);
WALK(sAnim64_GoNorth, 128, 192, 8, FALSE);
WALK(sAnim64_GoWest, 256, 320, 8, FALSE);
WALK(sAnim64_GoEast, 256, 320, 8, TRUE);
WALK(sAnim64_FastSouth, 0, 64, 6, FALSE);
WALK(sAnim64_FastNorth, 128, 192, 6, FALSE);
WALK(sAnim64_FastWest, 256, 320, 6, FALSE);
WALK(sAnim64_FastEast, 256, 320, 6, TRUE);
WALK(sAnim64_FasterSouth, 0, 64, 4, FALSE);
WALK(sAnim64_FasterNorth, 128, 192, 4, FALSE);
WALK(sAnim64_FasterWest, 256, 320, 4, FALSE);
WALK(sAnim64_FasterEast, 256, 320, 4, TRUE);
WALK(sAnim64_FastestSouth, 0, 64, 3, FALSE);
WALK(sAnim64_FastestNorth, 128, 192, 3, FALSE);
WALK(sAnim64_FastestWest, 256, 320, 3, FALSE);
WALK(sAnim64_FastestEast, 256, 320, 3, TRUE);

/* 40 個の並びは 3 つの表で共通。東だけ差し替える */
#define ANIM_TABLE(name, P, east, goEast, fastEast, fasterEast, fastestEast) \
const union AnimCmd *const name[BW_FOLLOWER_ANIM_COUNT] = { \
    /*  0〜 3 向き   */ P##_FaceSouth, P##_FaceNorth, P##_FaceWest, east, \
    /*  4〜 7 歩き   */ P##_GoSouth, P##_GoNorth, P##_GoWest, goEast, \
    /*  8〜11 速い   */ P##_FastSouth, P##_FastNorth, P##_FastWest, fastEast, \
    /* 12〜15 もっと */ P##_FasterSouth, P##_FasterNorth, P##_FasterWest, fasterEast, \
    /* 16〜19 最速   */ P##_FastestSouth, P##_FastestNorth, P##_FastestWest, fastestEast, \
    /* 20〜23 走る   */ P##_FastestSouth, P##_FastestNorth, P##_FastestWest, fastestEast, \
    /* 24〜27 回る   */ P##_FaceSouth, P##_FaceNorth, P##_FaceWest, east, \
    /* 28 足踏み     */ P##_GoSouth, \
    /* 29〜31        */ P##_GoNorth, P##_GoWest, goEast, \
    /* 32〜35 自転車 */ P##_FastestSouth, P##_FastestNorth, P##_FastestWest, fastestEast, \
    /* 36〜39 自転車 */ P##_FastestSouth, P##_FastestNorth, P##_FastestWest, fastestEast, \
}

ANIM_TABLE(gBwFollowerAnims_Sym32, sAnim32, sAnim32_FaceEast, sAnim32_GoEast,
           sAnim32_FastEast, sAnim32_FasterEast, sAnim32_FastestEast);
ANIM_TABLE(gBwFollowerAnims_Asym32, sAnim32, sAnim32_FaceEastAsym, sAnim32_GoEastAsym,
           sAnim32_FastEastAsym, sAnim32_FasterEastAsym, sAnim32_FastestEastAsym);
ANIM_TABLE(gBwFollowerAnims_Sym64, sAnim64, sAnim64_FaceEast, sAnim64_GoEast,
           sAnim64_FastEast, sAnim64_FasterEast, sAnim64_FastestEast);
