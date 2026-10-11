#include "defines.h"

/*
 * Deflate（RFC 1951・生のストリーム）の展開器（2026-10-11 ユーザー「その案で実装」＝地図のマス目の Deflate 化・patches/0319）。
 * 相手は地図のレイアウトのマス目（tools/rom/layout_pack.py の縮めた形・頭 0x5A・生の大きさ 24 ビット・そのあと raw deflate）。
 * 読むのは src/hack_packed_layout.c（地図に入るとき・つなぎ目を越えるときに VMap の後ろへ展開）。作るのは tools/mapfcomp/layout_deflate.py（zlib・wbits -15）。
 * 資料 docs/research/2026-10-11_layout-deflate.md
 *
 * 作り（zlib の contrib/puff の手順を Thumb 向けに書き直したもの）:
 *   - 表と作業用の入れ物（struct InfState・3,612 バイト＝HackInflate_WorkSize() 3,616）は呼ぶ側が渡す作業場に置く（★ 書き換え可能な static は使えない・6.13。
 *     スタックの枠も 1,024 B まで・6.35。地図のときは VMap の頭の空き＝Malloc に頼らない・5e の指摘 2026-10-11）
 *   - 符号は正準ハフマン（長さごとの数 count[] と記号の並び symbol[]）。文字・長さは先の 9 ビット、距離は先の 7 ビットの速い表で引き、
 *     それより長い符号だけ 1 ビットずつ読む。中の輪（InfCodes）は状態をレジスタに置く（つなぎ目を越える 1 歩の中で動くので速さが要る）
 *   - 出力は生の大きさぶんだけ（outSize を超えて書かない）。入力は 生の大きさ + 64 の先を読まない（化けたデータで止まる・bad）
 *   - 固定ハフマン（BTYPE 1）・動的（2）・無圧縮（0）の 3 つとも読む
 */

#define INF_MAXBITS 15
#define INF_MAXLCODES 286
#define INF_MAXDCODES 30
#define INF_FIXLCODES 288
#define INF_FAST_BITS 10                 //文字・長さの速い表（先の 10 ビット・9 だと長い符号の遅い道が 12% あった）
#define INF_DFAST_BITS 8                 //距離の速い表（先の 8 ビット）
#define INF_SHIFT 10                     //速い表の値: 記号 | 符号の長さ << INF_SHIFT（記号は 0〜287）
#define INF_MAGIC 0x5A
#define INF_IN_MARGIN 64                 //srcSize が 0 のときの入力の終わり = 頭 4 + 生の大きさ + これ（ROM の塊は縮めた長さを持たない・Deflate は生より短い）
#define INF_PREFETCH 8                   //srcSize が分かるときの読み越しの余裕（速い表が先の 10 ビット・距離の余りの 13 ビットを先に読む＝最大 3 バイト）。呼ぶ側は終わりの先 8 バイトが読める所に置く（SD の写しの入れ物は長さ + 1023・ROM はそのまま）

struct InfHuff
{
	u16 count[INF_MAXBITS + 1];      //長さ k の符号の数
	u16 symbol[INF_FIXLCODES];       //記号（正準の並び）
};

struct InfHuffDist
{
	u16 count[INF_MAXBITS + 1];
	u16 symbol[32];                  //距離の記号は 30 まで（5e の指摘: 288 は要らない）
};

struct InfState
{
	const u8* in;
	const u8* inEnd;
	u32 bitBuf;
	u32 bitCnt;
	u8* out;
	u32 outPos;
	u32 outSize;
	u8 bad;                          //入力の終わりを越えて読んだ（化けたデータ）
	struct InfHuff lencode;
	struct InfHuffDist distcode;
	u8 lengths[INF_MAXLCODES + INF_MAXDCODES + 2];   //符号の長さは 0〜15（u8 で足りる）
	u16 fast[1 << INF_FAST_BITS];    //文字・長さ: 記号 | 符号の長さ << 10（0 なら 10 ビットより長い → 1 ビットずつ）
	u16 dfast[1 << INF_DFAST_BITS];  //距離: 記号 | 符号の長さ << 10
};

#define HACK_INFLATE_WORK_SIZE (sizeof(struct InfState) + 4)

static const u16 sLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const u8 sLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const u16 sDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const u8 sDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
static const u8 sClenOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

//入力を 1 バイト読み足す（終わりを越えたら 0 を足して bad）
static inline u32 InfNextByte(struct InfState* s)
{
	if (s->in < s->inEnd)
		return *s->in++;
	s->bad = 1;
	return 0;
}

