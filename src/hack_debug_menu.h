#pragma once

/* デバッグメニュー（src/hack_debug_menu.c・pm-start/debug-menu の書き直し・build_rom.py --debug-menu で -DHACK_DEBUG_MENU のときだけ）*/

#define HACK_FLAG_DEBUG_CATCH 0xF00     /* 捕獲 100% の ON/OFF（予約フラグ・5e の台帳 2026-10-11・セーブに残る）*/
/* 2026-10-12 切り替え 4 つ（5e の台帳 0xF0E〜0xF15・0xF12〜0xF15 は予備）。セーブに残るが、読むのはデバッグ版（HACK_DEBUG_MENU）だけ */
#define HACK_FLAG_DEBUG_NOWILD     0xF0E   /* やせいなし（草むら・水・岩・つり・あまいかおり・ずかんナビ）*/
#define HACK_FLAG_DEBUG_INVINCIBLE 0xF0F   /* むてき（主人公の側がダメージを受けない）*/
#define HACK_FLAG_DEBUG_OHKO       0xF10   /* いちげき（主人公の側の技で相手の HP を全部）*/
#define HACK_FLAG_DEBUG_NOTRAINER  0xF11   /* トレーナーむし（視線で戦いが始まらない）*/

#ifdef HACK_DEBUG_MENU
void HackDebugMenu_TryOpen(void);   /* B＋Select（src/hack_network_trade.c の NetTrade_CB1 から）*/
bool8 HackDebugMenu_CatchAll(void);
bool8 HackDebugMenu_NoWild(void);                 /* wild_encounter.c・dexnav.c（patches/0342）*/
bool8 HackDebugMenu_NoTrainer(void);              /* overworld.c の CheckForTrainersWantingBattle */
void HackDebugMenu_AdjustDamage(u8 bank);         /* general_bs_commands.c の atk0C_datahpupdate */
#endif
