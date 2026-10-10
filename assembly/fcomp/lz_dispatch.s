@ LZ77 の展開の振り分け役（2026-09-28・DPE の戦闘の絵の F 方式）。docs/research/2026-09-28_dpe-sprite-fcomp.md
@
@ ★ 自前のビルドの道具（tools/build/build_rom.py の patch_lz_stubs）が、BIOS の入口 2 つを書き換えてここへ飛ばす:
@     081C7A8C LZ77UnCompVram:  movs r2,#1 / b 081C7AF0
@     081C7A90 LZ77UnCompWram:  movs r2,#0 / b 081C7AF0
@     081C7AF0（使われていない _call_via_r10 / r11。呼び元 0）: ldr r3,[pc,#0] / bx r3 / .word LzUnCompDispatch|1
@   r2・r3 は LZ77UnComp の引数ではなく、呼ばれた側が壊してよいレジスタ（BIOS も壊す）
@ ★ 頭の 1 バイトが F の印 0x46 のときだけ DpeFTryDecode（src/fcomp_dpe.c）。不正な F データも BIOS には渡さない。
@   それ以外は今までどおり swi 0x11（WRAM）/ swi 0x12（VRAM）をそのまま撃つ
@ r0 = 元・r1 = 先・r2 = 1 なら VRAM

    .arch armv4t
    .text
    .align 2
    .thumb

    .global LzUnCompDispatch
    .thumb_func
LzUnCompDispatch:
    lsr r3, r0, #25             @ 2026-10-07 元が 0x0A000000〜0x0BFFFFFF（ROM の後ろ半分・build_rom.py --split-pics の絵）なら
    cmp r3, #5                  @   src/hack_split_pics.c（直した mGBA はそのまま・SuperCard は SD の PKSPICS.BIN から）
    beq .Lsplit
.Lnormal:
    ldrb r3, [r0]
    cmp r3, #0x46
    beq .Lf
    cmp r3, #0x47                @ 2026-10-06 地図のタイルセットの F（src/fcomp_map.c・tools/mapfcomp/map_fcomp.py）
    beq .Lm
.Lbios:
    cmp r2, #0
    bne .Lvram
    swi 0x11
    bx lr
.Lvram:
    swi 0x12
    bx lr

.Lf:
    push {r0-r2, lr}
    ldr r3, =DpeFTryDecode
    bl .Lcallr3
    cmp r0, #0
    pop {r0-r2}
    pop {r3}
    mov lr, r3                  @ 高いレジスタへの mov は旗を変えない
    beq .Lbios
    bx lr

.Lm:
    push {r0-r2, lr}
    ldr r3, =MapFTryDecode
    bl .Lcallr3
    cmp r0, #0
    pop {r0-r2}
    pop {r3}
    mov lr, r3
    beq .Lbios
    bx lr

.Lsplit:
    push {r0-r2, lr}
    ldr r3, =HackSplitPics_Decompress
    bl .Lcallr3
    cmp r0, #0                  @ 0 = SD から読んで展開した（または読めない）・1 = 直した mGBA なのでそのまま読める
    pop {r0-r2}
    pop {r3}
    mov lr, r3                  @ 高いレジスタへの mov は旗を変えない
    bne .Lnormal
    bx lr

.Lcallr3:
    bx r3

    .pool
