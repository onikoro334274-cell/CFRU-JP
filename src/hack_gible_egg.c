#include "defines.h"
#include "../include/pokemon.h"
#include "../include/overworld.h"
#include "../include/new/build_pokemon.h"
#include "../include/new/daycare.h"
#include "../include/new/catching.h"
#include "../include/constants/abilities.h"
#include "../include/constants/moves.h"
#include "../include/constants/pokemon.h"
#include "../include/new/learn_move.h"

/*
 * クチバのポケモンだいすきクラブの フカマルのタマゴ（2026-10-03・assembly/kanto/kanto_gible_egg.s から callasm）。
 * ユーザー「特別な設定　個体値MAX 色違い ガブリアスをみせるとメガストーンとZ両方もらえる」。
 *
 *   HackGibleEgg_Give         … フカマルのタマゴ（個体値 6 つとも 31・色違い）を手持ちへ → LASTRESULT = GiveMonToPlayer の結果
 *   HackGibleEgg_HasGarchomp  … 手持ちにガブリアス（タマゴでない）がいるか → LASTRESULT
 *
 *   （タマゴは りゅうのちかい（MOVE_DRAGONSOATH・優先度 -3・すべて 2 段階）も覚えている）
 *   HackGibleEgg_SpecialAbility … この子（印あり）とその進化・メガシンカの特性 → りゅうのよろい（ABILITY_DRAGONARMOR 319）
 *
 * ★ 2026-10-03 ユーザー「特性をちからもちとマルチスケイルとさめはだを複合した特性を作成してその特性にしてほしい
 *   メガ進化してもそのままで」。1 匹だけに付けるので、タマゴを作るとき MON_DATA_OBEDIENCE（うんめいてきな であい の印・80）を立て、
 *   GetMonAbility（src/build_pokemon.c・patches/0150）がフカマル・ガバイト・ガブリアス・メガガブリアス（・Z）でこの印を見る。
 *   メガシンカは DoFormChange が GetMonAbility で特性を読み直すので、そのまま りゅうのよろい。
 *   同じ印はお母さんのピカチュウの 2 つ目の特性（src/second_ability.c）でも使うが、どちらも種族も見るのでまざらない。
 *   効き目は ちからもち（こうげき 2 倍）・マルチスケイル（HP まんたんで受けるダメージ半分・かたやぶりの技なら効かない）・
 *   さめはだ（ふれた相手に最大 HP の 1/8）を、それぞれの特性の所に足した（damage_calc.c・battle_util.c・ability_battle_effects.c・AI）。
 *
 * 色違いは CFRU の ForceMonShiny（性格・性別はそのまま性格値を選び直す・最初のポケモンの色違いと同じ・src/hack_gift_shiny.c）。
 * 親はタマゴを作ったときの主人公（CreateEgg）なので、孵っても色違いのまま。
 */

void HackGibleEgg_Give(void)
{
	struct Pokemon mon;
	u8 iv = 31;
	u32 i;

	CreateEgg(&mon, SPECIES_GIBLE);
	for (i = 0; i < NUM_STATS; ++i)
		SetMonData(&mon, MON_DATA_HP_IV + i, &iv);
	ForceMonShiny(&mon);
	/* ★ 2026-10-03 ユーザー「りゅうのちかい 優先度-3 すべてののうりょくをぐーんとあげる ってわざ作って覚えさせて」:
	   MOVE_DRAGONSOATH（0x3E5）を覚えている。空きが無ければ 4 つ目と入れ替える */
	if (GiveMoveToMon(&mon, MOVE_DRAGONSOATH) == MON_HAS_MAX_MOVES)
		SetMonMoveSlot(&mon, MOVE_DRAGONSOATH, MAX_MON_MOVES - 1);
	{
		u8 mark = TRUE;     /* りゅうのよろいの印（HackGibleEgg_SpecialAbility） */
		SetMonData(&mon, MON_DATA_OBEDIENCE, &mark);
	}
	gSpecialVar_LastResult = GiveMonToPlayer(&mon);
}

void HackGibleEgg_HasGarchomp(void)
{
	u32 i;

	gSpecialVar_LastResult = FALSE;
	for (i = 0; i < PARTY_SIZE; ++i)
	{
		if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES2, NULL) == SPECIES_GARCHOMP)
		{
			gSpecialVar_LastResult = TRUE;
			return;
		}
	}
}

/* ★ 2026-10-03 ユーザー「010の孵化したフカマルの特性が通常になっている」: CFRU の CreateHatchedMon（src/daycare.c）は
   うんめいの印を引き継いでいなかった（patches/0155 で直した）。v0.10 で孵した子は印が消えているので、
   「色違い・個体値 6 つとも 31（すごいとっくんでない素の値）・親が主人公」のフカマルの仲間もこの子とみなす
   （クチバのタマゴ以外でこの 3 つがそろうことは、まず無い） */
