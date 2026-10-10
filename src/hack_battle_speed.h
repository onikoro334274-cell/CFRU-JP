#pragma once

//たいせんそくど（src/hack_battle_speed.c）。値は 0 = 1 倍 … 3 = 4 倍
#define HACK_VAR_BATTLE_SPEED 0x50F7   //tools/rom/build_hoenn_scripts.py の RESERVED_VARS（もとミラクル交換の回数）

u16 HackBattleSpeed_Get(void);
void HackBattleSpeed_Set(u16 speed);
