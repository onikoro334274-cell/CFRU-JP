#include "defines.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/random.h"
#include "../include/constants/species.h"
#include "../include/new/dynamax.h"
#include "../include/new/frontier.h"
#include "../include/new/ram_locs.h"

/*
 * 伝説レイド（2026-10-06）: バトルフロンティアの広場の人に話しかけるたび、ちがう伝説・幻と ★6 のレイドバトル。
 * ユーザー「バトルフロンティアに話しかけるたびに異なる伝説とレイドバトルが始まるところ作ってほしい」→
 * 広場に人・全世代の伝説と幻・勝ったら捕まえられる・★6・「味方の助っ人主人公のアイコンとか使えない？ポケモンもかなり強く」。
 * レイドの仕組み（紹介画面・助っ人・ダイマックス・捕獲）は CFRU のまま。印 HACK_FLAG_LEGEND_RAID が立っている間だけ、
 * src/dynamax.c の「相手を決める関数」が下の関数を見る（patches/0179）。スクリプトは assembly/hack_legend_raid/。
 * 助っ人は src/Tables/raid_partners.h の最後の 4 人（ユウキ・ハルカ・トウヤ・トウコ）。トウヤとトウコは主人公と逆の性別の方だけ出す。
 */

#define HACK_FLAG_LEGEND_RAID 0x12EC          //build_hoenn_scripts.RESERVED_FLAGS（伝説レイドの間だけ立てる）
#define HACK_VAR_LEGEND_RAID_SPECIES 0x51F5   //build_hoenn_scripts.RESERVED_VARS（今回の伝説・次は別のものを選ぶ）
#define LEGEND_RAID_LEVEL 100
#define NUM_HACK_RAID_PARTNERS 4              //raid_partners.h の最後の 4 人

static const u16 sLegendRaidSpecies[] =
{
	SPECIES_ARTICUNO, SPECIES_ZAPDOS, SPECIES_MOLTRES, SPECIES_MEWTWO, SPECIES_MEW,
	SPECIES_RAIKOU, SPECIES_ENTEI, SPECIES_SUICUNE, SPECIES_LUGIA, SPECIES_HO_OH, SPECIES_CELEBI,
	SPECIES_REGIROCK, SPECIES_REGICE, SPECIES_REGISTEEL, SPECIES_LATIAS, SPECIES_LATIOS,
	SPECIES_KYOGRE, SPECIES_GROUDON, SPECIES_RAYQUAZA, SPECIES_JIRACHI, SPECIES_DEOXYS,
	SPECIES_UXIE, SPECIES_MESPRIT, SPECIES_AZELF, SPECIES_DIALGA, SPECIES_PALKIA, SPECIES_HEATRAN,
	SPECIES_REGIGIGAS, SPECIES_GIRATINA, SPECIES_CRESSELIA, SPECIES_PHIONE, SPECIES_MANAPHY,
	SPECIES_DARKRAI, SPECIES_SHAYMIN, SPECIES_ARCEUS,
	SPECIES_VICTINI, SPECIES_COBALION, SPECIES_TERRAKION, SPECIES_VIRIZION, SPECIES_TORNADUS,
	SPECIES_THUNDURUS, SPECIES_RESHIRAM, SPECIES_ZEKROM, SPECIES_LANDORUS, SPECIES_KYUREM,
	SPECIES_KELDEO, SPECIES_MELOETTA, SPECIES_GENESECT,
	SPECIES_XERNEAS, SPECIES_YVELTAL, SPECIES_ZYGARDE, SPECIES_DIANCIE, SPECIES_HOOPA, SPECIES_VOLCANION,
	SPECIES_SILVALLY, SPECIES_TAPU_KOKO, SPECIES_TAPU_LELE, SPECIES_TAPU_BULU, SPECIES_TAPU_FINI,
	SPECIES_SOLGALEO, SPECIES_LUNALA, SPECIES_NECROZMA, SPECIES_MAGEARNA, SPECIES_MARSHADOW,
	SPECIES_ZERAORA, SPECIES_MELMETAL,
	SPECIES_ZACIAN, SPECIES_ZAMAZENTA, SPECIES_ETERNATUS, SPECIES_URSHIFU_SINGLE, SPECIES_ZARUDE,
	SPECIES_REGIELEKI, SPECIES_REGIDRAGO, SPECIES_GLASTRIER, SPECIES_SPECTRIER, SPECIES_CALYREX,
	SPECIES_ARTICUNO_G, SPECIES_ZAPDOS_G, SPECIES_MOLTRES_G, SPECIES_ENAMORUS,
	SPECIES_WO_CHIEN, SPECIES_CHIEN_PAO, SPECIES_TING_LU, SPECIES_CHI_YU, SPECIES_KORAIDON,
	SPECIES_MIRAIDON, SPECIES_OKIDOGI, SPECIES_MUNKIDORI, SPECIES_FEZANDIPITI, SPECIES_OGERPON,
	SPECIES_TERAPAGOS, SPECIES_PECHARUNT,
};

