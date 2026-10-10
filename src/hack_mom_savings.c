#include "defines.h"
#include "defines_battle.h"
#include "../include/event_data.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/item.h"
#include "../include/money.h"
#include "../include/script.h"
#include "../include/string_util.h"
#include "../include/constants/items.h"

/*
 * お母さんの貯金（金銀の「ははおやの ちょきん」・2026-10-06）。
 * ユーザー「金銀の母親貯金みたいなの実装できそう？」→ 割合は自分で決める（10〜50%）・
 * 買い物の知らせは電話の演出・道具は家に帰ったとき渡す。
 *
 *  - トレーナー戦の賞金: 戦闘命令 0x5D（素の getmoneyreward 080250A0）を HackMomSavings_GetMoneyReward に向け
 *    （CFRU assembly/data/battle_script_commands_table.s・patches/0190）、素の処理のあと増えた分の割合を貯金へ。
 *  - 貯金が表の次の値段に届くとお母さんが道具を買う（貯金から引く）→ 電話の印を立てる。
 *  - 電話: フィールドの毎コマ（src/overworld_speedup.c）。スクリプトが動いていない・主人公が止まっているときに
 *    EventScript_MomSavingsCall（assembly/kanto/kanto_mom_savings.s）を始める。
 *  - 家のお母さん（kanto_home_scripts.s の いつもの会話の手前）: 買った道具を渡す・貯金の額・ひきだす・割合・はじめる／やめる。
 *  状態は予約変数（build_hoenn_scripts.RESERVED_VARS・状態は書き換え可能な static に置けない＝6.13）。
 */

#define VAR_MOM_SAVINGS_LO 0x50F8
#define VAR_MOM_SAVINGS_HI 0x50F9
#define VAR_MOM_SETTINGS   0x50FA   //bit 15 = ためる・下 8 ビット = 割合（%）
#define VAR_MOM_PENDING    0x50FB   //家で渡す道具（0 = なし）
#define VAR_MOM_STEP       0x50FC   //下 8 ビット = 次に買う表の番号・bit 15 = 電話がまだ
#define VAR_MOM_PENDING_COUNT 0x50ED   //★ 2026-10-10 まだ渡していない数（表の「次に買う番号」の手前から数える）・build_hoenn_scripts.RESERVED_VARS
#define MOM_PENDING_MAX 99
extern const u8 gText_MomSav_Etc[];

#define SETTING_ON 0x8000
#define CALL_PENDING 0x8000
#define DEFAULT_PERCENT 20
#define WILD_YEN_PER_LEVEL 10   //野生に勝ったとき 相手のレベル × これ（2026-10-08）
#define SAVINGS_MAX 9999999
#define PLAYER_MONEY_MAX 9999999   //AddMoney（0809FC80）の上限 0x0098967F

extern const u8 EventScript_MomSavingsCall[];

struct MomPurchase
{
	u32 price;
	u16 item;
};

//買う順（最後まで買ったら最初へ）。値段は貯金から引く
//★ 2026-10-10 ユーザー「ママの買い物リスト マスターボール・タマムシデパートの商品（4F 進化の石・つながりのヒモ・5F ドーピング・どりょくリセット・
//  とくせいパッチ・カプセル）・王冠・ミント」・値段は「Claude におまかせ」→ 値段 0 は道具の表の値段（お店と同じ）。
//  並びの順に買う（次に買う番号は VAR_MOM_STEP の下 8 ビット・表を伸ばしても前のセーブの番号はそのまま使える）。
#define ITEM_EV_RESET 0x340
static const struct MomPurchase sMomPurchases[] =
{
	{3000,   ITEM_EXP_CANDY_S},
	{10000,  ITEM_EXP_CANDY_M},
	{20000,  ITEM_BOTTLE_CAP},
	{30000,  ITEM_PP_UP},
	{50000,  ITEM_ABILITY_CAPSULE},
	{100000, ITEM_GOLD_BOTTLE_CAP},
	{100000, ITEM_ABILITY_PATCH},
	{0, ITEM_FIRE_STONE}, {0, ITEM_THUNDER_STONE}, {0, ITEM_WATER_STONE}, {0, ITEM_LEAF_STONE}, {0, ITEM_MOON_STONE},
	{0, ITEM_SUN_STONE}, {0, ITEM_SHINY_STONE}, {0, ITEM_DUSK_STONE}, {0, ITEM_DAWN_STONE}, {0, ITEM_ICE_STONE},
	{0, ITEM_LINK_CABLE},
	{0, ITEM_HP_UP}, {0, ITEM_PROTEIN}, {0, ITEM_IRON}, {0, ITEM_CALCIUM}, {0, ITEM_ZINC}, {0, ITEM_CARBOS},
	{0, ITEM_EV_RESET},
	{0, ITEM_LONELY_MINT}, {0, ITEM_ADAMANT_MINT}, {0, ITEM_NAUGHTY_MINT}, {0, ITEM_BRAVE_MINT}, {0, ITEM_BOLD_MINT},
	{0, ITEM_IMPISH_MINT}, {0, ITEM_LAX_MINT}, {0, ITEM_RELAXED_MINT}, {0, ITEM_MODEST_MINT}, {0, ITEM_MILD_MINT},
	{0, ITEM_RASH_MINT}, {0, ITEM_QUIET_MINT}, {0, ITEM_CALM_MINT}, {0, ITEM_GENTLE_MINT}, {0, ITEM_CAREFUL_MINT},
	{0, ITEM_SASSY_MINT}, {0, ITEM_TIMID_MINT}, {0, ITEM_HASTY_MINT}, {0, ITEM_JOLLY_MINT}, {0, ITEM_NAIVE_MINT},
	{0, ITEM_SERIOUS_MINT},
	{32768, ITEM_MASTER_BALL},   //シルフカンパニー 1F の売り場（65535）の半分（2026-10-10 ユーザー「シルフカンパニーの金額から半額で」）
	{0, ITEM_MOEMON_FOSSIL},    //Moemonかせき（10000 円・手に入るのはここだけ＝2026-10-10 ユーザー「「Moemonかせき」はママのみで」・src/hack_moemon_fossil.c）
	{0, ITEM_GOLD_CAPSULE},     //おうごんのカプセル（15000 円・手に入るのはここだけ＝2026-10-10 ユーザー「お母さんの買い物」）
};

