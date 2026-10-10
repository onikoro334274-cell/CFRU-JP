#include "defines.h"
#include "../include/save.h"
#include "../include/pokemon_storage_system.h"
#include "../include/new/pokemon_storage_system.h"
#include "../include/string_util.h"
#include "../include/sound.h"
#include "../include/text.h"
#include "../include/text_window.h"
#include "../include/window.h"
#include "hack_box_shelf.h"
#include "hack_ext_save.h"

/*
 * ボックスの仮想化（棚・付け替え方式・2026-10-06 ユーザー「本番お願い」「F=5・39箱」）。
 * 設計: docs/research/2026-10-06_box-virtualization-design.md
 *
 * フラッシュ（4KB × 32 区画）:
 *   0〜4 / 5〜9   本体の組 A / B（src/save.c の NUM_SECTORS_PER_SAVE_SLOT 5＝SaveBlock2・SaveBlock1・parasite 1/2）
 *   10〜31        棚（物理 22）。論理ページ 17（0〜16）・予備 5
 *   ★ もとの CFRU の区画 30・31（RAM 0x0203BFAC〜0x1FE0・バッグ・ずかんナビ・20〜22 箱など）は棚のページ 15・16 に入れた
 *     （CFRU は本体を書いたあとに二重保存なしで上書きしていて、その間の電源断で混ざった・ユーザー「Aでお願い」）
 *   ★ 殿堂入りの記録（もと 28・29）はもう書かない
 * 論理ページ（0xFF0 = 4,080 バイト）:
 *   ページ 0 = 今の箱の番号・箱の名前と壁紙・parasite 3（もとは組の id 13 の空きに置いていた 0xBA0）・ポケモン 12 匹
 *   ページ 1〜14 = ポケモン 70 匹ずつ。棚の箱（1〜19 箱・26〜39 箱 = 33 箱 = 990 匹）は、ポケモンの帯（ページ 0 の 12 匹 → ページ 1 から 70 匹ずつ）に続けて並ぶ
 * 対応表（論理 → 物理）は SaveBlock1 + 0x1400（questLog の跡の空き・本体の二重保存の中）。
 * レポート: 汚れたページ（＋ページ 0）だけを、今の対応表で使っていない物理区画へ書いてから本体の組を書く（本体を書き終えたら切り替わり）。
 *
 * RAM: 素の PokemonStorage（0x0202924C〜）を使い直す
 *   +0         今の箱の番号（素と同じ）
 *   +4         箱の入れ物 19 個（1,740 バイトずつ・1 箱の 30 匹が続けて並ぶ＝ステータス画面などがポインタを足し算する）
 *   +0x8128    15〜25 箱の壁紙（CFRU のまま）
 *   +0x8134    26〜39 箱の名前（9 バイトずつ）・+0x81B2 26〜39 箱の壁紙
 *   +0x81C0    入れ物の管理（struct ShelfCache）
 *   +0x82E1    15〜25 箱の名前・+0x8344 1〜14 箱の名前・+0x83C2 1〜14 箱の壁紙（素・CFRU のまま）
 * 20〜25 箱（id 19〜24）は今までどおり CFRU の置き場（棚ではない）。
 *
 * 40〜50 箱（id 39〜49・2026-10-09 ユーザー「ボックス50に拡張できる？」「今のmGBAとセーブデータ拡張で行けない？」）:
 *   直した mGBA の追加のセーブ領域（src/hack_ext_save.c）だけ。ポケモンの帯の 990 匹目から（棚の箱の番号 33〜43）、
 *   拡張のページ 0〜4（論理ページ 17〜21・70 匹ずつ）・ページ 4 の EXT_META_OFF から 40〜50 箱の名前と壁紙。
 *   物理は追加の区画 8〜23（16）。対応表はページ 0 の後ろ（P0_EXT・印 'X' ＋ 5 バイト）＝本体 → ページ 0 → 拡張とたどるので、
 *   付け替え方式の強さは棚と同じ。追加の領域が使えない版（32MB・実機・SuperCard）では 40〜50 箱は触れない（中身は空に見え、
 *   対応表はそのまま残す＝直した mGBA に戻せばまた見える）。
 *   RAM: +0x8260 40〜50 箱の名前（9 バイトずつ）・+0x82C3 40〜50 箱の壁紙
 */

#define STORAGE_BASE 0x0202924C
#define SHELF_PHYS_FIRST 10
#define SHELF_PHYS_COUNT 22
#define SHELF_PAGES 17
#define BOX_PAGES 15                //ページ 0〜14 がポケモンの帯（0 は名前なども）
#define REGION_RAM 0x0203BFAC       //もとの CFRU の区画 30・31 の中身（SAVE_BLOCK_PARASITE + PARASITE_SIZE）→ ページ 15・16
#define SHELF_PAGE_SIZE 0xFF0
#define SHELF_FOOTER_ID 0x40
#define SHELF_SIGNATURE 0x08012025
#define SHELF_MAP_MAGIC 0x4C454853     // 'SHEL'
#define SHELF_CACHE_MAGIC 0x45484353   // 'SCHE'
#define SHELF_FRAMES 19
#define MON_SIZE sizeof(struct CompressedPokemon)   // 58
#define BOX_BYTES (IN_BOX_COUNT * MON_SIZE)         // 1,740
#define PAGE0_MONS 12
#define PAGE_MONS 70
#define NO_PAGE 0xFF
#define NO_BOX 0xFF

//40〜50 箱（追加のセーブ領域）
#define EXT_BOX_FIRST 39                        //箱の id
#define EXT_BOXES 11
#define EXT_SHELF_FIRST 33                      //棚の箱の番号
#define EXT_G_FIRST (EXT_SHELF_FIRST * IN_BOX_COUNT)   //990
#define EXT_PAGES 5
#define EXT_PAGE_BASE SHELF_PAGES               //論理ページ 17〜21
#define EXT_PAGE_LAST (EXT_PAGE_BASE + EXT_PAGES - 1)
#define ALL_PAGES (SHELF_PAGES + EXT_PAGES)
#define EXT_PHYS_FIRST 8                        //追加の区画 8〜23（0 は src/hack_save_slot.c・1 と 735 は HackExtSave_DebugTest が書く）
#define EXT_PHYS_COUNT 16
#define EXT_META_OFF ((EXT_BOXES * IN_BOX_COUNT - (EXT_PAGES - 1) * PAGE_MONS) * MON_SIZE)   //ページ 4 の 50 匹の後ろ
#define EXT_META_SIZE (EXT_BOXES * 9 + EXT_BOXES)
#define EXT_NAMES_RAM ((u8*) STORAGE_BASE + 0x8260)
#define EXT_WALLS_RAM ((u8*) STORAGE_BASE + 0x82C3)
#define EXT_MAP_MAGIC 0x58                      //'X'
#define IS_EXT_PAGE(p) ((p) >= EXT_PAGE_BASE)

