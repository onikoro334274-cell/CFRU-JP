@ 連れ歩き（#16）のフック 1 本。根拠は docs/research/2026-09-20_follower-charizard.md
@
@   hooks の行:
@     BwFollowerTemplateHook 0805F61C 3   GetEventObjectTemplateByLocalIdAndMap の入口
@
@ ★ CFRU の CreateFollowerAvatar は
@     clone = *GetEventObjectTemplateByLocalIdAndMap(gFollowerState.map.id, 番号, 群);
@   で**地図に置いてある NPC の枠**を写す。ポケモンの連れ歩きは地図に実体が無いので、
@   localId が 0xFE（DEFAULT_FOLLOWER_LOCAL_ID）のときだけ、こちらの固定の枠を返す。
@   素の localId は 1 から数十なので 0xFE とぶつからない。
@
@ ★ 0805F61C は 4 の倍数なので潰れるのは 8 バイト＝ 4 命令（6.14）:
@     B510 PUSH {r4,lr} / 0600 LSL r0,r0,#24 / 0E04 LSR r4,r0,#24 / 0609 LSL r1,r1,#24
@   やり直して 0805F624 へ戻す。r3 は引数（r0〜r2）でないので踏み台に使ってよい。

    .arch armv4t
    .text
    .align 2
    .thumb

    .thumb_func
    .global BwFollowerTemplateHook
BwFollowerTemplateHook:
    cmp     r0, #0xFE
    beq     BwFollowerOwnTemplate
    push    {r4, lr}
    lsl     r0, r0, #0x18
    lsr     r4, r0, #0x18
    lsl     r1, r1, #0x18
    ldr     r3, =(0x0805F624 + 1)
    bx      r3

BwFollowerOwnTemplate:
    ldr     r0, =gBwFollowerTemplate
    bx      lr
    .align 2

@ ============================================================================
@ 連れ歩きに話しかけたときのスクリプト（XSE のバイト列）
@
@ ★ C の配列では書けない。`callnative` に渡すのは**関数の番地 + 1**（Thumb の印）で、
@   C の初期化式では「関数の番地に 1 を足したもの」を定数として置けないため。
@
@ 命令は素 FRLG のもの（`参考/pokefirered/asm/macros/event.inc` で確認）:
@   6A       lock
@   5A       faceplayer
@   23 ....  callnative <関数>      … C の関数を呼ぶだけ（引数なし）
@   0F 00 .. loadword 0, <文章>     … 文章の番地を 0 番の枠へ
@   09 04    callstd MSGBOX_DEFAULT … 素の標準スクリプト 4 番（message/waitmessage/waitbuttonpress）
@   6C       release
@   02       end
@
@ ★ gStringVar4 は 0x02021C88（BPRJ.ld）。文章は BwFollowerTalk が組み立てる。
@ ★ 中の 4 バイトは境界に合っていなくてよい（スクリプトの機械は 1 バイトずつ読む）。

    .global gBwFollowerScript
gBwFollowerScript:
    .byte   0x6A                        @ lock
    .byte   0x5A                        @ faceplayer
    .byte   0x23                        @ callnative
    .4byte  BwFollowerTalk + 1
    .byte   0x0F, 0x00                  @ loadword 0,
    .4byte  0x02021C88                  @   gStringVar4
    .byte   0x09, 0x04                  @ callstd MSGBOX_DEFAULT
    .byte   0x6C                        @ release
    .byte   0x02                        @ end
    .align 2
