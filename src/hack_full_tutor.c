#include "defines.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/constants/moves.h"
#include "../include/new/daycare.h"
#include "../include/new/item.h"
#include "../include/new/learn_move.h"

/*
hack_full_tutor.c
    バトルフロンティアのフレンドリィショップの「なんでも おしえる ひと」（2026-10-05）。
    ユーザー「選択したポケモンが覚えるすべての技を教えてくれる技教えのひとも追加」→ 無料。
    スクリプトは assembly/hoenn_frontier_mart/frontier_mart_scripts.s。

    素の技思い出し（special 0xDB・0xE0）を使い、印 HACK_FLAG_FULL_TUTOR（0x12EE）が立っている間だけ
    CFRU の GetMoveRelearnerMoves（src/learn_move.c・patches/0171）がここを呼ぶ:
      レベル技（レベルに関係なく）・タマゴ技・わざマシン・教え技の全部（CFRU の BuildLearnableMoveset）から、
      重なりと覚えている技を除いた一覧。
    ★ 思い出しの画面の入れ物は 40 技まで（MAX_LEARNABLE_MOVES・入れ物の形は素の画面の番地に合わせてあるので広げない）。
      多いときは 40 ずつの「ページ」に分け、スクリプトが先にページを選ぶ（変数 HACK_VAR_FULL_TUTOR_PAGE = 一時変数 0x4009）
*/

#define HACK_FLAG_FULL_TUTOR 0x12EE         //tools/rom/build_hoenn_scripts.RESERVED_FLAGS と同じ
#define HACK_VAR_FULL_TUTOR_PAGE 0x4009     //一時変数（地図に入ると 0）
#define FULL_TUTOR_BUFFER (MAX_LEARNABLE_MOVES + EGG_MOVES_ARRAY_COUNT + EGG_MOVES_ARRAY_COUNT + NUM_TMSHMS + (LAST_TOTAL_TUTOR_NUM + 1))

u16 BuildLearnableMoveset(struct Pokemon* mon, u16* moves);

bool8 HackFullTutor_IsActive(void)
{
	return FlagGet(HACK_FLAG_FULL_TUTOR);
}

//重なりと覚えている技を除いた全部の数（→ out にも入れる）
static u16 FullTutorAllMoves(struct Pokemon* mon, u16* out)
{
	u16 n, i, j, k, count = 0;

	Memset(out, 0, FULL_TUTOR_BUFFER * sizeof(u16));
	n = BuildLearnableMoveset(mon, out);
	for (i = 0; i < n && i < FULL_TUTOR_BUFFER; ++i)
	{
		u16 move = out[i];

		if (move == MOVE_NONE)
			continue;
		for (j = 0; j < MAX_MON_MOVES && GetMonData(mon, MON_DATA_MOVE1 + j, NULL) != move; ++j)
			;
		if (j < MAX_MON_MOVES)
			continue;               //覚えている
		for (k = 0; k < count && out[k] != move; ++k)
			;
		if (k < count)
			continue;               //重なり
		// count <= i なので、まだ読んでいない技を上書きせず同じバッファに詰める。
		out[count++] = move;
	}
	return count;
}

//src/learn_move.c GetMoveRelearnerMoves の頭（印が立っているとき）: 今のページの技 → moves（最大 MAX_LEARNABLE_MOVES）
u8 HackFullTutor_GetMoves(struct Pokemon* mon, u16* moves)
{
	u16* all = Malloc(FULL_TUTOR_BUFFER * sizeof(u16));
	u16 total;
	u32 start, i;
	u8 n = 0;

	if (all == NULL)
		return 0;
	total = FullTutorAllMoves(mon, all);
	start = VarGet(HACK_VAR_FULL_TUTOR_PAGE) * MAX_LEARNABLE_MOVES;
	for (i = start; i < total && n < MAX_LEARNABLE_MOVES; ++i)
		moves[n++] = all[i];
	Free(all);
	return n;
}

//callasm: 0x8004 の手持ちのページの数 → LASTRESULT（0 なら教えられる技が無い）
void HackFullTutor_CountPages(void)
{
	u16* all;
	u16 total;

	gSpecialVar_LastResult = 0;
	if (Var8004 >= PARTY_SIZE)
		return;
	all = Malloc(FULL_TUTOR_BUFFER * sizeof(u16));
	if (all == NULL)
		return;
	total = FullTutorAllMoves(&gPlayerParty[Var8004], all);
	Free(all);
	gSpecialVar_LastResult = (total + MAX_LEARNABLE_MOVES - 1) / MAX_LEARNABLE_MOVES;
}
