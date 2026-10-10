@ BW 風 HP バーのフック 2 本（やりたいこと #17）。
@ 元: 参考/BW-hpbars-master/src/caller.s と hp_nums_hook.s（BPRE 用）を
@ BPRJ の番地に直したもの。根拠は docs/research/2026-09-19_bw-hpbars-port.md。
@
@   hooks の行:
@     BwHpbarDrawHook  08049680 3   UpdateHealthboxAttribute(08049674) + 0x0C
@     BwHpbarHpNumHook 08047B08 3   UpdateHpTextInHealthbox の先頭
@
@ ★ 08049680 は前置き（PUSH / MOV x3 / PUSH / ADD sp,#-4）の直後。
@   BwHpbarDrawElements が 1 を返したら（描いた）素の後始末 080498A2 へ、
@   0 を返したら（サファリなど素に任せる）フックが潰した 8 バイト
@   （MOV r8,r1 / LSL r0 / LSR r6 / LSL r2）をやり直して素の続き 08049688 へ戻る。
@   r0=スプライト / r1=ポケモン / r2=要素 はフック時点で生きているので、
@   C を呼ぶ前に積んでおき、素へ戻すときに戻す（6.14 節）。
@ ★ 飛び先には r3 を使う。r3 はフックの踏み台が既に潰している caller-saved で、
@   素の続きが r4 を読む前提を壊さない。
@ ★ 08047B08 は関数の先頭。戻らないので潰した 8 バイトはやり直さない。
@ ★ どちらも潰す範囲の途中へ飛び込む分岐が無いことを確認済み
@   （tools/bwhp/verify_bw_hpbar.py が毎回見る）。

    .arch armv4t
    .text
    .align 2
    .thumb

    .thumb_func
    .global BwHpbarDrawHook
BwHpbarDrawHook:
    push    {r0, r1, r2}
    bl      BwHpbarDrawElements
    cmp     r0, #0
    beq     BwHpbarDrawHook_Vanilla
    add     sp, #12
    ldr     r3, =(0x080498A2 + 1)      @ 素の後始末へ
    bx      r3
BwHpbarDrawHook_Vanilla:
    pop     {r0, r1, r2}
    mov     r8, r1                      @ 08049680 の 8 バイトのやり直し
    lsl     r0, r0, #24
    lsr     r6, r0, #24
    lsl     r2, r2, #24
    ldr     r3, =(0x08049688 + 1)      @ 素の続きへ
    bx      r3
    .align 2

    .thumb_func
    .global BwHpbarHpNumHook
BwHpbarHpNumHook:
    push    {lr}
    bl      BwHpbarUpdateHpNums
    pop     {pc}
    .align 2
