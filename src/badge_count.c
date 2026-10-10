#include "defines.h"
#include "../include/event_data.h"
#include "../include/text.h"
#include "../include/constants/flags.h"

#include "Tables/hoenn_script_flags.h"
#include "Tables/johto_flags.h"
#include "Tables/sinnoh_flags.h"

/*
badge_count.c
    持っているバッジの数（2026-09-29 ユーザー「レポート時のバッジが８のままカウントが増えない」）

    素 FRLG はレポートの画面（SaveStatToString の SAVE_STAT_BADGES・080F9114）と
    「つづきから」の画面（PrintBadgeCount・0800C9E4）で、カントーの旗 0x820〜0x827 の 8 つだけを数える。
    ホウエンのバッジは別の旗（HOENN_FLAG_BADGE1〜8・移植したジムのスクリプトが立てる）なので増えなかった。
    → カントー＋ホウエン＋ジョウトの合計（いまは最大 24）。シンオウのバッジができたらここに足す。
    フックは assembly/hoenn/badges_hook.s・hooks（080F9114 / 0800C9E8）。
*/

/* FLAG_BADGE01_GET（0x820）・CHAR_0（0xA1）・EOS は CFRU のヘッダのもの。
   ★ 「こ」は素のレポート画面と同じ 0x0A（text.h の CHAR_KO は 0x5A で別の字）*/
#define CHAR_HIRAGANA_KO 0x0A

u8 GetAllBadgeCount(void)
{
	static const u16 sHoennBadgeFlags[8] =
	{
		HOENN_FLAG_BADGE1, HOENN_FLAG_BADGE2, HOENN_FLAG_BADGE3, HOENN_FLAG_BADGE4,
		HOENN_FLAG_BADGE5, HOENN_FLAG_BADGE6, HOENN_FLAG_BADGE7, HOENN_FLAG_BADGE8,
	};
	static const u16 sJohtoBadgeFlags[8] =
	{
		JOHTO_FLAG_BADGE1, JOHTO_FLAG_BADGE2, JOHTO_FLAG_BADGE3, JOHTO_FLAG_BADGE4,
		JOHTO_FLAG_BADGE5, JOHTO_FLAG_BADGE6, JOHTO_FLAG_BADGE7, JOHTO_FLAG_BADGE8,
	};
	static const u16 sSinnohBadgeFlags[8] =
	{
		SINNOH_FLAG_BADGE1, SINNOH_FLAG_BADGE2, SINNOH_FLAG_BADGE3, SINNOH_FLAG_BADGE4,
		SINNOH_FLAG_BADGE5, SINNOH_FLAG_BADGE6, SINNOH_FLAG_BADGE7, SINNOH_FLAG_BADGE8,
	};
	u8 n = 0;
	u32 i;

	for (i = 0; i < 8; ++i)
	{
		if (FlagGet(FLAG_BADGE01_GET + i))
			++n;
		if (FlagGet(sHoennBadgeFlags[i]))
			++n;
		if (FlagGet(sJohtoBadgeFlags[i]))		/* 2026-09-30 ジョウトのジム（tools/johto/build_johto_gyms.py）*/
			++n;
		if (FlagGet(sSinnohBadgeFlags[i]))		/* 2026-10-01 シンオウのジム（tools/sinnoh/build_sinnoh_gyms.py）*/
			++n;
	}
	return n;
}

/* レポートの画面: 「16こ」の形で dest に書く（素は 1 桁しか書けない）*/
void SaveStatBadgesToString(u8* dest)
{
	u8 n = GetAllBadgeCount();

	if (n >= 10)
		*dest++ = CHAR_0 + n / 10;
	*dest++ = CHAR_0 + n % 10;
	*dest++ = CHAR_HIRAGANA_KO;
	*dest = EOS;
}
