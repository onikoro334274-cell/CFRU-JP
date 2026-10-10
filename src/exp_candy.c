#include "defines.h"
#include "defines_battle.h"
#include "../include/item_use.h"
#include "../include/party_menu.h"
#include "../include/sound.h"
#include "../include/string_util.h"
#include "../include/constants/items.h"
#include "../include/constants/pokemon.h"
#include "../include/constants/songs.h"
#include "../include/new/rom_locs.h"
#include "../include/new/learn_move.h"
#include "../include/new/evolution.h"
#include "../include/evolution_scene.h"
#include "../include/task.h"
#include "../include/event_data.h"
#include "../include/quest_log.h"
#include "party_menu_internal.h"

/*
    けいけんアメ XS〜XL（2026-10-01）

    ユーザー「経験アメXLとかボールをもう一人のショップ店員に売って欲しい」→（AskUserQuestion）「新しく作る」。
    CFRU に経験アメは無いので道具を 5 つ足した（ITEM_EXP_CANDY_XS〜XL・0x338〜0x33C・include/constants/items.h）。
    経験値は本家と同じ XS 100 / S 800 / M 3000 / L 10000 / XL 30000。

    使い方（手持ちの画面・ふしぎなアメと同じ流れ）:
      ・レベルが上がらない … 経験値を足して「〇〇は けいけんちを N もらった！」
      ・レベルが上がる     … 1 レベルずつ能力値の表示と技習得を進め、最後に経験値の余りと進化を処理する。
                             ExpCandyNextLevel は素の表示・技習得タスクへつなぐが、バッグには触れない。
                             経験アメは開始時に 1 個だけ減らす。満杯のバッグでも同じ流れを使う。
    ★★ 2026-10-03 ユーザー「けいけんアメでレベル上げると、スキップされたレベルの技覚えられない」→ 上の「途中のレベルは枠が
       空いていれば」をやめ、**1 レベルずつ素のふしぎなアメの流れ**（レベルアップの窓 → 技（いっぱいなら わすれさせますか）
       → 進化の確かめ）を続ける。素の PartyMenuTryEvolution（08127048・functionrewrites）を HackExpCandy_TryEvolution に
       置き換え、けいけんアメの途中（印の小さなタスクがある）なら進化させずに次のレベルの流れを始める。最後のレベルで経験値の
       余りを足し直してから、素と同じく進化を確かめる（途中のレベルでは進化しない＝ふしぎなアメを続けて使うのと違う所）。
       状態は書き換え可能な static に置けない（6.13）ので、印のタスクの data に持つ（→ 2026-10-06 予約変数へ・下）
*/

#define gText_WontHaveEffect (const u8*) 0x83DDE0B       /* src/party_menu.c と同じ（素 FRLG） */

void __attribute__((long_call)) GetMonLevelUpWindowStats(struct Pokemon *mon, s16 *stats);
bool8 __attribute__((long_call)) ExecuteTableBasedItemEffect_(u8 slot, u16 item, u8 move);
void __attribute__((long_call)) Task_DisplayLevelUpStatsPg1(u8 taskId);
u8 __attribute__((long_call)) DisplayPartyMenuMessage(const u8* str, u8 b);          /* 素 FRLG（src/party_menu.c 103 行と同じ宣言） */
void __attribute__((long_call)) UpdateMonDisplayInfoAfterRareCandy(u8 slot, struct Pokemon *mon);
extern const u8 gText_ExpCandyGotExp[];

static const u16 sExpCandyAmount[] = {100, 800, 3000, 10000, 30000};   /* ITEM_EXP_CANDY_XS から順 */

static void ItemUseCB_ExpCandy(u8 taskId, TaskFunc func);

void FieldUseFunc_ExpCandy(u8 taskId)
{
	gItemUseCB = ItemUseCB_ExpCandy;
	SetUpItemUseCallback(taskId);
}