static u32 InfBits(struct InfState* s, u32 need)
{
	u32 val = s->bitBuf;

	while (s->bitCnt < need)
	{
		val |= InfNextByte(s) << s->bitCnt;
		s->bitCnt += 8;
	}
	s->bitBuf = val >> need;
	s->bitCnt -= need;
	return val & ((1u << need) - 1);
}

//正準ハフマンの表を作る → 0 なら完全・正なら足りない（1 つだけの符号なども許す）・負なら多すぎる（壊れている）
static s32 InfConstruct(u16* count, u16* symbol, const u8* length, u32 n)
{
	u32 sym, len;
	s32 left;
	u16 offs[INF_MAXBITS + 1];

	for (len = 0; len <= INF_MAXBITS; len++)
		count[len] = 0;
	for (sym = 0; sym < n; sym++)
		count[length[sym]]++;
	if (count[0] == n)
		return 0;
	left = 1;
	for (len = 1; len <= INF_MAXBITS; len++)
	{
		left <<= 1;
		left -= count[len];
		if (left < 0)
			return left;
	}
	offs[1] = 0;
	for (len = 1; len < INF_MAXBITS; len++)
		offs[len + 1] = offs[len] + count[len];
	for (sym = 0; sym < n; sym++)
	{
		if (length[sym] != 0)
			symbol[offs[length[sym]]++] = sym;
	}
	return left;
}

//速い表（先の fastBits ビット＝読む順なので符号のビットを逆にして引く）: 記号 | 符号の長さ << 9・0 なら長い符号（1 ビットずつ）
static void InfBuildFast(const u16* count, const u16* symbol, u16* fast, u32 fastBits)
{
	u32 len, code = 0, index = 0, i, k;

	for (i = 0; i < (1u << fastBits); i++)
		fast[i] = 0;
	for (len = 1; len <= INF_MAXBITS; len++)
	{
		u32 n = count[len];

		for (i = 0; i < n; i++, index++, code++)
		{
			if (len <= fastBits)
			{
				u32 rev = 0, b;

				for (b = 0; b < len; b++)
					rev |= ((code >> b) & 1) << (len - 1 - b);
				for (k = rev; k < (1u << fastBits); k += 1 << len)
					fast[k] = symbol[index] | (len << INF_SHIFT);
			}
		}
		code <<= 1;
	}
}

//1 ビットずつ読む（puff の decode）→ 記号・無ければ -1
static s32 InfDecodeSlow(struct InfState* s, const u16* count, const u16* symbol)
{
	s32 len, code = 0, first = 0, index = 0, cnt;

	for (len = 1; len <= INF_MAXBITS; len++)
	{
		code |= InfBits(s, 1);
		cnt = count[len];
		if (code - cnt < first)
			return symbol[index + (code - first)];
		index += cnt;
		first += cnt;
		first <<= 1;
		code <<= 1;
	}
	return -1;
}

//文字・長さ＋距離の表で 1 ブロックぶん書く → 0 なら OK。状態はレジスタに置き、長い符号のときだけ struct に戻して InfDecodeSlow
#define INF_NEED(n) \
	while (bitCnt < (n)) \
	{ \
		if (in < inEnd) \
			bitBuf |= (u32) *in++ << bitCnt; \
		else \
			s->bad = 1; \
		bitCnt += 8; \
	}
#define INF_TAKE(n, v) \
	do \
	{ \
		(v) = bitBuf & ((1u << (n)) - 1); \
		bitBuf >>= (n); \
		bitCnt -= (n); \
	} while (0)
#define INF_SAVE() do { s->in = in; s->bitBuf = bitBuf; s->bitCnt = bitCnt; } while (0)
#define INF_LOAD() do { in = s->in; bitBuf = s->bitBuf; bitCnt = s->bitCnt; } while (0)

