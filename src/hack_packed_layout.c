#include "defines.h"
#include "../include/decompress.h"
#include "../include/fieldmap.h"
#include "../include/overworld.h"

/*
 * 圧縮した地図のレイアウト（2026-10-06・ROM の容量対策 3 番・docs/research/2026-10-06_rom-compression-review.md）。
 * ユーザー「3番お願い」。
 *
 * 素の FRLG は地図に入るとき（InitMapLayoutData 080582C0）レイアウトのマス目を VMap（gBackupMapData）へ丸ごと写し、
 * 隣の地図からは端の帯だけを写す（InitBackupMapLayoutConnections 08058388 → Fill*Connection → FillConnection）。
 * そのあとはマス目を VMap からしか読まない。そこで生成器（tools/rom/layout_pack.py）は
 *   レイアウトの +0x1A に印 PACKED_LAYOUT_MARK・マス目の番地 → struct PackedLayoutMap（4 辺の帯 + LZ77）
 * を書き、ここで
 *   InitBackupMapLayoutData（08058330・hooks r3）: 印があれば VMap の後ろへ展開してから前へ写す（作業用のメモリは要らない）
 *   InitBackupMapLayoutConnections（08058388・hooks r1）: 素と同じ処理を C で。隣が圧縮なら残してある生の帯から写す
 * にする。マス目を直に読む所（バトルピラミッドの部屋・おしえテレビ）の地図は圧縮しない（生成器が外す）。
 */

#define PACKED_LAYOUT_MARK 0x4C
#define VIRTUAL_MAP_SIZE 0x2800          //gBackupMapData のマス数（素の InitMapLayoutData の CpuFastFill と assert）
#define MAPGRID_UNDEFINED_CELL 0x3FF
#define MAP_OFFSET_CELLS 7

struct PackedLayoutMap
{
	const u16* top;      //上 7 行（幅 width）: 北の地図から写される帯（FillSouthConnection）
	const u16* bottom;   //下 7 行: 南の地図から（FillNorthConnection）
	const u16* left;     //左 8 列（1 行 8 マス）: 西の地図から（FillEastConnection）
	const u16* right;    //右 7 列（1 行 7 マス）: 東の地図から（FillWestConnection）
	const u8* lz;        //マス目全体の LZ77（頭 0x10）か Deflate（頭 0x5A・2026-10-11・src/hack_inflate.c・tools/mapfcomp/layout_deflate.py）。★ 2026-10-11 埋め込みからポインタに＝64MB の後ろ半分へ出せる（tools/build/split_back.py・入口 lz_dispatch.s が 0x0A… を振り分ける）
};

bool8 HackInflate(const u8* src, u32 srcSize, u8* dst, u32 dstSize, void* work, u32 workSize);   //src/hack_inflate.c（patches/0319）
bool8 HackSplit_IsBack(const void* src);                                        //src/hack_split_pics.c（dd）: 0x0A… の後ろ半分か
bool8 HackSplit_Native(void);                                                   //直した mGBA（後ろ半分が見える）か
u32 HackSplit_FetchEntryTo(const void* src, void* dst, u32 cap, const u8** out);  //後ろ半分の塊（4 バイト前に長さ）を dst（cap バイト・長さ + 1023 まで要る）へ。→ 長さ（0 = 読めない）・*out は中身の頭
#define PACKED_FETCH_CAP 6144   //SD の写しの入れ物（縮めたマス目は最大 4,208 バイト + 区画の端数 1023・tools/mapfcomp/layout_deflate.py --check が見る）
#define PACKED_DEFLATE_MAGIC 0x5A

struct BackupMapLayoutFR { s32 Xsize; s32 Ysize; u16 *map; };
#define VMap (*(struct BackupMapLayoutFR *)0x03005080)
#define gMapConnectionFlagsWord (*(u32*) 0x02036D58)  //素は 4 バイトで初期化し、向きの印は 1 バイト目に or する
#define gMapConnectionFlagsByte (*(u8*) 0x02036D58)
#define sDummyConnectionFlagsWord (*(const u32*) 0x08316F9C)

