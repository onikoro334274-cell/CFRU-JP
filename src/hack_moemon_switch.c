#include "defines.h"
#include "../include/event_data.h"
#include "../include/item.h"
#include "../include/item_use.h"
#include "../include/script.h"
#include "../include/task.h"
#include "../include/constants/items.h"
#include "../include/pokemon.h"
#include "../include/pokemon_storage_system.h"
#include "../include/new/pokemon_storage_system.h"
#include "hack_mon_id.h"

/*
 * Moemonボタン（ITEM_MOEMON_SWITCH 0x347・たいせつなもの・2026-10-08）。
 * ユーザー「Moemonいれていい」「たいせつなもので変更ON/OFFできるように」。
 *
 * - Moémon の絵（DPE の src/hack_moemon.c）を出すかを切り替える。旗 0x12E9 が立っていれば ON（最初は OFF・ユーザー「デフォルトOFFでおねがい」）。
 * - バッグで つかう → バッグを閉じて EventScript_MoemonSwitch（assembly/hack_moemon_switch/moemon_switch.s）が切り替えて知らせる
 *   （かがやくおまもりと同じ形）。
 * - 持っていなければフィールドで黙って渡す（64 コマごと・src/overworld_speedup.c から・いくせいポーチと同じ）。
 * - オミット版（HACK_NO_MOEMON）では渡さない（Moémon の絵が無いので）。
 * - 前からいた子は Moémon にしない（2026-10-08 ユーザー「萌えもん対応パッチ以前に捕獲したポケモンが萌えもん化する」→「印つけて」）:
 *   絵を選ぶ側（DPE の HackMoemon_Slot）には種族と性格値しか来ないので、個体に印を付けても見えない。
 *   → 一覧が無いセーブでフィールドに入ったら、手持ち・育て屋・39 箱を 1 コマに 1 箱ずつ見て回り、
 *     当たっている子（1/16）の性格値を SaveBlock1 + 0x2680（questLog の跡の空き 0x02027B0C〜0x0202812C・本体の二重保存の中）に書く。
 *     はじめからは最初のコマで空の一覧になる。DPE はこの一覧にある性格値を Moémon にしない。
 * - 通信対戦はお互いの設定で（2026-10-08 ユーザー「お互いの設定でたたかえるようにして」）:
 *   手持ちのポケモンのデータの +0x13 の空きビット 0x08（BoxPokemon の unused:5 の一番下・チェックサムに入らない）に
 *   「この持ち主の画面で萌えもんか」（ON・当たり（1/16）・前からいた子でない）をフィールドで毎コマ書く。
 *   通信対戦では手持ちがまるごと相手へ送られるので、DPE の src/hack_moemon.c が相手（と味方）のポケモンはこのビットで決める。
 */
#define HACK_FLAG_MOEMON_ON 0x12E9   //build_hoenn_scripts.RESERVED_FLAGS

#ifndef HACK_NO_MOEMON
#define MOEMON_OLD_MAGIC 0x584F4F4D   //'MOOX'＝1/16 のころの一覧（当たりは上 4 ビットが 0xA の子だけ）
#define MOEMON_OLD_MAGIC_UPGRADE 0x554F4F4D   //'MOOU'＝1/8 にしたときの足し直しの途中（0xB の子だけ足す）
#define MOEMON_OLD_MAGIC_8 0x384F4F4D   //'MOO8'＝1/8 の一覧
//当たり: 性格値をかき混ぜた値の上 4 ビットが 0xA（1/16）。2026-10-08 に一度 1/8（上 3 ビット 0b101）にして、同じ日に 1/16 へ戻した
//（ユーザー「moemon1/16に戻して」）。1/8 のころの一覧（'MOO8'・'MOOU'）にも 0xA の子は全部入っているのでそのまま使う。
//1/16 のころの当たり（上 4 ビット 0xA）を含むので、今まで萌えもんだった子はそのまま（DPE の src/hack_moemon.c と同じ式）
#define MOEMON_HIT(pid) ((((u32) (pid) * 0x9E3779B1u) >> 28) == 0xA)
#define MOEMON_HIT_NEW(pid) ((((u32) (pid) * 0x9E3779B1u) >> 28) == 0xB)   //1/8 で増えた側
#define MOEMON_OLD_MAX 384
#define MOEMON_OLD_STEP_DONE (1 + 39)   //39 箱のころに作った一覧（40〜50 箱は 2026-10-09 から・一覧のあとに来た子なので見ない）

