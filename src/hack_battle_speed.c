#include "defines.h"
#include "../include/event_data.h"
#include "../include/main.h"
#include "../include/palette.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "../include/text.h"
#include "../include/battle.h"
#include "../include/constants/battle.h"
#include "hack_battle_speed.h"

/*
    たいせんそくど（戦闘の倍速 1〜4 倍）— 2026-10-09 ユーザー「後設定スクロールできない？」→ 増やす項目「たいせんそくど」。
    資料: docs/research/2026-10-09_battle-speed.md

    素 FRLG の BattleMainCB2（080109C0）を functionrewrites で丸ごとこれに置き換える（番地は同じ＝
    `callback2 == BattleMainCB2` と比べている所（src/evolution.c など）はそのまま効く）。
    中身は素と同じ（スプライト・OAM・文字・パレットのフェード・タスク・ポケだんじの B で抜ける）で、
    そのあとに「callback1（BattleMainCB1＝戦いの進行と各陣営の操作）と同じ中身」を余分に回す。
      - 文字・HP バー・技のアニメ・待ちのコマ数は、回した数だけ速くなる
      - 曲・効果音・鳴き声は本物の速さ（それを待つ所はそのぶん待つ）
      - VBlank の仕事（OAM・パレットの転送・BG の位置）は 1 フレームに 1 回のまま＝途中のコマが飛ぶだけ
    余分に回さないとき: 通信の戦い（相手と 1 フレームずつそろえる）／途中で callback が変わった（バッグ・手持ちの画面・戦いの終わり）。

    設定は変数 HACK_VAR_BATTLE_SPEED（0x50F7・もとミラクル交換の回数＝前のセーブに数が残っているので、上 8 ビットが
    0xBA のときだけ下 8 ビットを倍速 0〜3 として読む・ほかは 1 倍）。せってい画面（src/bw_option_menu.c）が書く。
*/

#define BATTLE_SPEED_MAX 3                                               /* 4 倍 = 余分に 3 回 */
#define BATTLE_SPEED_TAG 0xBA00
#define BattleMainCB2_ ((MainCallback) (0x080109C0 | 1))                 /* この関数の入口（functionrewrites） */
#define CB2_QuitPokedudeBattle_ ((MainCallback) (0x08010A7C | 1))        /* 素の BattleMainCB2 の literal pool（08010A30）*/

u16 HackBattleSpeed_Get(void)
{
    u16 v = VarGet(HACK_VAR_BATTLE_SPEED);

    if ((v & 0xFF00) != BATTLE_SPEED_TAG || (v & 0xFF) > BATTLE_SPEED_MAX)
        return 0;
    return v & 0xFF;
}

void HackBattleSpeed_Set(u16 speed)
{
    VarSet(HACK_VAR_BATTLE_SPEED, BATTLE_SPEED_TAG | (speed > BATTLE_SPEED_MAX ? 0 : speed));
}

static u8 ExtraIterations(void)
{
    if (gBattleTypeFlags & (BATTLE_TYPE_LINK | BATTLE_TYPE_TOWER_LINK_MULTI))
        return 0;
    return HackBattleSpeed_Get();
}

/* 素の BattleMainCB2 の中身（pokefirered の battle_main.c と同じ）。
   ★ 2026-10-09 ユーザー「たいせんそくど」を2倍以上にすると「グラスメイカー」「ミストメイカー」等のフィールドを変更する特性が
     発動した時にフリーズする」: 余分に回すときは UpdatePaletteFade を呼ばない（fade = FALSE）。
     素のハードウェアのフェード（BeginHardwarePaletteFade・フィールドの背景の切り替え Task_FadeToBg など）は、目標に届くと
     「終わりかけ」の 1 ビット（gPaletteFade.hardwareFadeFinishing・08070AA4）を呼ぶたびに +1 し（1 ビットなので反転）、
     VBlank の UpdateBlendRegisters がそれを見て active を下ろす。1 フレームに偶数回呼ぶと VBlank のときいつも 0 で終わらない。
     ふつうのフェードは sPlttBufferTransferPending で 1 フレーム 1 回にしか進まないので、呼ばなくても速さは変わらない。 */
static void BattleMainCB2_Body(bool8 fade)
{
    AnimateSprites();
    BuildOamBuffer();
    RunTextPrinters();
    if (fade)
        UpdatePaletteFade();
    RunTasks();

    if (JOY_HELD(B_BUTTON) && (gBattleTypeFlags & BATTLE_TYPE_POKE_DUDE))
    {
        gSpecialVar_LastResult = gBattleOutcome = B_OUTCOME_DREW;
        ResetPaletteFadeControl();
        BeginNormalPaletteFade(0xFFFFFFFF, 0, 0, 16, RGB_BLACK);
        SetMainCallback2(CB2_QuitPokedudeBattle_);
    }
}

void HackBattleSpeed_BattleMainCB2(void)
{
    u8 extra, i;

    u16 newKeys, newRaw, newRepeat;

    BattleMainCB2_Body(TRUE);
    extra = ExtraIterations();
    if (extra == 0)
        return;

    /* ★ 余分に回す間は「押したばかり」のボタンを空に（1 回の A が 2 回効かないように）。押しっぱなしは残す */
    newKeys = gMain.newKeys;
    newRaw = gMain.newKeysRaw;
    newRepeat = gMain.newAndRepeatedKeys;
    gMain.newKeys = 0;
    gMain.newKeysRaw = 0;
    gMain.newAndRepeatedKeys = 0;
    for (i = 0; i < extra; ++i)
    {
        MainCallback cb1 = gMain.callback1;

        if (gMain.callback2 != BattleMainCB2_ || cb1 == NULL)
            break;
        cb1();
        if (gMain.callback2 != BattleMainCB2_ || gMain.callback1 != cb1)
            break;
        BattleMainCB2_Body(FALSE);
    }
    gMain.newKeys = newKeys;
    gMain.newKeysRaw = newRaw;
    gMain.newAndRepeatedKeys = newRepeat;
}
