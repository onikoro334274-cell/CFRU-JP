#pragma once

/*
 * 自前の圧縮「F 方式」の展開の中核（2026-09-28）。連れ歩き（src/bw_follower_fcomp.c）と DPE の戦闘の絵（src/fcomp_dpe.c）が使う。
 * 形式は tools/bwfollow/fcomp.py（同じ手順の Python の展開器 decode() で検算している）。
 */

struct BwFTable
{
	u16 first[16];		//長さ n の最初の符号
	u16 count[16];		//長さ n の符号の数
	u16 index[16];		//長さ n の最初の記号が symbols の何番目か
};

//1 組の辞書とハフマンの表（連れ歩きと DPE の絵で別々。生成物の .s にある）
struct FCompSet
{
	const u8 *dict;
	u32 dictSize;
	const struct BwFTable *litTable;
	const u16 *litSymbols;
	const u16 *litFast;		//先の 10 ビットで引く表
	const struct BwFTable *distTable;
	const u16 *distSymbols;
	const u16 *distFast;	//先の 8 ビットで引く表
	const u8 *pixT1;		//v2（頭の 6 バイト目が 1）の文脈の表: 1 件 64 バイト × 289（tools/bwfollow/fcomp.py の pix_table_lines）
	const u8 *pixT2;		//1 件 32 バイト × 289
};

//F のデータの頭（8 バイト）: u16 タイルの数・u16 使うタイルの数・u8 番号の幅・u8 0・u16 ビット列の語の数
#define FCOMP_NUM_TILES(d) ((d)[0] | ((d)[1] << 8))
#define FCOMP_NUM_UNIQ(d) ((d)[2] | ((d)[3] << 8))
#define FCOMP_WIDTH(d) ((d)[4])
#define FCOMP_NUM_WORDS(d) ((d)[6] | ((d)[7] << 8))
#define FCOMP_VERSION(d) ((d)[5])		//0 = v1・1 = v2（タイルの画素を左・上の画素の文脈の表で・2026-10-06）

//data（頭 8 バイトつき）のビット列を out に展開する。ちょうど outSize バイトになれば TRUE
bool8 FCompInflate(const struct FCompSet *set, const u8 *data, u8 *out, u32 outSize);