struct MoemonOldList   //SaveBlock1 + 0x2680（DPE の src/hack_moemon.c と同じ形・1,544 バイト）
{
	u32 magic;
	u16 count;
	u16 step;          //0 = 手持ちと育て屋・1〜39 = その箱・40 = 終わり
	u32 personality[MOEMON_OLD_MAX];
};

/* The legacy quest-log tail reserved for this list ends at SaveBlock1 + 0x2CA0. */
_Static_assert(sizeof(struct MoemonOldList) <= 0x2CA0 - 0x2680,
               "Moemon exclusion list exceeds its reserved save region");
#define MOEMON_OLD_LIST ((struct MoemonOldList*) ((u8*) gSaveBlock1 + 0x2680))

#define MOEMON_LINK_BIT 0x08   //BoxPokemon +0x13（DPE の src/hack_moemon.c と同じ）

static bool8 MoemonOld_Has(u32 personality)
{
	struct MoemonOldList* list = MOEMON_OLD_LIST;
	u32 i;

	if (list->magic != MOEMON_OLD_MAGIC && list->magic != MOEMON_OLD_MAGIC_UPGRADE && list->magic != MOEMON_OLD_MAGIC_8)
		return FALSE;
	for (i = 0; i < list->count && i < MOEMON_OLD_MAX; ++i)
	{
		if (list->personality[i] == personality)
			return TRUE;
	}
	return FALSE;
}

static void MoemonOld_Add(u32 personality)
{
	struct MoemonOldList* list = MOEMON_OLD_LIST;

	//1/8 にしたときの足し直しは、増えた側（0xB）の子だけ＝それまでふつうの絵だった子はふつうのまま。
	//1/16 の当たり（0xA）で一覧に無い子は、一覧を作ったあとに来た萌えもんなので足さない
	if (list->magic == MOEMON_OLD_MAGIC_UPGRADE ? !MOEMON_HIT_NEW(personality) : !MOEMON_HIT(personality))
		return;
	if (MoemonOld_Has(personality))
		return;
	if (list->count < MOEMON_OLD_MAX)
		list->personality[list->count++] = personality;
}

//手持ちの 6 匹に「この画面で萌えもんか」のビットを書く（通信対戦で相手に送る）
static void MoemonLink_MarkParty(void)
{
	bool8 on = FlagGet(HACK_FLAG_MOEMON_ON) != 0;
	u32 i;

	for (i = 0; i < PARTY_SIZE; ++i)
	{
		u8* flags = ((u8*) &gPlayerParty[i]) + 0x13;
		u32 pid = gPlayerParty[i].personality;

		(void) pid;
		if (on && HackMonId_IsMoemonHit((struct BoxPokemon*) &gPlayerParty[i]))   //★ 2026-10-10 #70 第 2 段: 裏 ID で（ID の無い子は性格値と一覧）
			*flags |= MOEMON_LINK_BIT;
		else
			*flags &= ~MOEMON_LINK_BIT;
	}
}