//★ 2026-10-11 ホウエンのミラクル交換（src/hack_wonder_trade.c）が同じ一覧から引く
u16 HackLegendRaid_RandomSpecies(void)
{
	return sLegendRaidSpecies[Random() % ARRAY_COUNT(sLegendRaidSpecies)];
}

bool8 HackLegendRaid_Active(void)
{
	return FlagGet(HACK_FLAG_LEGEND_RAID);
}

//スクリプトから（callasm）: 前回とちがう伝説を選んで印を立てる
void HackLegendRaid_Begin(void)
{
	u16 last = VarGet(HACK_VAR_LEGEND_RAID_SPECIES);
	u16 species;

	do
	{
		species = sLegendRaidSpecies[Random() % ARRAY_COUNT(sLegendRaidSpecies)];
	} while (species == last);

	VarSet(HACK_VAR_LEGEND_RAID_SPECIES, species);
	FlagSet(HACK_FLAG_LEGEND_RAID);
}

void HackLegendRaid_End(void)
{
	FlagClear(HACK_FLAG_LEGEND_RAID);
}

//以下は src/dynamax.c から（印が立っているときだけ呼ばれる）
void HackLegendRaid_SetStarsSpeciesLevel(void)
{
	gRaidBattleStars = SIX_STAR_RAID;
	gRaidBattleSpecies = VarGet(HACK_VAR_LEGEND_RAID_SPECIES);
	gRaidBattleLevel = LEGEND_RAID_LEVEL;
}

u8 HackLegendRaid_Level(void)
{
	return LEGEND_RAID_LEVEL;
}

//助っ人はユウキ・ハルカと、主人公と逆の性別の BW の主人公（トウヤ／トウコ）
void HackLegendRaid_Partners(bool8* checkedPartners)
{
	u32 i;
	u32 base = gNumRaidPartners - NUM_HACK_RAID_PARTNERS;

	for (i = 0; i < gNumRaidPartners; ++i)
		checkedPartners[i] = 0xFF;

	checkedPartners[base + 0] = TRUE; //ユウキ
	checkedPartners[base + 1] = TRUE; //ハルカ
	if (gSaveBlock2->playerGender == MALE)
		checkedPartners[base + 3] = TRUE; //トウコ
	else
		checkedPartners[base + 2] = TRUE; //トウヤ
}

//スクリプトから（callasm・special 0x117 のあと）: 相手を個体値 31・HP の努力値 252 に
void HackLegendRaid_StrengthenBoss(void)
{
	struct Pokemon* mon = &gEnemyParty[0];
	u8 iv = 31;
	u8 ev = 252;
	u32 i;

	for (i = 0; i < NUM_STATS; ++i)
		SetMonData(mon, MON_DATA_HP_IV + i, &iv);
	SetMonData(mon, MON_DATA_HP_EV, &ev);
	CalculateMonStats(mon);
}
