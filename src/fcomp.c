#include "defines.h"
#include "fcomp.h"

/*
 * 自前の圧縮「F 方式」の展開の中核（2026-09-28。もとは src/bw_follower_fcomp.c にあったものを DPE の戦闘の絵と共用に）
 * 圧縮は tools/bwfollow/fcomp.py。docs/research/2026-09-28_follower-fcomp.md / 2026-09-28_dpe-sprite-fcomp.md
 *
 * ビット列 = LZ77 ＋ ハフマン（Deflate と同じ長さ・距離の区切り。距離は共通辞書のぶん 65536 まで）。32 ビットずつ・下位ビットから
 * 窓は「共通辞書（ROM）＋ いま展開した分」。距離が展開した分より遠ければ辞書の後ろから読む
 *
 * ★ 書き換え可能な static は使えない（CLAUDE.md 6.13）。状態はすべて関数の中の変数
 */

static const u16 sLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const u8 sLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const u16 sDistBase[32] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073,
								  4097, 6145, 8193, 12289, 16385, 24577, 32769, 49153};
static const u8 sDistExtra[32] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14};

#define FC_END 256
#define FC_BAD 0xFFFF

//★ 速く引く表（1 ビットずつ調べると大きい絵で約 80 万命令かかった）:
//  先の BITS ビット（読む順 = 下位ビットから）で引く。値 = 記号 | (符号の長さ << 9)。0 なら BITS より長い符号 → 遅い道
#define FC_LIT_FAST_BITS 10		//tools/bwfollow/build_bw_follower.py の FAST_BITS と同じ
#define FC_DIST_FAST_BITS 8

//遅い道（BITS より長い符号）: 1 ビットずつ（正準ハフマン符号・長さごとの最初の符号と数）。→ 記号 | (長さ << 9)、無ければ FC_BAD
//★ 2026-10-06 展開の中核は ARM 命令で作り、IWRAM へ写して動かす（下の FCompInflate）。ARM の中核からほかの関数を呼ばない（写すと BL の飛び先がずれる）
#define FC_ARM __attribute__((target("arm"), optimize("no-tree-loop-distribute-patterns")))
static inline __attribute__((always_inline)) FC_ARM u32 FCompSlowSym(u32 bits, const struct BwFTable *t, const u16 *symbols)
{
	u32 len, code = 0;

	for (len = 1; len <= 15; ++len)
	{
		code = (code << 1) | ((bits >> (len - 1)) & 1);
		if (code - t->first[len] < t->count[len])	//符号なしの比べ方（code < first なら大きな数になる）
			return symbols[t->index[len] + code - t->first[len]] | (len << 9);
	}
	return FC_BAD;
}

//★ ビットの状態（buf・n・p）は関数の中の変数（レジスタ）に置き、記号を引く所はループの中に直接書く
//  （関数を呼んで状態をメモリに通すと 1 記号 約 49 命令かかった・エミュレータでの測定）
//buf を 17 ビット以上にする（下の FC_REFILL）。末尾の先読みは 0 だが、FC_DROP は宣言されたビット数を超える消費を拒否する。
//2026-10-06 2 バイトずつ（v2 は 1 バイトに 2 回読み足すので 1 バイトずつだと重かった）。読み足したあと 17 ビット以上（どの記号も 15 ビットまで）。
//  p はビット列の頭（4 の倍数）から 2 ずつ進むので u16 で読める
#define FC_REFILL() 	if (n <= 16) { if (p < end) { buf |= (u32) *(const u16 *) p << n; p += 2; } n += 16; }
#define FC_DROP(k) do { u32 drop_ = (k); if (drop_ > remainingBits) return FALSE; remainingBits -= drop_; buf >>= drop_; n -= drop_; } while (0)

