#include "defines.h"
#include "fcomp.h"

/*
 * DPE の戦闘の絵（前向き・後ろ向き）の F 方式の展開（2026-09-28 ユーザー「DPEも対象」「その案で進めて」）
 * docs/research/2026-09-28_dpe-sprite-fcomp.md
 *
 * ★ ゲーム中の LZ77 の展開はすべて BIOS の入口 2 つ（LZ77UnCompVram 0x081C7A8C / LZ77UnCompWram 0x081C7A90）を通る。
 *   自前のビルドの道具（tools/build/build_rom.py）が入口を振り分け役（assembly/fcomp/lz_dispatch.s）へ飛ばし、
 *   頭の 1 バイトが F の印（0x46）のものだけここへ来る。それ以外は今までどおり BIOS が展開する
 * ★ 絵のデータ = 印 0x46・生の大きさ 24 ビット（LZ77 と同じ形。LoadCompressedSpriteSheetUsingHeap などが頭の大きさで入れ物を取る）
 *   ＋ F のデータ（src/fcomp.h）。作るのは tools/dpefcomp/dpe_fcomp.py（DPE のビルドのとき grit の LZ77 を入れ替える）
 * ★ 入れ物を取らない: 使うタイル → 番号の並び の順に展開先の頭へ展開し、並びだけスタック（128 バイト）に写してから
 *   **後ろから**タイルを広げる。使うタイルは初めて出た順に並ぶので、タイル i が使う番号は i 以下 → 読む前に潰さない
 *   （条件に合わない絵は道具が LZ77 のまま残す: タイル 128 枚まで・番号の幅 1・使うタイル＋並び ≦ 生の大きさ）
 * ★ VRAM は 1 バイトの書き込みが効かないので、VRAM へは Malloc した入れ物に展開して 16 ビットずつ写す
 */

#define DPEF_MAX_TILES 128		//tools/dpefcomp/dpe_fcomp.py の MAX_TILES と同じ

extern const struct BwFTable gDpeFLitTable;
extern const struct BwFTable gDpeFDistTable;
extern const u16 gDpeFLitSymbols[];
extern const u16 gDpeFDistSymbols[];
extern const u16 gDpeFLitFast[];
extern const u16 gDpeFDistFast[];
extern const u8 gDpeFDict[];
extern const u32 gDpeFDictSize;
extern const u8 gDpeFPixT1[];	//2026-10-06 F 方式 v2 の文脈の表（生成物）
extern const u8 gDpeFPixT2[];

static bool8 DpeFExpand(const u8 *data, u8 *out, u32 numTiles, u32 numUniq)
{
	u8 map[DPEF_MAX_TILES];
	struct FCompSet set;
	u32 i;

	set.dict = gDpeFDict;
	set.dictSize = gDpeFDictSize;
	set.litTable = &gDpeFLitTable;
	set.litSymbols = gDpeFLitSymbols;
	set.litFast = gDpeFLitFast;
	set.distTable = &gDpeFDistTable;
	set.distSymbols = gDpeFDistSymbols;
	set.distFast = gDpeFDistFast;
	set.pixT1 = gDpeFPixT1;
	set.pixT2 = gDpeFPixT2;
	if (!FCompInflate(&set, data, out, numUniq * 32 + numTiles))
		return FALSE;

	for (i = 0; i < numTiles; ++i)
		map[i] = out[numUniq * 32 + i];

	//後ろから広げる（タイル i が使う番号 j は i 以下。j < i の j はまだ潰していない）
	for (i = numTiles; i-- != 0;)
	{
		u32 j = map[i];

		if (j > i)
			return FALSE;
		if (j != i)
		{
			u32 k;

			if (((u32) out & 3) == 0)
			{
				const u32 *src = (const u32 *) (out + j * 32);
				u32 *dst = (u32 *) (out + i * 32);

				for (k = 0; k < 8; ++k)
					dst[k] = src[k];
			}
			else
			{
				for (k = 0; k < 32; ++k)
					out[i * 32 + k] = out[j * 32 + k];
			}
		}
	}
	return TRUE;
}

//振り分け役から呼ばれる。→ 1 = こちらで片付けた / 0 = F の形になっていない（振り分け役が BIOS に回す）
u32 DpeFTryDecode(const u8 *src, u8 *dst, u32 vram)
{
	u32 rawSize = src[1] | (src[2] << 8) | (src[3] << 16);
	const u8 *data = src + 4;
	u32 numTiles = FCOMP_NUM_TILES(data);
	u32 numUniq = FCOMP_NUM_UNIQ(data);

	if (FCOMP_WIDTH(data) != 1 || FCOMP_VERSION(data) > 1 || numTiles * 32 != rawSize || numTiles > DPEF_MAX_TILES
	 || numUniq == 0 || numUniq > numTiles || numUniq * 32 + numTiles > rawSize)
		return 0;

	if (vram)
	{
		u8 *tmp = Malloc(rawSize);
		u32 i;

		if (tmp == NULL)
			return 1;	//BIOS に回すと F のデータを LZ77 として読んでしまう → 何もしない
		if (DpeFExpand(data, tmp, numTiles, numUniq))
		{
			for (i = 0; i < rawSize; i += 2)
				*(u16 *) (dst + i) = tmp[i] | (tmp[i + 1] << 8);
		}
		Free(tmp);
		return 1;
	}

	DpeFExpand(data, dst, numTiles, numUniq);
	return 1;
}
