@ 捕まえたときの図鑑の画面（2026-10-10・src/hack_dex_habitat.c）。すみかの表に無い種族を基本の姿に読み替えてから素の処理へ。
@ 素（DexScreen_RegisterMonToPokedex・081076B8〜）: push {r4,r5,lr} / lsls r0,r0,#16 / lsrs r5,r0,#16 / adds r0,r5,#0 /
@   081076C0 movs r1,#2 / movs r2,#1 / bl DexScreen_GetSetPokedexFlag …
@ hooks で 081076B8 via r1（4 の倍数・潰すのは 081076B8〜081076BF の 4 命令＝関数の入口なので途中へ飛んでくる分岐は無い）。
@ r1 は 081076C0 で入れ直されるので使ってよい。

.text
.thumb
.align 2

.global HackDex_RegisterHook
.thumb_func
HackDex_RegisterHook:
	push {r4, r5, lr}
	bl HackDex_HabitatSpecies
	lsl r0, r0, #16
	lsr r5, r0, #16
	mov r0, r5
	ldr r1, =0x081076C0 | 1
	bx r1
.pool