static const struct MapHeader* GetMapHeaderFromConnection(const struct MapConnection* connection)
{
	return ((const struct MapHeader* (*)(const struct MapConnection*)) (0x08058284 | 1))(connection);
}

static bool8 IsPackedLayout(const struct MapLayout* layout)
{
	return ((const u8*) layout)[0x1A] == PACKED_LAYOUT_MARK;
}

//hooks 08058330（素の InitBackupMapLayoutData(map, width, height)）
void HackLayout_InitBackupMapLayoutData(const u16* map, u16 width, u16 height)
{
	const struct MapLayout* layout = gMapHeader.mapLayout;
	u16* dest = VMap.map + VMap.Xsize * MAP_OFFSET_CELLS + MAP_OFFSET_CELLS;
	u16* tail = NULL;
	s32 y;

	if (layout->map == map && IsPackedLayout(layout))
	{
		//VMap の後ろへ展開してから前へ写す。行 y の写し先 (y + 7) * (width + 15) + 7 は展開した所 (0x2800 - width * height + y * width)
		//より前にある（VMap に収まる大きさなら必ず）ので、前から順に写せば読む前の所を潰さない
		//★ 2026-10-11 本体は後ろ半分（--split-pics）にあることがある。直した mGBA（後ろ半分が見える）なら src そのもの・SuperCard の SD からなら
		//  RAM の写し（dd の HackSplit_FetchEntryTo・4 バイト前の長さ・入れ物は Malloc）。頭のバイトを見る前にもらう（SD の道では 0x0A… を直に読めない）。
		//  長さ（srcLen）が分かるときは展開器に渡して、化けた写しで入力の先を読まない（5e の指摘 2）。
		//  読めなければ（SD が読めない・ヒープが無い）空のマス（0x3FF）で埋める（固まらせない・検算が先に捕まえる。LZ77 の入口の道も SD では同じくヒープが要る）
		const u8* lz = ((const struct PackedLayoutMap*) map)->lz;
		const u8* packed = lz;
		void* alloc = NULL;
		u32 srcLen = 0;
		u32 rawSize = width * height * sizeof(u16);
		bool8 ok = FALSE;

		if (HackSplit_IsBack(lz))
		{
			if (HackSplit_Native())
				srcLen = *(const u32*) ((u32) lz - 4);
			else
			{
				alloc = Malloc(PACKED_FETCH_CAP);
				if (alloc != NULL)
					srcLen = HackSplit_FetchEntryTo(lz, alloc, PACKED_FETCH_CAP, &packed);
				if (srcLen == 0 || srcLen > PACKED_FETCH_CAP - 1023)
					packed = NULL;
			}
		}

		tail = VMap.map + VIRTUAL_MAP_SIZE - width * height;
		if (packed != NULL && packed[0] == PACKED_DEFLATE_MAGIC)
		{
			//★ 2026-10-11 Deflate（LZ77 より約 20% 小さい）。作業場（3,616 バイト）は VMap の頭の空き (0x2800 − w×h)×2（展開先 tail より前）。
			//  足りない地図は生成器（tools/mapfcomp/layout_deflate.work_fits）が LZ77 のままにする（いちばん狭い 120×61 で 5,840 バイト）
			//  ＝展開の作業場は Malloc に頼らない（5e の指摘: ヒープが無いと空のマスになって詰む）。頭の生の大きさは展開器が w×h×2 と突き合わせる
			ok = HackInflate(packed, srcLen, (u8*) tail, rawSize, VMap.map, (u8*) tail - (u8*) VMap.map);
		}
		else if (packed != NULL && packed[0] == 0x10
		      && (u32) (packed[1] | (packed[2] << 8) | (packed[3] << 16)) == rawSize)
		{
			//LZ77（work_fits が偽の地図）: 頭が 0x10 で大きさが w×h×2 のときだけ（化けた写しで BIOS が VMap の先まで書かないように・5e の指摘 1）
			LZ77UnCompWram(packed, tail);   //写しなら RAM から（入口の SD の道は通らない）・そうでなければ今までどおり
			ok = TRUE;
		}
		if (!ok)
		{
			CpuFill16(MAPGRID_UNDEFINED_CELL, tail, rawSize);   //CpuFill16 はマクロ（中かっこ）なので if の中は囲む
		}
		if (alloc != NULL)
			Free(alloc);
		map = tail;
	}

	for (y = 0; y < height; y++)
	{
		CpuCopy16(map, dest, width * sizeof(u16));
		if (tail != NULL)
			CpuFill16(MAPGRID_UNDEFINED_CELL, dest + width, (VMap.Xsize - width) * sizeof(u16)); //行の間（展開した残りを消す）
		dest += VMap.Xsize;
		map += width;
	}

	if (tail != NULL)
	{
		u16* end = VMap.map + VIRTUAL_MAP_SIZE;
		dest += width - VMap.Xsize; //最後の行の右端の次
		if (dest < end)
			CpuFill16(MAPGRID_UNDEFINED_CELL, dest, (end - dest) * sizeof(u16));
	}
}

