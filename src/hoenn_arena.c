#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/party_menu.h"

#include "../include/new/battle_util.h"
#include "Tables/hoenn_script_flags.h"

/*
 * ホウエンのバトルアリーナの戦い（#5・2026-09-29 ユーザー「バトルアリーナ進めて」）
 * docs/research/2026-09-28_battle-frontier-npcs.md の「バトルアリーナの進め方」① 戦いの仕組み
 *
 * エメラルドのアリーナは 1 対 1 で 3 ターン戦い、決着がつかなければ審判の判定（こころ・わざ・からだ）で負けた子が倒れた扱い。
 * 入れ替えはできず、倒れたら次の子が並び順で出る。pokeemerald の次の所を CFRU の上に書き直した:
 *   - 点数と判定            … src/battle_arena.c（BattleArena_InitPoints / AddMindPoints / AddSkillPoints / DeductSkillPoints / ShowJudgmentSprite）
 *   - こころの点の表        … sMindRatings（354 技を数えて決まりに直した: 威力のある技 +1・カウンター／がまん／ミラーコート 0・
 *                             まもる／みきり／こらえる／ねこだまし −1・ほかは 0）
 *   - ターンの数え方        … src/battle_main.c（入れ替えで 0xFF・ターンの終わりに +1・0 になったターンの頭に「しょうぶ！ はじめ」）・
 *                             src/battle_util.c:1856（ターンの終わりの効果のあとで 2 なら判定＝3 ターン）
 *   - 負けた子              … src/battle_script_commands.c:6409（VARIOUS_ARENA_*_MON_LOST）
 *   - 入れ替え禁止・次の子  … src/battle_main.c:4243・src/battle_controller_player.c:2672・src/battle_ai_switch_items.c:452 / 648
 *
 * 差し込み口（上流は数行だけ・patches/0096）:
 *   - 戦いの始め           … CFRU src/battle_start_turn_start.c BattleBeginFirstTurn の最後 → HoennArena_OnBattleStart
 *   - こころ               … 同 HandleAction_UseMove（バトルパレスと同じ所）→ HoennArena_OnUseMove
 *   - わざ                 … 戦闘スクリプトの命令 end（0x3D・assembly/data/battle_script_commands_table.s）→ HoennArena_AtkEnd
 *   - わざの減点           … CFRU src/battle_strings.c PrepareStringBattle → HoennArena_OnPrepareString
 *   - 判定                 … CFRU src/end_turn.c HandleDynamaxOnTurnEnd の頭（素の BattleTurnPassed で倒れた子の入れ替えのあと）→ HoennArena_TryJudgment
 *   - ターンの終わり       … CFRU src/trainer_sliding.c CheckLastMonLowHPSlide（素の BattleTurnPassed の最後）→ HoennArena_OnTurnPassed
 *   - 入れ替わり           … CFRU src/switching.c atk4D_switchindataupdate → HoennArena_OnSwitchIn
 *   - 入れ替え禁止         … CFRU src/switching.c PartyMenuSwitchingUpdate（逃げられないのと同じ PARTY_CANT_SWITCH）
 *   - 次の子（主人公）     … CFRU src/multi.c PlayerHandleChoosePokemon → HoennArena_PlayerChooseNext
 *   - 次の子（相手）       … CFRU src/battle_controller_opponent.c OpponentHandleChoosePokemon・src/Battle_AI/ai_master.c ShouldSwitch
 *
 * ★ 「アリーナの戦いか」は FLAG_BATTLE_FACILITY ＋ HOENN_VAR_FRONTIER_FACILITY == アリーナ ＋ トレーナー戦 ＋ シングル。
 * ★ 状態の置き場（6.13・static は使えない）: 一時変数 0x4008〜0x400F（地図に入ると 0・戦いの間は残る）。
 *   アリーナの部屋のスクリプトはこの 8 つを使わない（tools/frontier の生成物・検算が見る）
 *
 * エメラルドとの違い（わざと）:
 *   - 判定の窓（○△× の絵）は無い。審判のセリフ（日本語版エメラルドから）のあとに「〜の かち！」を文で出す（○ は文字の表に無い）
 *   - 負けた子は手持ちの HP も 0 にする（エメラルドは HP を残して「負けた子の印」で選べなくする。施設は戦いのあとで手持ちを元に戻す）
 *   - エメラルドの「失敗した変化技 −2」（alreadyStatusedMoveAttempt・エメラルドがアリーナのために足した印で素 FRLG に無い）は、
 *     エメラルドが印を立てる 8 か所で出す文 7 種（もう ねむっている・どく・まひ・やけど・こんらん・みがわり）が出たら技を使った側に印
 *     （2026-09-29 ユーザー「おねがい」。印が無いと「ふつうに当てた」の +1 になっていた）
 *   - わざの減点の文 19 種は素 FRLG と同じ番号で見る。CFRU が自前の文に置き換えた特性の文では減点しない
 *   - 合図の音はエメラルドの審判の笛（SE_ARENA_TIMEUP1 / 2 を移した）
 *   - こころの表は CFRU の新しい技にも同じ決まりで当てる（キングシールドなど守る技 −1・メタルバースト／しっぺがえし系の返す技 0）
 */