//ページ 0 の並び
#define P0_HEAD 0                       //+0〜4（今の箱の番号）
#define P0_TAIL_A 4                     //+0x8128〜+0x81C0（152）
#define P0_TAIL_B (4 + 0x98)            //+0x82E1〜+0x83D0（239）
#define P0_PARASITE (P0_TAIL_B + 0xEF)  //parasite 3（0xBA0）
#define P0_MONS (P0_PARASITE + 0xBA0)   //12 匹（〜4,067）
#define P0_EXT (P0_MONS + PAGE0_MONS * MON_SIZE)   //40〜50 箱の対応表（印 1 ＋ 5・〜4,073）
#define PARASITE3_RAM (0x0203B0E8 + 0xCC + 0x258)
#define PARASITE3_SIZE 0xBA0

struct ShelfMap        //SaveBlock1 + 0x1400（本体の組に入る）
{
	u32 magic;
	u32 seq;
	u8 phys[SHELF_PAGES];
};

struct ShelfCache      //PokemonStorage + 0x81C0（保存しない・0x121 バイトまで＝+0x82E1 から先は箱の名前）
{
	u32 magic;
	u32 sum16;                   //ページ 16（20〜22 箱）の RAM の和（最後に書き終えたレポートのとき）→ 変わったかの目安
	u32 pendingSum16;
	u8 frameBox[SHELF_FRAMES];
	u8 frameDirty[SHELF_FRAMES];
	u8 frameStamp[SHELF_FRAMES];
	u8 stamp;
	u8 cursor;
	u8 committed[ALL_PAGES];     //最後に書き終えたレポートの対応表（17〜21 は追加の区画の番号）
	u8 written[ALL_PAGES];       //このレポートで書いたページ
	u8 late[SHELF_PAGES];        //予備が足りず、本体のあとに今の区画へ上書きするページ（起きないはず・M2）
	u8 overBudget;
	u8 extPending[EXT_PAGES];    //このレポートの 40〜50 箱の対応表（ページ 0 に書く）
};

typedef char ShelfCacheFits[(sizeof(struct ShelfCache) <= 0x8260 - 0x81C0) ? 1 : -1];   //+0x8260 から 40〜50 箱の名前
typedef char ExtMetaFits[(EXT_META_OFF + EXT_META_SIZE <= SHELF_PAGE_SIZE && P0_EXT + 1 + EXT_PAGES <= SHELF_PAGE_SIZE) ? 1 : -1];

#define SHELF_MAP ((struct ShelfMap*) ((u8*) gSaveBlock1 + 0x1400))
#define REGION16_RAM ((const u32*) (REGION_RAM + SHELF_PAGE_SIZE))
#define SHELF_CACHE ((struct ShelfCache*) (STORAGE_BASE + 0x81C0))
#define FRAME_PTR(f) ((struct CompressedPokemon*) (STORAGE_BASE + 4 + (f) * BOX_BYTES))

extern struct SaveSection gSaveDataBuffer;
#include "hack_save_crypt.h"
#define ReadFlash HackSaveCrypt_ReadFlash   //★ 2026-10-09 かき混ぜを戻して読む（src/hack_save_crypt.c）・もとは素の ReadFlash 081C2A54
#ifdef HACK_BOX_PROTO
u8 BoxShelf_TryWriteSector(u8 sector, u8* data);
#define SHELF_WRITE BoxShelf_TryWriteSector
#else
#define SHELF_WRITE TryWriteSector
#endif

static void CopyBytes(u8* dst, const u8* src, u32 n)
{
	while (n-- != 0)
		*dst++ = *src++;
}

static void ZeroBytes(u8* dst, u32 n)
{
	while (n-- != 0)
		*dst++ = 0;
}

//棚の箱の番号（0〜32）・棚でなければ -1
s32 BoxShelf_ShelfIndex(u8 boxId)
{
	if (boxId < 19)
		return boxId;
	if (boxId >= 25 && boxId < EXT_BOX_FIRST)
		return boxId - 6;
	if (boxId >= EXT_BOX_FIRST && boxId < EXT_BOX_FIRST + EXT_BOXES)
		return EXT_SHELF_FIRST + boxId - EXT_BOX_FIRST;
	return -1;
}

//ポケモンの帯の g 番目 → (ページ, ページの中のバイト)
static void MonLocation(u32 g, u32* page, u32* off)
{
	if (g >= EXT_G_FIRST)
	{
		g -= EXT_G_FIRST;
		*page = EXT_PAGE_BASE + g / PAGE_MONS;
		*off = (g % PAGE_MONS) * MON_SIZE;
	}
	else if (g < PAGE0_MONS)
	{
		*page = 0;
		*off = P0_MONS + g * MON_SIZE;
	}
	else
	{
		g -= PAGE0_MONS;
		*page = 1 + g / PAGE_MONS;
		*off = (g % PAGE_MONS) * MON_SIZE;
	}
}

//ページ p の off から n バイト（追加の区画のページは使えるときだけ・使えなければ 0）
static void ReadPage(u32 p, u32 off, u8* dst, u32 n)
{
	u8 phys = SHELF_CACHE->committed[p];

	if (phys == NO_PAGE || (IS_EXT_PAGE(p) && !HackExtSave_Available()))
		ZeroBytes(dst, n);
	else if (IS_EXT_PAGE(p))
		HackExtSave_Read(phys, off, dst, n);
	else
		ReadFlash(phys, off, dst, n);
}