/* ---- 隣の地図（素の Fill*Connection を C で・pokefirered src/fieldmap.c と同じ） ---------------------------- */
enum { BAND_TOP, BAND_BOTTOM, BAND_LEFT, BAND_RIGHT };

static void FillConnection(s32 x, s32 y, const struct MapHeader* connectedMapHeader, s32 x2, s32 y2, s32 width, s32 height, u8 band)
{
	const struct MapLayout* layout = connectedMapHeader->mapLayout;
	s32 mapWidth = layout->width;
	const u16* src;
	s32 stride;
	u16* dest = &VMap.map[VMap.Xsize * y + x];
	s32 i;

	if (IsPackedLayout(layout))
	{
		const struct PackedLayoutMap* p = (const struct PackedLayoutMap*) layout->map;
		switch (band) {
			case BAND_TOP:
				src = p->top;
				stride = mapWidth;
				break;
			case BAND_BOTTOM:
				src = p->bottom;
				stride = mapWidth;
				y2 -= layout->height - MAP_OFFSET_CELLS;
				break;
			case BAND_LEFT:
				src = p->left;
				stride = MAP_OFFSET_CELLS + 1;
				break;
			default:
				src = p->right;
				stride = MAP_OFFSET_CELLS;
				x2 -= mapWidth - MAP_OFFSET_CELLS;
				break;
		}
		if (src == NULL) //帯が無い（生成器の検算 verify_packed_layouts.py が無いことを確かめる）→ 何も写さない
			return;
	}
	else
	{
		src = layout->map;
		stride = mapWidth;
	}

	src += stride * y2 + x2;
	for (i = 0; i < height; i++)
	{
		CpuCopy16(src, dest, width * 2);
		dest += VMap.Xsize;
		src += stride;
	}
}

static void FillSouthConnection(const struct MapHeader* mapHeader, const struct MapHeader* connectedMapHeader, s32 offset)
{
	s32 x, y, x2, width, cWidth;

	if (connectedMapHeader)
	{
		cWidth = connectedMapHeader->mapLayout->width;
		x = offset + MAP_OFFSET_CELLS;
		y = mapHeader->mapLayout->height + MAP_OFFSET_CELLS;
		if (x < 0)
		{
			x2 = -x;
			x += cWidth;
			width = (x < VMap.Xsize) ? x : VMap.Xsize;
			x = 0;
		}
		else
		{
			x2 = 0;
			width = (x + cWidth < VMap.Xsize) ? cWidth : VMap.Xsize - x;
		}
		FillConnection(x, y, connectedMapHeader, x2, 0, width, MAP_OFFSET_CELLS, BAND_TOP);
	}
}