/* けいけんアメの途中（★ 2026-10-06 ユーザー「経験アメしようすると技覚えたときそこでとまる」）。
   前はパーティの画面のタスク（印）の data に持っていたが、技がいっぱいで「わすれさせる技」を選ぶとステータス画面を開き、
   そのときタスクが全部消える（戻ると続きが無く、そのレベルで止まる・経験値の余りも消える）。予約変数に持つ
   （build_hoenn_scripts.RESERVED_VARS）。続きは必ず HackExpCandy_TryEvolution で終わり、そこで 0 に戻すので、レポートに残らない */
#define VAR_EXP_CANDY_TARGET 0x50FD     /* 目標のレベル | 手持ちの番号 << 8 | 道具（XS からの順）<< 11・0 は続きなし */
#define VAR_EXP_CANDY_EXP_LO 0x50FE
#define VAR_EXP_CANDY_EXP_HI 0x50FF
#define FreePartyPointers ((void (*)(void)) (0x0811F878 | 1))
void Task_ClosePartyMenuAfterText(u8 taskId);   /* src/party_menu.c（CFRU） */

static void ExpCandyChainClear(void)
{
	VarSet(VAR_EXP_CANDY_TARGET, 0);
	VarSet(VAR_EXP_CANDY_EXP_LO, 0);
	VarSet(VAR_EXP_CANDY_EXP_HI, 0);
}

/* 次の 1 レベルを素のふしぎなアメの流れで（ふしぎなアメを 1 つ足して、素の処理が 1 つ減らす） */
static void ExpCandyNextLevel(u8 taskId)
{
	struct Pokemon *mon = &gPlayerParty[gPartyMenu.slotId];
	s16 *stats = sPartyMenuInternal->data;
	u8 level;

	/* BPRJ RareCandyStep UI/learning flow, without synthetic bag items. */
	gSpecialVar_ItemId = ITEM_RARE_CANDY;
	GetMonLevelUpWindowStats(mon, stats);
	ExecuteTableBasedItemEffect_(gPartyMenu.slotId, ITEM_RARE_CANDY, 0);
	GetMonLevelUpWindowStats(mon, &stats[NUM_STATS]);
	gPartyMenuUseExitCallback = TRUE;
	WriteQuestLog(4, 0, ITEM_RARE_CANDY, 0xFFFF);
	PlayFanfareByFanfareNum(0);
	UpdateMonDisplayInfoAfterRareCandy(gPartyMenu.slotId, mon);
	GetMonNickname(mon, gStringVar1);
	level = GetMonData(mon, MON_DATA_LEVEL, NULL);
	ConvertIntToDecimalStringN(gStringVar2, level, STR_CONV_MODE_LEFT_ALIGN, 3);
	StringExpandPlaceholders(gStringVar4, (const u8*) 0x083DE267);
	DisplayPartyMenuMessage(gStringVar4, TRUE);
	ScheduleBgCopyTilemapToVram(2);
	gTasks[taskId].func = Task_DisplayLevelUpStatsPg1;
}

static bool8 ExpCandyChainValid(u16 chain, u8 slot)
{
	u32 target = chain & 0xFF;
	u32 savedSlot = (chain >> 8) & 7;
	u32 candy = (chain >> 11) & 7;
	return (chain & 0xC000) == 0 && target > 0 && target <= MAX_LEVEL
		&& savedSlot < PARTY_SIZE && savedSlot == slot
		&& candy < ARRAY_COUNT(sExpCandyAmount);
}