#define HOENN_FACILITY_ARENA 2              //HOENN_VAR_FRONTIER_FACILITY（0 タワー / 1 パレス / 2 アリーナ）

#define VAR_ARENA_MIND_0   0x4008           //こころ（主人公・相手）。s16 として読む
#define VAR_ARENA_MIND_1   0x4009
#define VAR_ARENA_SKILL_0  0x400A           //わざ
#define VAR_ARENA_SKILL_1  0x400B
#define VAR_ARENA_HP_0     0x400C           //判定の組の始めの HP（からだ）
#define VAR_ARENA_HP_1     0x400D
#define VAR_ARENA_TURN     0x400E           //エメラルドの arenaTurnCounter（入れ替えで 0xFF・ターンの終わりに +1）
#define VAR_ARENA_JUDGED   0x400F           //判定の結果（下位 2 ビット: 0 主人公の勝ち / 1 相手の勝ち / 2 引き分け）・0x80 このターン判定した・
                                            //ビット 8〜13: 3 項目の結果・ビット 14 / 15: 主人公 / 相手の「失敗した変化技」の印（エメラルドの alreadyStatusedMoveAttempt）
#define ARENA_JUDGED_FLAG  0x80
#define ARENA_FAILED_BIT(bank) (1 << (14 + (bank)))

#define ARENA_RESULT_PLAYER_WON  0
#define ARENA_RESULT_PLAYER_LOST 1
#define ARENA_RESULT_TIE         2

#define MISS_TYPE_INDEX 6                   //素 FRLG の MISS_TYPE（pokefirered include/constants/battle_script_commands.h）
#define B_MSG_PROTECTED 1
#define CHAR_0 0xA1

#define VanillaAtkEnd ((void (*)(void)) (0x080224CC | 1))   //素の戦闘スクリプトの命令 end（0x3D）

extern const u8 HoennArena_BattleScript_TurnBeginning[];
extern const u8 HoennArena_BattleScript_Judgment[];
extern const u8 HoennArena_BattleScript_PlayerWon[];
extern const u8 HoennArena_BattleScript_PlayerLost[];
extern const u8 HoennArena_BattleScript_Draw[];
extern const u8 HoennArenaText_WinPlayer[];     //「{主人公の子}の かち！」
extern const u8 HoennArenaText_WinOpponent[];   //「{相手の子}の かち！」
extern const u8 HoennArenaText_WinTie[];        //「ひきわけ！」
extern const u8 HoennArenaText_RefPlayerWon[];
extern const u8 HoennArenaText_RefOpponentWon[];
extern const u8 HoennArenaText_RefDraw[];

