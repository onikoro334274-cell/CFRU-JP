#include "defines.h"
#include "defines_battle.h"
#include "../include/constants/hold_effects.h"
#include "../include/new/battle_util.h"
#include "hack_danger_leaf.h"

/*
    あぶないはっぱ（もたせる道具・ITEM_DANGER_LEAF 0x348・2026-10-09 ユーザー「あぶないはっぱってアイテム追加して
    ステータスがぐぐーんと上昇するけど永続混乱状態」→ もたせる道具・5 つの能力 +3・何をしても治らない・タマムシデパートで売る）。
    資料: docs/research/2026-10-09_danger-leaf.md

    - 場に出るたびに（CFRU の ItemBattleEffects の ItemEffects_SwitchIn・src/item_battle_effects.c）こうげき・ぼうぎょ・
      すばやさ・とくこう・とくぼう を 3 段階上げ（+6 まで）、こんらんにする。マイペースでも こんらんにする（特性の確かめを通さない）。
      1 回の登場で 1 回だけ: gDisableStructs[bank].unk18_a_2（素も CFRU も使っていない 2 ビット・交代で 0 に戻る）に印。
    - ★ ユーザー「メンタルハーブとかと同じで戦闘時に消費する系で」: 効いたら はっぱは なくなる（removeitem）。
      ★ 「バトル後消費されるように例外にしてほしい」: 戦いのあとも戻らない（gConsumableItemEffects に入れた＝トレーナー戦の持ち物の戻しでも戻さない・
      フロンティアの施設はほかの道具と同じく全部戻す）。
      こんらんは その登場の間ずっと（引っこめれば治り、はっぱも無いので次は ふつう）。
    - 何をしても治らない: 技を出す前（CANCELLER_CONFUSED・src/attackcanceler.c）に毎回「こんらん・残り 4」に戻す
      → 数え終わって治ることがなく、マイペース・ラムのみ・キーのみ などで治されても次に動くときに また こんらんしている。
*/

#define CONFUSION_TURNS 4
#define DANGER_LEAF_DONE 1

bool8 HackDangerLeaf_SwitchIn(u8 bank)
{
	u32 stat;

	if (gDisableStructs[bank].unk18_a_2 == DANGER_LEAF_DONE)
		return FALSE;
	gDisableStructs[bank].unk18_a_2 = DANGER_LEAF_DONE;

	for (stat = STAT_STAGE_ATK; stat <= STAT_STAGE_SPDEF; ++stat)
	{
		u8 stage = gBattleMons[bank].statStages[stat - 1] + 3;
		gBattleMons[bank].statStages[stat - 1] = (stage > STAT_STAGE_MAX) ? STAT_STAGE_MAX : stage;
	}
	gBattleMons[bank].status2 = (gBattleMons[bank].status2 & ~STATUS2_CONFUSION) | CONFUSION_TURNS;

	gBattleScripting.bank = gBankTarget = bank;
	gStringBank = bank;
	return TRUE;
}

void HackDangerLeaf_KeepConfused(u8 bank)
{
	if (gDisableStructs[bank].unk18_a_2 == DANGER_LEAF_DONE)   //はっぱは使ってなくなっているので印で見る（場にいる間ずっと）
		gBattleMons[bank].status2 = (gBattleMons[bank].status2 & ~STATUS2_CONFUSION) | CONFUSION_TURNS;
}
