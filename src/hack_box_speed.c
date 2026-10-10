#include "defines.h"
#include "../include/main.h"
#include "../include/sprite.h"
#include "../include/task.h"

/*
    ボックスの画面を 2 倍に（2026-10-09 ユーザー「ボックス内での操作早くできる？」→「いつも 2 ばい」）。
    資料: docs/research/2026-10-09_battle-speed.md（同じやり方）

    素 FRLG の CB2_PokeStorage（0808C800・0x1E バイト）を functionrewrites で丸ごとこれに置き換える（番地は同じ）。
    中身は素と同じ（RunTasks・BG の写しの予約を片づける・背景のスクロール・「ボックスをとじる」の点滅・スプライト・OAM）を
    1 フレームに 2 回。カーソルの移動・箱の切り替え・つかむ／おく・メニューの開閉が 2 倍になる。
    ★ 2 回目は「押したばかり」のボタンを空にする（1 回の A が 2 回効かないように・押しっぱなしは残す）。
    ★ 途中で callback2 が変わったら（画面を出る・ステータス画面へ）2 回目は回さない。
*/

#define CB2_PokeStorage_ ((MainCallback) (0x0808C800 | 1))                   /* この関数の入口（functionrewrites） */
#define DoScheduledBgTilemapCopiesToVram_ ((void (*)(void)) (0x080F7810 | 1)) /* 素の CB2_PokeStorage の BL の行き先 */
#define ScrollBackground_ ((void (*)(void)) (0x0808EA04 | 1))
#define UpdateCloseBoxButtonFlash_ ((void (*)(void)) (0x0808F3D8 | 1))
#define BOX_SPEED_EXTRA 1

static void Body(void)
{
    RunTasks();
    DoScheduledBgTilemapCopiesToVram_();
    ScrollBackground_();
    UpdateCloseBoxButtonFlash_();
    AnimateSprites();
    BuildOamBuffer();
}

void HackBoxSpeed_CB2PokeStorage(void)
{
    u16 newKeys, newRaw, newRepeat;
    u32 i;

    Body();
    newKeys = gMain.newKeys;
    newRaw = gMain.newKeysRaw;
    newRepeat = gMain.newAndRepeatedKeys;
    gMain.newKeys = 0;
    gMain.newKeysRaw = 0;
    gMain.newAndRepeatedKeys = 0;
    for (i = 0; i < BOX_SPEED_EXTRA && gMain.callback2 == CB2_PokeStorage_; ++i)
        Body();
    gMain.newKeys = newKeys;
    gMain.newKeysRaw = newRaw;
    gMain.newAndRepeatedKeys = newRepeat;
}
