@ B2W2 風バッグのフック 1 本。根拠は docs/research/2026-09-19_bw-bag.md
@
@   hooks の行:
@     BwBagSpritePalHook 08108E44 0    DoLoadBagGraphics 相当の、スプライトのパレットを読む所
@
@ ★ 08108E44 の 8 バイトは LDR r0,=0839C6FC / BL LoadCompressedSpritePalette / B 08108E56。
@   丸ごと差し替えて、男女で選んだパレットを読んでから素の続き 08108E56 へ戻る。
@   r0 は直後に上書きされる予定のレジスタなので踏み台に使ってよい。
@   lr は関数の前置きで積んであり（POP {r1} / BX r1 で戻る）、BL で潰してよい。

    .arch armv4t
    .text
    .align 2
    .thumb

    .thumb_func
    .global BwBagSpritePalHook
BwBagSpritePalHook:
    bl      BwBagLoadSpritePalette
    ldr     r0, =(0x08108E56 + 1)
    bx      r0
    .align 2