bool8 HoennArena_IsActive(void)
{
	return (gBattleTypeFlags & BATTLE_TYPE_TRAINER)
		&& !(gBattleTypeFlags & BATTLE_TYPE_DOUBLE)
		&& FlagGet(FLAG_BATTLE_FACILITY)
		&& VarGet(HOENN_VAR_FRONTIER_FACILITY) == HOENN_FACILITY_ARENA;
}

static s16 GetPoints(u16 var)
{
	return (s16) VarGet(var);
}

static void AddPoints(u16 var, s16 n)
{
	VarSet(var, (u16) (GetPoints(var) + n));
}

//エメラルドの BattleArena_InitPoints（戦いの始め・入れ替わったとき）
static void InitPoints(void)
{
	VarSet(VAR_ARENA_MIND_0, 0);
	VarSet(VAR_ARENA_MIND_1, 0);
	VarSet(VAR_ARENA_SKILL_0, 0);
	VarSet(VAR_ARENA_SKILL_1, 0);
	VarSet(VAR_ARENA_HP_0, gBattleMons[0].hp);
	VarSet(VAR_ARENA_HP_1, gBattleMons[1].hp);
}

//戦いの始め（BattleBeginFirstTurn の最後・ターンの行動選びへ移る直前）。エメラルドは最初のターンの頭に「しょうぶ！ はじめ」
void HoennArena_OnBattleStart(void)
{
	if (!HoennArena_IsActive())
		return;
	InitPoints();
	VarSet(VAR_ARENA_TURN, 0);
	VarSet(VAR_ARENA_JUDGED, 0);
	BattleScriptExecute(HoennArena_BattleScript_TurnBeginning);
}

//エメラルドの sMindRatings を決まりに直したもの（上の説明）
static s8 MindRating(u16 move)
{
	switch (move)
	{
		case MOVE_PROTECT:
		case MOVE_DETECT:
		case MOVE_ENDURE:
		case MOVE_FAKEOUT:
		case MOVE_KINGSSHIELD:      //ここから CFRU の新しい守る技（エメラルドに無い・同じ仲間として −1）
		case MOVE_SPIKYSHIELD:
		case MOVE_BANEFULBUNKER:
		case MOVE_OBSTRUCT:
		case MOVE_SILKTRAP:
		case MOVE_BURNINGBULWARK:
		case MOVE_MAX_GUARD:
			return -1;
		case MOVE_COUNTER:
		case MOVE_BIDE:
		case MOVE_MIRRORCOAT:
		case MOVE_METALBURST:       //CFRU の新しい返す技（同じ仲間として 0）
		case MOVE_COMEUPPANCE:
			return 0;
	}
	return gBattleMoves[move].power != 0 ? 1 : 0;
}

//技を出すとき（HandleAction_UseMove）。エメラルドの BattleArena_AddMindPoints
void HoennArena_OnUseMove(void)
{
	if (!HoennArena_IsActive() || gBankAttacker > 1)
		return;
	AddPoints(gBankAttacker == 0 ? VAR_ARENA_MIND_0 : VAR_ARENA_MIND_1, MindRating(gCurrentMove));
}

//エメラルドの BattleArena_AddSkillPoints（技の終わり）
static void AddSkillPoints(u8 bank)
{
	u16 var = (bank == 0) ? VAR_ARENA_SKILL_0 : VAR_ARENA_SKILL_1;

	if (!(gHitMarker & HITMARKER_OBEYS))
		return;

	if (VarGet(VAR_ARENA_JUDGED) & ARENA_FAILED_BIT(bank)) //もう まひしている相手に でんじは など（エメラルドの failedMoveBits）
	{
		VarSet(VAR_ARENA_JUDGED, VarGet(VAR_ARENA_JUDGED) & ~ARENA_FAILED_BIT(bank));
		AddPoints(var, -2);
	}
	else if (gMoveResultFlags & MOVE_RESULT_NO_EFFECT)
	{
		if (!(gMoveResultFlags & MOVE_RESULT_MISSED) || gBattleCommunication[MISS_TYPE_INDEX] != B_MSG_PROTECTED)
			AddPoints(var, -2);
	}
	else if ((gMoveResultFlags & MOVE_RESULT_SUPER_EFFECTIVE) && (gMoveResultFlags & MOVE_RESULT_NOT_VERY_EFFECTIVE))
		AddPoints(var, 1);
	else if (gMoveResultFlags & MOVE_RESULT_SUPER_EFFECTIVE)
		AddPoints(var, 2);
	else if (gMoveResultFlags & MOVE_RESULT_NOT_VERY_EFFECTIVE)
		AddPoints(var, -1);
	else if (!gProtectStructs[bank].protected)
		AddPoints(var, 1);
}

