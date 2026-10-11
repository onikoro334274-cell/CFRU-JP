#include "defines.h"
#include "fcomp.h"

/*
 * 地図のタイルセットの絵の F 方式の展開（2026-10-06 ユーザー「2もお願い」・docs/research/2026-10-06_rom-compression-review.md）
 * 作るのは tools/mapfcomp/map_fcomp.py（生成物の .s の LZ77 を書き換える）。頭 0x47・生の大きさ 24 ビット・F のデータ（v2・幅 0）。
 * 幅 0 = タイルの使い回しも番号の並びも無い → 展開した中身がそのまま生のシート（展開先に直に書く・入れ物が要らない）。
 * 振り分け役（assembly/fcomp/lz_dispatch.s）が頭 0x47 をここへ。0 を返すと BIOS の LZ77 に回る（形が合わないとき）。
 * VRAM は 1 バイトの書き込みが効かないので、VRAM へは Malloc した入れ物に展開して 16 ビットずつ写す（DPE の絵と同じ）
 */

extern const struct BwFTable gMapFLitTable;
extern const struct BwFTable gMapFDistTable;
extern const u16 gMapFLitSymbols[];
extern const u16 gMapFDistSymbols[];
extern const u16 gMapFLitFast[];
extern const u16 gMapFDistFast[];
extern const u8 gMapFDict[];
extern const u32 gMapFDictSize;
extern const u8 gMapFPixT1[];
extern const u8 gMapFPixT2[];

u32 MapFTryDecode(const u8 *src, u8 *dst, u32 vram)
{
	u32 rawSize = src[1] | (src[2] << 8) | (src[3] << 16);
	const u8 *data = src + 4;
	struct FCompSet set;

	if (FCOMP_WIDTH(data) != 0 || FCOMP_VERSION(data) > 1 || (u32) FCOMP_NUM_TILES(data) * 32 != rawSize
	 || FCOMP_NUM_UNIQ(data) != FCOMP_NUM_TILES(data) || rawSize == 0)
		return 0;

	set.dict = gMapFDict;
	set.dictSize = gMapFDictSize;
	set.litTable = &gMapFLitTable;
	set.litSymbols = gMapFLitSymbols;
	set.litFast = gMapFLitFast;
	set.distTable = &gMapFDistTable;
	set.distSymbols = gMapFDistSymbols;
	set.distFast = gMapFDistFast;
	set.pixT1 = gMapFPixT1;
	set.pixT2 = gMapFPixT2;

	if (vram)
	{
		u8 *tmp = Malloc(rawSize);
		u32 i;

		if (tmp == NULL)
			return 1;	//BIOS に回すと F のデータを LZ77 として読んでしまう → 何もしない
		if (FCompInflate(&set, data, tmp, rawSize))
		{
			for (i = 0; i < rawSize; i += 2)
				*(u16 *) (dst + i) = tmp[i] | (tmp[i + 1] << 8);
		}
		Free(tmp);
		return 1;
	}

	FCompInflate(&set, data, dst, rawSize);
	return 1;
}