static void ReadMonsFromFlash(u32 g, u32 count, u8* dst)
{
	while (count != 0)
	{
		u32 page, off, n;
		MonLocation(g, &page, &off);
		n = ((page == 0) ? P0_MONS + PAGE0_MONS * MON_SIZE - off : PAGE_MONS * MON_SIZE - off) / MON_SIZE;
		if (n > count)
			n = count;
		ReadPage(page, off, dst, n * MON_SIZE);
		dst += n * MON_SIZE;
		g += n;
		count -= n;
	}
}

static s32 FindFrame(u8 shelf)
{
	struct ShelfCache* c = SHELF_CACHE;
	u32 f;
	for (f = 0; f < SHELF_FRAMES; ++f)
	{
		if (c->frameBox[f] == shelf)
			return f;
	}
	return -1;
}

//入れ物に読み込む（無ければ）→ 入れ物の番号
static u32 LoadFrame(u8 shelf)
{
	struct ShelfCache* c = SHELF_CACHE;
	s32 f = FindFrame(shelf);
	s32 pin = BoxShelf_ShelfIndex(StorageGetCurrentBox());
	u32 i;

	if (f < 0)
	{
		s32 best = -1;
		for (i = 0; i < SHELF_FRAMES; ++i)
		{
			if (c->frameBox[i] == NO_BOX)
			{
				best = i;
				break;
			}
			if (c->frameDirty[i] || c->frameBox[i] == pin)
				continue;
			if (best < 0 || (u8) (c->stamp - c->frameStamp[i]) > (u8) (c->stamp - c->frameStamp[best]))
				best = i;
		}
		if (best < 0) //全部汚れている（上限を守れば起きない・M2）
		{
			c->overBudget = TRUE;
			best = 0;
		}
		f = best;
		c->frameBox[f] = shelf;
		c->frameDirty[f] = FALSE;
		ReadMonsFromFlash(shelf * IN_BOX_COUNT, IN_BOX_COUNT, (u8*) FRAME_PTR(f));
	}
	c->frameStamp[f] = ++c->stamp;
	return f;
}

//箱の 30 匹の先頭（続けて並ぶ）。forWrite なら汚れの印
struct CompressedPokemon* BoxShelf_BoxMons(u8 boxId, bool8 forWrite)
{
	s32 shelf = BoxShelf_ShelfIndex(boxId);
	u32 f;

	if (shelf < 0)
		return NULL;
	f = LoadFrame(shelf);
	if (forWrite && (shelf < EXT_SHELF_FIRST || HackExtSave_Available()))   //40〜50 箱は追加の領域が無ければ書けない（汚れを残さない）
		SHELF_CACHE->frameDirty[f] = TRUE;
	return FRAME_PTR(f);
}

//★ 2026-10-09 #70: 入れ物にもう読み込んである箱だけ（読み込まない・入れ物を動かさない）。無ければ NULL・棚でない箱も NULL
struct CompressedPokemon* BoxShelf_PeekBoxMons(u8 boxId)
{
	s32 shelf = BoxShelf_ShelfIndex(boxId);
	s32 f = (shelf >= 0) ? FindFrame(shelf) : -1;
	return (f >= 0) ? FRAME_PTR(f) : NULL;
}

//1 匹を読む（入れ物に無ければフラッシュから直に・入れ物を動かさない）
void BoxShelf_ReadMon(u8 boxId, u8 pos, struct CompressedPokemon* out)
{
	s32 shelf = BoxShelf_ShelfIndex(boxId);
	s32 f;

	if (shelf < 0)
		return;
	f = FindFrame(shelf);
	if (f >= 0)
		CopyBytes((u8*) out, (u8*) &FRAME_PTR(f)[pos], MON_SIZE);
	else
		ReadMonsFromFlash(shelf * IN_BOX_COUNT + pos, 1, (u8*) out);
}

//入れ物の中を直に書いたとき（ステータス画面のわざの並べ替え）
void BoxShelf_MarkPtrDirty(const void* ptr)
{
	u32 a = (u32) ptr;
	u32 f;

	if (a < STORAGE_BASE + 4 || a >= STORAGE_BASE + 4 + SHELF_FRAMES * BOX_BYTES)
		return;
	f = (a - STORAGE_BASE - 4) / BOX_BYTES;
	if (SHELF_CACHE->frameBox[f] != NO_BOX)
		SHELF_CACHE->frameDirty[f] = TRUE;
}

static void ResetCache(void)
{
	struct ShelfCache* c = SHELF_CACHE;
	u32 i;

	ZeroBytes((u8*) c, sizeof(*c));
	c->magic = SHELF_CACHE_MAGIC;
	for (i = 0; i < SHELF_FRAMES; ++i)
		c->frameBox[i] = NO_BOX;
	for (i = 0; i < ALL_PAGES; ++i)
		c->committed[i] = NO_PAGE;
	for (i = 0; i < EXT_PAGES; ++i)
		c->extPending[i] = NO_PAGE;
}

//はじめから（src/save.c の NewGameWipeNewSaveData）: 棚は空（対応表なし）
void BoxShelf_NewGame(void)
{
	struct ShelfMap* m = SHELF_MAP;
	u32 i;

	ResetCache();
	m->magic = SHELF_MAP_MAGIC;
	m->seq = 0;
	for (i = 0; i < SHELF_PAGES; ++i)
		m->phys[i] = NO_PAGE;
}

//26〜39 箱の名前が空（終わりの印 0xFF が無い）なら「ボックスNN」・壁紙は素と同じ（番号 % 4）。
//はじめからは素の ResetPokemonStorageSystem（CFRU が箱の数を書き換え）が全部の箱に付けるので、要るのは引っ越したセーブだけ。
//★ 名前に終わりの印が無いと、ボックスの画面で名前を描くときに後ろまで読んで、画面が操作を受け付けなくなった（2026-10-06 mGBA）
#define gText_Box ((const u8*) 0x083DF249)
static void FixNewBoxNames(void)
{
	u32 i, k;
	for (i = 0; i < 14 + EXT_BOXES; ++i)   //26〜39 箱・40〜50 箱
	{
		u8* name = (i < 14) ? (u8*) STORAGE_BASE + 0x8134 + 9 * i : EXT_NAMES_RAM + 9 * (i - 14);
		u8* wall = (i < 14) ? (u8*) STORAGE_BASE + 0x81B2 + i : EXT_WALLS_RAM + (i - 14);
		bool8 ok = FALSE;
		for (k = 0; k < 9; ++k)
		{
			if (name[k] == 0xFF)
				ok = TRUE;
		}
		if (!ok)
		{
			u8* end = StringCopy(name, gText_Box);
			ConvertIntToDecimalStringN(end, 26 + i, STR_CONV_MODE_LEFT_ALIGN, 2);
		}
		if (*wall >= 18)   //★ 2026-10-09 壁紙は 18 番まで（17 = もも・src/hack_box_wallpaper.c）
			*wall = (25 + i) % 4;
	}
}