static u32 GetSavings(void)
{
	return VarGet(VAR_MOM_SAVINGS_LO) | ((u32) VarGet(VAR_MOM_SAVINGS_HI) << 16);
}

static void SetSavings(u32 v)
{
	if (v > SAVINGS_MAX)
		v = SAVINGS_MAX;
	VarSet(VAR_MOM_SAVINGS_LO, v & 0xFFFF);
	VarSet(VAR_MOM_SAVINGS_HI, v >> 16);
}

static u8 GetPercent(void)
{
	u8 p = VarGet(VAR_MOM_SETTINGS) & 0xFF;
	return (p == 0) ? DEFAULT_PERCENT : p;
}

static u32 PurchasePrice(u8 index)
{
	u32 p = sMomPurchases[index].price;
	if (p == 0)
		p = ItemId_GetPrice(sMomPurchases[index].item);
	return (p == 0) ? 1000 : p;
}

//★ 2026-10-10 ユーザー「ママのかいものを一気に渡すように」: 1 つずつ（家で受け取るまで次を買わない）をやめ、貯金で買えるだけ続けて買う
//  （まだ渡していない数は VAR_MOM_PENDING_COUNT・99 まで）。家で全部まとめて渡す（HackMomSavings_GiveAll）
static void TryMomPurchase(void)
{
	u16 step = VarGet(VAR_MOM_STEP);
	u8 index = (step & 0xFF) % ARRAY_COUNT(sMomPurchases);
	u32 savings = GetSavings();
	u16 count = VarGet(VAR_MOM_PENDING_COUNT);
	bool8 bought = FALSE;

	while (count < MOM_PENDING_MAX && savings >= PurchasePrice(index))
	{
		savings -= PurchasePrice(index);
		CopyItemName(sMomPurchases[index].item, gStringVar1);   //電話で言う名前（最後に買ったもの）
		index = (index + 1) % ARRAY_COUNT(sMomPurchases);
		++count;
		bought = TRUE;
	}
	if (!bought)
		return;
	SetSavings(savings);
	VarSet(VAR_MOM_PENDING_COUNT, count);
	VarSet(VAR_MOM_STEP, index | CALL_PENDING);
}

//★ 2026-10-09 ユーザー「ママの貯金金額を表示するようにしてほしい」→「おかね送る状態のときにひょうじしてほしい」:
//  賞金を送ったとき、素の「〜は ○えん もらった！」のあとに「ママに ○えん おくった！ ちょきんは いま ○えん」を出す。
//  素の台本は getmoneyreward のあと「printstring STRINGID_PLAYERGOTMONEY（10 xx xx）/ waitmessage（12 xx xx）」の 6 バイト。
//  その 6 バイトの代わりに BattleScript_HackMomSavingsSent（assembly/hack_mom_savings/）を通し、終わったらその次へ戻る。並びが違えば何もしない
extern const u8 BattleScript_HackMomSavingsSent[];

static void ShowSentMessage(u32 put)
{
	const u8* next = gBattlescriptCurrInstr;

	if (next[0] != 0x10 || next[3] != 0x12)
		return;
	ConvertIntToDecimalStringN(gStringVar1, put, STR_CONV_MODE_LEFT_ALIGN, 7);
	ConvertIntToDecimalStringN(gStringVar2, GetSavings(), STR_CONV_MODE_LEFT_ALIGN, 7);
	gBattlescriptCurrInstr = next + 6;
	BattleScriptPushCursor();
	gBattlescriptCurrInstr = BattleScript_HackMomSavingsSent;
}

