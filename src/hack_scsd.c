#include "defines.h"
#include "../include/malloc.h"
#include "../include/new/Vanilla_functions.h"

/*
 * SuperCard SD から、ゲームの途中で SD カードのファイルを読む（2026-10-07・docs/research/2026-10-07_rom-64mb.md）。
 * ユーザー「ROM側で仮想的に32MBとして読める仕組み」「実機でやってみたい」「SuperCard」「SD」「すすめて」。
 *
 *   - SD を触る中核（ARM・tools/supercard/core/scsd_core.c → assembly/hack_scsd/scsd_core.s の gHackScsdCore）を
 *     呼ぶたびに IWRAM 0x03003720 へ写して呼ぶ（F 方式の展開の中核と同じ置き場・src/fcomp.c・交互にしか使わない）。
 *     中核の中で割り込みを止め、SuperCard のモードを 3（SD の窓）にして、終わったら 1（ゲームと同じ）に戻す。
 *     SD の窓が出ている間は 0x09000000〜の ROM が見えないので、ROM のこのファイルの命令は中核の外でだけ動く。
 *   - SuperCard でない（mGBA・ほかのカートリッジ）ときは、中核が「SD の窓を開く前と後でコマンド線の番地が同じ」で見分けてすぐ戻る。
 *   - ファイルの仕組みは FAT16・FAT32・exFAT（根のフォルダの決まった名前・実機で確かめたのは FAT16 だけ）。
 *   - 状態は持たない（6.13）: 呼ぶたびに Alloc した入れ物で、見分け → RCA の取り直し → 区分 → ファイルを探す。
 *
 * いまは試し（Moémon は使う許可が取れたかわからないので保留・#55）: SD の SDTEST.BIN（tools/supercard/make_sdtest_pic.py）の絵を、
 * 戦闘などのポケモンの絵（正面・後ろ姿）の代わりに出す（HackSdPic_LoadPic・DPE の LoadSpecialPokePic から
 * DPE の src/hack_sd_bridge.c 経由）。色はそのポケモンのまま。SDTEST.BIN を消せば元の絵。
 * SDTEST.BIN が読めるかは、最初に絵を読もうとしたときに 1 回だけ見分けて 0x0203E041 に置く（0xA5 読める・0x5A 読めない・ほかはまだ）。
 * （初めはタイトルの「PUSH START BUTTON」が出るところで見分けていたが、その前にボタンを押すと見分けが走らず、実機で試しの絵が出なかった）
 * 0x0203E041 は CFRU の include/new/ram_locs.h が「FREE」と書いていた 1 バイト（BPRJ.ld に割り当て無し）。
 */

#define SCSD_IWRAM 0x03003720
#define SDPIC_FILE_83   "SDTEST  BIN"
#define SDPIC_FILE_LONG "SDTEST.BIN"
#define SDPIC_STATUS (*(vu8 *) 0x0203E041)   //0xA5 = SDTEST.BIN が読める（タイトルで見分けた）・ほか = 読めない
#define SDPIC_STATUS_OK 0xA5
#define SDPIC_STATUS_NG 0x5A

extern const u8 gHackScsdCore[];
extern const u32 gHackScsdCoreSize;

//tools/supercard/core/scsd_core.c の struct ScsdIo と同じ並び
struct ScsdIo
{
	u32 op;        //0 見分け＋RCA の取り直し・1 読む
	u32 lba;
	u32 n;
	u8 *out;
	u32 rca;
	u32 sdhc;
};

enum
{
	SCSD_OK = 0,
	SCSD_NOT_SUPERCARD = 1,
};

struct ScsdFs
{
	struct ScsdIo io;
	u32 err;
	u32 type;       //16 FAT16・32 FAT32・0xEF exFAT
	u32 part;
	u32 fat;
	u32 data;
	u32 rootLba;    //FAT16 の根のフォルダ
	u32 rootSecs;
	u32 spcShift;
	u32 rootClus;
	u32 fatCached;
	u32 fileClus;
	u32 fileSize;
	u32 fileNoChain;
	const char *name83;
	const char *longName;
	u8 buf[512];
	u8 fatBuf[512];
};

//ARM の踏み台（ROM・ARM）: IWRAM の中核を呼んで、Thumb の呼び元へ BX で戻る（src/fcomp.c の FCompCallIwram と同じ形）
__attribute__((naked, target("arm"))) static void HackScsd_CallIwram(void)
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

