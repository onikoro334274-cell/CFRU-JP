#include "defines.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"

/*
 * ホウエンのストーリーの「相棒」（#57・2026-10-08・tools/rom/hoenn_story.py の終章・docs/research/2026-10-08_hoenn-original-story.md）。
 * ユーザー「最後に何がもらえるか相棒ポケモン一体できる　相棒ポケモンにすると種族値計算で全部が+20された状態」
 *         「相棒は選び直しができないで　きょせいどうぐとか使えなくして」。
 *
 * どの個体が相棒か: 旗 0x12DF（決めた）＋ 性格値（変数 0x50F0 下位・0x50F1 上位）＋ 親の ID（0x50F2 下位・0x50F3 上位）。
 *   性格値を書き換えるのは きょせいどうぐ だけ（6.5）→ 相棒には使えなくした（src/hack_gender_tool.c）ので、この 2 つで見分けられる。
 *   ボックスに預けても・交換して戻っても同じ個体なら相棒のまま。
 * 種族値: 6 つとも +20（src/build_pokemon.c の CalculateMonStatsNew・HP は patches/0216・ほかは HackGibleEgg_BaseStat の後ろで足す）。
 * 選び直し: 旗が立っていたら決め直さない（スクリプトも 2 回目は通らない）。
 */

#define FLAG_STORY_PARTNER  0x12DF
#define VAR_PARTNER_PID_LO  0x50F0
#define VAR_PARTNER_PID_HI  0x50F1
#define VAR_PARTNER_OTID_LO 0x50F2
#define VAR_PARTNER_OTID_HI 0x50F3
#define PARTNER_BONUS       20


bool8 HackPartner_IsMon(const struct Pokemon* mon)
{
	u32 pid, otId;

	if (!FlagGet(FLAG_STORY_PARTNER))
		return FALSE;
	pid = VarGet(VAR_PARTNER_PID_LO) | (VarGet(VAR_PARTNER_PID_HI) << 16);
	otId = VarGet(VAR_PARTNER_OTID_LO) | (VarGet(VAR_PARTNER_OTID_HI) << 16);
	return mon->personality == pid && mon->otid == otId;
}

//種族値に足す分（相棒なら 20）
u8 HackPartner_BaseBonus(const struct Pokemon* mon)
{
	return HackPartner_IsMon(mon) ? PARTNER_BONUS : 0;
}

//callnative: 手持ちの 0x8004 番を相棒にできるか -> LASTRESULT 0 できる / 1 できない（タマゴ・選ばなかった）
void HoennNative_StoryPartnerCheck(void)
{
	u16 slot = Var8004;

	gSpecialVar_LastResult = 1;
	if (slot >= PARTY_SIZE)
		return;
	if (GetMonData(&gPlayerParty[slot], MON_DATA_SPECIES, NULL) == SPECIES_NONE
	 || GetMonData(&gPlayerParty[slot], MON_DATA_IS_EGG, NULL))
		return;
	gSpecialVar_LastResult = 0;
}

//callnative: 手持ちの 0x8004 番を相棒にする（旗が立っていたら何もしない＝選び直せない）
void HoennNative_StoryMakePartner(void)
{
	struct Pokemon* mon;

	if (FlagGet(FLAG_STORY_PARTNER) || Var8004 >= PARTY_SIZE)
		return;
	mon = &gPlayerParty[Var8004];
	if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE
	 || GetMonData(mon, MON_DATA_IS_EGG, NULL))
		return;
	VarSet(VAR_PARTNER_PID_LO, mon->personality & 0xFFFF);
	VarSet(VAR_PARTNER_PID_HI, mon->personality >> 16);
	VarSet(VAR_PARTNER_OTID_LO, mon->otid & 0xFFFF);
	VarSet(VAR_PARTNER_OTID_HI, mon->otid >> 16);
	FlagSet(FLAG_STORY_PARTNER);
	CalculateMonStats(mon);
}
