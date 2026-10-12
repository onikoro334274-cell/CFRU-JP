#include "defines.h"
#include "../include/bg.h"
#include "../include/malloc.h"
#include "../include/menu.h"
#include "../include/palette.h"
#include "../include/pokemon.h"
#include "../include/pokemon_icon.h"
#include "../include/pokemon_storage_system.h"   //TOTAL_BOXES_COUNT・IN_BOX_COUNT
#include "../include/sound.h"
#include "../include/window.h"
#include "../include/constants/songs.h"
#include "../include/constants/species.h"
#include "../include/new/pokemon_storage_system.h"
#include "hack_box_jump.h"
#include "hack_box_shelf.h"
#include "hack_ext_save.h"

/*
	ボックスジャンプ（2026-10-12 ユーザー「https://github.com/pm-start/box-jump これ取り込んで」・#105・patches/0341）。

	元は pm-start/box-jump（作者 liru さん・cd9a044）: 素の日本語版 FR の「箱を選ぶ画面」を、画面の下に 5 箱のカード
	（6 × 5 の 8×8 アイコンで 30 匹・箱の名前・番号・匹数・使用率の帯・BW の PC の絵）を横に並べて左右で選ぶ画面に置き換える。
	こちらは CFRU の C に書き直した（元の src の .c と同じ流れ・同じ画面の寸法）:
	  - 箱を選ぶ画面は CFRU でも素の 0808C 帯のコード（CFRU は箱の数 49 を bytereplacement で書くだけ）。素の 4 つを functionrewrites で置き換える:
	    0808C398 入力（返り値 0xC8 なにもしない／0xC9 やめる／箱の番号）・0808C400 作る(u8 箱)・0808C5E0 消す・0808C6B0 描き直す。
	    素の入れ物（0x020396F8 が指す構造・+0x244 が今の箱）の先頭の語に、ヒープに取った状態の番地を置く（元と同じ・6.13）
	  - 箱の数: 追加のセーブ領域が使えれば 50・無ければ 39 で回る（40〜50 箱は 6MB のセーブの mGBA だけ・#51）
	  - 中身は読むだけ（6.68）: 棚の箱は BoxShelf_ReadMon（入れ物を動かさない写し）・19〜24 箱は GetCompressedMonPtr の直の番地。
	    読むのは開いたときの 5 箱と、左右で新しく見える 1 箱（30 匹）だけ（6.80）
	  - アイコン: 元は 8×8 に縮めた表を ROM に前もって作っていた（素の 440 種）。こちらは表を持たず、読むときに
	    GetIconSpecies → GetMonIconTiles の 32×32 から元と同じ取り方（4×4 の真ん中の点・無ければその 4×4 の最初の色）で作る。
	    何も無い点は元の作りでは色 3（素の色では濃い色）だったが、こちらの色 3 は白で 3 匹が 1 本の白い帯に見えた（mGBA）→ 0 のまま残し、描くときに
	    アイコンの 16 色のうちカードの地の色にいちばん近い色に置き換える（アイコンの 16 色は全部使われていて空きが無い）＝
	    DPE の全種族・Moémon・タマゴ・アンノーンが自動で入る。色はアイコンの色 0〜2 → BG の 10〜12（元と同じ）
	  - Moémon の当たりは #70 の裏 ID（HackMonId_MoemonKey は手持ち・相手・今の箱しか探さない）→ 1 匹を読む間だけ状態の curBoxMon に
	    その子の縮めた 58 バイトの番地を置き、HackMonId_MoemonKey が HackBoxJump_IconHint で引く（読み終わったら 0）
	  - 窓の枠のタイル 9 枚と色は tools/box_jump/build_box_jump_gfx.py（BW の PC の絵）。ふだんの PC の絵は #78 で入っている
*/

#define CHOOSE_CONTEXT (*(u8**) 0x020396F8)
#define SELECTED_BOX_OFFSET 0x244
#define BOX_JUMP_MAGIC 0x504A5842   //'BXJP'
#define GetBgTilemapBufferRaw ((u16* (*)(u8)) 0x08002009)
#define INPUT_NONE 0xC8
#define INPUT_CANCEL 0xC9

#define PREVIEW_COUNT 5
#define BOX_SLOTS IN_BOX_COUNT
#define MENU_TILE_BYTES (512 * 32)
#define ICON_PAL_FIRST 10            //BG の色 10〜12
#define FRAME_PAL 14

struct BoxPreview
{
	u8 box, count;
	u8 pal[BOX_SLOTS];               //0xFF＝空き
	u16 icon[BOX_SLOTS];             //アイコンの種族（色を読むとき）
	u8 tiles[BOX_SLOTS][32];         //縮めた 8×8 のアイコン
};

