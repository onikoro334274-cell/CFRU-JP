#include "defines.h"

/*
 * 追加の RAM 16MB（2026-10-08 ユーザー「mgbaのメモリってふやせる？」→「RAMもふやしてCPUもやる」）。
 * docs/research/2026-10-08_mgba-overclock.md
 *
 * - 直した mGBA（tools/mgba/xram.patch）だけ、0x01000000〜0x01FFFFFF が読み書きできる RAM になる（待ち 0・DMA の写し元にもなる・
 *   ステート保存に入る・電源を切れば消える＝セーブには入らない）。実機・SuperCard・素の mGBA では何も無い所（読むと前の命令の値）。
 * - 使う前に HackXram_Available() を見る。終わりの 4 バイトに 2 通りの値を書いて読み戻せたら使える（終わりの 4 バイトは見分けのために空けておく）。
 * - ★ BIOS の CpuSet／CpuFastSet は写し元が 0x02000000 より前だと断る（本物の BIOS の決まり）→ この RAM から写すときは DMA か自前の写し。
 * - まだ何も置いていない（使う側はこれから）。
 */
#define XRAM_PROBE ((vu32*) 0x01FFFFFC)

bool8 HackXram_Available(void)
{
	*XRAM_PROBE = 0x584D4152;   //'XMAR'
	if (*XRAM_PROBE != 0x584D4152)
		return FALSE;
	*XRAM_PROBE = 0xA7B2BEAD;
	if (*XRAM_PROBE != 0xA7B2BEAD)
		return FALSE;
	*XRAM_PROBE = 0;
	return TRUE;
}
