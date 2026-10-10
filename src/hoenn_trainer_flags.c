#include "defines.h"
#include "defines_battle.h"        /* gTrainerBattleOpponent_A */
#include "../include/event_data.h"

/*
hoenn_trainer_flags.c
    トレーナーフラグの置き場を広げる（やりたいこと #2・ホウエンのジム）。

    ★ 詳細は docs/research/2026-09-21_trainer-flag-space.md

    ## なぜ要るか

    素 FRLG のトレーナーフラグは **`0x500 + トレーナー番号`**。
    `0x800` からはシステムフラグ（`SYS_FLAGS`）なので、
    **767 番が上限**。素 FRLG が 742 番まで使っているので
    **空きは 25 枠しかない**。ホウエンの 8 ジムだけで 65 人要る。

    ## どう広げたか

        番号 0〜767    … 0x500 + 番号   （素のまま。カントーは一切変わらない）
        番号 768〜2047 … 0x1400 + (番号 - 768) ＝ 番号 + 0x1100

    行き先の `0x1400`〜`0x18FF` は CFRU の**拡張フラグ**の一部
    （`src/save.c` の `GetExpandedFlagPointer` が 0x900〜0x18FF を見る）。
    0x18FF が拡張フラグの終わりなので **2047 番が新しい上限**。

    ★ ホウエンのイベント用フラグは `0x1000`〜`0x13FF`
      （`tools/rom/build_hoenn_scripts.py` の `HOENN_FLAG_BASE`）。
      **こことぶつからない**ように `0x1400` から始めてある。

    ★★ **素の番号（0〜767）はわざと動かしていない。**
      カントーの 743 人ぶんのフラグはセーブの同じ場所に残るので、
      素 FRLG のスクリプト・VS シーカー・ずかんナビの挙動は変わらない。
      「全部を拡張フラグへ移す」ほうが式は 1 本で済むが、
      素の 743 人を巻き込むぶん壊す範囲が大きい。

    ## 触る所

    トレーナーフラグを作っているのは、ゲーム全体で **9 箇所しか無い**
    （`参考/pokefirered` を `TRAINER_FLAGS_START` で検索して確認。
    VS シーカーもずかんナビも下の 3 関数を呼んでいるだけ）。

      ROM（`functionrewrites` で下の 4 つに差し替える）
        0807F7D8  GetTrainerAFlag        呼び元 3
        0807FB44  HasTrainerBeenFought   呼び元 6
        0807FB5C  SetTrainerFlag         呼び元 1
        0807FB70  ClearTrainerFlag       呼び元 1

      CFRU の C（`src/overworld.c` の 5 箇所。patches/0038）
        CheckTrainerSpotting / GetTrainerFlagFromScriptPointer /
        SetTrainerFlags / HasOneTrainerBeenDefeated

    ★ 呼び元の数は ROM の BL を数えた実測で、decomp の数と一致している。
*/

/* 素 FRLG の枠はここまで（`MAX_TRAINERS_COUNT`）*/
#define HOENN_TRAINER_ID_SPLIT   768
/* 768 番以降の行き先。拡張フラグ 0x1400〜0x18FF */
#define HOENN_TRAINER_FLAG_BASE  0x1400

u16 HoennTrainerFlag(u16 trainerId);
u16 HoennGetTrainerAFlag(void);
bool8 HoennHasTrainerBeenFought(u16 trainerId);
void HoennSetTrainerFlag(u16 trainerId);
void HoennClearTrainerFlag(u16 trainerId);

u16 HoennPyramid_TrainerFlag(u16 trainerId); /* src/hoenn_pyramid.c */
u16 HoennHill_TrainerFlag(u16 trainerId);    /* src/hoenn_trainer_hill.c */

u16 HoennTrainerFlag(u16 trainerId)
{
	u16 pyramidFlag = HoennPyramid_TrainerFlag(trainerId); //★ 2026-10-03 バトルピラミッドのトレーナー（0x7F0〜0x7F7）は一時フラグ 0x11〜0x18

	if (pyramidFlag != 0)
		return pyramidFlag;
	pyramidFlag = HoennHill_TrainerFlag(trainerId); //★ 2026-10-03 トレーナーヒルの 2 人（0x7F8・0x7F9）は旗 0x12F0 + 階 × 2 + 何人目
	if (pyramidFlag != 0)
		return pyramidFlag;
	if (trainerId < HOENN_TRAINER_ID_SPLIT)
		return FLAG_TRAINER_FLAG_START + trainerId;

	return HOENN_TRAINER_FLAG_BASE + (trainerId - HOENN_TRAINER_ID_SPLIT);
}

/* 素 0807F7D8 GetTrainerAFlag */
u16 HoennGetTrainerAFlag(void)
{
	return HoennTrainerFlag(gTrainerBattleOpponent_A);
}

/* 素 0807FB44 HasTrainerBeenFought */
bool8 HoennHasTrainerBeenFought(u16 trainerId)
{
	return FlagGet(HoennTrainerFlag(trainerId));
}

/* 素 0807FB5C SetTrainerFlag */
void HoennSetTrainerFlag(u16 trainerId)
{
	FlagSet(HoennTrainerFlag(trainerId));
}

/* 素 0807FB70 ClearTrainerFlag */
void HoennClearTrainerFlag(u16 trainerId)
{
	FlagClear(HoennTrainerFlag(trainerId));
}