static void MoemonOld_ScanStep(void)
{
	struct MoemonOldList* list = MOEMON_OLD_LIST;
	u32 i;

	//1/16 に戻した（2026-10-08）: 'MOOX'（1/16）の一覧は足し直さない。一覧が無ければ 1/16 の一覧を作る
	if (list->magic != MOEMON_OLD_MAGIC && list->magic != MOEMON_OLD_MAGIC_UPGRADE && list->magic != MOEMON_OLD_MAGIC_8)
	{
		Memset(list, 0, sizeof(struct MoemonOldList));
		list->magic = MOEMON_OLD_MAGIC;
	}
	if (list->step >= MOEMON_OLD_STEP_DONE)
	{
		if (list->magic == MOEMON_OLD_MAGIC_UPGRADE)
			list->magic = MOEMON_OLD_MAGIC_8;
		return;
	}

	if (list->step == 0)
	{
		for (i = 0; i < PARTY_SIZE; ++i)
		{
			if (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES, NULL) != SPECIES_NONE)
				MoemonOld_Add(GetMonData(&gPlayerParty[i], MON_DATA_PERSONALITY, NULL));
		}
		for (i = 0; i < DAYCARE_MON_COUNT; ++i)
		{
			if (GetBoxMonData(&gSaveBlock1->daycare.mons[i].mon, MON_DATA_SPECIES, NULL) != SPECIES_NONE)
				MoemonOld_Add(GetBoxMonData(&gSaveBlock1->daycare.mons[i].mon, MON_DATA_PERSONALITY, NULL));
		}
	}
	else
	{
		u8 box = list->step - 1;
		for (i = 0; i < IN_BOX_COUNT; ++i)
		{
			if (GetBoxMonDataAt(box, i, MON_DATA_SPECIES) != SPECIES_NONE)
				MoemonOld_Add(GetBoxMonDataAt(box, i, MON_DATA_PERSONALITY));
		}
	}
	++list->step;
}
#endif

//★ 2026-10-10 #70 第 2 段: src/hack_mon_id.c が ID を付けるとき、前からいた子は はずれの裏 ID にする
bool8 HackMoemonSwitch_IsOldMon(u32 personality)
{
	#ifndef HACK_NO_MOEMON
	return MoemonOld_Has(personality);
	#else
	return FALSE;
	#endif
}

extern const u8 EventScript_MoemonSwitch[];

//callasm: ON/OFF を切り替え、LASTRESULT = 切り替えたあと ON か
void HackMoemonSwitch_Toggle(void)
{
	if (FlagGet(HACK_FLAG_MOEMON_ON))
		FlagClear(HACK_FLAG_MOEMON_ON);
	else
		FlagSet(HACK_FLAG_MOEMON_ON);
	gSpecialVar_LastResult = FlagGet(HACK_FLAG_MOEMON_ON) != 0;
}

static void Task_MoemonSwitchField(u8 taskId)
{
	ScriptContext1_SetupScript(EventScript_MoemonSwitch);
	DestroyTask(taskId);
}

void FieldUseFunc_MoemonSwitch(u8 taskId)
{
	sItemUseOnFieldCB = Task_MoemonSwitchField;
	SetUpItemUseOnFieldCallback(taskId);
}

void HackMoemonSwitch_FieldTick(void)
{
	#ifndef HACK_NO_MOEMON
	MoemonOld_ScanStep();
	//★ 2026-10-09 ユーザー「フィールド歩いてるときにカクつく」: 一覧（最大 392 件）を 6 匹ぶん毎コマ探していたので 16 コマに 1 回に
	//  （ビットを読むのは通信を始めてから＝フィールドで 16 コマより長くかかる・状態は持てない 6.13 のでコマの数で間引く）
	if ((gMain.vblankCounter2 & 0xF) == 0)
		MoemonLink_MarkParty();
	#endif
	//★ 2026-10-10 ユーザー「設定細分化 アイテムでON/OFF系」: ON/OFF は「せってい」の行に（src/bw_option_menu.c）。道具はもう渡さず、持っていれば消す
	if ((gMain.vblankCounter2 & 0x3F) == 0x20 && CheckBagHasItem(ITEM_MOEMON_SWITCH, 1))
		RemoveBagItem(ITEM_MOEMON_SWITCH, 1);
}