//★ v2（2026-10-06・ユーザー「F方式 v2 を作る」）: 使うタイルの所（out の頭 使うタイル × 32 バイト）の 1 バイト（2 画素）は、
//  タイルの中の左・上の画素（無ければ 16）で選んだ表で引く。1 件 = 長さ 1〜15 の数（15 バイト）・空き 1・記号（長さ・番号の順）。
//  T1 の記号 0〜15 = 1 つ目の画素（下位 4 ビット）・16〜44 = 一致の長さの区切り。T2 = 2 つ目の画素（左 = 1 つ目の画素）。
//  正準ハフマン符号を 1 ビットずつ（平均 3 ビットほどで短い）。buf は FC_REFILL のあと 17 ビット以上ある
#define FC_T1_REC 96	//頭 64 バイト（数・記号）＋ 速く引く表 u16 × 16（記号 | 長さ << 8・0 は遅い道）
#define FC_T2_REC 48	//頭 32 バイト ＋ 速く引く表 u8 × 16（記号 | 長さ << 4）
#define FC_CTX_SYM(rec, sym) do { \
	const u8 *cnt_ = (rec); \
	u32 code_ = buf & 1, first_ = 0, idx_ = 0, len_ = 1, c_; \
	for (;;) \
	{ \
		c_ = cnt_[len_ - 1]; \
		if (code_ - first_ < c_) \
		{ \
			(sym) = cnt_[16 + idx_ + code_ - first_]; \
			break; \
		} \
		if (len_ == 15) \
			return FALSE; \
		idx_ += c_; \
		first_ = (first_ + c_) << 1; \
		code_ = (code_ << 1) | ((buf >> len_) & 1); \
		++len_; \
	} \
	FC_DROP(len_); \
} while (0)

//★ この関数は IWRAM に写して動かす（ROM の Thumb より約 3 倍速い・推測）。BL・memcpy を出さない（tools/bwfollow/verify_fcomp_thumb.py が確かめる）
__attribute__((noinline, used)) FC_ARM u32 FCompInflateArm(const struct FCompSet *set, const u8 *data, u8 *out, u32 outSize)
{
	const u8 *p = data + 8;
	const u8 *end = p + FCOMP_NUM_WORDS(data) * 4;	//頭の語の数より先は読まない
	const u16 *litFast = set->litFast;
	const u16 *distFast = set->distFast;
	u32 buf = 0, n = 0, pos = 0, i;
	u32 pixEnd = 0;
	u32 remainingBits = FCOMP_NUM_WORDS(data) * 32;

	if (FCOMP_VERSION(data) == 1)
	{
		if (set->pixT1 == NULL || set->pixT2 == NULL)
			return FALSE;
		pixEnd = FCOMP_NUM_UNIQ(data) * 32;
		if (pixEnd > outSize)
			return FALSE;
	}
	else if (FCOMP_VERSION(data) != 0)
		return FALSE;

	for (;;)
	{
		u32 e, sym;

		FC_REFILL();
		if (pos < pixEnd)
		{
			u32 off = pos & 31;
			u32 left = (off & 3) ? (out[pos - 1] >> 4) : 16;
			u32 up = (off >= 4) ? (out[pos - 4] & 15) : 16;
			u32 lo, hi;

			const u8 *rec = set->pixT1 + (left * 17 + up) * FC_T1_REC;
			u32 f = ((const u16 *) (rec + FC_T1_REC - 32))[buf & 15];	//速く引く表（先の 4 ビット）

			if (f != 0)
			{
				lo = f & 0xFF;
				FC_DROP(f >> 8);
			}
			else
				FC_CTX_SYM(rec, lo);
			if (lo < 16)
			{
				FC_REFILL();	//T1 で 15 ビットまで使うと残りが T2 の 15 ビットに足りないことがある
				up = (off >= 4) ? (out[pos - 4] >> 4) : 16;
				rec = set->pixT2 + (lo * 17 + up) * FC_T2_REC;
				f = rec[FC_T2_REC - 16 + (buf & 15)];
				if (f != 0)
				{
					hi = f & 15;
					FC_DROP(f >> 4);
				}
				else
					FC_CTX_SYM(rec, hi);
				if (hi >= 16)
					return FALSE;
				out[pos++] = lo | (hi << 4);
				continue;
			}
			sym = 257 + lo - 16;	//一致の長さの区切り（下の v1 と同じ道へ）
			if (sym > 285)
				return FALSE;
		}
		else
		{
			e = litFast[buf & ((1 << FC_LIT_FAST_BITS) - 1)];
			if (e == 0 && (e = FCompSlowSym(buf, set->litTable, set->litSymbols)) == FC_BAD)
				return FALSE;
			FC_DROP(e >> 9);
			sym = e & 0x1FF;
		}

		if (sym < 256)
		{
			if (pos >= outSize)
				return FALSE;
			out[pos++] = sym;
		}
		else if (sym == FC_END)
			break;
		else if (sym <= 285)
		{
			u32 lc = sym - 257;
			u32 len = sLenBase[lc];
			u32 dist, dc, x;

			x = sLenExtra[lc];
			if (x != 0)
			{
				FC_REFILL();
				len += buf & ((1 << x) - 1);
				FC_DROP(x);
			}
			FC_REFILL();
			e = distFast[buf & ((1 << FC_DIST_FAST_BITS) - 1)];
			if (e == 0 && (e = FCompSlowSym(buf, set->distTable, set->distSymbols)) == FC_BAD)
				return FALSE;
			FC_DROP(e >> 9);
			dc = e & 0x1FF;
			if (dc >= 32)
				return FALSE;
			dist = sDistBase[dc];
			x = sDistExtra[dc];
			if (x != 0)
			{
				FC_REFILL();
				dist += buf & ((1 << x) - 1);
				FC_DROP(x);
			}
			if (pos + len > outSize || dist > pos + set->dictSize)
				return FALSE;
			//辞書から読むぶん（距離が展開した分より遠い）と、展開した分から読むぶんに分けて写す（1 バイトごとに判定しない）
			if (dist > pos)
			{
				const u8 *src = set->dict + set->dictSize - (dist - pos);
				u32 fromDict = dist - pos;

				if (fromDict > len)
					fromDict = len;
				len -= fromDict;
				for (i = 0; i < fromDict; ++i)
					out[pos++] = src[i];
			}
			if (len != 0)
			{
				u8 *dst = out + pos;
				const u8 *src = dst - dist;

				pos += len;
				do
				{
					*dst++ = *src++;
				} while (--len != 0);
			}
		}
		else
			return FALSE;
	}
	return pos == outSize;
}

