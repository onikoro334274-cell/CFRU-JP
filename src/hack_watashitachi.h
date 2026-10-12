#pragma once

/* 隠しトレーナー「わたしたち」10 人（#99・src/hack_watashitachi.c）。番号は 5e の台帳（2026-10-11） */

#define WATASHI_COUNT          10
#define WATASHI_TRAINER_FIRST  940     /* トレーナー 940〜949（tools/kanto/kanto_trainer_ids.json） */
#define WATASHI_FLAG_FIRST     0xF01   /* 初めて勝った印 0xF01〜0xF0A（実績 106〜115 の条件） */
#define WATASHI_BASE_BONUS     50      /* 種族値 6 つとも +50（2026-10-12 ユーザー「種族値計算＋50で」・前は +20・手持ち全員・相棒の +20 は hack_story_partner.c の PARTNER_BONUS で別） */

#define WATASHI_FLAG_HIDE      0xF0D   /* 10 人の人の表の「隠す旗」（5e の台帳 2026-10-12）。HackWatashi_FieldTick が保つ */
#define WATASHI_FLAG_UNLOCK    0x13EB  /* 戦える条件＝バトルフロンティアへ行ける（HOENN_FLAG_FRONTIER_INVITED・エニシダの招待・
                                          ミナモの船の行き先にフロンティアが出る旗＝assembly/hoenn_islands/islands_scripts.s）。ユーザー 2026-10-12 */

void HackWatashi_FieldTick(void);
void HackWatashi_ShowFace(void);
void HackWatashi_HideFace(void);
u8 HackWatashi_BaseBonus(const struct Pokemon* mon);