/* 素の PartyMenuTryEvolution（08127048）の置き換え（functionrewrites） */
void HackExpCandy_TryEvolution(u8 taskId)
{
	struct Pokemon *mon = &gPlayerParty[gPartyMenu.slotId];
	u16 chain = VarGet(VAR_EXP_CANDY_TARGET);
	u16 targetSpecies;

	if (chain != 0 && !ExpCandyChainValid(chain, gPartyMenu.slotId))
	{
		ExpCandyChainClear();                    /* 別のポケモン（残っていた続き）: 使わない */
		chain = 0;
	}
	if (chain != 0)
	{
		if (GetMonData(mon, MON_DATA_LEVEL, NULL) < (chain & 0xFF))
		{
			ExpCandyNextLevel(taskId);       /* 途中のレベル: 進化させずに次へ */
			return;
		}
		/* 最後のレベル: 経験値の余りを足し直して、道具の番号を戻す */
		{
			u32 exp = VarGet(VAR_EXP_CANDY_EXP_LO) | ((u32) VarGet(VAR_EXP_CANDY_EXP_HI) << 16);
			SetMonData(mon, MON_DATA_EXP, &exp);
			gSpecialVar_ItemId = ITEM_EXP_CANDY_XS + ((chain >> 11) & 7);
		}
		ExpCandyChainClear();
	}

	/* ここからは素と同じ（pokefirered party_menu.c PartyMenuTryEvolution） */
	targetSpecies = GetEvolutionTargetSpecies(mon, EVO_MODE_NORMAL, ITEM_NONE);
	if (targetSpecies != SPECIES_NONE)
	{
		FreePartyPointers();
		gCB2_AfterEvolution = gPartyMenu.exitCallback;
		BeginEvolutionScene(mon, targetSpecies, TRUE, gPartyMenu.slotId);
		DestroyTask(taskId);
	}
	else
		gTasks[taskId].func = Task_ClosePartyMenuAfterText;
}

static u32 AddCandyExperience(u32 exp, u32 amount, u32 maxExp)
{
	if (exp >= maxExp || amount >= maxExp - exp)
		return maxExp;
	return exp + amount;
}

static void ItemUseCB_ExpCandy(u8 taskId, TaskFunc func)
{
	struct Pokemon *mon = &gPlayerParty[gPartyMenu.slotId];
	u16 item = gSpecialVar_ItemId;
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	u8 growth = gBaseStats[species].growthRate;
	u8 level = GetMonData(mon, MON_DATA_LEVEL, NULL);
	u32 exp = GetMonData(mon, MON_DATA_EXP, NULL);
	u32 maxExp = gExperienceTables[growth][MAX_LEVEL];
	u32 amount = (item >= ITEM_EXP_CANDY_XS && item <= ITEM_EXP_CANDY_XL) ? sExpCandyAmount[item - ITEM_EXP_CANDY_XS] : 0;
	u32 newExp;
	u8 newLevel;

	PlaySE(SE_SELECT);
	if (amount == 0 || level >= MAX_LEVEL || GetMonData(mon, MON_DATA_IS_EGG, NULL)
	|| !RemoveBagItem(item, 1))
	{
		gPartyMenuUseExitCallback = FALSE;
		DisplayPartyMenuMessage(gText_WontHaveEffect, TRUE);
		ScheduleBgCopyTilemapToVram(2);
		gTasks[taskId].func = func;
		return;
	}

	newExp = AddCandyExperience(exp, amount, maxExp);
	newLevel = level;
	while (newLevel < MAX_LEVEL && gExperienceTables[growth][newLevel + 1] <= newExp)
		++newLevel;

	if (newLevel == level)
	{
		/* レベルが上がらない: 経験値を足すだけ */
		SetMonData(mon, MON_DATA_EXP, &newExp);
		CalculateMonStats(mon);
		UpdateMonDisplayInfoAfterRareCandy(gPartyMenu.slotId, mon);
		gPartyMenuUseExitCallback = TRUE;
		GetMonNickname(mon, gStringVar1);
		ConvertIntToDecimalStringN(gStringVar2, amount, STR_CONV_MODE_LEFT_ALIGN, 5);
		StringExpandPlaceholders(gStringVar4, gText_ExpCandyGotExp);
		DisplayPartyMenuMessage(gStringVar4, TRUE);
		ScheduleBgCopyTilemapToVram(2);
		gTasks[taskId].func = func;
		return;
	}

	/* レベルが上がる: 1 レベルずつ素のふしぎなアメの流れで（HackExpCandy_TryEvolution が次のレベルへつなぐ） */
	VarSet(VAR_EXP_CANDY_TARGET, newLevel | (gPartyMenu.slotId << 8) | ((item - ITEM_EXP_CANDY_XS) << 11));
	VarSet(VAR_EXP_CANDY_EXP_LO, newExp & 0xFFFF);
	VarSet(VAR_EXP_CANDY_EXP_HI, newExp >> 16);
	ExpCandyNextLevel(taskId);
}
