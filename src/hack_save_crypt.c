#include "defines.h"
#include "../include/save.h"
#include "hack_save_crypt.h"

/*
    セーブのかき混ぜ（2026-10-09 ユーザー「ポケモンIDとか変えてほしい　PokeHexとかで連れて行かれないように」→
    「CFRUとも互換なくすように　1やな」＝セーブの中身を PokemonStart だけの鍵でかき混ぜる）。
    資料: docs/research/2026-10-09_save-crypt.md

    フラッシュに書く区画（4KB）を丸ごと、区画の中の位置で決まる鍵（HackSaveCrypt_KeyAt・tools/save_editor/savecrypt.py と同じ式）で
    XOR してから書き、読むときに戻す。本体の組・棚のページ・40〜50 箱・追加の区画・ぼうけんのしょ 2（バンク +48）すべてに効く。
      書く: 素の ProgramFlashSectorAndVerify（081C2C50・functionrewrites）をこれに。書く前に XOR、書き終えたら元に戻す（呼んだ側のバッファは変えない）
      読む: 素の DoReadFlashWholeSection（080DB178・functionrewrites）と、棚・追加の区画が直に呼んでいた ReadFlash を HackSaveCrypt_ReadFlash に
    前の版のセーブ（かき混ぜていない区画）は、区画の終わりの署名（+0xFF8 = 0x08012025）がそのまま読めるかで見分けてそのまま読む
    ＝引っ越しは要らない（次のレポートでかき混ぜて書く）。消えたままの区画（FF）は混ぜたものとして戻すので、署名は合わない（前と同じく「無い」）。
    かき混ぜないまま読むもの: 書き損じの画面の消去の確かめ（素の ReadFlash のまま）・ぼうけんのしょの 6MB 版の確かめ（hack_save_slot.c・読み比べるだけ）。
*/

#define SECTOR_SIZE 0x1000
#define SIG_OFFSET 0xFF8
#define FILE_SIG 0x08012025
#define CRYPT_SEED 0x504B5354u

#define ReadFlashVanilla ((void (*)(u16, u32, void*, u32)) (0x081C2A54 | 1))
#define VerifyFlashSectorVanilla ((u32 (*)(u16, u8*)) (0x081C2B20 | 1))
#define ProgramFlashSectorPtr (*(u16 (**)(u16, void*)) 0x0300746C)

u8 HackSaveCrypt_KeyAt(u32 i)
{
	u32 x = i * 0x9E3779B1u + CRYPT_SEED;
	x ^= x >> 15;
	x *= 0x2C1B3C6Du;
	x ^= x >> 12;
	x *= 0x297A2D39u;
	x ^= x >> 15;
	return x >> 24;
}

static void Crypt(u8* p, u32 off, u32 n)
{
	u32 i;
	for (i = 0; i < n; ++i)
		p[i] ^= HackSaveCrypt_KeyAt(off + i);
}

static bool8 IsPlainSector(u16 sector)
{
	u32 sig;
	ReadFlashVanilla(sector, SIG_OFFSET, &sig, sizeof(sig));
	return sig == FILE_SIG;
}

void HackSaveCrypt_ReadFlash(u16 sector, u32 off, void* dst, u32 n)
{
	bool8 plain = IsPlainSector(sector);
	ReadFlashVanilla(sector, off, dst, n);
	if (!plain)
		Crypt((u8*) dst, off, n);
}

//素の DoReadFlashWholeSection（080DB178）の代わり
u8 HackSaveCrypt_DoReadFlashWholeSection(u8 sector, struct SaveSection* section)
{
	HackSaveCrypt_ReadFlash(sector, 0, section, SECTOR_SIZE);
	return TRUE;
}

//素の ProgramFlashSectorAndVerify（081C2C50）の代わり（3 回まで書いて確かめる・0 なら書けた）
u32 HackSaveCrypt_ProgramFlashSectorAndVerify(u16 sector, u8* src)
{
	u32 result = 0;
	u8 tries;

	Crypt(src, 0, SECTOR_SIZE);
	for (tries = 0; tries < 3; ++tries)
	{
		result = ProgramFlashSectorPtr(sector, src);
		if (result != 0)
			continue;
		result = VerifyFlashSectorVanilla(sector, src);
		if (result == 0)
			break;
	}
	Crypt(src, 0, SECTOR_SIZE);
	return result;
}