static u32 HackScsd_Run(struct ScsdIo *io)
{
	u32 (* volatile fn)(struct ScsdIo *) = (void *) HackScsd_CallIwram;

	CpuFastSet(gHackScsdCore, (void *) SCSD_IWRAM, gHackScsdCoreSize / 4);
	return fn(io);
}

static bool8 HackScsd_Read(struct ScsdFs *fs, u32 lba, u32 n, u8 *out)
{
	if (fs->err != SCSD_OK)
		return FALSE;
	fs->io.op = 1;
	fs->io.lba = lba;
	fs->io.n = n;
	fs->io.out = out;
	fs->err = HackScsd_Run(&fs->io);
	return fs->err == SCSD_OK;
}

static u32 Rd16(const u8 *p) { return p[0] | (p[1] << 8); }
static u32 Rd32(const u8 *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32) p[3] << 24); }
static u32 Log2(u32 v) { u32 s = 0; while (v > 1) { v >>= 1; s++; } return s; }

static bool8 HackScsd_Mount(struct ScsdFs *fs)
{
	u8 *b = fs->buf;
	u32 part = 0;
	bool8 isVbr;

	if (!HackScsd_Read(fs, 0, 1, b))
		return FALSE;
	isVbr = (b[3] == 'E' && b[4] == 'X' && b[5] == 'F' && b[6] == 'A' && b[7] == 'T')
	     || ((b[0] == 0xEB || b[0] == 0xE9) && Rd16(b + 0x0B) == 512 && b[0x0D] != 0 && (b[0x0D] & (b[0x0D] - 1)) == 0);
	if (!isVbr) //MBR: 1 つ目の区分
	{
		part = Rd32(b + 0x1C6);
		if (!HackScsd_Read(fs, part, 1, b))
			return FALSE;
	}
	fs->part = part;
	fs->fatCached = 0xFFFFFFFF;
	if (b[3] == 'E' && b[4] == 'X' && b[5] == 'F' && b[6] == 'A' && b[7] == 'T')
	{
		/* exFAT clusters are at most 32 MiB: 2^16 sectors at 512 bytes. */
		if (b[0x6C] != 9 || b[0x6D] > 16
		 || Rd32(b + 0x50) > 0xFFFFFFFFu - part
		 || Rd32(b + 0x58) > 0xFFFFFFFFu - part
		 || Rd32(b + 0x60) < 2)
			return FALSE;
		fs->type = 0xEF;
		fs->fat = part + Rd32(b + 0x50);
		fs->data = part + Rd32(b + 0x58);
		fs->spcShift = b[0x6D];
		fs->rootClus = Rd32(b + 0x60);
		return TRUE;
	}
	if (Rd16(b + 0x0B) != 512 || b[0x0D] == 0
	 || (b[0x0D] & (b[0x0D] - 1)) != 0
	 || b[0x10] == 0 || Rd16(b + 0x0E) == 0
	 || Rd16(b + 0x0E) > 0xFFFFFFFFu - part)
		return FALSE;
	if (Rd16(b + 0x16) != 0) //FAT16
	{
		fs->type = 16;
		fs->fat = part + Rd16(b + 0x0E);
		if (Rd16(b + 0x16) > (0xFFFFFFFFu - fs->fat) / b[0x10])
			return FALSE;
		fs->rootLba = fs->fat + b[0x10] * Rd16(b + 0x16);
		fs->rootSecs = (Rd16(b + 0x11) * 32 + 511) / 512;
		if (fs->rootSecs == 0 || fs->rootSecs > 0xFFFFFFFFu - fs->rootLba)
			return FALSE;
		fs->data = fs->rootLba + fs->rootSecs;
		fs->spcShift = Log2(b[0x0D]);
		return TRUE;
	}
	if (Rd32(b + 0x24) != 0) //FAT32
	{
		fs->type = 32;
		fs->fat = part + Rd16(b + 0x0E);
		if (Rd32(b + 0x24) > (0xFFFFFFFFu - fs->fat) / b[0x10]
		 || Rd32(b + 0x2C) < 2)
			return FALSE;
		fs->data = fs->fat + b[0x10] * Rd32(b + 0x24);
		fs->spcShift = Log2(b[0x0D]);
		fs->rootClus = Rd32(b + 0x2C);
		return TRUE;
	}
	return FALSE;
}

static u32 ClusLba(struct ScsdFs *fs, u32 c) { return fs->data + ((c - 2) << fs->spcShift); }