static u32 Region16Sum(void)
{
	const u32* w = REGION16_RAM;
	u32 i, s = 0;
	for (i = 0; i < SHELF_PAGE_SIZE / 4; ++i)
		s += w[i] * (i + 1); //並べ替えにも気づく重み付きの和
	return s;
}

static bool8 PageValid(u8 phys, u32 page)
{
	struct SaveSection* s = &gSaveDataBuffer;

	if (IS_EXT_PAGE(page))
	{
		if (!HackExtSave_ReadSector(phys, s))
			return FALSE;
	}
	else
		DoReadFlashWholeSection(phys, s);
	return s->id == SHELF_FOOTER_ID + page && s->signature == SHELF_SIGNATURE
		&& s->checksum == CalculateSaveChecksum(s->data, SHELF_PAGE_SIZE);
}

//続きから（src/save.c の HandleLoadSector・本体の組を読んだあと）
void BoxShelf_OnLoad(void)
{
	struct ShelfMap* m = SHELF_MAP;
	struct ShelfCache* c;
	u32 i;

	if (m->magic != SHELF_MAP_MAGIC)
	{
		BoxShelf_NewGame(); //棚を使う前のセーブ（引っ越していない）: 棚は空
		return;
	}
	ResetCache();
	c = SHELF_CACHE;
	for (i = 0; i < SHELF_PAGES; ++i)
	{
		u8 p = m->phys[i];
		if (p != NO_PAGE && (p < SHELF_PHYS_FIRST || p >= SHELF_PHYS_FIRST + SHELF_PHYS_COUNT || !PageValid(p, i)))
			p = NO_PAGE; //壊れたページ（起きないはず）: 空として扱う
		c->committed[i] = m->phys[i] = p;
	}

	if (c->committed[0] != NO_PAGE) //ページ 0 → 今の箱の番号・名前と壁紙・parasite 3
	{
		u8* d = gSaveDataBuffer.data;
		DoReadFlashWholeSection(c->committed[0], &gSaveDataBuffer);
		CopyBytes((u8*) STORAGE_BASE, d + P0_HEAD, 4);
		CopyBytes((u8*) STORAGE_BASE + 0x8128, d + P0_TAIL_A, 0x98);
		CopyBytes((u8*) STORAGE_BASE + 0x82E1, d + P0_TAIL_B, 0xEF);
		CopyBytes((u8*) PARASITE3_RAM, d + P0_PARASITE, PARASITE3_SIZE);
		if (d[P0_EXT] == EXT_MAP_MAGIC) //40〜50 箱の対応表（使えない版では確かめずにそのまま残す）
		{
			for (i = 0; i < EXT_PAGES; ++i)
				c->committed[EXT_PAGE_BASE + i] = d[P0_EXT + 1 + i];
		}
	}
	ZeroBytes(EXT_NAMES_RAM, EXT_META_SIZE);
	if (HackExtSave_Available())
	{
		for (i = EXT_PAGE_BASE; i < ALL_PAGES; ++i)
		{
			u8 p = c->committed[i];
			if (p != NO_PAGE && (p < EXT_PHYS_FIRST || p >= EXT_PHYS_FIRST + EXT_PHYS_COUNT || !PageValid(p, i)))
				c->committed[i] = NO_PAGE; //壊れたページ: 空として扱う
		}
		ReadPage(EXT_PAGE_LAST, EXT_META_OFF, EXT_NAMES_RAM, EXT_META_SIZE);
	}
	for (i = BOX_PAGES; i < SHELF_PAGES; ++i) //ページ 15・16 → もとの区画 30・31 の RAM
	{
		if (c->committed[i] != NO_PAGE)
			ReadFlash(c->committed[i], 0, (u8*) REGION_RAM + (i - BOX_PAGES) * SHELF_PAGE_SIZE, SHELF_PAGE_SIZE);
		else
			ZeroBytes((u8*) REGION_RAM + (i - BOX_PAGES) * SHELF_PAGE_SIZE, SHELF_PAGE_SIZE); //書いたことのないページ = 0
	}
	c->sum16 = Region16Sum();
	FixNewBoxNames();
}

static void PageRange(u32 p, u32* gStart, u32* gEnd)
{
	if (IS_EXT_PAGE(p))
	{
		*gStart = EXT_G_FIRST + (p - EXT_PAGE_BASE) * PAGE_MONS;
		*gEnd = *gStart + PAGE_MONS;
	}
	else if (p == 0)
	{
		*gStart = 0;
		*gEnd = PAGE0_MONS;
	}
	else
	{
		*gStart = PAGE0_MONS + (p - 1) * PAGE_MONS;
		*gEnd = *gStart + PAGE_MONS;
	}
}

