#include "defines.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/palette.h"
#include "../include/sprite.h"
#include "../include/task.h"

/*
 * タイトル画面のガブリアスが咆哮をくり返す（2026-10-09・ユーザー「タイトル画面でつかう口閉じて開ける」→
 * 「ずっとパクパク」→ mGBA で見て「口パクパクじゃなくて咆哮している感じを繰り返してほしい」）。
 *
 * 絵は tools/title/prep_mouth.py → art/mon.png（閉じた口）・art/mon_open.png（開いた口）→ build_title_screen.py。
 * 開いた口で違うマス（29 マスほど）のタイルを BG1 のタイルに足してあり、gHackTitleMouthCells に
 * 「数・（マス・閉じた口の語・開いた口の語）× 数」を書き出す。
 *
 * 1 回の咆哮: 口を閉じて溜める（ROAR_REST）→ 口を開けて吼える（ROAR_OPEN）。吼え始めは BG1（ガブリアスだけ・
 * ロゴと集中線は動かない）を細かく震わせ、ROAR_SHAKE フレームかけて弱めて止める。
 *
 * 呼ばれ方: 素の CB2_TitleScreenRun（08078334・RunTasks / AnimateSprites / BuildOamBuffer / UpdatePaletteFade）を
 * 指す語は ROM に 1 つだけ（CB2_InitTitleScreen の 080782AC）。そこを repoints でこの関数へ向け、同じ 4 つを呼んでから動かす。
 * 状態は持たない（6.13）。今どこかは gMain.vblankCounter2 で決める（6.21・vblankCounter1 は素 FRLG ではポインタ）。
 * BG1 のずれは素のタイトル画面では最初に 0 にするだけ（ResetGpuRegs）なので、ここで毎フレーム書いてよい。
 */

#define TITLE_BG1_MAP ((vu16 *)(VRAM + 30 * 0x800))   // 素の sBgTemplates: BG1 は mapBaseIndex 30
#define ROAR_REST  60      // 口を閉じて溜める
#define ROAR_OPEN  90      // 口を開けて吼える
#define ROAR_SHAKE 48      // 吼え始めに震わせる長さ（ROAR_OPEN 以下）
#define ROAR_SHAKE_MAX 3   // 震えの幅（画素）

extern const u16 gHackTitleMouthCells[];

static void SetMouth(bool32 open)
{
    const u16 *cells = gHackTitleMouthCells;
    u32 count = cells[0];
    u32 i;

    cells++;
    if (TITLE_BG1_MAP[cells[0]] == cells[open ? 2 : 1])   // 1 つ目のマスがもうその口なら書かない
        return;
    for (i = 0; i < count; i++, cells += 3)
        TITLE_BG1_MAP[cells[0]] = cells[open ? 2 : 1];
}

void HackTitleMouth_CB2Run(void)
{
    u32 t = gMain.vblankCounter2 % (ROAR_REST + ROAR_OPEN);
    s32 dx = 0, dy = 0;

    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();

    if (t < ROAR_REST)
    {
        SetMouth(FALSE);
    }
    else
    {
        t -= ROAR_REST;
        SetMouth(TRUE);
        if (t < ROAR_SHAKE)
        {
            s32 amp = 1 + (ROAR_SHAKE_MAX - 1) * (s32)(ROAR_SHAKE - t) / ROAR_SHAKE;   // 3 → 1 へ弱める
            dx = (t & 2) ? amp : -amp;          // 2 フレームごとに左右・上下を入れ替える
            dy = (t & 4) ? amp / 2 : -(amp / 2);
        }
    }
    SetGpuReg(REG_OFFSET_BG1HOFS, (u16)dx);
    SetGpuReg(REG_OFFSET_BG1VOFS, (u16)dy);
}