static bool8 EndClus(struct ScsdFs *fs, u32 c)
{
	return c < 2 || c >= (fs->type == 16 ? 0xFFF8u : fs->type == 32 ? 0x0FFFFFF8u : 0xFFFFFFF7u);
}

static u32 NextClus(struct ScsdFs *fs, u32 c)
{
	u32 perSec = fs->type == 16 ? 256 : 128;
	u32 lba = fs->fat + c / perSec;

	if (fs->fatCached != lba)
	{
		if (!HackScsd_Read(fs, lba, 1, fs->fatBuf))
			return 0xFFFFFFFF;
		fs->fatCached = lba;
	}
	if (fs->type == 16)
		return Rd16(fs->fatBuf + (c % perSec) * 2);
	return fs->type == 32 ? (Rd32(fs->fatBuf + (c % perSec) * 4) & 0x0FFFFFFF) : Rd32(fs->fatBuf + (c % perSec) * 4);
}

//FAT16・FAT32 の 1 区画ぶん（8.3 の名前）。1 見つけた・-1 終わり・0 続き
static s32 ScanFat(struct ScsdFs *fs)
{
	u32 e, i;

	for (e = 0; e < 512; e += 32)
	{
		const u8 *d = fs->buf + e;
		bool8 ok = TRUE;
		if (d[0] == 0)
			return -1;
		if (d[0] == 0xE5 || d[11] == 0x0F || (d[11] & 0x18))
			continue;
		for (i = 0; i < 11; i++)
			if (d[i] != (u8) fs->name83[i])
				ok = FALSE;
		if (ok)
		{
			fs->fileClus = (fs->type == 32 ? (Rd16(d + 0x14) << 16) : 0) | Rd16(d + 0x1A);
			fs->fileSize = Rd32(d + 0x1C);
			fs->fileNoChain = FALSE;
			return 1;
		}
	}
	return 0;
}

