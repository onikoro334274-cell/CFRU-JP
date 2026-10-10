#include "defines.h"
#include "defines_battle.h"

/*
 * カントーのジムリーダーの 1 回目の戦いでもダイマックスを使えるように（2026-10-03 ユーザー「1回目からで」）。
 * 素の FRLG のジムのスクリプトは FLAG_DYNAMAX_BATTLE（0x918）を立てないので、戦いを始めるときに番号で見る
 * （src/overworld.c・patches/0144）。再戦（927〜934・assembly/kanto/kanto_leader_rematch.s）は今までどおりスクリプトが旗を立てる。
 * ダイマックスバンドは最初にお母さんからもらう（assembly/kanto/kanto_home_scripts.s）。
 */

//pokefirered include/constants/opponents.h（番号は素の FRLG と同じ・tools/kanto/build_kanto_trainers.py も同じ番号を置き直す）
static const u16 sKantoGymLeaders[] =
{
	414, //TRAINER_LEADER_BROCK
	415, //TRAINER_LEADER_MISTY
	416, //TRAINER_LEADER_LT_SURGE
	417, //TRAINER_LEADER_ERIKA
	418, //TRAINER_LEADER_KOGA
	419, //TRAINER_LEADER_BLAINE
	420, //TRAINER_LEADER_SABRINA
	350, //TRAINER_LEADER_GIOVANNI
};

bool8 HackKanto_IsGymLeaderBattle(void)
{
	u32 i;

	if (!(gBattleTypeFlags & BATTLE_TYPE_TRAINER))
		return FALSE;
	for (i = 0; i < ARRAY_COUNT(sKantoGymLeaders); ++i)
	{
		if (gTrainerBattleOpponent_A == sKantoGymLeaders[i])
			return TRUE;
	}
	return FALSE;
}