//戦闘スクリプトの命令 end（0x3D）の代わり。アリーナなら点数を足してから素の end へ
void HoennArena_AtkEnd(void)
{
	if (HoennArena_IsActive() && gBankAttacker <= 1)
		AddSkillPoints(gBankAttacker);
	VanillaAtkEnd();
}

//文を出すとき（PrepareStringBattle）。エメラルドの BattleArena_DeductSkillPoints（特性などで技を防がれた文 19 種で −3）
void HoennArena_OnPrepareString(u16 stringId, u8 bank)
{
	if (!HoennArena_IsActive() || bank > 1)
		return;

	switch (stringId)
	{
		//エメラルドが setalreadystatusedmoveattempt BS_ATTACKER を置く 8 か所（data/battle_scripts_1.s）が出す文。技を使った側に印
		case 37:  //STRINGID_PKMNALREADYASLEEP
		case 38:  //STRINGID_PKMNALREADYASLEEP2（ねむる）
		case 43:  //STRINGID_PKMNALREADYPOISONED
		case 58:  //STRINGID_PKMNISALREADYPARALYZED
		case 68:  //STRINGID_PKMNALREADYCONFUSED（あやしいひかり・フラフラダンス）
		case 127: //STRINGID_PKMNHASSUBSTITUTE
		case 348: //STRINGID_PKMNALREADYHASBURN
			if (gBankAttacker <= 1)
				VarSet(VAR_ARENA_JUDGED, VarGet(VAR_ARENA_JUDGED) | ARENA_FAILED_BIT(gBankAttacker));
			break;

		case 327: //STRINGID_PKMNSXMADEYUSELESS（番号は素 FRLG・エメラルドと同じ）
		case 346: //STRINGID_PKMNSXMADEITINEFFECTIVE
		case 347: //STRINGID_PKMNSXPREVENTSFLINCHING
		case 350: //STRINGID_PKMNSXBLOCKSY2
		case 309: //STRINGID_PKMNSXPREVENTSYLOSS
		case 311: //STRINGID_PKMNSXMADEYINEFFECTIVE
		case 305: //STRINGID_PKMNSXPREVENTSBURNS
		case 306: //STRINGID_PKMNSXBLOCKSY
		case 195: //STRINGID_PKMNPROTECTEDBY
		case 196: //STRINGID_PKMNPREVENTSUSAGE
		case 197: //STRINGID_PKMNRESTOREDHPUSING
		case 199: //STRINGID_PKMNPREVENTSPARALYSISWITH
		case 200: //STRINGID_PKMNPREVENTSROMANCEWITH
		case 201: //STRINGID_PKMNPREVENTSPOISONINGWITH
		case 202: //STRINGID_PKMNPREVENTSCONFUSIONWITH
		case 203: //STRINGID_PKMNRAISEDFIREPOWERWITH
		case 204: //STRINGID_PKMNANCHORSITSELFWITH
		case 206: //STRINGID_PKMNPREVENTSSTATLOSSWITH
		case 119: //STRINGID_PKMNSTAYEDAWAKEUSING
			AddPoints(bank == 0 ? VAR_ARENA_SKILL_0 : VAR_ARENA_SKILL_1, -3);
			break;
	}
}

//入れ替わったとき（atk4D_switchindataupdate）。エメラルドの SwitchInClearSetData（0xFF）＋ Cmd_switchindataupdate（InitPoints）
void HoennArena_OnSwitchIn(void)
{
	if (!HoennArena_IsActive())
		return;
	VarSet(VAR_ARENA_TURN, 0xFF);
	InitPoints();
}