static u32 Upper(u32 c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

static bool8 HackScsd_FindFile(struct ScsdFs *fs)
{
	const char *name = fs->longName;
	u32 clus, s, guard;
	u32 pend = 0, nameLen = 0, namePos = 0, pendClus = 0, pendSize = 0, pendNoChain = 0;
	bool8 match = FALSE;

	if (fs->type == 16)
	{
		for (s = 0; s < fs->rootSecs; s++)
		{
			s32 r;
			if (!HackScsd_Read(fs, fs->rootLba + s, 1, fs->buf))
				return FALSE;
			r = ScanFat(fs);
			if (r != 0)
				return r > 0;
		}
		return FALSE;
	}

	clus = fs->rootClus;
	for (guard = 0; guard < 64 && !EndClus(fs, clus); guard++)
	{
		for (s = 0; s < (1u << fs->spcShift); s++)
		{
			u32 e;
			if (!HackScsd_Read(fs, ClusLba(fs, clus) + s, 1, fs->buf))
				return FALSE;
			if (fs->type == 32)
			{
				s32 r = ScanFat(fs);
				if (r != 0)
					return r > 0;
				continue;
			}
			for (e = 0; e < 512; e += 32) //exFAT
			{
				const u8 *d = fs->buf + e;
				if (d[0] == 0)
					return FALSE;
				if (d[0] == 0x85)
				{
					pend = d[1];
					match = FALSE;
				}
				else if (d[0] == 0xC0 && pend)
				{
					nameLen = d[3];
					namePos = 0;
					match = TRUE;
					pendNoChain = (d[1] >> 1) & 1;
					pendClus = Rd32(d + 0x14);
					pendSize = Rd32(d + 0x18);
					pend--;
				}
				else if (d[0] == 0xC1 && pend)
				{
					u32 i;
					for (i = 0; i < 15 && namePos < nameLen; i++, namePos++)
						if (name[namePos] == 0 || Upper(Rd16(d + 2 + i * 2)) != Upper((u8) name[namePos]))
							match = FALSE;
					pend--;
					if (pend == 0 && match && namePos == nameLen && name[nameLen] == 0)
					{
						fs->fileClus = pendClus;
						fs->fileSize = pendSize;
						fs->fileNoChain = pendNoChain;
						return TRUE;
					}
				}
				else
					pend = 0;
			}
		}
		clus = NextClus(fs, clus);
	}
	return FALSE;
}

//ファイルの n 区画目の LBA（FAT をたどる・0 なら失敗）
static u32 FileLba(struct ScsdFs *fs, u32 n)
{
	u32 per = 1u << fs->spcShift;
	u32 c = fs->fileClus;
	u32 k;

	if (fs->fileNoChain)
		return ClusLba(fs, c) + n;
	for (k = n / per; k != 0; k--)
	{
		c = NextClus(fs, c);
		if (EndClus(fs, c))
			return 0;
	}
	return ClusLba(fs, c) + (n % per);
}

//SD のファイルを開く（Alloc した入れ物を返す・使い終わったら Free）。*result: 0 SuperCard でない・1 途中で失敗・2 ファイルが無い・3 あった
static struct ScsdFs *HackScsd_Open(const char *name83, const char *longName, u32 *result)
{
	struct ScsdFs *fs = AllocZeroed(sizeof(struct ScsdFs));

	*result = 1;
	if (fs == NULL)
		return NULL;
	fs->name83 = name83;
	fs->longName = longName;
	fs->io.op = 0;
	fs->err = HackScsd_Run(&fs->io);
	if (fs->err == SCSD_NOT_SUPERCARD)
		*result = 0;
	else if (fs->err != SCSD_OK || !HackScsd_Mount(fs))
		*result = 1;
	else if (!HackScsd_FindFile(fs))
		*result = fs->err == SCSD_OK ? 2 : 1;
	else
		*result = 3;
	return fs;
}

//ファイルの sector 区画目から n 区画を out へ（区切りをまたぐときは分けて読む）
static bool8 HackScsd_ReadOpened(struct ScsdFs *fs, u32 sector, u32 n, u8 *out)
{
	u32 sectors = fs->fileSize / 512;
	u32 per;

	if (sector > sectors || n > sectors - sector
	 || fs->spcShift >= 32 || (n != 0 && out == NULL))
		return FALSE;
	per = 1u << fs->spcShift;
	while (n != 0)
	{
		u32 run = per - sector % per;
		u32 lba = FileLba(fs, sector);
		if (run > n || fs->fileNoChain)
			run = n;
		if (lba == 0 || !HackScsd_Read(fs, lba, run, out))
			return FALSE;
		sector += run;
		n -= run;
		out += run * 512;
	}
	return TRUE;
}

//SD の根のフォルダのファイルの sector 区画目から n 区画を読む（SuperCard でない・無い・失敗なら FALSE）
bool8 HackScsd_ReadFile(const char *name83, const char *longName, u32 sector, u32 n, void *out)
{
	u32 result;
	bool8 ok = FALSE;
	struct ScsdFs *fs = HackScsd_Open(name83, longName, &result);

	if (fs == NULL)
		return FALSE;
	if (result == 3)
		ok = HackScsd_ReadOpened(fs, sector, n, out);
	Free(fs);
	return ok;
}

//SDTEST.BIN を見分けて SDPIC_STATUS に置く（見出し "PKSSDPIC" を確かめる）
static void HackScsd_ProbeSdPic(void)
{
	u32 result;
	struct ScsdFs *fs = HackScsd_Open(SDPIC_FILE_83, SDPIC_FILE_LONG, &result);

	if (fs != NULL)
	{
		if (result == 3 && (!HackScsd_ReadOpened(fs, 0, 1, fs->buf)
		 || fs->buf[0] != 'P' || fs->buf[1] != 'K' || fs->buf[2] != 'S' || fs->buf[3] != 'S'
		 || fs->buf[4] != 'D' || fs->buf[5] != 'P' || fs->buf[6] != 'I' || fs->buf[7] != 'C'))
			result = 2;
		Free(fs);
	}
	SDPIC_STATUS = (fs != NULL && result == 3) ? SDPIC_STATUS_OK : SDPIC_STATUS_NG;
}

//試しの絵: SDTEST.BIN の区画 1〜4 が正面・5〜8 が後ろ姿（4bpp 64×64・2048 バイト）。読めたら TRUE
bool8 HackSdPic_LoadPic(void *dest, u16 species, u32 personality, bool8 isFrontPic)
{
	(void) species;
	(void) personality;
	if (SDPIC_STATUS != SDPIC_STATUS_OK && SDPIC_STATUS != SDPIC_STATUS_NG)
		HackScsd_ProbeSdPic();     //まだ見分けていない（電源を入れて最初の 1 回）
	if (SDPIC_STATUS != SDPIC_STATUS_OK)
		return FALSE;
	if (!HackScsd_ReadFile(SDPIC_FILE_83, SDPIC_FILE_LONG, isFrontPic ? 1 : 5, 4, dest))
	{
		SDPIC_STATUS = SDPIC_STATUS_NG;   //読めなかったら、そのあとは元の絵
		return FALSE;
	}
	return TRUE;
}
