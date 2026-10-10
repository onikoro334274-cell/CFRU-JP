#include "defines.h"
#include "../include/malloc.h"
#include "../include/decompress.h"

/*
 * ポケモンの正面・後ろ姿の絵を ROM の外へ（2026-10-07・docs/research/2026-10-07_rom-64mb.md）。
 * ユーザー「今のポケモンの画像全部そっちの領域において」「正面と後ろ姿だけ」「新しいmGBAとSuperCardに対応」。
 *
 *   build_rom.py --split-pics で組むと、DPE の正面・後ろ姿の絵（…Tiles）は ROM に入らず、後ろ半分のかたまり PKSPICS.BIN
 *   （見出し "PKSPICS1"）へ移り、絵の表は 0x0A000000 + 位置 を指す（tools/build/dpe_fcomp_apply.split_out）。
 *   LZ77 の展開の入口（assembly/fcomp/lz_dispatch.s）は、元が 0x0A000000〜0x0BFFFFFF ならここへ来る。
 *     - 直した mGBA（tools/mgba・64MB の ROM）: 0x0A000000 に見出しが見える → 1 を返し、入口がそのまま展開する
 *       （ここから展開を呼ぶと、元がまた 0x0A… なので入口からここへ戻ってきて回り続ける）
 *     - SuperCard: 0x0A000000 は ROM の頭の写し → SD の PKSPICS.BIN の同じ位置から 6 区画（3KB）読んで展開する
 *       （圧縮した絵は 1 枚 2.3KB まで・ファイルの終わりは 0 で 4KB 埋めてある）
 *     - どちらも無い（素の mGBA・ほかのカートリッジ）: 何もしない（絵が出ない）
 *   ふつうのビルドでは 0x0A000000〜 を指すデータは無いので、ここには来ない。
 */

#define SPLIT_BASE 0x0A000000
#define SPLIT_FILE_83   "PKSPICS BIN"
#define SPLIT_FILE_LONG "PKSPICS.BIN"
#define SPLIT_READ_SECTORS 6

bool8 HackScsd_ReadFile(const char *name83, const char *longName, u32 sector, u32 n, void *out);

static bool8 HackSplitPics_Native(void)
{
	const u8 *p = (const u8 *) SPLIT_BASE;

	return p[0] == 'P' && p[1] == 'K' && p[2] == 'S' && p[3] == 'P' && p[4] == 'I' && p[5] == 'C' && p[6] == 'S' && p[7] == '1';
}

//1 = そのまま読める（入口のふつうの道で展開する）・0 = ここで済ませた（SD から読んで展開した・読めなかった）
u32 HackSplitPics_Decompress(const void *src, void *dest, u32 vram)
{
	u32 off = (u32) src - SPLIT_BASE;
	u8 *buf;

	if (HackSplitPics_Native())
		return 1;
	buf = AllocZeroed(SPLIT_READ_SECTORS * 512);
	if (buf == NULL)
		return 0;
	if (HackScsd_ReadFile(SPLIT_FILE_83, SPLIT_FILE_LONG, off / 512, SPLIT_READ_SECTORS, buf))
	{
		//RAM の写しを展開（入口に戻るが、元が RAM なのでここには来ない）
		if (vram)
			LZ77UnCompVram(buf + off % 512, dest);
		else
			LZ77UnCompWram(buf + off % 512, dest);
	}
	Free(buf);
	return 0;
}