//ターンの終わり（素の BattleTurnPassed の最後）。+1 して 0 になったら「しょうぶ！ はじめ」。TRUE なら戦闘スクリプトを始めた
bool8 HoennArena_OnTurnPassed(void)
{
	if (!HoennArena_IsActive())
		return FALSE;
	VarSet(VAR_ARENA_TURN, (VarGet(VAR_ARENA_TURN) + 1) & 0xFF);
	VarSet(VAR_ARENA_JUDGED, 0); //判定の印・失敗した変化技の印とも（どちらも技の終わりかターンの中で使い切る）
	if (VarGet(VAR_ARENA_TURN) == 0)
	{
		BattleScriptExecute(HoennArena_BattleScript_TurnBeginning);
		return TRUE;
	}
	return FALSE;
}

//1 項目の結果（エメラルドの ShowJudgmentSprite）→ 0 主人公の勝ち / 1 相手の勝ち / 2 引き分け
static u8 CategoryResult(u8 category)
{
	s32 p, o;

	switch (category)
	{
		case 0: //こころ
			p = GetPoints(VAR_ARENA_MIND_0);
			o = GetPoints(VAR_ARENA_MIND_1);
			break;
		case 1: //わざ
			p = GetPoints(VAR_ARENA_SKILL_0);
			o = GetPoints(VAR_ARENA_SKILL_1);
			break;
		default: //からだ（残り HP の割合）
			p = (gBattleMons[0].hp * 100) / (VarGet(VAR_ARENA_HP_0) ? VarGet(VAR_ARENA_HP_0) : 1);
			o = (gBattleMons[1].hp * 100) / (VarGet(VAR_ARENA_HP_1) ? VarGet(VAR_ARENA_HP_1) : 1);
			break;
	}
	if (p > o)
		return ARENA_RESULT_PLAYER_WON;
	if (p < o)
		return ARENA_RESULT_PLAYER_LOST;
	return ARENA_RESULT_TIE;
}

//ターンの終わりの効果と倒れた子の入れ替えのあと（HandleDynamaxOnTurnEnd の頭）。3 ターンたって両方立っていれば判定
bool8 HoennArena_TryJudgment(void)
{
	u8 cat, r, player = 0, opponent = 0;
	u16 packed = 0;

	if (!HoennArena_IsActive() || (VarGet(VAR_ARENA_JUDGED) & ARENA_JUDGED_FLAG))
		return FALSE;
	if (VarGet(VAR_ARENA_TURN) != 2 || gBattleMons[0].hp == 0 || gBattleMons[1].hp == 0)
		return FALSE;

	CancelMultiTurnMoves(0);
	CancelMultiTurnMoves(1);

	//3 項目を先に決める（エメラルドは窓を出しながら決めるが、HP も点数もこの間に変わらないので同じ）
	for (cat = 0; cat < 3; ++cat)
	{
		r = CategoryResult(cat);
		packed |= r << (2 * cat + 8);
		if (r == ARENA_RESULT_PLAYER_WON)
			player += 2;
		else if (r == ARENA_RESULT_PLAYER_LOST)
			opponent += 2;
		else
		{
			player += 1;
			opponent += 1;
		}
	}
	gBattleTextBuff1[0] = CHAR_0 + player;
	gBattleTextBuff1[1] = 0xFF;
	gBattleTextBuff2[0] = CHAR_0 + opponent;
	gBattleTextBuff2[1] = 0xFF;
	packed |= (player > opponent) ? ARENA_RESULT_PLAYER_WON : (player < opponent) ? ARENA_RESULT_PLAYER_LOST : ARENA_RESULT_TIE;
	VarSet(VAR_ARENA_JUDGED, packed | ARENA_JUDGED_FLAG); //このターン判定した（下位の 0〜2 と重ならない）

	BattleScriptExecute(HoennArena_BattleScript_Judgment);
	return TRUE;
}

