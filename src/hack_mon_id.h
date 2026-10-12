#pragma once
#include "../include/global.h"

/* ポケモン自身の表 ID・裏 ID（#70・src/hack_mon_id.c） */
u32 HackMonId_Get(const struct BoxPokemon* mon);                 //表 ID | 裏 ID << 16（0 = まだ無い）
void HackMonId_Set(struct BoxPokemon* mon, u32 id);
void HackMonId_Ensure(struct BoxPokemon* mon);
void HackMonId_MakeMoemon(struct BoxPokemon* mon);
void HackMonId_MakeShiny(struct BoxPokemon* mon);                //性格値は触らず自身の ID を色違いの当たりに（ホウエンのミラクル交換・2026-10-11）               //今の色違いのまま裏 ID を Moémon の当たりに（2026-10-10）                   //無ければ今の色違いのまま付ける
void HackMonId_SetShiny(struct BoxPokemon* mon, bool8 on);       //デバッグメニューだけ（2026-10-12・性格値は触らない）
void HackMonId_SetMoemon(struct BoxPokemon* mon, bool8 on);
bool8 HackMonId_IsShinyBox(const struct BoxPokemon* mon);
bool8 HackMonId_IsMoemonHit(const struct BoxPokemon* mon);         //第 2 段: 萌えもんの当たり（裏 ID・ON/OFF は見ない）
u32 HackMonId_MoemonKey(u32 personality);
//ボックスの縮めた 58 バイト（src/pokemon_storage_system.c が使う）
u32 HackMonId_UnpackComp(const void* comp);
void HackMonId_PackComp(void* comp, u32 id);
void HackMonId_ClearSpare(struct BoxPokemon* mon);
void HackMonId_FieldTick(void);
