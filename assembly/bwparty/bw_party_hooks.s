@ BW 風の手持ち画面（2 列 3 段）のフック。根拠は docs/research/2026-09-19_bw-party-menu.md
@
@   hooks の行:
@     BwPmSlotsOffscreenHook 08123A94 3    Task_SlideSelectedSlotsOffscreen の「2 つとも画面の外へ出たか」
@
@ ★ 素は「左端の位置 > 33」を u16 で見る。右へ滑る枠にはそれで足りるが、左へ滑る枠は 1 歩目で負になり
@   65535 > 33 で「出た」扱いになる。素は左へ滑る枠が 1 つ（左の大枠）だけで、右の枠を待つあいだに
@   左の枠も流れて見えていた。こちらは 2 列なので、左の列どうし（1 匹目と 3 匹目など）を入れ替えると
@   1 歩で終わって絵が動かない（実機 2026-09-20）。C の BwPmBothSlotsOffscreen が
@   「右へ: 左端 > 33 / 左へ: 左端 + 幅 <= 0」で判定する。
@
@   08123A94 の 8 バイト: MOV r0,r1 / LDRH r0,[r0] / CMP r0,#0x21 / BLS 08123B3E
@   潰した続き（08123A9C〜08123AA2）は 2 つ目の比較なので飛ばしてよい。
@   r6 = gTasks[taskId].data（関数の頭からずっと生きている）。r3 は直前の ADD で使い終わっている。
@   lr は PUSH {r4-r7,lr} で積んであるので BL で潰してよい。

    .arch armv4t
    .text
    .align 2
    .thumb

    .thumb_func
    .global BwPmSlotsOffscreenHook
BwPmSlotsOffscreenHook:
    mov     r0, r6
    bl      BwPmBothSlotsOffscreen
    cmp     r0, #0
    beq     BwPmSlotsStillSliding
    ldr     r0, =(0x08123AA4 + 1)       @ 2 つとも外: 向きを反転して入れ替える
    bx      r0
BwPmSlotsStillSliding:
    ldr     r0, =(0x08123B3E + 1)       @ まだ: 関数の後始末（次のフレームへ）
    bx      r0
    .align 2