//ページ p を gSaveDataBuffer に組み立てる（今のフラッシュ ＋ 入れ物の箱 ＋ ページ 0 の中身）
static void BuildPage(u32 p)
{
	struct ShelfCache* c = SHELF_CACHE;
	u8* d = gSaveDataBuffer.data;
	u32 f, gStart, gEnd;

	Memset(&gSaveDataBuffer, 0, sizeof(gSaveDataBuffer));
	if (p >= BOX_PAGES && !IS_EXT_PAGE(p)) //ページ 15・16 = もとの区画 30・31 の RAM をそのまま（17〜21 は 40〜50 箱）
	{
		CopyBytes(d, (u8*) REGION_RAM + (p - BOX_PAGES) * SHELF_PAGE_SIZE, SHELF_PAGE_SIZE);
		goto FOOTER;
	}
	ReadPage(p, 0, d, SHELF_PAGE_SIZE);
	PageRange(p, &gStart, &gEnd);
	for (f = 0; f < SHELF_FRAMES; ++f)
	{
		u32 b = c->frameBox[f], g, page, off;
		if (b == NO_BOX)
			continue;
		for (g = b * IN_BOX_COUNT; g < (b + 1) * IN_BOX_COUNT; ++g)
		{
			if (g < gStart || g >= gEnd)
				continue;
			MonLocation(g, &page, &off);
			CopyBytes(d + off, (u8*) &FRAME_PTR(f)[g - b * IN_BOX_COUNT], MON_SIZE);
		}
	}
	if (p == 0)
	{
		CopyBytes(d + P0_HEAD, (u8*) STORAGE_BASE, 4);
		CopyBytes(d + P0_TAIL_A, (u8*) STORAGE_BASE + 0x8128, 0x98);
		CopyBytes(d + P0_TAIL_B, (u8*) STORAGE_BASE + 0x82E1, 0xEF);
		CopyBytes(d + P0_PARASITE, (u8*) PARASITE3_RAM, PARASITE3_SIZE);
		d[P0_EXT] = EXT_MAP_MAGIC;
		CopyBytes(d + P0_EXT + 1, c->extPending, EXT_PAGES);
	}
	if (p == EXT_PAGE_LAST)
		CopyBytes(d + EXT_META_OFF, EXT_NAMES_RAM, EXT_META_SIZE);
FOOTER:
	gSaveDataBuffer.id = SHELF_FOOTER_ID + p;
	gSaveDataBuffer.signature = SHELF_SIGNATURE;
	gSaveDataBuffer.counter = gSaveCounter;
	gSaveDataBuffer.checksum = CalculateSaveChecksum(d, SHELF_PAGE_SIZE);
}

static bool8 PageDirty(u32 p)
{
	struct ShelfCache* c = SHELF_CACHE;
	u32 f, gStart, gEnd;

	if (IS_EXT_PAGE(p))
	{
		if (!HackExtSave_Available())
			return FALSE;
		if (p == EXT_PAGE_LAST)
			return TRUE; //40〜50 箱の名前と壁紙（同じなら書かない）
	}
	else if (p == 0 || p >= BOX_PAGES)
		return TRUE; //ページ 0（変数）・15・16（バッグなど）: 中身をフラッシュと比べるのは BoxShelf_Commit（同じなら書かない）
	PageRange(p, &gStart, &gEnd);
	for (f = 0; f < SHELF_FRAMES; ++f)
	{
		u32 b = c->frameBox[f];
		if (b != NO_BOX && c->frameDirty[f] && b * IN_BOX_COUNT < gEnd && (b + 1) * IN_BOX_COUNT > gStart)
			return TRUE;
	}
	return FALSE;
}

//汚れたページの数（ページ 0 を入れる）
u32 BoxShelf_DirtyPageCount(void)
{
	u32 p, n = 0;
	for (p = 0; p < SHELF_PAGES; ++p)
		n += PageDirty(p);
	return n;
}

//ページ p を書こうとしている中身が、今のフラッシュと同じか（同じなら書かない＝予備を使わない）
static bool8 SameAsFlash(u32 p, const u8* data)
{
	u8 tmp[64];
	u32 off, i;

	if (SHELF_CACHE->committed[p] == NO_PAGE) //書いたことのないページ（読むと 0）: 0 だけなら書かない
	{
		for (i = 0; i < SHELF_PAGE_SIZE; ++i)
		{
			if (data[i] != 0)
				return FALSE;
		}
		return TRUE;
	}
	for (off = 0; off < SHELF_PAGE_SIZE; off += sizeof(tmp))
	{
		u32 n = (SHELF_PAGE_SIZE - off < sizeof(tmp)) ? SHELF_PAGE_SIZE - off : sizeof(tmp);
		ReadPage(p, off, tmp, n);
		for (i = 0; i < n; ++i)
		{
			if (tmp[i] != data[off + i])
				return FALSE;
		}
	}
	return TRUE;
}

static s32 TakeFreePhys(void)
{
	struct ShelfCache* c = SHELF_CACHE;
	struct ShelfMap* m = SHELF_MAP;
	u32 k, i;

	for (k = 0; k < SHELF_PHYS_COUNT; ++k)
	{
		u8 phys = SHELF_PHYS_FIRST + (c->cursor + k) % SHELF_PHYS_COUNT;
		bool8 used = FALSE;
		for (i = 0; i < SHELF_PAGES; ++i)
		{
			if (c->committed[i] == phys || m->phys[i] == phys)
				used = TRUE;
		}
		if (!used)
		{
			c->cursor = (c->cursor + k + 1) % SHELF_PHYS_COUNT;
			return phys;
		}
	}
	return -1;
}

static s32 TakeFreeExt(void)
{
	struct ShelfCache* c = SHELF_CACHE;
	u32 k, i;

	for (k = 0; k < EXT_PHYS_COUNT; ++k)
	{
		u8 phys = EXT_PHYS_FIRST + (c->stamp + k) % EXT_PHYS_COUNT;
		bool8 used = FALSE;
		for (i = 0; i < EXT_PAGES; ++i)
		{
			if (c->committed[EXT_PAGE_BASE + i] == phys || c->extPending[i] == phys)
				used = TRUE;
		}
		if (!used)
			return phys;
	}
	return -1;
}

