#pragma once

/* ネット交換（src/hack_network_trade.c・pm-start/enchanted-network-trade の書き直し・patches/0336）*/

#define HACK_VAR_NET_TRADE_TX_LO 0x50D5   /* 取引番号の下 16 ビット（build_hoenn_scripts.RESERVED_VARS・5e の台帳 2026-10-12）*/
#define HACK_VAR_NET_TRADE_TX_HI 0x50D6   /* 取引番号の上 16 ビット */
#define HACK_VAR_NET_TRADE_MARK 0x50D7    /* 印（元のセーブブロック1 + 0x636 の u16 と同じ値）*/
#define HACK_FLAG_NET_TRADE_BUTTON 0xF0C  /* 立っていればフィールドの A＋Select で開く（せっていの「ネットこうかんボタン」・最初は下りている・5e の台帳 2026-10-12）*/

void NetTrade_FieldTick(void);
void NetTrade_CB1(void);   /* 記号で引けるように（tools/rom/verify_debug_menu.py）*/
void NetTrade_BlockReceived(u8 who);
void CB2_NetTradeFromMain(void);
