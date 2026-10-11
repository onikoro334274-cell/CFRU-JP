// このファイルは tools/frontier/build_frontier_brains.py が生成しました。手で編集しないこと。
// フロンティアブレーンの手持ち（pokeemerald src/frontier_util.c sFrontierBrainsMons）。src/hoenn_frontier.c が読む
#pragma once

#include "../include/constants/items.h"
#include "../include/constants/moves.h"
#include "../include/constants/species.h"
#include "../include/pokemon.h"

struct HoennFrontierBrainMon
{
	u16 species;
	u16 item;
	u8 fixedIV;
	u8 nature;
	u8 evs[6];	// HP・攻撃・防御・素早さ・特攻・特防
	u16 moves[4];
	u8 ability;	// FRONTIER_ABILITY_*（0 隠れ・1・2）
	u8 teraType;	// 0 なし・1〜 タイプ + 1
};

#define HOENN_FRONTIER_BRAIN_COUNT 14

static const struct HoennFrontierBrainMon sHoennFrontierBrainMons[HOENN_FRONTIER_BRAIN_COUNT][3] =
{
	[0] = {	// TRAINER_ANABEL の銀
		{408, 191, 31, 10, {0, 0, 4, 252, 252, 0}, {369, 991, 347, 105}, 1, 9},	// LATIOS（Smogon の型）
		{1398, 703, 31, 10, {0, 0, 0, 252, 252, 4}, {386, 247, 949, 374}, 1, 24},	// GHOLDENGO（Smogon の型）
		{498, 702, 31, 8, {252, 0, 216, 40, 0, 0}, {89, 444, 468, 191}, 0, 8},	// GARCHOMP（Smogon の型）
	},
	[1] = {	// TRAINER_ANABEL の金
		{243, 725, 31, 10, {0, 0, 4, 252, 252, 0}, {85, 459, 359, 443}, 0, 12},	// RAIKOU（Smogon の型）
		{408, 191, 31, 10, {0, 0, 4, 252, 252, 0}, {369, 991, 347, 105}, 1, 9},	// LATIOS（Smogon の型）
		{1380, 200, 31, 3, {0, 252, 4, 252, 0, 0}, {14, 458, 945, 382}, 2, 8},	// KINGAMBIT（Smogon の型）
	},
	[2] = {	// TRAINER_SPENSER の銀
		{245, 200, 31, 10, {252, 0, 4, 252, 0, 0}, {164, 347, 459, 182}, 1, 18},	// SUICUNE（Smogon の型）
		{59, 725, 31, 3, {160, 252, 0, 96, 0, 0}, {174, 245, 434, 234}, 1, 1},	// ARCANINE（Smogon の型）
		{366, 678, 31, 14, {0, 252, 0, 252, 4, 0}, {216, 228, 89, 126}, 1, 0},	// SLAKING（Smogon の型）
	},
	[3] = {	// TRAINER_SPENSER の金
		{245, 200, 31, 10, {252, 0, 4, 252, 0, 0}, {164, 347, 459, 182}, 1, 18},	// SUICUNE（Smogon の型）
		{1380, 141, 31, 3, {0, 252, 4, 252, 0, 0}, {14, 458, 945, 382}, 2, 8},	// KINGAMBIT（Smogon の型）
		{149, 725, 31, 3, {0, 252, 4, 252, 0, 0}, {349, 245, 89, 943}, 0, 1},	// DRAGONITE（Smogon の型）
	},
	[4] = {	// TRAINER_GRETA の銀
		{214, 200, 31, 13, {0, 252, 4, 252, 0, 0}, {339, 365, 282, 89}, 2, 17},	// HERACROSS（Smogon の型）
		{197, 725, 31, 20, {252, 0, 4, 0, 0, 252}, {535, 273, 92, 182}, 0, 4},	// UMBREON（Smogon の型）
		{94, 703, 31, 10, {0, 0, 4, 252, 252, 0}, {386, 247, 531, 374}, 1, 8},	// GENGAR（Smogon の型）
	},
	[5] = {	// TRAINER_GRETA の金
		{1381, 725, 31, 13, {0, 252, 4, 252, 0, 0}, {753, 943, 229, 365}, 1, 9},	// GREAT_TUSK（Smogon の型）
		{197, 200, 31, 20, {252, 0, 4, 0, 0, 252}, {535, 273, 182, 86}, 0, 12},	// UMBREON（Smogon の型）
		{94, 703, 31, 10, {0, 0, 4, 252, 252, 0}, {386, 247, 531, 374}, 1, 8},	// GENGAR（Smogon の型）
	},
	[6] = {	// TRAINER_LUCY の銀
		{329, 725, 31, 5, {252, 0, 248, 8, 0, 0}, {105, 459, 728, 988}, 1, 24},	// MILOTIC（Smogon の型）
		{965, 200, 31, 5, {248, 0, 252, 0, 0, 8}, {105, 92, 114, 469}, 0, 13},	// TOXAPEX（Smogon の型）
		{379, 678, 31, 4, {8, 56, 0, 220, 224, 0}, {531, 202, 89, 458}, 1, 0},	// SEVIPER（Smogon の型）
	},
	[7] = {	// TRAINER_LUCY の金
		{130, 142, 31, 13, {0, 252, 0, 252, 0, 4}, {349, 127, 242, 164}, 1, 0},	// GYARADOS（Smogon の型）
		{329, 725, 31, 5, {252, 0, 248, 8, 0, 0}, {105, 459, 728, 988}, 1, 24},	// MILOTIC（Smogon の型）
		{1179, 696, 31, 10, {0, 0, 0, 252, 252, 4}, {369, 247, 53, 442}, 2, 8},	// DRAGAPULT（Smogon の型）
	},
	[8] = {	// TRAINER_NOLAND の銀
		{SPECIES_METANG, ITEM_SITRUS_BERRY, 31, NATURE_BRAVE, {0, 252, 252, 0, 6, 0}, {MOVE_LIGHTSCREEN, MOVE_PSYCHIC, MOVE_REFLECT, MOVE_METALCLAW}, 1, 0},
		{SPECIES_SKARMORY, ITEM_SITRUS_BERRY, 31, NATURE_IMPISH, {252, 0, 0, 0, 6, 252}, {MOVE_TOXIC, MOVE_AERIALACE, MOVE_PROTECT, MOVE_STEELWING}, 1, 0},
		{SPECIES_AGGRON, ITEM_SITRUS_BERRY, 31, NATURE_ADAMANT, {0, 252, 0, 0, 252, 6}, {MOVE_THUNDERBOLT, MOVE_PROTECT, MOVE_SOLARBEAM, MOVE_DRAGONCLAW}, 1, 0},
	},
	[9] = {	// TRAINER_NOLAND の金
		{SPECIES_METANG, ITEM_SITRUS_BERRY, 31, NATURE_BRAVE, {0, 252, 252, 0, 6, 0}, {MOVE_LIGHTSCREEN, MOVE_PSYCHIC, MOVE_REFLECT, MOVE_METALCLAW}, 1, 0},
		{SPECIES_SKARMORY, ITEM_SITRUS_BERRY, 31, NATURE_IMPISH, {252, 0, 0, 0, 6, 252}, {MOVE_TOXIC, MOVE_AERIALACE, MOVE_PROTECT, MOVE_STEELWING}, 1, 0},
		{SPECIES_AGGRON, ITEM_SITRUS_BERRY, 31, NATURE_ADAMANT, {0, 252, 0, 0, 252, 6}, {MOVE_THUNDERBOLT, MOVE_PROTECT, MOVE_SOLARBEAM, MOVE_DRAGONCLAW}, 1, 0},
	},
	[10] = {	// TRAINER_TUCKER の銀
		{285, 142, 31, 13, {0, 252, 0, 252, 0, 4}, {127, 89, 728, 468}, 1, 0},	// SWAMPERT（Smogon の型）
		{397, 725, 31, 10, {0, 0, 4, 252, 252, 0}, {369, 412, 89, 395}, 1, 24},	// SALAMENCE（Smogon の型）
		{6, 141, 31, 10, {0, 0, 4, 252, 252, 0}, {311, 76, 731, 395}, 1, 0},	// CHARIZARD（Smogon の型）
	},
	[11] = {	// TRAINER_TUCKER の金
		{285, 142, 31, 13, {0, 252, 0, 252, 0, 4}, {127, 89, 728, 468}, 1, 0},	// SWAMPERT（Smogon の型）
		{400, 186, 31, 3, {0, 252, 0, 252, 0, 4}, {532, 89, 282, 363}, 1, 9},	// METAGROSS（Smogon の型）
		{407, 710, 31, 10, {252, 0, 4, 252, 0, 0}, {347, 482, 359, 105}, 1, 4},	// LATIAS（Smogon の型）
	},
	[12] = {	// TRAINER_BRANDON の銀
		{1186, 725, 31, 10, {4, 0, 0, 252, 252, 0}, {443, 85, 966, 229}, 1, 16},	// REGIELEKI（Smogon の型）
		{1187, 695, 31, 10, {0, 0, 0, 252, 252, 4}, {740, 369, 372, 370}, 1, 17},	// REGIDRAGO（Smogon の型）
		{403, 200, 31, 8, {252, 0, 180, 0, 0, 76}, {468, 532, 692, 334}, 1, 8},	// REGISTEEL（Smogon の型）
	},
	[13] = {	// TRAINER_BRANDON の金
		{145, 725, 31, 5, {248, 0, 252, 0, 8, 0}, {412, 443, 86, 395}, 0, 9},	// ZAPDOS（Smogon の型）
		{146, 142, 31, 5, {248, 0, 248, 12, 0, 0}, {53, 395, 442, 46}, 0, 24},	// MOLTRES（Smogon の型）
		{144, 141, 31, 20, {248, 0, 8, 0, 0, 252}, {395, 501, 442, 114}, 1, 17},	// ARTICUNO（Smogon の型）
	},
};