//レポート（src/save.c の SaveWriteToFlash・gSaveCounter を進めたあと・本体の組を書く前）
void BoxShelf_Commit(void)
{
	struct ShelfCache* c = SHELF_CACHE;
	struct ShelfMap* m = SHELF_MAP;
	u32 p;

	if (c->magic != SHELF_CACHE_MAGIC)
		BoxShelf_OnLoad();
	else if (m->magic != SHELF_MAP_MAGIC)
	{
		//★ 2026-10-09 ユーザー「ボックスにあずけるとレポート書いて再開とするとボックスが消失する」: はじめからは
		//  BoxShelf_NewGame（NewGameInitData の頭のフック）のあとに素の ClearSav1 が SaveBlock1 を 0 で消し、対応表の印も消える。
		//  そこで OnLoad → NewGame を呼ぶと入れ物（預けた箱）まで捨てるので、入れ物は残して印だけ付け直す（対応表は下で committed から）
		m->magic = SHELF_MAP_MAGIC;
		m->seq = 0;
	}
	c->overBudget = FALSE;
	for (p = 0; p < SHELF_PAGES; ++p)
	{
		m->phys[p] = c->committed[p];
		c->late[p] = FALSE;
	}
	for (p = 0; p < ALL_PAGES; ++p)
		c->written[p] = FALSE;
	for (p = 0; p < EXT_PAGES; ++p)
		c->extPending[p] = c->committed[EXT_PAGE_BASE + p];
	c->pendingSum16 = Region16Sum();
	for (p = EXT_PAGE_BASE; p < ALL_PAGES; ++p) //40〜50 箱（ページ 0 に対応表を書くので先に）
	{
		s32 phys;
		if (!PageDirty(p))
			continue;
		BuildPage(p);
		if (SameAsFlash(p, gSaveDataBuffer.data))
		{
			c->written[p] = TRUE;
			continue;
		}
		phys = TakeFreeExt();
		if (phys < 0 || !HackExtSave_WriteSector(phys, gSaveDataBuffer.data))
		{
			c->overBudget = TRUE; //書けなかった: 前の中身のまま（箱の汚れは残る）
			continue;
		}
		c->extPending[p - EXT_PAGE_BASE] = phys;
		c->written[p] = TRUE;
	}
	for (p = 0; p < SHELF_PAGES; ++p)
	{
		s32 phys;
		if (!PageDirty(p))
			continue;
		BuildPage(p);
		if (SameAsFlash(p, gSaveDataBuffer.data))
		{
			c->written[p] = TRUE; //中身が今のフラッシュと同じ（書かない・書いたのと同じ＝箱の汚れを取ってよい）
			continue;
		}
		phys = TakeFreePhys();
		if (phys < 0) //予備が足りない（ボックスの画面で止めるので、起きるのはレポートのあとで 15・16 が変わったときくらい）
		{
			c->overBudget = TRUE;
			if (c->committed[p] != NO_PAGE)
				c->late[p] = TRUE; //本体を書き終えたあとに今の区画へ上書き（CFRU のもとの区画 30・31 と同じ弱さ・失いはしない）
			continue;
		}
		SHELF_WRITE(phys, gSaveDataBuffer.data);
		m->phys[p] = phys;
		c->written[p] = TRUE;
	}
	m->seq = gSaveCounter;
}

//レポートのあと（src/save.c の SaveWriteToFlash の最後）
void BoxShelf_AfterSave(bool8 ok)
{
	struct ShelfCache* c = SHELF_CACHE;
	struct ShelfMap* m = SHELF_MAP;
	u32 p, f;

	if (!ok)
	{
		for (p = 0; p < SHELF_PAGES; ++p)
			m->phys[p] = c->committed[p];
		for (p = 0; p < EXT_PAGES; ++p)
			c->extPending[p] = c->committed[EXT_PAGE_BASE + p];
		return;
	}
	for (p = 0; p < SHELF_PAGES; ++p)
		c->committed[p] = m->phys[p];
	for (p = 0; p < EXT_PAGES; ++p)
		c->committed[EXT_PAGE_BASE + p] = c->extPending[p];
	for (p = 0; p < SHELF_PAGES; ++p) //予備が足りなかったページ: 今の区画へ上書き
	{
		if (c->late[p])
		{
			BuildPage(p);
			if (SHELF_WRITE(c->committed[p], gSaveDataBuffer.data) == SAVE_STATUS_OK)
				c->written[p] = TRUE;
			c->late[p] = FALSE;
		}
	}
	if (c->written[BOX_PAGES + 1])
		c->sum16 = c->pendingSum16;
	for (f = 0; f < SHELF_FRAMES; ++f) //書いたページだけでできている箱は汚れが取れる
	{
		u32 b = c->frameBox[f], g, page, off;
		bool8 all = TRUE;
		if (b == NO_BOX || !c->frameDirty[f])
			continue;
		for (g = b * IN_BOX_COUNT; g < (b + 1) * IN_BOX_COUNT; ++g)
		{
			MonLocation(g, &page, &off);
			if (!c->written[page])
				all = FALSE;
		}
		if (all)
			c->frameDirty[f] = FALSE;
	}
}

/* ---- 1 回のレポートで書けるページの数を守る（M2・2026-10-06 ユーザー「レポートを書いてくださいと出して止める」） ----------
 * 書けるのは予備（どのページも指していない物理区画）の数まで（ふつう 22 − 17 = 5）。
 * 数えるページ: 0（いつも）・15（バッグ・ずかんナビ＝ほぼ毎回変わるので、いつも数える）・汚れた箱のページ 1〜14・16（20〜22 箱が変わったら）。
 * 箱を変える操作の前に「その箱のページを足しても予備に収まるか」を見て、収まらなければ止める（ボックスの画面）／その箱を使わない（捕まえたとき）。 */

static u32 FreePhysCount(void)
{
	struct ShelfCache* c = SHELF_CACHE;
	u32 i, used = 0;
	for (i = 0; i < SHELF_PAGES; ++i)
		used += (c->committed[i] != NO_PAGE);
	return SHELF_PHYS_COUNT - used;
}

static u32 PagesToWrite(void)
{
	u32 p, mask = (1 << 0) | (1 << BOX_PAGES);
	for (p = 1; p < BOX_PAGES; ++p)
	{
		if (PageDirty(p))
			mask |= 1 << p;
	}
	if (Region16Sum() != SHELF_CACHE->sum16)
		mask |= 1 << (BOX_PAGES + 1);
	return mask;
}

