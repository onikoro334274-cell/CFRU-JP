#include "defines.h"
#include "../include/malloc.h"
#include "../include/decompress.h"
#include "../include/sprite.h"

/*
 * ROM の後ろ半分（0x0A000000〜）に置いたデータを読む（2026-10-07・docs/research/2026-10-07_rom-64mb.md）。
 * ユーザー「今のポケモンの画像全部そっちの領域において」「正面と後ろ姿だけ」「新しいmGBAとSuperCardに対応」
 * → 2026-10-11「64MB の後ろ半分を使い始めよう」: CFRU のデータ（地図のタイルセットの絵・縮めたマス目の LZ・連れ歩きのシート・
 *   Moémon の連れ歩き）も同じ道へ（tools/build/split_back.py・docs/research/2026-10-11_back-half-general.md）。
 *
 *   build_rom.py --split-pics で組むと、後ろ半分のかたまり（見出し "PKSPICS1"）は CFRU の塊（build/split_cfru.bin）＋ DPE の
 *   正面・後ろ姿の絵。各塊は [u32 長さ][データ] で、表やラベルはデータの頭（0x0A000000 + 位置）を指す。
 *   - 直した mGBA（tools/mgba・64MB の ROM）: 0x0A000000 に見出しが見える → そのまま読める（Native）
 *   - SuperCard: 0x0A000000 は ROM の頭の写し → SD の PKSPICS.BIN の同じ位置から、長さぶんの区画を読む
 *   - どちらも無い（素の mGBA・ほかのカートリッジ）: 読めない（絵が出ない）
 *   LZ77 の展開の入口（assembly/fcomp/lz_dispatch.s）は、元が 0x0A000000〜0x0BFFFFFF ならここ（HackSplitPics_Decompress）へ来る。
 *   ★ 5e の指摘 2026-10-11: 素の処理には、入口より前に「展開後の大きさ」を元のポインタから直に読んで Alloc するものがある
 *     （MallocAndDecompress 080F7AF8＝地図のタイルセット・LoadCompressedSpriteSheetUsingHeap 0800E9E8＝Moémon の連れ歩き・ボール）。
 *     SuperCard では 0x0A… は 32MB の鏡なのでゴミを読む → functionrewrites でこの 2 つを置き換え、後ろ半分なら先に写しを取ってから
 *     素と同じ処理をする（下の HackSplit_MallocAndDecompress・HackSplit_LoadCompressedSpriteSheetUsingHeap）。
 *   入口を通らずに直に読むもの（連れ歩きのシート＝src/bw_follower_sheet.c）は HackSplit_Fetch で RAM の写しをもらう。
 *   ヒープが無いとき（SuperCard は 7KB しか無いことがある）は gDecompressionBuffer（0x0201C000・0x4000）を借りる。
 *   ふつうのビルドでは 0x0A000000〜 を指すデータは無いので、ここには来ない。
 */

#define SPLIT_BASE 0x0A000000
#define SPLIT_FILE_83   "PKSPICS BIN"
#define SPLIT_FILE_LONG "PKSPICS.BIN"
#define SPLIT_MAX_READ  0x8000      //1 回に読む上限（ヒープは 0x1AE00・連れ歩きの 1 シートは 3KB・タイルセットは 6KB・マス目は 4.2KB まで）
#define FALLBACK_BUF      ((u8 *) 0x0201C000)   //gDecompressionBuffer（絵の展開の一時置き場＝地図の読み込み中は空いている）
#define FALLBACK_BUF_SIZE 0x4000

void *HackScsd_FileOpen(const char *name83, const char *longName);
bool8 HackScsd_FileRead(void *fs, u32 sector, u32 n, void *out);
void HackScsd_FileClose(void *fs);

bool8 HackSplit_Native(void)
{
	const u8 *p = (const u8 *) SPLIT_BASE;

	return p[0] == 'P' && p[1] == 'K' && p[2] == 'S' && p[3] == 'P' && p[4] == 'I' && p[5] == 'C' && p[6] == 'S' && p[7] == '1';
}

bool8 HackSplit_IsBack(const void *src)
{
	return ((u32) src >> 25) == (SPLIT_BASE >> 25);
}

