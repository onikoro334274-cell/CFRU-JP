#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/random.h"

/*
    きんこんしき（金婚式）の夫婦（2026-10-09 ユーザーの表「各3:3で手持ちがあるとしたら … のテーブルが選ばれるみたいな」）。
    資料: docs/research/2026-10-09_golden-wedding.md

    トレーナー 936（ゲンゾウ）・937（ハナ）は 13 組の左・右を 13 匹ずつ持つ（tools/kanto/build_kanto_trainers.py の GOLDEN_WEDDING_PAIRS・
    並びが組の番号）。戦うたびに 1 人目（ゲンゾウ）の手持ちを作るとき 3 組を選んで変数 0x50F5 に入れ、2 人とも その 3 組の自分の側の 3 匹で戦う。
    CFRU の CreateNPCTrainerParty（src/build_pokemon.c・patches/0243）が、作る直前に これを呼んで、3 匹だけの写しのトレーナーに差し替える
    （2 人を相手にする戦いは CFRU が 1 人 3 匹まで＝写しの 3 匹がそのまま使われる）。型・努力値・持ち物は ROM の 13 匹の行のまま。
*/

#define TRAINER_GOLDEN_WEDDING_M 936   //tools/kanto/kanto_trainer_ids.json
#define TRAINER_GOLDEN_WEDDING_F 937
#define GOLDEN_WEDDING_ROWS 13
#define GOLDEN_WEDDING_PICK 3
#define VAR_GOLDEN_WEDDING_ROWS 0x50F5  //もとミラクル交換の回数（もう使わない・tools/rom/build_hoenn_scripts.py の予約）
#define ROWS_TAG 0xB000                 //上 4 ビットの印（印が無ければ選び直す）

static void PickRows(void)
{
	u8 rows[GOLDEN_WEDDING_PICK];
	u32 n = 0, i;

	while (n < GOLDEN_WEDDING_PICK)
	{
		u8 r = Random() % GOLDEN_WEDDING_ROWS;
		for (i = 0; i < n; ++i)
		{
			if (rows[i] == r)
				break;
		}
		if (i == n)
			rows[n++] = r;
	}
	VarSet(VAR_GOLDEN_WEDDING_ROWS, ROWS_TAG | rows[0] | (rows[1] << 4) | (rows[2] << 8));
}

bool8 HackGoldenWedding_PickParty(u16 trainerId, bool8 firstTrainer, struct Trainer* copy, struct TrainerMonItemCustomMoves* party)
{
	const struct Trainer* trainer;
	u16 v;
	u32 i;

	if (trainerId != TRAINER_GOLDEN_WEDDING_M && trainerId != TRAINER_GOLDEN_WEDDING_F)
		return FALSE;
	trainer = &gTrainers[trainerId];
	if (trainer->partyFlags != (PARTY_FLAG_CUSTOM_MOVES | PARTY_FLAG_HAS_ITEM) || trainer->partySize < GOLDEN_WEDDING_ROWS)
		return FALSE;

	v = VarGet(VAR_GOLDEN_WEDDING_ROWS);
	if ((trainerId == TRAINER_GOLDEN_WEDDING_M && firstTrainer) || (v & 0xF000) != ROWS_TAG)
	{
		PickRows();
		v = VarGet(VAR_GOLDEN_WEDDING_ROWS);
	}

	*copy = *trainer;
	copy->partySize = GOLDEN_WEDDING_PICK;
	copy->party.ItemCustomMoves = party;
	for (i = 0; i < GOLDEN_WEDDING_PICK; ++i)
	{
		u32 row = (v >> (4 * i)) & 0xF;
		if (row >= GOLDEN_WEDDING_ROWS)
			row = i;
		party[i] = trainer->party.ItemCustomMoves[row];
	}
	return TRUE;
}