static u32 BoxPageMask(u8 boxId)
{
	s32 shelf = BoxShelf_ShelfIndex(boxId);
	u32 p0, p1, off, mask = 0;

	if (shelf >= 0)
	{
		MonLocation(shelf * IN_BOX_COUNT, &p0, &off);
		MonLocation(shelf * IN_BOX_COUNT + IN_BOX_COUNT - 1, &p1, &off);
		for (; p0 <= p1; ++p0)
			mask |= 1 << p0;
		return mask;
	}
	if (boxId == 19) //20 箱（0x0203CABC〜）: ページ 15 の後ろと 16 の頭
		return (1 << BOX_PAGES) | (1 << (BOX_PAGES + 1));
	if (boxId == 20 || boxId == 21) //21・22 箱: ページ 16
		return 1 << (BOX_PAGES + 1);
	return 0; //23〜25 箱は本体の組（SaveBlock1・2）の中
}

static u32 CountBits(u32 x)
{
	u32 n = 0;
	for (; x != 0; x &= x - 1)
		++n;
	return n;
}

//この箱を変えても、次のレポートで書くページが予備に収まるか
bool8 BoxShelf_CanTouchBox(u8 boxId)
{
	u32 used, box;

	if (boxId >= EXT_BOX_FIRST)
		return HackExtSave_Available(); //40〜50 箱: 追加の区画は 16・ページは 5（予備はいつも足りる）
	if (SHELF_CACHE->magic != SHELF_CACHE_MAGIC)
		return TRUE;
	used = PagesToWrite();
	box = BoxPageMask(boxId);
	if ((used | box) == used)
		return TRUE; //もう書くことになっているページだけ（増えない）
	return CountBits(used | box) <= FreePhysCount();
}

/* ボックスの画面（assembly/hack_box_shelf/box_shelf_hooks.s から） */
#define sCursorArea (*(s8*) 0x0203976C)            //0 = 箱の中・1 = 手持ち（素の CanShiftMon 08093370 が読む）
#define sIsMonBeingMoved (*(u8*) 0x0203976E)
#define gStorageState (**(u8**) 0x020396FC)       //gStorage->state
#define HandleInput ((u8 (*)(void)) (0x080942E8 | 1))
#define TryStorePartyMonInBox ((bool8 (*)(u8)) (0x08092B18 | 1))
#define CURSOR_AREA_IN_BOX 0
#define SE_FAILURE 26
extern const u8 gText_BoxShelf_SaveFirst[];
extern const u8 gText_BoxShelf_ExtOnly[];

enum //素の HandleInput の答え（pokefirered の pokemon_storage_system_internal.h）
{
	INPUT_NONE,
	INPUT_IN_MENU = 8,
	INPUT_DEPOSIT = 11,
	INPUT_WITHDRAW,
	INPUT_MOVE_MON,
	INPUT_SHIFT_MON,
	INPUT_PLACE_MON,
	INPUT_TAKE_ITEM,
	INPUT_GIVE_ITEM,
	INPUT_SWITCH_ITEMS,
	INPUT_PRESSED_B,
	INPUT_MULTIMOVE_START,
	INPUT_MULTIMOVE_CHANGE_SELECTION,
	INPUT_MULTIMOVE_SINGLE,
	INPUT_MULTIMOVE_GRAB_SELECTION,
	INPUT_MULTIMOVE_UNABLE,
	INPUT_MULTIMOVE_MOVE_MONS,
	INPUT_MULTIMOVE_PLACE_MONS,
};

//素の PrintStorageMessage（0808F5E0）と同じ窓・同じ書き方で、自前の文を出す
static void PrintShelfMessage(u8 boxId)
{
	PlaySE(SE_FAILURE);
	FillWindowPixelBuffer(1, 0x11);
	AddTextPrinterParameterized(1, 1, (boxId >= EXT_BOX_FIRST) ? gText_BoxShelf_ExtOnly : gText_BoxShelf_SaveFirst, 0, 2, 0xFF, NULL);
	DrawTextBorderOuter(1, 2, 13);
	PutWindowTilemap(1);
	CopyWindowToVram(1, 2);
	ScheduleBgCopyTilemapToVram(0);
}

//Task_PokeStorageMain の state 0。今の箱の中身を変える入力（メニューを開くのも＝にがす・もちもの・マークなど）を、予備に収まらなければ止める
u8 BoxShelf_PssHandleInput(void)
{
	u8 input = HandleInput();

	switch (input)
	{
	case INPUT_IN_MENU:
	case INPUT_WITHDRAW:
	case INPUT_MOVE_MON:
	case INPUT_SHIFT_MON:
	case INPUT_PLACE_MON:
	case INPUT_TAKE_ITEM:
	case INPUT_GIVE_ITEM:
	case INPUT_SWITCH_ITEMS:
	case INPUT_MULTIMOVE_START:
	case INPUT_MULTIMOVE_SINGLE:
	case INPUT_MULTIMOVE_GRAB_SELECTION:
	case INPUT_MULTIMOVE_PLACE_MONS:
		if (sCursorArea == CURSOR_AREA_IN_BOX && !BoxShelf_CanTouchBox(StorageGetCurrentBox()))
		{
			PrintShelfMessage(StorageGetCurrentBox());
			gStorageState = 3; //素の state 3: ボタンで窓を消して state 0 へ
			return INPUT_NONE;
		}
		break;
	}
	return input;
}

//Task_DepositMenu（あずける箱を選んだ）: 0 = いっぱい・1 = あずけた（素と同じ）・2 = 止めた
u8 BoxShelf_DepositTry(u8 boxId)
{
	if (!BoxShelf_CanTouchBox(boxId))
	{
		PrintShelfMessage(boxId);
		gStorageState = 4; //素の state 4: ボタンで「どの ボックスに？」へ戻る
		return 2;
	}
	return TryStorePartyMonInBox(boxId);
}

//素の IsPokemonStorageFull（08040694）の置き換え（hooks）: 予備に収まる箱だけで空きを探す（SendMonToPC と同じ）
bool8 BoxShelf_IsPokemonStorageFull(void)
{
	u32 box, pos;
	for (box = 0; box < TOTAL_BOXES_COUNT; ++box)
	{
		if (!BoxShelf_CanTouchBox(box))
			continue;
		for (pos = 0; pos < IN_BOX_COUNT; ++pos)
		{
			if (GetBoxMonDataAt(box, pos, MON_DATA_SPECIES) == SPECIES_NONE)
				return FALSE;
		}
	}
	return TRUE;
}

