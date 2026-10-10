#pragma once

//セーブのかき混ぜ（src/hack_save_crypt.c）。フラッシュから読むときはこれを通す（素の ReadFlash を直に呼ばない）
u8 HackSaveCrypt_KeyAt(u32 i);
void HackSaveCrypt_ReadFlash(u16 sector, u32 off, void* dst, u32 n);