static s32 InfCodes(struct InfState* s)
{
	const u8* in = s->in;
	const u8* inEnd = s->inEnd;
	u32 bitBuf = s->bitBuf;
	u32 bitCnt = s->bitCnt;
	u8* out = s->out;
	u32 outPos = s->outPos;
	const u32 outSize = s->outSize;
	const u16* fast = s->fast;
	const u16* dfast = s->dfast;
	s32 err = 0;

	for (;;)
	{
		u32 e, sym, len, dist, nb;
		s32 r;

		if (s->bad)
		{
			err = -16;
			break;
		}
		INF_NEED(INF_FAST_BITS);
		e = fast[bitBuf & ((1 << INF_FAST_BITS) - 1)];
		if (e != 0)
		{
			bitBuf >>= e >> INF_SHIFT;
			bitCnt -= e >> INF_SHIFT;
			sym = e & ((1 << INF_SHIFT) - 1);
		}
		else
		{
			INF_SAVE();
			r = InfDecodeSlow(s, s->lencode.count, s->lencode.symbol);
			INF_LOAD();
			if (r < 0)
			{
				err = -1;
				break;
			}
			sym = r;
		}
		if (sym < 256)
		{
			if (outPos >= outSize)
			{
				err = -2;
				break;
			}
			out[outPos++] = sym;
			continue;
		}
		if (sym == 256)
			break;
		sym -= 257;
		if (sym >= 29)
		{
			err = -3;
			break;
		}
		nb = sLenExtra[sym];
		INF_NEED(nb);
		INF_TAKE(nb, len);
		len += sLenBase[sym];

		INF_NEED(INF_DFAST_BITS);
		e = dfast[bitBuf & ((1 << INF_DFAST_BITS) - 1)];
		if (e != 0)
		{
			bitBuf >>= e >> INF_SHIFT;
			bitCnt -= e >> INF_SHIFT;
			sym = e & ((1 << INF_SHIFT) - 1);
		}
		else
		{
			INF_SAVE();
			r = InfDecodeSlow(s, s->distcode.count, s->distcode.symbol);
			INF_LOAD();
			if (r < 0)
			{
				err = -4;
				break;
			}
			sym = r;
		}
		if (sym >= 30)
		{
			err = -4;
			break;
		}
		nb = sDistExtra[sym];
		INF_NEED(nb);
		INF_TAKE(nb, dist);
		dist += sDistBase[sym];
		if (dist > outPos || outPos + len > outSize)
		{
			err = -5;
			break;
		}
		{
			u8* d = out + outPos;
			const u8* from = d - dist;

			outPos += len;
			if (((((u32) d) | dist) & 1) == 0 && dist >= 2)   //マス目は u16: 2 バイトそろいなら 2 バイトずつ（dist 2 以上なら from は書く前の所）
			{
				u16* d2 = (u16*) d;
				const u16* f2 = (const u16*) from;

				for (; len >= 2; len -= 2)
					*d2++ = *f2++;
				d = (u8*) d2;
				from = (const u8*) f2;
			}
			while (len--)
				*d++ = *from++;
		}
	}
	INF_SAVE();
	s->outPos = outPos;
	return err;
}

static s32 InfStored(struct InfState* s)
{
	u32 len, nlen;

	if (s->bad)
		return -17;
	//バイトの境目へ。速い表のために読み足した丸ごとのバイトは戻す（InfCodes は 9 ビットまで読み足すので 8 ビット以上残ることがある）
	while (s->bitCnt >= 8)
	{
		s->in--;
		s->bitCnt -= 8;
	}
	s->bitBuf = 0;
	s->bitCnt = 0;
	if (s->in + 4 > s->inEnd)
		return -18;
	len = s->in[0] | (s->in[1] << 8);
	nlen = s->in[2] | (s->in[3] << 8);
	s->in += 4;
	if (len != (~nlen & 0xFFFF) || s->outPos + len > s->outSize || s->in + len > s->inEnd)
		return -6;
	while (len--)
		s->out[s->outPos++] = *s->in++;
	return 0;
}

static s32 InfFixed(struct InfState* s)
{
	u32 sym;

	for (sym = 0; sym < 144; sym++)
		s->lengths[sym] = 8;
	for (; sym < 256; sym++)
		s->lengths[sym] = 9;
	for (; sym < 280; sym++)
		s->lengths[sym] = 7;
	for (; sym < INF_FIXLCODES; sym++)
		s->lengths[sym] = 8;
	InfConstruct(s->lencode.count, s->lencode.symbol, s->lengths, INF_FIXLCODES);
	InfBuildFast(s->lencode.count, s->lencode.symbol, s->fast, INF_FAST_BITS);
	for (sym = 0; sym < INF_MAXDCODES; sym++)
		s->lengths[sym] = 5;
	InfConstruct(s->distcode.count, s->distcode.symbol, s->lengths, INF_MAXDCODES);
	InfBuildFast(s->distcode.count, s->distcode.symbol, s->dfast, INF_DFAST_BITS);
	return InfCodes(s);
}

