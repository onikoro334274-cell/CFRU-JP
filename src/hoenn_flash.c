#include "defines.h"

/*
hoenn_flash.c
    明るさの段階をエメラルドと同じ 9 段に（2026-09-29 ユーザー「ムロジムの暗闇の仕掛けがない」「ムロジムリーダーに勝つと動けなくなる」）

    ・素 FRLG の明るさの表 sFlashLevelToRadius（0838D658）は 5 段 { 200, 72, 56, 40, 24 }・最大の段 gMaxFlashLevel（0838D664）= 4。
      エメラルドは 9 段 { 200, 72, 64, 56, 48, 40, 32, 24, 0 }・最大 8（pokeemerald src/field_screen_effect.c:53）。
    ・ムロジムはトレーナーを倒した数で段 7〜1 を setflashlevel する。素 FRLG の SetFlashLevel は 4 をこえると 0 にするので暗くならず、
      勝ったあとの animateflash 0（全部明るく）が「半径 200 → 200」になる。素 FRLG の UpdateFlashLevelEffect は
      「半径が目標を**こえたら**終わり」（明るくする向きしか想定していない）なので、同じ半径からは逆向きに減り続けて終わらない → 止まる。
    → 表をエメラルドの 9 段にして（repoints 0807E7A0 / 0807E7DC）、最大を 8 に（bytereplacement 0838D664）。
      ★ 一番暗い段（8）はエメラルドの 0 ではなく素 FRLG と同じ 24: カントーの暗い洞窟は SetDefaultFlashLevel で最大の段を使うので、
        見た目を変えない（エメラルドでも洞窟は requires_flash の地図だけ）
*/

const u16 gHoennFlashLevelToRadius[9] = { 200, 72, 64, 56, 48, 40, 32, 24, 24 };