//戦闘命令 0x5D（getmoneyreward）
void HackMomSavings_GetMoneyReward(void)
{
	u32 before = GetMoney(&gSaveBlock1->money);
	u32 after;

	((void (*)(void)) (0x080250A0 | 1))(); //素の getmoneyreward（賞金を渡す・文の数字を用意する）

	after = GetMoney(&gSaveBlock1->money);
	if (after > before && (VarGet(VAR_MOM_SETTINGS) & SETTING_ON))
	{
		u32 put = (after - before) * GetPercent() / 100;
		if (put > 0)
		{
			RemoveMoney(&gSaveBlock1->money, put);
			//★ 2026-10-08 ユーザー「タメルでもたまりにくいみたい」→「お母さんが上乗せ」: ためた分と同じだけお母さんが足す
			SetSavings(GetSavings() + put * 2);
			TryMomPurchase();
			ShowSentMessage(put);
		}
	}
}

//★ 2026-10-08 ユーザー「野生の戦いでも貯まる」: 野生に勝ったとき（CFRU の src/end_battle.c の EndOfBattleThings・patches/0224）、
//  たおした相手のレベル × WILD_YEN_PER_LEVEL 円をお母さんがためる（おこづかいは減らない・割合は関係しない・上乗せは無し）
void HackMomSavings_WildWin(void)
{
	u32 add = 0, i;

	if (!(VarGet(VAR_MOM_SETTINGS) & SETTING_ON))
		return;
	if (gBattleTypeFlags & (BATTLE_TYPE_TRAINER | BATTLE_TYPE_LINK | BATTLE_TYPE_OLD_MAN | BATTLE_TYPE_SAFARI | BATTLE_TYPE_POKE_DUDE | BATTLE_TYPE_GHOST))
		return;
	if (gBattleOutcome != B_OUTCOME_WON)
		return;
	for (i = 0; i < PARTY_SIZE; ++i)
	{
		if (GetMonData(&gEnemyParty[i], MON_DATA_SPECIES, NULL) != SPECIES_NONE)
			add += GetMonData(&gEnemyParty[i], MON_DATA_LEVEL, NULL) * WILD_YEN_PER_LEVEL;
	}
	if (add > 0)
	{
		SetSavings(GetSavings() + add);
		TryMomPurchase();
	}
}

//フィールドの毎コマ: 電話
void HackMomSavings_FieldTick(void)
{
	if (!(VarGet(VAR_MOM_STEP) & CALL_PENDING))
		return;
	if (ScriptContext2_IsEnabled())
		return;
	if (gPlayerAvatar->tileTransitionState != T_NOT_MOVING || gPlayerAvatar->runningState != NOT_MOVING)
		return;
	//★ 2026-10-10 ユーザー「お母さんからの電話は家にいるときは来ないように」: 自分の家（マサラタウンの 1F・2F・群 4 の 0／1）では鳴らさない（出たら鳴る）
	if (gSaveBlock1->location.mapGroup == 4 && (u8) gSaveBlock1->location.mapNum <= 1)
		return;

	VarSet(VAR_MOM_STEP, VarGet(VAR_MOM_STEP) & ~CALL_PENDING);
	{
		//電話: 「ちょきんで [BUFFER1]を かっておいたわ」＝最後に買ったもの（2 こ以上なら「〜など」）
		u16 count = VarGet(VAR_MOM_PENDING_COUNT);
		u8 last = ((VarGet(VAR_MOM_STEP) & 0xFF) + ARRAY_COUNT(sMomPurchases) - 1) % ARRAY_COUNT(sMomPurchases);
		if (count == 0 && VarGet(VAR_MOM_PENDING) != ITEM_NONE)
			CopyItemName(VarGet(VAR_MOM_PENDING), gStringVar1);
		else
			CopyItemName(sMomPurchases[last].item, gStringVar1);
		if (count + (VarGet(VAR_MOM_PENDING) != ITEM_NONE) > 1)
			StringAppend(gStringVar1, gText_MomSav_Etc);
	}
	ScriptContext1_SetupScript(EventScript_MomSavingsCall);
}

//以下はスクリプトから（callasm）