static s32 InfDynamic(struct InfState* s)
{
	u32 nlen, ndist, ncode, index;
	s32 err;

	nlen = InfBits(s, 5) + 257;
	ndist = InfBits(s, 5) + 1;
	ncode = InfBits(s, 4) + 4;
	if (nlen > INF_MAXLCODES || ndist > INF_MAXDCODES)
		return -7;
	for (index = 0; index < ncode; index++)
		s->lengths[sClenOrder[index]] = InfBits(s, 3);
	for (; index < 19; index++)
		s->lengths[sClenOrder[index]] = 0;
	if (InfConstruct(s->lencode.count, s->lencode.symbol, s->lengths, 19) != 0)   //符号の長さの符号（lencode を作業に使う）
		return -8;
	index = 0;
	while (index < nlen + ndist)
	{
		s32 sym = InfDecodeSlow(s, s->lencode.count, s->lencode.symbol);
		u32 len, rep;

		if (sym < 0 || s->bad)
			return -9;
		if (sym < 16)
		{
			s->lengths[index++] = sym;
			continue;
		}
		len = 0;
		if (sym == 16)
		{
			if (index == 0)
				return -10;
			len = s->lengths[index - 1];
			rep = 3 + InfBits(s, 2);
		}
		else if (sym == 17)
			rep = 3 + InfBits(s, 3);
		else
			rep = 11 + InfBits(s, 7);
		if (index + rep > nlen + ndist)
			return -11;
		while (rep--)
			s->lengths[index++] = len;
	}
	if (s->lengths[256] == 0)
		return -12;
	err = InfConstruct(s->lencode.count, s->lencode.symbol, s->lengths, nlen);
	if (err < 0 || (err > 0 && nlen - s->lencode.count[0] != 1))
		return -13;
	InfBuildFast(s->lencode.count, s->lencode.symbol, s->fast, INF_FAST_BITS);
	err = InfConstruct(s->distcode.count, s->distcode.symbol, s->lengths + nlen, ndist);
	if (err < 0 || (err > 0 && ndist - s->distcode.count[0] != 1))
		return -14;
	InfBuildFast(s->distcode.count, s->distcode.symbol, s->dfast, INF_DFAST_BITS);
	return InfCodes(s);
}

u32 HackInflate_WorkSize(void)
{
	return HACK_INFLATE_WORK_SIZE;
}

//頭 0x5A・生の大きさ 24 ビット・raw deflate → dst に dstSize バイト。work は作業場（HackInflate_WorkSize() バイト以上・dst と重ねない）。元に戻せたら TRUE
//★ 2026-10-11 srcSize = 入力の長さ（頭を含む・SD の写しなど長さが分かるとき）。0 なら ROM の塊とみなして 頭 4 + dstSize + 64 まで（5e の指摘 2）
bool8 HackInflate(const u8* src, u32 srcSize, u8* dst, u32 dstSize, void* work, u32 workSize)
{
	struct InfState* s;
	u32 last, type;
	s32 err = 0;

	if (src[0] != INF_MAGIC || (u32) (src[1] | (src[2] << 8) | (src[3] << 16)) != dstSize)
		return FALSE;
	if (work == NULL || workSize < HACK_INFLATE_WORK_SIZE)
		return FALSE;
	s = (struct InfState*) (((u32) work + 3) & ~3);          //u32 の欄があるので 4 の倍数に
	s->in = src + 4;
	if (srcSize != 0 && srcSize < 5)
		return FALSE;
	s->inEnd = srcSize ? src + srcSize + INF_PREFETCH : src + 4 + dstSize + INF_IN_MARGIN;
	s->bitBuf = 0;
	s->bitCnt = 0;
	s->bad = 0;
	s->out = dst;
	s->outPos = 0;
	s->outSize = dstSize;
	do
	{
		last = InfBits(s, 1);
		type = InfBits(s, 2);
		if (s->bad)
			err = -16;
		else if (type == 0)
			err = InfStored(s);
		else if (type == 1)
			err = InfFixed(s);
		else if (type == 2)
			err = InfDynamic(s);
		else
			err = -15;
	} while (!last && err == 0);
	return err == 0 && !s->bad && s->outPos == dstSize;
}

u32 HackInflate_RawSize(const u8* src)
{
	return src[0] == INF_MAGIC ? (u32) (src[1] | (src[2] << 8) | (src[3] << 16)) : 0;
}
