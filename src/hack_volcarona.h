#pragma once
#include "defines.h"

/* 特別なウルガモス（#95・2026-10-11 ユーザー提案・docs/research/2026-10-11_volcarona-sun-avatar.md）
   1 匹だけ: 種族ウルガモス＋印（MON_DATA_OBEDIENCE・親が主人公＝HackMark_IsPlayersMarkedMon）。
   - 1 つ目の特性 たいようのけしん（ABILITY_SUN_AVATAR 323）: ひでり＋もらいび＋ほのおのからだ（src/ability_battle_effects.c ほかの判定に OR）
   - 2 つ目の特性 たいようのちょう（ABILITY_SUN_BUTTERFLY 324・Lv50 から・src/second_ability.c）: 晴れのとき 晴れを保ち とくこう・すばやさ 1.5 倍
   - 技 えんぶちょう（MOVE_ENBUCHO 0x3E9・EFFECT_ENBUCHO 251）はこの個体だけが覚える
   - えんとつ山の山頂（広場の右の端 (30, 15)）の人から 1 回（HACK_FLAG_VOLCARONA）・ミラクル交換に出せない（src/hack_wonder_trade.c） */
#define HACK_FLAG_VOLCARONA 0x12C9     //もらった（build_hoenn_scripts.py の HOENN_FLAG_END の下・5e 2026-10-11）
#define VOLCARONA_GIFT_LEVEL 50

bool8 HackVolcarona_IsSpecial(const struct Pokemon* mon);
u16 HackVolcarona_SpecialAbility(const struct Pokemon* mon, u16 species);   //1 つ目（GetMonAbility から）
u16 HackVolcarona_SecondAbility(u16 species, const struct Pokemon* mon);    //2 つ目（second_ability.c から）
bool8 HackVolcarona_ButterflyBoost(u8 bank);                               //晴れで 2 つ目が働いている（とくこう 1.5 倍・src/damage_calc.c）
u32 HackVolcarona_BoostSpeed(u8 bank, u32 speed);                          //すばやさ 1.5 倍（src/battle_start_turn_start.c）
bool8 HackVolcarona_HoldsSun(void);                                        //場に 2 つ目が働く子がいれば 晴れのターンを減らさない（src/end_turn.c）
void HackVolcarona_Give(void);                                             //callasm: もらう（LASTRESULT = GiveMonToPlayer）