//★ 2026-10-10 家で全部まとめて渡す（バッグに入れるだけ・入らなかった分は次に）。
//  LASTRESULT 0 = 渡すものが無い / 1 = 全部渡した / 2 = バッグがいっぱいで残った。gStringVar1 = 渡した数
void HackMomSavings_GiveAll(void)
{
	u16 count = VarGet(VAR_MOM_PENDING_COUNT);
	u8 next = (VarGet(VAR_MOM_STEP) & 0xFF) % ARRAY_COUNT(sMomPurchases);
	u8 start = (next + ARRAY_COUNT(sMomPurchases) - (count % ARRAY_COUNT(sMomPurchases))) % ARRAY_COUNT(sMomPurchases);
	u16 given = 0;
	u16 legacy = VarGet(VAR_MOM_PENDING);   //前の形（1 つだけの道具）

	if (count == 0 && legacy == ITEM_NONE)
	{
		gSpecialVar_LastResult = 0;
		return;
	}
	gSpecialVar_LastResult = 1;
	if (legacy != ITEM_NONE)
	{
		if (AddBagItem(legacy, 1))
		{
			VarSet(VAR_MOM_PENDING, ITEM_NONE);
			++given;
		}
		else
			gSpecialVar_LastResult = 2;
	}
	while (count > 0 && gSpecialVar_LastResult == 1)
	{
		if (!AddBagItem(sMomPurchases[start].item, 1))
		{
			gSpecialVar_LastResult = 2;
			break;
		}
		start = (start + 1) % ARRAY_COUNT(sMomPurchases);
		--count;
		++given;
	}
	VarSet(VAR_MOM_PENDING_COUNT, count);
	if (count == 0 && VarGet(VAR_MOM_PENDING) == ITEM_NONE)
		VarSet(VAR_MOM_STEP, VarGet(VAR_MOM_STEP) & ~CALL_PENDING);   //★ 2026-10-10 家で全部受け取ったら、まだ鳴っていない電話は取り消す
	ConvertIntToDecimalStringN(gStringVar1, given, STR_CONV_MODE_LEFT_ALIGN, 3);
	if (given == 0)
		gSpecialVar_LastResult = 3;   //1 つも入らなかった
}

//LASTRESULT = 家で渡す道具（0 = なし）。0x8000 にも入れる（obtainitem 0x8000）
void HackMomSavings_TakePending(void)
{
	Var8000 = VarGet(VAR_MOM_PENDING);
	gSpecialVar_LastResult = Var8000;
}

//渡し終えた（obtainitem のあと）
void HackMomSavings_ClearPending(void)
{
	VarSet(VAR_MOM_PENDING, ITEM_NONE);
	TryMomPurchase(); //貯金がまだ足りていれば次を買う（電話はまたかかる）
}

//gStringVar1 = 貯金・gStringVar2 = 割合。LASTRESULT = ためているか
void HackMomSavings_BufferStatus(void)
{
	ConvertIntToDecimalStringN(gStringVar1, GetSavings(), STR_CONV_MODE_LEFT_ALIGN, 7);
	ConvertIntToDecimalStringN(gStringVar2, GetPercent(), STR_CONV_MODE_LEFT_ALIGN, 3);
	gSpecialVar_LastResult = (VarGet(VAR_MOM_SETTINGS) & SETTING_ON) != 0;
}

void HackMomSavings_ToggleOn(void)
{
	u16 s = VarGet(VAR_MOM_SETTINGS);
	if ((s & 0xFF) == 0)
		s |= DEFAULT_PERCENT;
	VarSet(VAR_MOM_SETTINGS, s ^ SETTING_ON);
	gSpecialVar_LastResult = (VarGet(VAR_MOM_SETTINGS) & SETTING_ON) != 0;
}

//LASTRESULT（0〜6）→ 10・20・30・40・50・75・100%（2026-10-08 ユーザー「割合を 100% まで」）
void HackMomSavings_SetPercent(void)
{
	static const u8 sPercents[] = {10, 20, 30, 40, 50, 75, 100};
	u16 s = VarGet(VAR_MOM_SETTINGS) & SETTING_ON;
	u8 i = gSpecialVar_LastResult < ARRAY_COUNT(sPercents) ? gSpecialVar_LastResult : 1;
	VarSet(VAR_MOM_SETTINGS, s | sPercents[i]);
	ConvertIntToDecimalStringN(gStringVar2, GetPercent(), STR_CONV_MODE_LEFT_ALIGN, 3);
}

//できるだけひきだす（おこづかいの上限まで）。gStringVar1 = ひきだした額・LASTRESULT = ひきだした額が 0 でない
void HackMomSavings_Withdraw(void)
{
	u32 savings = GetSavings();
	u32 money = GetMoney(&gSaveBlock1->money);
	u32 room = (money < PLAYER_MONEY_MAX) ? PLAYER_MONEY_MAX - money : 0;
	u32 take = (savings < room) ? savings : room;

	if (take > 0)
	{
		AddMoney(&gSaveBlock1->money, take);
		SetSavings(savings - take);
	}
	ConvertIntToDecimalStringN(gStringVar1, take, STR_CONV_MODE_LEFT_ALIGN, 7);
	gSpecialVar_LastResult = take != 0;
}