//SD の PKSPICS.BIN の off にある塊（4 バイト前に長さ）を、区画の頭から入れ物に読む（1 回のマウント）。
//len を知っていれば渡す（0 なら 4 バイト前から読む）。入れ物は Malloc・だめなら借り物（*owned = FALSE）。→ 入れ物の頭（NULL = 読めない）
//allowFallback: 借り物の入れ物を使ってよいか（★ 5e の指摘: 展開先が gDecompressionBuffer の呼び元＝素の LoadCompressedSpriteSheet などでは、
//入力と出力が同じ入れ物で重なる → 使わない。Malloc がだめなら読まない）
static u8 *ReadEntry(u32 off, u32 len, bool8 *owned, u32 *outLen, bool8 allowFallback)
{
	void *fs = HackScsd_FileOpen(SPLIT_FILE_83, SPLIT_FILE_LONG);
	u8 *buf = NULL;
	u32 first, nsec;

	*owned = FALSE;
	*outLen = 0;
	if (fs == NULL)
		return NULL;
	if (len == 0)
	{
		//長さは off - 4 の語。借り物の入れ物の末尾の 1 区画に読む（本体の読みより前に使い終わる）
		u8 *tmp = FALLBACK_BUF + FALLBACK_BUF_SIZE - 512;
		u32 lenOff = off - 4;

		if (!HackScsd_FileRead(fs, lenOff / 512, 1, tmp))
			goto DONE;
		len = tmp[lenOff % 512] | (tmp[lenOff % 512 + 1] << 8) | (tmp[lenOff % 512 + 2] << 16) | (tmp[lenOff % 512 + 3] << 24);
	}
	if (len == 0 || len > SPLIT_MAX_READ)
		goto DONE;
	first = off / 512;
	nsec = (off % 512 + len + 511) / 512;
	buf = Malloc(nsec * 512);
	if (buf != NULL)
		*owned = TRUE;
	else if (allowFallback && nsec * 512 <= FALLBACK_BUF_SIZE)
		buf = FALLBACK_BUF;
	else
		goto DONE;
	if (!HackScsd_FileRead(fs, first, nsec, buf))
	{
		if (*owned)
			Free(buf);
		*owned = FALSE;
		buf = NULL;
		goto DONE;
	}
	*outLen = len;
DONE:
	HackScsd_FileClose(fs);
	return buf;
}

//後ろ半分の src から長さ len の写しを RAM にもらう（len 0 なら 4 バイト前の長さ）。直した mGBA なら src そのもの（*alloc は NULL）。
//SD から読んだときは *alloc に Malloc した入れ物（呼んだ側が Free する・借り物なら NULL）
const u8 *HackSplit_Fetch(const void *src, u32 len, void **alloc)
{
	u32 off = (u32) src - SPLIT_BASE;
	u8 *buf;
	bool8 owned;
	u32 got;

	*alloc = NULL;
	if (!HackSplit_IsBack(src) || HackSplit_Native())
		return src;
	buf = ReadEntry(off, len, &owned, &got, TRUE);        //呼んだ側の展開先は Alloc した先か VMap（借り物と重ならない）
	if (buf == NULL)
		return NULL;
	if (owned)
		*alloc = buf;
	return buf + off % 512;
}

//後ろ半分の塊（4 バイト前に長さ）を丸ごと RAM にもらう。長さを知らない読み手用（Deflate のマス目など）。直した mGBA なら src そのもの
const u8 *HackSplit_FetchEntry(const void *src, void **alloc)
{
	return HackSplit_Fetch(src, 0, alloc);
}