//判定の戦闘スクリプトから（callasm）。1 項目の結果の文を用意する。項目は gBattleScripting の空きではなく呼ぶ順（こころ→わざ→からだ）で数える
static const u8* const sCategoryTexts[] = {HoennArenaText_WinPlayer, HoennArenaText_WinOpponent, HoennArenaText_WinTie};

static void LoadCategoryText(u8 category)
{
	u8 r = (VarGet(VAR_ARENA_JUDGED) >> (2 * category + 8)) & 3;
	gBattleStringLoader = sCategoryTexts[r];
}

void HoennArena_LoadMindResult(void)  { LoadCategoryText(0); }
void HoennArena_LoadSkillResult(void) { LoadCategoryText(1); }
void HoennArena_LoadBodyResult(void)  { LoadCategoryText(2); }

//判定の結果（審判の「はんてい X たい Y」）を用意して、結果ごとの続きへ跳ぶ（callasm は 5 足す）
void HoennArena_JudgmentResult(void)
{
	switch (VarGet(VAR_ARENA_JUDGED) & 3)
	{
		case ARENA_RESULT_PLAYER_WON:
			gBattleStringLoader = HoennArenaText_RefPlayerWon;
			gBattlescriptCurrInstr = HoennArena_BattleScript_PlayerWon - 5;
			break;
		case ARENA_RESULT_PLAYER_LOST:
			gBattleStringLoader = HoennArenaText_RefOpponentWon;
			gBattlescriptCurrInstr = HoennArena_BattleScript_PlayerLost - 5;
			break;
		default:
			gBattleStringLoader = HoennArenaText_RefDraw;
			gBattlescriptCurrInstr = HoennArena_BattleScript_Draw - 5;
			break;
	}
}

//負けた子を倒れた扱いにする（エメラルドの VARIOUS_ARENA_*_MON_LOST）。★ 手持ちの HP も 0（上の説明）
static void MonLost(u8 bank)
{
	u16 zero = 0;
	struct Pokemon* party = (bank == 0) ? gPlayerParty : gEnemyParty;

	gBattleMons[bank].hp = 0;
	SetMonData(&party[gBattlerPartyIndexes[bank]], MON_DATA_HP, &zero);
	gHitMarker |= HITMARKER_FAINTED(bank);
	if (bank == 0)
		gHitMarker |= HITMARKER_PLAYER_FAINTED;
}

void HoennArena_OpponentMonLost(void) { MonLost(1); }
void HoennArena_PlayerMonLost(void)   { MonLost(0); }
void HoennArena_BothMonsLost(void)    { MonLost(0); MonLost(1); }

//主人公が倒れた子の代わりを選ぶとき（PlayerHandleChoosePokemon）。アリーナなら次の子（並び順）を黙って出す。TRUE なら済ませた
//★ エメラルドの battle_controller_player.c:2672 と同じ: 「入れ替えられない」で開いたとき（ポケモンの画面を見るだけ）は普通に開く
bool8 HoennArena_PlayerChooseNext(void)
{
	u32 i;

	if (!HoennArena_IsActive() || (gBattleBufferA[gActiveBattler][1] & 0xF) == PARTY_CANT_SWITCH)
		return FALSE;
	for (i = 0; i < 3; ++i)
		gBattlePartyCurrentOrder[i] = gBattleBufferA[gActiveBattler][4 + i];
	EmitChosenMonReturnValue(1, gBattlerPartyIndexes[gActiveBattler] + 1, gBattlePartyCurrentOrder);
	PlayerBufferExecCompleted();
	return TRUE;
}

//相手の次の子（OpponentHandleChoosePokemon・エメラルドの GetMostSuitableMonToSwitchInto）。PARTY_SIZE ならアリーナではない
u8 HoennArena_OpponentNextMon(void)
{
	if (!HoennArena_IsActive())
		return PARTY_SIZE;
	return gBattlerPartyIndexes[gActiveBattler] + 1;
}
