#include "defines.h"
#include "../include/sprite.h"
#include "fcomp.h"

/*
 * 連れ歩きの絵の自前の圧縮「F 方式」の展開（2026-09-28 ユーザー「連れ歩きポケモンの色違いも入れたいのでF方式で」）
 * 圧縮は tools/bwfollow/fcomp.py。展開の中核は src/fcomp.c（DPE の戦闘の絵と共用）。
 * docs/research/2026-09-28_follower-fcomp.md
 *
 * 展開した中身 = 使うタイル（使うタイルの数 x 32 バイト）＋ 番号の並び → 番号どおりにタイルを並べて生のシート
 *
 * ★ 書き換え可能な static は使えない（CLAUDE.md 6.13）。辞書と表の組は関数の中で作る
 * ★ 入れ物は Malloc。取れなければ FALSE（呼び元は次のコマでやり直す）
 */

extern const struct BwFTable gBwFLitTable;
extern const struct BwFTable gBwFDistTable;
extern const u16 gBwFLitSymbols[];
extern const u16 gBwFDistSymbols[];
extern const u16 gBwFLitFast[];
extern const u16 gBwFDistFast[];
extern const u8 gBwFDict[];
extern const u32 gBwFDictSize;
extern const u8 gBwFPixT1[];	//2026-10-06 F 方式 v2 の文脈の表（生成物）
extern const u8 gBwFPixT2[];

//data（1 種族）→ raw（rawSize バイトの生のシート）
static bool8 BwFDecode(const u8 *data, u8 *raw, u32 rawSize)
{
	u32 numTiles = FCOMP_NUM_TILES(data);
	u32 numUniq = FCOMP_NUM_UNIQ(data);
	u32 width = FCOMP_WIDTH(data);
	u32 bodySize = numUniq * 32 + numTiles * width;
	u32 i;
	u8 *body;
	struct FCompSet set;

	if (numTiles * 32 != rawSize || (width != 1 && width != 2) || numUniq == 0 || numUniq > numTiles)
		return FALSE;
	body = Malloc(bodySize + 4);
	if (body == NULL)
		return FALSE;

	set.dict = gBwFDict;
	set.dictSize = gBwFDictSize;
	set.litTable = &gBwFLitTable;
	set.litSymbols = gBwFLitSymbols;
	set.litFast = gBwFLitFast;
	set.distTable = &gBwFDistTable;
	set.distSymbols = gBwFDistSymbols;
	set.distFast = gBwFDistFast;
	set.pixT1 = gBwFPixT1;
	set.pixT2 = gBwFPixT2;
	if (!FCompInflate(&set, data, body, bodySize))
		goto FAIL;

	//番号の並びどおりにタイルを並べる（32 バイト = 8 語ずつ写す）
	for (i = 0; i < numTiles; ++i)
	{
		u32 idx = (width == 1) ? body[numUniq * 32 + i] : (body[numUniq * 32 + 2 * i] | (body[numUniq * 32 + 2 * i + 1] << 8));
		const u32 *src;
		u32 *dst = (u32 *) (raw + i * 32);
		u32 k;

		if (idx >= numUniq)
			goto FAIL;
		src = (const u32 *) (body + idx * 32);
		for (k = 0; k < 8; ++k)
			dst[k] = src[k];
	}
	Free(body);
	return TRUE;

FAIL:
	Free(body);
	return FALSE;
}

//連れ歩きのシートを F 方式で展開して VRAM に積む（src/bw_follower.c の BwFollowerLoadSheet が呼ぶ）
bool8 BwFollowerLoadFSheet(const u8 *data, u16 rawSize, u16 tag)
{
	struct SpriteSheet sheet;
	u8 *raw = Malloc(rawSize);

	if (raw == NULL)
		return FALSE;
	if (!BwFDecode(data, raw, rawSize))
	{
		Free(raw);
		return FALSE;
	}
	sheet.data = raw;
	sheet.size = rawSize;
	sheet.tag = tag;
	LoadSpriteSheet(&sheet);
	Free(raw);
	return TRUE;
}
