@ ボックスの仮想化 M2（src/hack_box_shelf.c・2026-10-06）: 1 回のレポートで書けるページの数を超える操作を、ボックスの画面で止める。
@
@ 1. Task_PokeStorageMain（0808CCF8）の state 0 の HandleInput（080942E8）の呼び出し 0808CD48 に hooks（r0）。
@    潰す 3 命令（bl HandleInput / lsls / lsrs）の代わりに BoxShelf_PssHandleInput を呼び、0808CD50（subs r0,#1・入力で分ける）へ戻る。
@ 2. Task_DepositMenu（0808D7C4）の TryStorePartyMonInBox（08092B18）の呼び出し 0808D858 に hooks（r0）。
@    潰す 3 命令（mov r0,r4 / bl TryStorePartyMonInBox / lsls r0,#24）の代わりに BoxShelf_DepositTry を呼ぶ。
@    0 / 1 は素と同じ（lsls をここで行い 0808D860 の cmp へ）・2 は止めた（文は C が出した・state も C が 4 に）→ 0808D8F6（関数の終わり）へ。

.thumb
.text
.align 2

.global BoxShelf_PssInputHook
BoxShelf_PssInputHook:
	bl BoxShelf_PssHandleInput
	lsl r0, r0, #24
	lsr r0, r0, #24
	ldr r1, =0x0808CD50 | 1
	bx r1

.global BoxShelf_DepositHook
BoxShelf_DepositHook:
	mov r0, r4
	bl BoxShelf_DepositTry
	cmp r0, #2
	beq BoxShelf_DepositBlocked
	lsl r0, r0, #24
	ldr r1, =0x0808D860 | 1
	bx r1

BoxShelf_DepositBlocked:
	ldr r1, =0x0808D8F6 | 1
	bx r1

.pool