static bool8 LooksLikeClubGible(const struct Pokemon* mon)
{
	struct Pokemon* m = (struct Pokemon*) mon;
	u32 otId, i;

	if (!IsMonShiny(m))
		return FALSE;
	for (i = 0; i < NUM_STATS; ++i)
	{
		if (GetMonData(m, MON_DATA_HP_IV + i, NULL) != 31)
			return FALSE;
	}
	otId = gSaveBlock2->playerTrainerId[0] | (gSaveBlock2->playerTrainerId[1] << 8)
	     | (gSaveBlock2->playerTrainerId[2] << 16) | ((u32) gSaveBlock2->playerTrainerId[3] << 24);
	return GetMonData(m, MON_DATA_OT_ID, NULL) == otId;
}

/* ★ 2026-10-05 ユーザー「バトルタワーかなんかでとくべつなガブリアスでてくるみたい」:
   CFRU はフロンティアのポケモンを作るとき mon->obedient = TRUE（src/build_pokemon.c）にするので、相手のガブリアスにも
   印（MON_DATA_OBEDIENCE）が付いていた。印は「親が主人公」のときだけ見る（相手は親の ID がでたらめ）。src/second_ability.c（ピカチュウの印）も使う。
   ★ 同じ日 ユーザー「バトルフロンティアなどの施設でたたかえるようにして」: 施設の中でも自分の子は特別のまま。
     施設が主人公に渡すポケモン（ファクトリーのレンタルなど・親は主人公・印あり）だけ外す＝施設の中で、今いる地名で出会っていて、
     タマゴからかえった子（出会ったレベル 0）でないもの */
bool8 HackMark_IsPlayersMarkedMon(const struct Pokemon* mon)
{
	struct Pokemon* m = (struct Pokemon*) mon;
	u32 otId;

	if (!GetMonData(m, MON_DATA_OBEDIENCE, NULL))
		return FALSE;
	if (FlagGet(FLAG_BATTLE_FACILITY)
	 && GetMonData(m, MON_DATA_MET_LEVEL, NULL) != 0
	 && GetMonData(m, MON_DATA_MET_LOCATION, NULL) == GetCurrentRegionMapSectionId())
		return FALSE;   //施設が渡したレンタル
	otId = gSaveBlock2->playerTrainerId[0] | (gSaveBlock2->playerTrainerId[1] << 8)
	     | (gSaveBlock2->playerTrainerId[2] << 16) | ((u32) gSaveBlock2->playerTrainerId[3] << 24);
	return GetMonData(m, MON_DATA_OT_ID, NULL) == otId;
}

/* ★ 2026-10-05 ユーザー「フカマルの卵だけどおとこからもらったになっている」:
   ステータス画面のタマゴのトレーナーメモ（src/bw_summary_screen.c PrintEggInfo）で、クラブのタマゴか見分ける */
bool8 HackGibleEgg_IsClubEgg(const struct Pokemon* mon)
{
	struct Pokemon* m = (struct Pokemon*) mon;

	if (!GetMonData(m, MON_DATA_IS_EGG, NULL) || GetMonData(m, MON_DATA_SPECIES, NULL) != SPECIES_GIBLE)
		return FALSE;
	return HackMark_IsPlayersMarkedMon(mon) || LooksLikeClubGible(mon);
}

u16 HackGibleEgg_SpecialAbility(const struct Pokemon* mon, u16 species)
{
	if (mon == NULL)
		return ABILITY_NONE;
	switch (species)
	{
		case SPECIES_GIBLE:
		case SPECIES_GABITE:
		case SPECIES_GARCHOMP:
		case SPECIES_GARCHOMP_MEGA:
		case SPECIES_GARCHOMP_MEGA_Z:
			if (HackMark_IsPlayersMarkedMon(mon) || LooksLikeClubGible(mon))
				return ABILITY_DRAGONARMOR;
			break;
	}
	return ABILITY_NONE;
}

/* ★ 2026-10-03 ユーザー「とくべつなガブリアスのメガ進化のメガガブリアスの種族値をベースガブリアスで上昇値だけ反映したポケモンにしてほしい」:
   印のある子がメガガブリアス・メガガブリアスZ のとき、能力ごとに ガブリアスの種族値 ＋（メガの方が高ければ その差）＝ 2 つの大きい方。
   下がる分（メガガブリアスの すばやさ 102→92・メガガブリアスZ の ぼうぎょ 95→85）は反映しない。
     メガガブリアス:  108 / 170 / 115 / 120 / 95 / 102（HP こうげき ぼうぎょ とくこう とくぼう すばやさ）
     メガガブリアスZ: 108 / 130 /  95 / 141 / 85 / 151
   CFRU の CalculateMonStatsNew（src/build_pokemon.c・patches/0152）が種族値を読む所から呼ぶ。HP はどれも 108 で同じ。
   stat は CFRU の並び（STAT_ATK など・gBaseStats の baseHP からのバイトの並びと同じ） */
u8 HackGibleEgg_BaseStat(const struct Pokemon* mon, u16 species, u8 stat)
{
	u8 base = ((const u8*) (&gBaseStats[species].baseHP))[stat];

	if ((species == SPECIES_GARCHOMP_MEGA || species == SPECIES_GARCHOMP_MEGA_Z)
	 && HackGibleEgg_SpecialAbility(mon, species) != ABILITY_NONE)
	{
		u8 normal = ((const u8*) (&gBaseStats[SPECIES_GARCHOMP].baseHP))[stat];
		if (normal > base)
			base = normal;
	}
	return base;
}