/*
 * 2026-10-06 ユーザー「1からお願い」（展開を速く・docs/research/2026-10-06_rom-compression-review.md）:
 * 中核（FCompInflateArm・ARM 命令）を IWRAM の空き 0x03003720〜0x03003DC0 に写してから呼ぶ。
 * ここは素の InitIntrHandlers（0800068C）が割り込みの処理 intr_main（0x15C バイト）を 2KB まとめて写す入れ物（0x030035C0〜）の後ろで、
 * 中身は使われない写しの残り（素の ROM と Pokemon.gba で、ここを指す ldr は InitIntrHandlers の 1 か所だけ）。
 * リセットのたびに intr_main の写しで上書きされるので、呼ぶたびに写し直す（1,696 バイト・CpuFastSet で 数千サイクル）。
 * 中核が FC_IWRAM_SIZE を超えたら写さずに ROM のまま呼ぶ（遅いが動く・verify_fcomp_thumb.py が大きさを見る）
 */
#define FC_IWRAM 0x03003720
#define FC_IWRAM_SIZE 0x6A0

//ARM の踏み台（ROM・ARM）: IWRAM の中核を呼んで、Thumb の呼び元へ BX で戻る。
//★ ARMv4T の ARM は pop {pc} で Thumb に戻れない（中核は pop {…, pc} で戻る）ので、中核の戻り先はこの踏み台（ARM）にする
__attribute__((naked, target("arm"))) static void FCompCallIwram(void)
{
	asm volatile(
		"stmfd sp!, {lr}\n"
		"ldr ip, 1f\n"
		"mov lr, pc\n"
		"bx ip\n"
		"ldmfd sp!, {lr}\n"
		"bx lr\n"
		"1: .word 0x03003720\n");
}

bool8 FCompInflate(const struct FCompSet *set, const u8 *data, u8 *out, u32 outSize)
{
	//volatile: 直接 BL させない（Thumb の BL では ARM に切り替わらない・ポインタなら BX で呼ぶ）
	u32 (* volatile fn)(const struct FCompSet *, const u8 *, u8 *, u32) = (void *) FCompCallIwram;

	CpuFastSet((const void *) FCompInflateArm, (void *) FC_IWRAM, FC_IWRAM_SIZE / 4);
	return fn(set, data, out, outSize) != 0;
}
