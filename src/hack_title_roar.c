#include "defines.h"
#include "../include/bg.h"
#include "../include/gpu_regs.h"
#include "../include/main.h"
#include "../include/decompress.h"
#include "../include/palette.h"
#include "../include/sprite.h"
#include "../include/task.h"

/*
 * タイトル画面のガブリアスが咆哮する（14 コマ・2026-10-11 ユーザー「タイトル画面ガブリアスアニメーション.zip
 * これらいい感じにガッチャンコしてタイトルお願い 咆哮している感じね」）。2026-10-09 の口の開け閉め（src/hack_title_mouth.c・
 * 29 マスの語の差し替え）をこれに置き換えた。
 *
 * 絵は tools/title/prep_roar.py → art/roar/f00〜f13.png → build_title_screen.py が 1 コマずつタイル（LZ77）とマップ（LZ77）に
 * して gHackTitleRoarFrames に並べ、並び（GIF と同じ 36 歩）を gHackTitleRoarSeq に、数を gHackTitleRoarInfo に書く。
 *
 * BG1 は文字 1（VRAM 0x4000〜・タイル 512 枚）だが 4bpp のマップはタイル番号を 1023 まで持てるので、
 * 文字 2（0x8000〜・タイル 512〜1023）の BG2（PUSH START・TEXT_TILES_MAX 64 枚）の後ろ＝タイル BANK_B_TILE（576）〜 を
 * 2 つ目の置き場にする。今見せている置き場の裏に次のコマを展開しておき、歩が変わるフレームの VBlank の中でマップを差し替える。
 *
 * 呼ばれ方: 素の CB2_TitleScreenRun（08078334）を指す語（CB2_InitTitleScreen の 080782AC・ROM に 1 つ）を repoints で
 * この関数へ。メインの処理は VBlankIntrWait のあとに呼ばれるので、最初にする VRAM の書き換えは VBlank の中に収まる
 * （マップ 0x500 バイトの展開と 640 語の足し算）。裏の置き場への展開（9KB の LZ77・約半フレーム）はそのあと＝描画中でも見えない。
 *
 * 状態は持たない（6.13）。今どこかは gMain.vblankCounter2 で決める（6.21）。どの置き場に何コマ目が入っているかは
 * BG1 のマップの見えない行（25 行目・画面は 20 行・震えは 3 画素まで）に書いておく（タイトルの初期化が VRAM を 0 で埋める＝
 * DmaFill16 08078174 なので最初は 0 ＝「まだ」）。0 コマ目は素の読み込み（gHackTitleMonTiles = 0 コマ目）で置き場 A に入っているが、
 * 印は 0 なので最初のフレームで同じものを展開し直す（そのときはまだパレットが黒）。
 */

#define TITLE_BG1_MAP   ((vu16 *)(VRAM + 30 * 0x800))   // 素の sBgTemplates: BG1 は mapBaseIndex 30
#define TITLE_BG1_TILES ((u8 *)(VRAM + 0x4000))         // 文字 1
#define MARK_ROW        25                               // 印を書くマップの行（画面の外・震えの分を見ても出ない）
#define MARK_BANK(b)    (TITLE_BG1_MAP[MARK_ROW * 32 + (b)])        // 置き場 b に入っているコマ + 1（0 = まだ）
#define MARK_SHOWN      (TITLE_BG1_MAP[MARK_ROW * 32 + 2])          // 見せているコマ + 1
#define MARK_SHOWN_BANK (TITLE_BG1_MAP[MARK_ROW * 32 + 3])          // 見せている置き場 + 1
#define TITLE_BLINK_TASK ((TaskFunc)(0x08078BB0 | 1))                // 素の Task_TitleScreen_BlinkPressStart（JP・CreateTask の語 080787C0 ほか 4 か所）

struct HackTitleRoarFrame
{
    const u8 *tiles;   // LZ77（タイル 0 から）
    const u8 *map;     // LZ77（0x500 バイト・置き場 A の番号・パレット 13）
    u8 shake;          // 震えの幅（画素・0 = 震えない）
    u8 pad[3];
};

struct HackTitleRoarInfo
{
    u16 frameCount;
    u16 stepCount;
    u16 stepLen;       // 1 歩のフレーム数
    u16 bankBTile;     // 2 つ目の置き場の先頭のタイル
    const struct HackTitleRoarFrame *frames;
    const u8 *seq;     // 歩 → コマ
};

