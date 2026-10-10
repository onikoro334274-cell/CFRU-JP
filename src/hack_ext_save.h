#pragma once

//追加のセーブ領域（src/hack_ext_save.c・直した mGBA だけ）。区画 n は 0〜735
bool8 HackExtSave_Available(void);
bool8 HackExtSave_ReadSector(u16 n, void* dst);
bool8 HackExtSave_Read(u16 n, u32 off, void* dst, u32 size);
bool8 HackExtSave_WriteSector(u16 n, const void* src);