static void FillNorthConnection(const struct MapHeader* mapHeader, const struct MapHeader* connectedMapHeader, s32 offset)
{
	s32 x, x2, y2, width, cWidth, cHeight;

	if (connectedMapHeader)
	{
		cWidth = connectedMapHeader->mapLayout->width;
		cHeight = connectedMapHeader->mapLayout->height;
		x = offset + MAP_OFFSET_CELLS;
		y2 = cHeight - MAP_OFFSET_CELLS;
		if (x < 0)
		{
			x2 = -x;
			x += cWidth;
			width = (x < VMap.Xsize) ? x : VMap.Xsize;
			x = 0;
		}
		else
		{
			x2 = 0;
			width = (x + cWidth < VMap.Xsize) ? cWidth : VMap.Xsize - x;
		}
		FillConnection(x, 0, connectedMapHeader, x2, y2, width, MAP_OFFSET_CELLS, BAND_BOTTOM);
	}
}

static void FillWestConnection(const struct MapHeader* mapHeader, const struct MapHeader* connectedMapHeader, s32 offset)
{
	s32 y, x2, y2, height, cWidth, cHeight;

	if (connectedMapHeader)
	{
		cWidth = connectedMapHeader->mapLayout->width;
		cHeight = connectedMapHeader->mapLayout->height;
		y = offset + MAP_OFFSET_CELLS;
		x2 = cWidth - MAP_OFFSET_CELLS;
		if (y < 0)
		{
			y2 = -y;
			height = (y + cHeight < VMap.Ysize) ? y + cHeight : VMap.Ysize;
			y = 0;
		}
		else
		{
			y2 = 0;
			height = (y + cHeight < VMap.Ysize) ? cHeight : VMap.Ysize - y;
		}
		FillConnection(0, y, connectedMapHeader, x2, y2, MAP_OFFSET_CELLS, height, BAND_RIGHT);
	}
}

static void FillEastConnection(const struct MapHeader* mapHeader, const struct MapHeader* connectedMapHeader, s32 offset)
{
	s32 x, y, y2, height, cHeight;

	if (connectedMapHeader)
	{
		cHeight = connectedMapHeader->mapLayout->height;
		x = mapHeader->mapLayout->width + MAP_OFFSET_CELLS;
		y = offset + MAP_OFFSET_CELLS;
		if (y < 0)
		{
			y2 = -y;
			height = (y + cHeight < VMap.Ysize) ? y + cHeight : VMap.Ysize;
			y = 0;
		}
		else
		{
			y2 = 0;
			height = (y + cHeight < VMap.Ysize) ? cHeight : VMap.Ysize - y;
		}
		FillConnection(x, y, connectedMapHeader, 0, y2, MAP_OFFSET_CELLS + 1, height, BAND_LEFT);
	}
}

//hooks 08058388（素の InitBackupMapLayoutConnections(mapHeader)）
void HackLayout_InitBackupMapLayoutConnections(const struct MapHeader* mapHeader)
{
	s32 count, i;
	const struct MapConnection* connection;

	gMapConnectionFlagsWord = sDummyConnectionFlagsWord;
	if (mapHeader->connections == NULL)
		return;

	count = mapHeader->connections->count;
	connection = mapHeader->connections->connections;
	for (i = 0; i < count; i++, connection++)
	{
		const struct MapHeader* cMap = GetMapHeaderFromConnection(connection);
		s32 offset = connection->offset;

		switch (connection->direction) {
			case CONNECTION_SOUTH:
				FillSouthConnection(mapHeader, cMap, offset);
				gMapConnectionFlagsByte |= 1;
				break;
			case CONNECTION_NORTH:
				FillNorthConnection(mapHeader, cMap, offset);
				gMapConnectionFlagsByte |= 2;
				break;
			case CONNECTION_WEST:
				FillWestConnection(mapHeader, cMap, offset);
				gMapConnectionFlagsByte |= 4;
				break;
			case CONNECTION_EAST:
				FillEastConnection(mapHeader, cMap, offset);
				gMapConnectionFlagsByte |= 8;
				break;
		}
	}
}
