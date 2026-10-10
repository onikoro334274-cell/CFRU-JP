#include "defines.h"
#include "../include/event_data.h"
#include "../include/main.h"
#include "../include/pokemon.h"
#include "../include/string_util.h"
#include "hack_mon_id.h"

/*
 * ★ 2026-10-10 ユーザー「なつきどが最大になると親が自分になるようにしてほしい」
 *
 * 手持ちの子（タマゴでない）の なつき度が 255 になり、親が主人公でなければ、親の名前・ID（表 | 裏 << 16）・性別を主人公にする。
 * - 先に HackMonId_Ensure（#70）でポケモン自身の表 ID・裏 ID を付ける＝色違いと Moémon は自身の ID で決まるので、
 *   親の ID を変えても色と見た目は変わらない（6.5・PID は触らない）
 * - ストーリーの相棒（src/hack_story_partner.c は性格値＋親の ID で見分ける）なら、覚えている親の ID も新しい値にする
 * - フロンティアの挑戦中（FLAG_BATTLE_FACILITY・レンタルの子が手持ちにいる）は何もしない
 * - フィールドで 8 コマに 1 回・1 匹ずつ（src/overworld_speedup.c・HackMonId_FieldTick のあと・2026-10-10 に毎コマ 6 匹から軽くした）
 */

bool8 HackPartner_IsMon(const struct Pokemon* mon);   //src/hack_story_partner.c
#define VAR_PARTNER_OTID_LO 0x50F2                       //src/hack_story_partner.c と同じ
#define VAR_PARTNER_OTID_HI 0x50F3
#define MAX_FRIENDSHIP 255

static u32 PlayerOtId(void)
{
	const u8* t = gSaveBlock2->playerTrainerId;
	return t[0] | (t[1] << 8) | (t[2] << 16) | ((u32) t[3] << 24);
}

static bool8 SameName(const u8* a, const u8* b)
{
	u32 i;
	for (i = 0; i < OT_NAME_LENGTH; ++i)
	{
		if (a[i] != b[i])
			return FALSE;
		if (a[i] == EOS)
			return TRUE;
	}
	return TRUE;
}

//★ 2026-10-10 重さを測った（build_rom.py --profile・tools/rom/profile_overworld.py）: 6 匹を毎コマ見ていて、何も起きていなくても
//  1 コマ平均 6.3 本（228 本のうち）使っていた（GetMonData は呼ぶたびに暗号を解く）。なつき度が 255 になるのはたまにしか起きないので、
//  8 コマに 1 回・1 匹ずつ順番に見る（6 匹で 48 コマ＝1 秒弱で一巡）。状態は持たない（6.13）＝コマの数で決める
#define FRIEND_OT_EVERY 8

void HackFriendOt_FieldTick(void)
{
	u32 i, otId, tick;
	u8 name[OT_NAME_LENGTH + 1];
	u8 gender;

	tick = gMain.vblankCounter2;
	if (tick % FRIEND_OT_EVERY != 0)
		return;
	if (FlagGet(FLAG_BATTLE_FACILITY))
		return;
	otId = PlayerOtId();
	gender = gSaveBlock2->playerGender;
	i = (tick / FRIEND_OT_EVERY) % PARTY_SIZE;
	{
		struct Pokemon* mon = &gPlayerParty[i];
		bool8 partner;

		if (GetMonData(mon, MON_DATA_SPECIES, NULL) == SPECIES_NONE || GetMonData(mon, MON_DATA_IS_EGG, NULL)
		 || GetMonData(mon, MON_DATA_FRIENDSHIP, NULL) < MAX_FRIENDSHIP)
			return;
		GetMonData(mon, MON_DATA_OT_NAME, name);
		if (GetMonData(mon, MON_DATA_OT_ID, NULL) == otId && SameName(name, gSaveBlock2->playerName)
		 && GetMonData(mon, MON_DATA_OT_GENDER, NULL) == gender)
			return;
		partner = HackPartner_IsMon(mon);
		HackMonId_Ensure((struct BoxPokemon*) mon);       //色違い・Moémon を自身の ID に移してから親を変える
		SetMonData(mon, MON_DATA_OT_ID, &otId);
		SetMonData(mon, MON_DATA_OT_NAME, gSaveBlock2->playerName);
		SetMonData(mon, MON_DATA_OT_GENDER, &gender);
		if (partner)
		{
			VarSet(VAR_PARTNER_OTID_LO, otId & 0xFFFF);
			VarSet(VAR_PARTNER_OTID_HI, otId >> 16);
		}
	}
}