#ifdef HACK_BOX_PROTO
/* ---- 試験のビルドだけ（tools/boxvirt/shelf_test.py）: 電源断のまね ---------------------------------------- */
struct ShelfDebug
{
	u32 crashAfter;
	u32 writes;
	u32 crashed;
	u32 done;
};
#define SHELF_DBG ((struct ShelfDebug*) 0x0203FE00)

u8 BoxShelf_TryWriteSector(u8 sector, u8* data)
{
	struct ShelfDebug* dbg = SHELF_DBG;

	dbg->writes++;
	if (dbg->crashed)
		return 1;
	if (dbg->crashAfter != 0 && dbg->writes == dbg->crashAfter)
	{
		Memset(data, 0xFF, sizeof(struct SaveSection));
		TryWriteSector(sector, data);
		dbg->crashed = 1;
		return 1;
	}
	return TryWriteSector(sector, data);
}

//試験: 箱から箱へ 2 匹動かす（1→2・5→39・それぞれの先頭）
void BoxShelfTest_Move(void)
{
	static const u8 moves[2][2] = {{0, 1}, {4, 38}}; //汚れるページ 0・1・2・14（予備 5 の中）
	u32 i;
	for (i = 0; i < 2; ++i)
	{
		struct BoxPokemon mon = *GetBoxedMonPtr(moves[i][0], 0);
		SetBoxMonAt(moves[i][1], 0, &mon);
		ZeroBoxMonAt(moves[i][0], 0);
	}
}

//試験: 全部の箱の中身の指紋（箱 × 30 匹の 性格値 ^ 種族）を 0x0201C000（gDecompressionBuffer）へ
void BoxShelfTest_Snapshot(void)
{
	u32* out = (u32*) 0x0201C000;
	u32 b, p;
	for (b = 0; b < TOTAL_BOXES_COUNT; ++b)
		for (p = 0; p < IN_BOX_COUNT; ++p)
			*out++ = GetBoxMonDataAt(b, p, MON_DATA_PERSONALITY) ^ (GetBoxMonDataAt(b, p, MON_DATA_SPECIES) << 16)
				^ (GetBoxMonDataAt(b, p, MON_DATA_MARKINGS) << 28);
}

//試験（M2）: 0x0203FE20 の箱（[0] = 数・[1..] = 箱の id）の 1 匹目のマークを変えて汚し、
//全部の箱の「触れるか」を 0x0203FE40（39 バイト）・1 匹目の種族を 0x0203FE80（u16 × 39）・書くページの印を 0x0203FED0 へ
void BoxShelfTest_Budget(void)
{
	const u8* req = (const u8*) 0x0203FE20;
	u32 i;
	for (i = 0; i < req[0]; ++i)
	{
		u8 mark = GetBoxMonDataAt(req[1 + i], 0, MON_DATA_MARKINGS) ^ 1;
		SetBoxMonDataAt(req[1 + i], 0, MON_DATA_MARKINGS, &mark);
	}
	for (i = 0; i < TOTAL_BOXES_COUNT; ++i)
	{
		((u8*) 0x0203FE40)[i] = BoxShelf_CanTouchBox(i);
		((u16*) 0x0203FE80)[i] = GetBoxMonDataAt(i, 0, MON_DATA_SPECIES);
	}
	*(u32*) 0x0203FED0 = PagesToWrite();
	*(u32*) 0x0203FED4 = FreePhysCount();
}

//試験（M2）: 手持ちの 1 匹目の写しを SendMonToPC へ（送る先の探し始めは 0x0203FE20 の箱）→ 0x0203FEE0 に 結果・箱・位置・IsPokemonStorageFull
void BoxShelfTest_SendToPC(void)
{
	struct Pokemon mon = gPlayerParty[0];
	u8* out = (u8*) 0x0203FEE0;
	VarSet(VAR_PC_BOX_TO_SEND_MON, *(u8*) 0x0203FE20);
	out[3] = IsPokemonStorageFull();
	out[0] = SendMonToPC(&mon);
	out[1] = gSpecialVar_MonBoxId;
	out[2] = gSpecialVar_MonBoxPos;
}

//試験（M3）: はじめからの消去（src/save.c の NewGameWipeNewSaveData）と素の ResetPokemonStorageSystem（0808C21C・CFRU が箱の数を 39 に）
void NewGameWipeNewSaveData(void);
void BoxShelfTest_NewGame(void)
{
	NewGameWipeNewSaveData();
	((void (*)(void)) (0x0808C21C | 1))();
}

//試験（M3）: 1・25・26・39 箱の名前（9 バイトずつ）を 0x0203FF00 へ
void BoxShelfTest_Names(void)
{
	static const u8 boxes[4] = {0, 24, 25, 38};
	u32 i;
	for (i = 0; i < 4; ++i)
		CopyBytes((u8*) 0x0203FF00 + 9 * i, GetBoxNamePtr(boxes[i]), 9);
}

void BoxShelfTest_Save(void)
{
	SHELF_DBG->writes = 0;
	SHELF_DBG->crashed = 0;
	SHELF_DBG->done = 0;
	((void (*)(void)) (0x08058994 | 1))(); //SaveMapView（メニューのレポートと同じ準備）
	TrySavingData(SAVE_NORMAL);
	SHELF_DBG->done = 1;
}
#endif

/* ---- 通信のあとの「少しずつのレポート」（素の LinkFullSave_*・ユニオンルームなど） ---------------------------------
 * 素は区画を 1 つずつ書き換える（組を 14 区画と決め打ち・gFirstSaveSector を 14 で回す）→ 棚の区画を壊す。
 * hooks（patches/0196）で入口を置き換え、入口で棚も込みの普通のレポートをし、あとの手順は「済んだ」と返す */
bool8 HackLinkSave_Init(void)          //LinkFullSave_Init 080DB394
{
	TrySavingData(SAVE_NORMAL);
	return FALSE;
}

bool8 HackLinkSave_WriteSector(void)   //LinkFullSave_WriteSector 080DB3C0（TRUE = 書き終えた）
{
	return TRUE;
}

bool8 HackLinkSave_NoOp(void)          //LinkFullSave_ReplaceLastSector 080DB3F4・LinkFullSave_SetLastSectorSignature 080DB41C
{
	return FALSE;
}
