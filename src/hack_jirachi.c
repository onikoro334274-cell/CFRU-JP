#include "defines.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/constants/game_stat.h"
#include "../include/new/Vanilla_functions.h"
#include "../include/pokemon.h"
#include "../include/constants/species.h"
#include "../include/new/catching.h"
#include "../include/new/util.h"

/*
 * トクサネシティの白い岩のジラーチ（2026-10-07）。
 * ユーザー「トクサネシティ　白い石　ジラーチ　白い岩に7回連続で話しかけるとジラーチプレシャスボール入が手に入る」
 *
 *   - 白い岩（エメラルドの看板 (57, 21)・MossdeepCity_EventScript_WhiteRock）を build_hoenn_scripts の PATTERNS で
 *     assembly/hack_jirachi/jirachi_script.s へ向ける。7 回目で渡す・1 度だけ（印 0x12EA）
 *   - ユーザー「連続じゃなければ無理なように」: 間に別のことをしたら 1 回目から数え直す（HackJirachi_CountRock）
 *       歩いた（歩数 GAME_STAT_STEPS が変わった）・向きを変えた・ほかの人やポケモンに話しかけた（LASTTALKED が書き換わった）・
 *       地図を出た（一時変数が 0 に戻る）。メニューやバッグを開いただけは見分けられない（数え直さない）
 *   - Lv5（エメラルドのころの配布「ねがいぼし」と同じ）・プレシャスボール・個体値・性格は CreateMon のまま（色違いの抽選もふつう）
 */

#define JIRACHI_LEVEL 5
#define ROCK_TIMES 7
#define VAR_ROCK_STEPS_LO 0x400B  //一時変数（エメラルドのトクサネのスクリプトが使うのは VAR_TEMP_1 だけ）
#define VAR_ROCK_STEPS_HI 0x400C
#define VAR_ROCK_FACING   0x400D
#define VAR_ROCK_COUNT    0x400E
#define ROCK_LAST_TALKED  0xFC    //岩に話しかけた印（人に話しかけると その人の番号に書き換わる）

//白い岩に話しかけるたびに呼ぶ。続けての回数を数え、7 回目なら LASTRESULT = TRUE（回数は 0 に戻す）
void HackJirachi_CountRock(void)
{
	u32 steps = GetGameStat(GAME_STAT_STEPS);
	u16 facing = gEventObjects[gPlayerAvatar->eventObjectId].facingDirection;
	u16 count = VarGet(VAR_ROCK_COUNT);

	if (count == 0
	|| VarGet(VAR_ROCK_STEPS_LO) != (u16) steps
	|| VarGet(VAR_ROCK_STEPS_HI) != (u16) (steps >> 16)
	|| VarGet(VAR_ROCK_FACING) != facing
	|| gSpecialVar_LastTalked != ROCK_LAST_TALKED)
		count = 1; //1 回目から
	else
		++count;

	VarSet(VAR_ROCK_STEPS_LO, (u16) steps);
	VarSet(VAR_ROCK_STEPS_HI, (u16) (steps >> 16));
	VarSet(VAR_ROCK_FACING, facing);
	gSpecialVar_LastTalked = ROCK_LAST_TALKED;

	if (count >= ROCK_TIMES)
	{
		VarSet(VAR_ROCK_COUNT, 0);
		gSpecialVar_LastResult = TRUE;
	}
	else
	{
		VarSet(VAR_ROCK_COUNT, count);
		gSpecialVar_LastResult = FALSE;
	}
}

void HackJirachi_Give(void)
{
	struct Pokemon mon;
	u8 ball = BALL_TYPE_CHERISH_BALL;

	CreateMon(&mon, SPECIES_JIRACHI, JIRACHI_LEVEL, 32, FALSE, 0, OT_ID_PLAYER_ID, 0); //個体値 32 以上 = ふつうの抽選
	SetMonData(&mon, MON_DATA_POKEBALL, &ball);
	CalculateMonStats(&mon);
	SetMonPokedexFlags(&mon);
	gSpecialVar_LastResult = GiveMonToPlayer(&mon);
}