extern const struct HackTitleRoarInfo gHackTitleRoarInfo;

static void LoadTiles(u32 frame, u32 bank)
{
    const struct HackTitleRoarInfo *info = &gHackTitleRoarInfo;
    u32 base = bank ? info->bankBTile : 0;

    LZ77UnCompVram(info->frames[frame].tiles, TITLE_BG1_TILES + base * 32);
    MARK_BANK(bank) = frame + 1;
}

static void LoadMap(u32 frame, u32 bank)
{
    const struct HackTitleRoarInfo *info = &gHackTitleRoarInfo;
    u32 i;

    LZ77UnCompVram(info->frames[frame].map, (void *)TITLE_BG1_MAP);
    if (bank)
    {
        u32 base = info->bankBTile;
        for (i = 0; i < 640; i++)
            TITLE_BG1_MAP[i] += base;   // 透明（タイル 0）も置き場 B の 0 番（空のタイル）へ。番号は 1023 をこえない（build_title_screen.py）
    }
    MARK_SHOWN = frame + 1;
    MARK_SHOWN_BANK = bank + 1;
}

void HackTitleRoar_CB2Run(void)
{
    const struct HackTitleRoarInfo *info = &gHackTitleRoarInfo;
    //★ 2026-10-11 同じ数の / と % を並べて書かない。GCC は 1 回の __aeabi_uidivmod にまとめるが、CFRU の
    //  assembly/thumb_compiler_helper.s のそれは余りを r0 と r1 の両方に返す（商を返さない）ので、step に余りが入って
    //  コマがでたらめに選ばれ、2 つの置き場の絵が混ざっていた（ユーザー「タイトル画面バグっている」・mGBA で印を読んで確かめた）。
    //  step は割り算だけ（__aeabi_uidiv）・phase は引き算で出す
    u32 t = gMain.vblankCounter2 % (info->stepCount * info->stepLen);
    u32 step = t / info->stepLen;
    u32 phase = t - step * info->stepLen;
    u32 frame = info->seq[step];
    u32 next = info->seq[(step + 1) % info->stepCount];
    u32 bank = step & 1;
    u32 shake = info->frames[frame].shake;
    s32 dx = 0, dy = 0;

    // 1) 今のコマ（VBlank の中）
    if (MARK_BANK(bank) != frame + 1)       // 最初・取りこぼし（裏に展開できていなかった）
        LoadTiles(frame, bank);
    if (MARK_SHOWN != frame + 1 || MARK_SHOWN_BANK != bank + 1)
        LoadMap(frame, bank);

    if (shake)
    {
        dx = (t & 2) ? (s32)shake : -(s32)shake;       // 2 フレームごとに左右・上下を入れ替える
        dy = (t & 4) ? (s32)(shake / 2) : -(s32)(shake / 2);
    }
    SetGpuReg(REG_OFFSET_BG1HOFS, (u16)dx);
    SetGpuReg(REG_OFFSET_BG1VOFS, (u16)dy);

    // 2) 素と同じ 4 つ
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();

    // 3) 次のコマを裏の置き場へ（見せていないので描画中に書いてよい）
    if (phase >= 1 && MARK_BANK(bank ^ 1) != next + 1)
        LoadTiles(next, bank ^ 1);

    // 4) PUSH START の点滅（5e の指摘 2026-10-11）: 素の点滅（Task_TitleScreen_BlinkPressStart 08078BB0・data[1] が 1 のとき
    //    パレット 15 の 1〜5 を 15 番の色にする）は 1 色に溶かす作りなので、板をやめた今は集中線の上に影が残る。
    //    点滅の仕事は素の task の data[1] から読み、BG2 ごと出し入れする（素の palette の書き換えはそのまま＝害は無い）。
    //    この task は出始めの演出（スライドで BG2 を出す hooks 08078B30）のあとの TITLESCREENSCENE_RUN で作られるので、
    //    演出の間に BG2 を出してしまうことは無い。
    {
        u8 taskId = FindTaskIdByFunc(TITLE_BLINK_TASK);

        if (taskId != 0xFF)
        {
            if (gTasks[taskId].data[1])
                HideBg(2);
            else
                ShowBg(2);
        }
    }
}
