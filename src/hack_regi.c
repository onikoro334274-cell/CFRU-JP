#include "defines.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"

/*
 * レジ系のイベント（2026-10-08・tools/rom/build_hoenn_scripts.py の PUZZLE_MAPS / PATTERNS と tools/rom/hoenn_regi.py・
 * docs/research/2026-10-08_regi-events.md）。
 * ユーザー「レジ系のイベントいれて」→ 解き方は「点字を読むだけに簡略化」・ギガス・エレキ・ドラゴも・Lv 70・置き場は「座に合う場所」。
 *
 * どちらも callnative で呼ぶ（名前は HoennNative_ で始める・CLAUDE.md 6.42）。答えは LASTRESULT（VAR_RESULT）。
 */

//とざされたへや（内）の奥の点字: 先頭がホエルオー・最後がジーランス（エメラルドの CheckRelicanthWailord と同じ順）
void HoennNative_CheckRelicanthWailord(void)
{
	u8 count = CalculatePlayerPartyCount();

	gSpecialVar_LastResult = FALSE;
	if (count == 0)
		return;
	if (GetMonData(&gPlayerParty[0], MON_DATA_SPECIES2, NULL) == SPECIES_WAILORD
	 && GetMonData(&gPlayerParty[count - 1], MON_DATA_SPECIES2, NULL) == SPECIES_RELICANTH)
		gSpecialVar_LastResult = TRUE;
}

//レジギガス: 手持ちにレジロック・レジアイス・レジスチルがそろっているか（タマゴは数えない）
void HoennNative_RegiHasTrio(void)
{
	bool8 rock = FALSE, ice = FALSE, steel = FALSE;
	u32 i;

	for (i = 0; i < PARTY_SIZE; ++i)
	{
		switch (GetMonData(&gPlayerParty[i], MON_DATA_SPECIES2, NULL)) //タマゴは SPECIES_EGG
		{
			case SPECIES_REGIROCK:
				rock = TRUE;
				break;
			case SPECIES_REGICE:
				ice = TRUE;
				break;
			case SPECIES_REGISTEEL:
				steel = TRUE;
				break;
		}
	}
	gSpecialVar_LastResult = rock && ice && steel;
}