struct BoxJumpState
{
	u32 magic;
	const struct CompressedPokemon* curBoxMon;   //いまアイコンを決めている子（HackMonId_MoemonKey が見る・読み終わったら NULL）
	u8 window, cachedBox, ready, padding;
	u32 tileAddress;
	struct CompressedPokemon readMon;            //1 匹の写し（棚の箱）
	struct BoxPreview previews[PREVIEW_COUNT];
	u16 palettes[80];
	u16 tilemap[32 * 32];
	u16 tiles[MENU_TILE_BYTES / 2];
};

struct CompressedPokemon* GetCompressedMonPtr(u8 boxId, u8 boxPosition);   //src/pokemon_storage_system.c
extern const u16 gBoxJumpPalette[16];
extern const u8 gBoxJumpFrameTiles[9][32];

static const u8 sDigits[10][5] =
{
	{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
	{7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7},
};
static const u8 sLetterB[5] = {6, 5, 6, 5, 6};
static const u8 sTitleColors[3] = {0, 1, 2};
static const struct WindowTemplate sWindowTemplate = {0, 0, 7, 30, 10, FRAME_PAL, 0xC0};

static struct BoxJumpState* State(void)
{
	u8* ctx = CHOOSE_CONTEXT;
	return (ctx != NULL) ? *(struct BoxJumpState**) ctx : NULL;
}

static u8 BoxCount(void)
{
	return HackExtSave_Available() ? TOTAL_BOXES_COUNT : 39;
}

static u8 Wrap(s32 box)
{
	s32 n = BoxCount();
	while (box < 0)
		box += n;
	while (box >= n)
		box -= n;
	return box;
}

//★ #70: HackMonId_MoemonKey（src/hack_mon_id.c）から。いまアイコンを決めている子（box-jump の状態が生きているときだけ）
const void* HackBoxJump_IconHint(void)
{
	u8* ctx = CHOOSE_CONTEXT;
	struct BoxJumpState* s;

	if ((u32) ctx < 0x02000000 || (u32) ctx >= 0x02040000)
		return NULL;
	s = *(struct BoxJumpState**) ctx;
	if ((u32) s < 0x02000000 || (u32) s >= 0x02040000 || s->magic != BOX_JUMP_MAGIC)
		return NULL;
	return s->curBoxMon;
}

//32×32 のアイコン（4bpp・4 × 4 タイル）から 8×8 を作る（元の tools/icon_cache.py の scale_icon と同じ取り方）
static u8 IconPixel(const u8* tiles, u32 x, u32 y)
{
	u8 packed = tiles[((y >> 3) * 4 + (x >> 3)) * 32 + (y & 7) * 4 + ((x & 7) >> 1)];
	return (x & 1) ? packed >> 4 : packed & 15;
}

static void ShrinkIcon(const u8* src, u8* dst)
{
	u32 x, y, dx, dy;

	for (x = 0; x < 32; ++x)
		dst[x] = 0;
	for (y = 0; y < 8; ++y)
	{
		for (x = 0; x < 8; ++x)
		{
			u8 color = IconPixel(src, x * 4 + 2, y * 4 + 2);
			for (dy = 0; color == 0 && dy < 4; ++dy)
				for (dx = 0; color == 0 && dx < 4; ++dx)
					color = IconPixel(src, x * 4 + dx, y * 4 + dy);
			dst[y * 4 + (x >> 1)] |= color << ((x & 1) ? 4 : 0);
		}
	}
}

static void ReadBox(struct BoxJumpState* s, u8 box, struct BoxPreview* out)
{
	u32 slot;

	out->box = box;
	out->count = 0;
	for (slot = 0; slot < BOX_SLOTS; ++slot)
	{
		const struct CompressedPokemon* mon;
		const u8* raw;
		u16 species, icon;
		u32 personality;

		out->pal[slot] = 0xFF;
		if (BoxShelf_ShelfIndex(box) >= 0)
		{
			BoxShelf_ReadMon(box, slot, &s->readMon);   //入れ物を動かさない写し（40〜50 箱は使えなければ 0）
			mon = &s->readMon;
		}
		else
			mon = GetCompressedMonPtr(box, slot);       //19〜24 箱（棚でない）は直の番地・読むだけ
		if (mon == NULL)
			continue;
		raw = (const u8*) mon;
		species = raw[0x1C] | (raw[0x1D] << 8);
		if (species == SPECIES_NONE || !(raw[0x13] & 2))   //hasSpecies
			continue;
		personality = raw[0] | (raw[1] << 8) | (raw[2] << 16) | ((u32) raw[3] << 24);
		if (raw[0x13] & 4)                                   //isEgg
			species = SPECIES_EGG;
		s->curBoxMon = mon;
		icon = GetIconSpecies(species, personality);
		s->curBoxMon = NULL;
		ShrinkIcon(GetMonIconTiles(icon, FALSE), out->tiles[slot]);
		out->pal[slot] = GetValidMonIconPalIndex(icon);
		out->icon[slot] = icon;
		out->count++;
	}
}

static void CopyPreview(struct BoxPreview* dst, const struct BoxPreview* src)
{
	Memcpy(dst, src, sizeof(*dst));
}

/* ---- 描く（元の draw.c・ui.c・icons.c） ---- */

static void Rect(u8 window, u16 x, u16 y, u16 w, u16 h, u8 color)
{
	FillWindowPixelRect(window, color | (color << 4), x, y, w, h);
}

static void Glyph(u8 window, const u8 shape[5], u16 x, u16 y, u8 color, u8 paper)
{
	u8 bitmap[32];   //タイルの並びで読むので 4×5 の字も 8×8 の 1 枚
	u32 i, line;

	for (i = 0; i < 32; ++i)
		bitmap[i] = paper | (paper << 4);
	for (line = 0; line < 5; ++line)
	{
		u8 bits = shape[line];
		u8 a = (bits & 4) ? color : paper;
		u8 b = (bits & 2) ? color : paper;
		u8 c = (bits & 1) ? color : paper;
		bitmap[line * 4] = a | (b << 4);
		bitmap[line * 4 + 1] = c | (paper << 4);
	}
	BlitBitmapRectToWindow(window, bitmap, 0, 0, 8, 8, x, y, 4, 5);
}

static void Number(u8 window, u8 value, u16 x, u16 y, u8 color, u8 paper)
{
	u8 tens = 0;
	while (value >= 10)
	{
		value -= 10;
		++tens;
	}
	Glyph(window, sDigits[tens], x, y, color, paper);
	Glyph(window, sDigits[value], x + 5, y, color, paper);
}

static void DrawBackground(u8 window)
{
	u8* pixels = gWindows[window].tileData;
	u32 row, column, word;

	LoadPalette(gBoxJumpPalette, FRAME_PAL * 16, 32);
	for (row = 0; row < 10; ++row)
	{
		for (column = 0; column < 30; ++column)
		{
			u32 frame = (row == 0 ? 0 : (row == 9 ? 2 : 1)) * 3 + (column == 0 ? 0 : (column == 29 ? 2 : 1));
			u32* dst = (u32*) (pixels + (row * 30 + column) * 32);
			const u32* src = (const u32*) gBoxJumpFrameTiles[frame];
			for (word = 0; word < 8; ++word)
				dst[word] = src[word];
		}
	}
}

//アイコンの色の組（BG の 10〜12 に読んだもの）のうち、マス目の地（窓の枠の中の色＝灰）にいちばん近い色の番号（1〜15）
//★ 2026-10-12 最初は見出しの帯の色（選んだカードは青）に寄せたが、アイコンのマスはどのカードも灰の地の上で、青に近い色は紫になった（mGBA）
static u8 NearestIconColor(u8 pal)
{
	const u16* icon = &gPlttBufferUnfaded[(ICON_PAL_FIRST + pal) * 16];
	u16 want = gBoxJumpPalette[gBoxJumpFrameTiles[4][0] & 15];   //枠の真ん中のタイルの色
	u32 i, best = 1, bestDist = 0xFFFFFFFF;

	for (i = 1; i < 16; ++i)
	{
		s32 dr = (s32) (icon[i] & 31) - (want & 31);
		s32 dg = (s32) ((icon[i] >> 5) & 31) - ((want >> 5) & 31);
		s32 db = (s32) ((icon[i] >> 10) & 31) - ((want >> 10) & 31);
		u32 d = dr * dr + dg * dg + db * db;
		if (d < bestDist)
		{
			bestDist = d;
			best = i;
		}
	}
	return best;
}

static void DrawIcon(u8 window, const struct BoxPreview* p, u32 slot, u16 x, u16 y, u8 paperIndex)
{
	u8* pixels = gWindows[window].tileData;
	u8* dst = pixels + ((y >> 3) * gWindows[window].window.width + (x >> 3)) * 32;
	const u8* src = p->tiles[slot];
	u32 i;

	for (i = 0; i < 32; ++i)   //何も無い点（0）はカードの地の色に近い色へ
	{
		u8 lo = src[i] & 15, hi = src[i] >> 4;
		dst[i] = (lo ? lo : paperIndex) | ((hi ? hi : paperIndex) << 4);
	}
	PutWindowRectTilemapOverridePalette(window, x >> 3, y >> 3, 1, 1, ICON_PAL_FIRST + p->pal[slot]);
}

static void Draw(struct BoxJumpState* s, u8 selected)
{
	u8 window = s->window;
	u8 loaded = 0;
	u32 i, line, row, column;

	DrawBackground(window);
	Rect(window, 0, 0, 240, 16, 2);
	AddTextPrinterParameterized3(window, 1, 96, 1, sTitleColors, -1, GetBoxNamePtr(selected));
	Rect(window, 96, 14, 48, 2, 5);
	for (line = 0; line < 3; ++line)
	{
		Rect(window, 90 + line, 6 - line, 1, 1 + line * 2, 5);
		Rect(window, 150 - line, 6 - line, 1, 1 + line * 2, 5);
	}
	PutWindowTilemap(window);   //地図は枠の色で書き、アイコンのタイルだけ色を差し替える
	for (i = 0; i < PREVIEW_COUNT; ++i)
	{
		const struct BoxPreview* p = &s->previews[i];
		u16 x = i * 48;
		u8 paper = (i == 2) ? 5 : 8;
		u16 filled;

		Rect(window, x + 1, 17, 46, 14, paper);
		Glyph(window, sLetterB, x + 4, 22, 1, paper);
		Number(window, p->box + 1, x + 9, 22, 1, paper);
		Number(window, p->count, x + 33, 22, 1, paper);
		for (row = 0; row < 5; ++row)
		{
			for (column = 0; column < 6; ++column)
			{
				u32 slot = row * 6 + column;
				u16 dx = x + column * 8, dy = 32 + row * 8;
				if (p->pal[slot] != 0xFF)
				{
					u8 pal = p->pal[slot];
					if (pal < 3 && !(loaded & (1 << pal)))   //アイコンの色の組 0〜2 → BG の 10〜12（元と同じ）
					{
						LoadPalette(GetValidMonIconPalettePtr(p->icon[slot]), (ICON_PAL_FIRST + pal) * 16, 32);
						loaded |= 1 << pal;
					}
					DrawIcon(window, p, slot, dx, dy, NearestIconColor(pal));
				}
				else
					Rect(window, dx + 2, dy + 2, 4, 4, 3);
			}
		}
		Rect(window, x + 5, 74, 38, 2, 8);
		filled = (p->count == BOX_SLOTS) ? 38 : p->count + (p->count >> 2);
		if (filled)
			Rect(window, x + 5, 74, filled, 2, (i == 2) ? 5 : 7);
	}
	for (i = 1; i < PREVIEW_COUNT; ++i)
		Rect(window, i * 48, 32, 1, 40, 14);
	CopyWindowToVram(window, COPYWIN_BOTH);
	CopyBgTilemapBufferToVram(0);
}

/* ---- 素の 4 つの置き換え（functionrewrites） ---- */

//0808C6B0 UpdateChooseBoxMenu: 今の箱が 1 つ動いたら 4 箱は使い回し、新しく見える 1 箱だけ読む（元の menu.c）
void HackBoxJump_Render(void)
{
	struct BoxJumpState* s = State();
	u8 selected;
	u32 i;

	if (s == NULL || s->magic != BOX_JUMP_MAGIC)
		return;
	selected = CHOOSE_CONTEXT[SELECTED_BOX_OFFSET];
	if (s->ready && selected == Wrap(s->cachedBox + 1))
	{
		for (i = 0; i < PREVIEW_COUNT - 1; ++i)
			CopyPreview(&s->previews[i], &s->previews[i + 1]);
		ReadBox(s, Wrap(selected + 2), &s->previews[PREVIEW_COUNT - 1]);
	}
	else if (s->ready && selected == Wrap(s->cachedBox - 1))
	{
		for (i = PREVIEW_COUNT - 1; i > 0; --i)
			CopyPreview(&s->previews[i], &s->previews[i - 1]);
		ReadBox(s, Wrap(selected - 2), &s->previews[0]);
	}
	else if (!s->ready || selected != s->cachedBox)
	{
		for (i = 0; i < PREVIEW_COUNT; ++i)
			ReadBox(s, Wrap(selected + i - 2), &s->previews[i]);
	}
	s->cachedBox = selected;
	s->ready = TRUE;
	Draw(s, selected);
}

//0808C400 CreateChooseBoxMenuSprites(u8 curBox)
void HackBoxJump_Create(u8 curBox)
{
	struct BoxJumpState* s;
	volatile u16* vramTiles;
	u16* map;
	u16 window;
	u32 i;

	CHOOSE_CONTEXT[SELECTED_BOX_OFFSET] = Wrap(curBox);
	*(u32*) CHOOSE_CONTEXT = 0;   //0＝取れなかった（素のスプライトは作らない）
	s = AllocZeroed(sizeof(*s));
	if (s == NULL)
		return;
	map = GetBgTilemapBufferRaw(0);
	if (map == NULL || GetBgAttribute(0, BG_ATTR_BASETILE) != 0 || GetBgAttribute(0, BG_ATTR_SCREENSIZE) != 0)
	{
		Free(s);
		return;
	}
	s->tileAddress = VRAM + ((u32) GetBgAttribute(0, BG_ATTR_CHARBASEINDEX) << 14);
	vramTiles = (volatile u16*) s->tileAddress;
	for (i = 0; i < MENU_TILE_BYTES / 2; ++i)
		s->tiles[i] = vramTiles[i];
	for (i = 0; i < 80; ++i)
		s->palettes[i] = gPlttBufferUnfaded[ICON_PAL_FIRST * 16 + i];
	for (i = 0; i < 32 * 32; ++i)
		s->tilemap[i] = map[i];
	window = AddWindow(&sWindowTemplate);
	if (window >= 32)
	{
		Free(s);
		return;
	}
	s->window = window;
	s->magic = BOX_JUMP_MAGIC;
	*(struct BoxJumpState**) CHOOSE_CONTEXT = s;
	//窓が借りるタイルだけ隠す（まわりの見出しと下の帯は素の背景のまま）
	for (i = 511 * 16; i < 512 * 16; ++i)
		vramTiles[i] = 0;
	for (i = 0; i < 32 * 32; ++i)
	{
		u16 tile = map[i] & 0x3FF;
		if (tile >= 0xC0 && tile < 0xC0 + 300)
			map[i] = 511;
	}
	HackBoxJump_Render();
}

//0808C5E0 DestroyChooseBoxMenuSprites
void HackBoxJump_Destroy(void)
{
	struct BoxJumpState* s = State();
	volatile u16* vramTiles;
	u16* map;
	u32 i;

	if (s == NULL || s->magic != BOX_JUMP_MAGIC)
		return;
	RemoveWindow(s->window);
	vramTiles = (volatile u16*) s->tileAddress;
	for (i = 0; i < MENU_TILE_BYTES / 2; ++i)
		vramTiles[i] = s->tiles[i];
	LoadPalette(s->palettes, ICON_PAL_FIRST * 16, sizeof(s->palettes));
	//★ 2026-10-12 地図は「この窓が書いたマス」だけ戻す（窓のタイル 0xC0〜・隠した 511）。素の流れは ClearBottomWindow → この関数の順なので、
	//  全部を戻すと消したばかりの「どの ボックスに ジャンプしますか?」がよみがえった（mGBA・元の作りは全部戻していた）
	map = GetBgTilemapBufferRaw(0);
	if (map != NULL)
		for (i = 0; i < 32 * 32; ++i)
		{
			u16 tile = map[i] & 0x3FF;
			if (tile == 511 || (tile >= sWindowTemplate.baseBlock && tile < sWindowTemplate.baseBlock + 30 * 10))
				map[i] = s->tilemap[i];
		}
	s->magic = 0;
	Free(s);
	*(u32*) CHOOSE_CONTEXT = 0;
	CopyBgTilemapBufferToVram(0);
}

//0808C398 HandleBoxChooseSelectionInput
u8 HackBoxJump_Input(void)
{
	s32 direction;

	if (JOY_NEW(B_BUTTON))
	{
		PlaySE(SE_SELECT);
		return INPUT_CANCEL;
	}
	if (JOY_NEW(A_BUTTON))
	{
		PlaySE(SE_SELECT);
		return CHOOSE_CONTEXT[SELECTED_BOX_OFFSET];
	}
	direction = JOY_NEW_AND_REPEATED(DPAD_LEFT) ? -1 : (JOY_NEW_AND_REPEATED(DPAD_RIGHT) ? 1 : 0);
	if (direction)
	{
		CHOOSE_CONTEXT[SELECTED_BOX_OFFSET] = Wrap(CHOOSE_CONTEXT[SELECTED_BOX_OFFSET] + direction);
		PlaySE(SE_SELECT);
		HackBoxJump_Render();
	}
	return INPUT_NONE;
}