//後ろ半分の塊を、呼んだ側の入れ物 dst（cap バイト・区画の頭から読むので 長さ + 1023 まで要る）に読む。ヒープを使わない読み手用
//（1e の Deflate のマス目＝VMap の末尾へ）。→ 長さ（0 = 読めない）・*out は中身の頭（直した mGBA なら src そのもの）
u32 HackSplit_FetchEntryTo(const void *src, void *dst, u32 cap, const u8 **out)
{
	u32 off = (u32) src - SPLIT_BASE;
	u32 lenOff = off - 4;
	u32 len = 0, first, nsec;
	void *fs;
	u8 *tmp = FALLBACK_BUF + FALLBACK_BUF_SIZE - 512;

	*out = src;
	if (!HackSplit_IsBack(src))
		return 0;
	if (HackSplit_Native())
		return *(const u32 *) ((u32) src - 4);
	fs = HackScsd_FileOpen(SPLIT_FILE_83, SPLIT_FILE_LONG);
	if (fs == NULL)
		return 0;
	if (HackScsd_FileRead(fs, lenOff / 512, 1, tmp))
		len = tmp[lenOff % 512] | (tmp[lenOff % 512 + 1] << 8) | (tmp[lenOff % 512 + 2] << 16) | (tmp[lenOff % 512 + 3] << 24);
	first = off / 512;
	nsec = (off % 512 + len + 511) / 512;
	if (len == 0 || nsec * 512 > cap || !HackScsd_FileRead(fs, first, nsec, dst))
		len = 0;
	HackScsd_FileClose(fs);
	if (len)
		*out = (const u8 *) dst + off % 512;
	return len;
}

//lz_dispatch.s から: 1 = そのまま読める（入口のふつうの道で展開する）・0 = ここで済ませた（SD から読んで展開した・読めなかった）
u32 HackSplitPics_Decompress(const void *src, void *dest, u32 vram)
{
	u32 off = (u32) src - SPLIT_BASE;
	u32 len;
	u8 *buf;
	bool8 owned;

	if (HackSplit_Native())
		return 1;
	//展開先が借り物の入れ物（gDecompressionBuffer）と重なる呼び元（素の LoadCompressedSpriteSheet など）では借りない
	buf = ReadEntry(off, 0, &owned, &len, !(!vram && (u32) dest >= (u32) FALLBACK_BUF && (u32) dest < (u32) FALLBACK_BUF + FALLBACK_BUF_SIZE));
	if (buf == NULL)
		return 0;
	//RAM の写しを展開（入口に戻るが、元が RAM なのでここには来ない。頭が 0x46／0x47 の F もそこで振り分けられる）
	if (vram)
		LZ77UnCompVram(buf + off % 512, dest);
	else
		LZ77UnCompWram(buf + off % 512, dest);
	if (owned)
		Free(buf);
	return 0;
}

//---- 素の処理の置き換え（functionrewrites）: 展開前に大きさを直に読むものは、後ろ半分なら先に写しを取る ----

//MallocAndDecompress 080F7AF8（pokefirered new_menu_helpers.c）: 地図のタイルセット（DecompressAndCopyTileDataToVram2）などが通る
void *HackSplit_MallocAndDecompress(const void *src, u32 *size)
{
	void *alloc;
	const u8 *s = HackSplit_FetchEntry(src, &alloc);
	void *ptr = NULL;

	if (s == NULL)
	{
		*size = 0;
		return NULL;
	}
	*size = s[1] | (s[2] << 8) | (s[3] << 16);
	ptr = Malloc(*size);
	if (ptr)
		LZ77UnCompWram(s, ptr);
	if (alloc)
		Free(alloc);
	return ptr;
}

//LoadCompressedSpriteSheetUsingHeap 0800E9E8（pokefirered decompress.c）: Moémon の連れ歩き・ボールのシートが通る。TRUE = 失敗（素と同じ）
bool8 HackSplit_LoadCompressedSpriteSheetUsingHeap(const struct CompressedSpriteSheet *src)
{
	struct SpriteSheet dest;
	void *buffer;
	void *alloc;
	const u8 *s = HackSplit_FetchEntry(src->data, &alloc);

	if (s == NULL)
		return TRUE;
	buffer = AllocZeroed((*(const u32 *) s) >> 8);
	if (!buffer)
	{
		if (alloc)
			Free(alloc);
		return TRUE;
	}
	LZ77UnCompWram(s, buffer);
	dest.data = buffer;
	dest.size = src->size;
	dest.tag = src->tag;
	LoadSpriteSheet(&dest);
	Free(buffer);
	if (alloc)
		Free(alloc);
	return FALSE;
}
