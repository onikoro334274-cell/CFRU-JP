#include "defines.h"
#include "../include/event_data.h"
#include "../include/malloc.h"
#include "hack_ext_save.h"

/*
 * 追加のセーブ領域 2.875MB（2026-10-08 ユーザー「1mで」→ 2026-10-09「先に 6MB に広げてから 2 つ目のセーブを作る」）。
 * docs/research/2026-10-08_save-1mb.md / 2026-10-09_save-6mb-two-slots.md
 *
 * - 直した mGBA（tools/mgba/save1m.patch）はフラッシュを 6MB（64KB × 96 バンク）まで持つ。ゲームがバンク 2 以上に
 *   切り替えたときだけ .sav を 6MB に広げる（前の 1MB の .sav は中身を残して広げる）。区画（4KB）の番号はバンク × 16 + バンクの中の番号＝
 *   本体の 128KB は 0〜31、追加は 32〜767（バンク 2〜47）。バンク 48〜95 は 2 つ目のセーブ（同じ並びを 48 バンク後ろへ）に取ってある。
 * - ★ 実機の 128KB のフラッシュにはバンク 0・1 しか無い（推測: バンクの番号の下 1 ビットしか見ない）→ 実機で 32 番以降に書くと本体のセーブを
 *   上書きして壊す。使ってよいのは「64MB の ROM の後ろ半分が見える」とき（＝直した mGBA で _split64.gba を遊んでいる）だけ。
 *   後ろ半分の頭には PKSPICS1（tools/build/build_rom.py の write_split_outputs）。SuperCard（0x0A000000 は ROM の写し）・素の mGBA・実機では見えない。
 * - 書くのは素の ProgramFlashSectorAndVerify（081C2C50・区画の番号は u16）。素の TryWriteSector（080DA9C0）は区画の番号が u8 で
 *   256 番から書けず、壊れた区画の印（1 << 番号）も 32 番からは外れるので通さない。書き込みの関数は区画の番号を gFlash->sector.count（32）で
 *   はねるので、書く間だけ gFlash（03007470 の指す FlashType）を count = 768 の写しに差し替える。読むのは素の ReadFlash（081C2A54・
 *   区画の番号 >> 4 でバンクを切り替える・count は見ない）。
 * - 使っている区画: 0 = 6MB 版の mGBA かの確かめ（src/hack_save_slot.c）・8〜23 = ボックスの 40〜50 箱（src/hack_box_shelf.c・2026-10-09）。ほかはまだ決めていない。使う側は HackExtSave_Available() を見てから読み書きし、使えない版では
 *   入れたものが無い前提で動くこと。
 */
#define EXT_FIRST_SECTOR 32
#define EXT_SECTOR_COUNT 736                 //32〜767（バンク 2〜47）
#define EXT_SECTOR_SIZE 0x1000
#define gFlashPtr (*(const u8**) 0x03007470)  //const struct FlashType *gFlash（romSize +0・sector.size +4・shift +8・count +0xA）
#define FLASHTYPE_COPY_SIZE 0x20
#define ProgramFlashSectorAndVerifyVanilla ((u32 (*)(u16, u8*)) (0x081C2C50 | 1))   //0 なら書けた
#include "hack_save_crypt.h"

bool8 HackExtSave_Available(void)
{
	const u32* head = (const u32*) 0x0A000000;

	return head[0] == 0x50534B50 && head[1] == 0x31534349;   //'PKSPICS1'
}

bool8 HackExtSave_ReadSector(u16 n, void* dst)
{
	if (n >= EXT_SECTOR_COUNT || !HackExtSave_Available())
		return FALSE;
	HackSaveCrypt_ReadFlash(EXT_FIRST_SECTOR + n, 0, dst, EXT_SECTOR_SIZE);   //★ 2026-10-09 かき混ぜを戻して読む（src/hack_save_crypt.c）
	return TRUE;
}

//区画 n の off から size バイト（ボックスの 40〜50 箱・src/hack_box_shelf.c）
bool8 HackExtSave_Read(u16 n, u32 off, void* dst, u32 size)
{
	if (n >= EXT_SECTOR_COUNT || !HackExtSave_Available())
		return FALSE;
	HackSaveCrypt_ReadFlash(EXT_FIRST_SECTOR + n, off, dst, size);
	return TRUE;
}

bool8 HackExtSave_WriteSector(u16 n, const void* src)
{
	const u8* old = gFlashPtr;
	u8* copy;
	u32 status;
	u32 i;

	if (n >= EXT_SECTOR_COUNT || !HackExtSave_Available() || old == NULL || *(const u32*) old != 0x20000)
		return FALSE;   //1MB（128KB）のフラッシュのときだけ
	copy = Malloc(FLASHTYPE_COPY_SIZE);
	if (copy == NULL)
		return FALSE;
	for (i = 0; i < FLASHTYPE_COPY_SIZE; ++i)
		copy[i] = old[i];
	*(u16*) (copy + 0xA) = EXT_FIRST_SECTOR + EXT_SECTOR_COUNT;
	gFlashPtr = copy;
	status = ProgramFlashSectorAndVerifyVanilla(EXT_FIRST_SECTOR + n, (u8*) src);
	gFlashPtr = old;
	Free(copy);
	return status == 0;
}

#ifndef HACK_DEMO_BUILD
//確かめ用（callasm）: 追加の区画 1 と 735 に模様を書いて読み戻す → 0x8004 = 1 なら書けて読めた・0 は使えない版・2 は食い違い・3 は入れ物が取れない
void HackExtSave_DebugTest(void)
{
	u32* buf = Malloc(EXT_SECTOR_SIZE);
	u32 i, k;
	u16 sectors[2] = {1, EXT_SECTOR_COUNT - 1};   //0 は src/hack_save_slot.c の確かめ用

	Var8004 = 0;
	if (buf == NULL)
	{
		Var8004 = 3;
		return;
	}
	if (!HackExtSave_Available())
	{
		Free(buf);
		return;
	}
	Var8004 = 1;
	for (k = 0; k < 2; ++k)
	{
		for (i = 0; i < EXT_SECTOR_SIZE / 4; ++i)
			buf[i] = 0x45585400 + sectors[k] * 0x10000 + i;   //'EXT'
		if (!HackExtSave_WriteSector(sectors[k], buf))
			Var8004 = 2;
		for (i = 0; i < EXT_SECTOR_SIZE / 4; ++i)
			buf[i] = 0;
		HackExtSave_ReadSector(sectors[k], buf);
		for (i = 0; i < EXT_SECTOR_SIZE / 4; ++i)
			if (buf[i] != 0x45585400 + sectors[k] * 0x10000 + i)
				Var8004 = 2;
	}
	Free(buf);
}
#endif
