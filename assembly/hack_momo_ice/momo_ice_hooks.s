@ ももアイスを持った子を通信交換に出せない（2026-10-09 ユーザー「ももアイスをトレード不可アイテムに」）。
@ 素の CanTradeSelectedMon（pokefirered src/trade.c・日本語版 0804F1E4・r0 手持ち・r1 数・r2 選んだ位置）の入口を hooks で（r3）。
@ 潰すのは 0804F1E4〜0804F1EB（push {r4-r7,lr} / mov r7,r9 / mov r6,r8 / push {r6,r7}）。この間へ飛んでくる分岐は無い（関数の入口）。
@ ももアイスなら CANT_TRADE_INVALID_MON（4＝素の「その ポケモンは こうかんに だせません」）を返す。違えば潰した 4 命令を写して 0804F1EC へ。

.text
.thumb
.align 2

.global HackMomoIceTradeHook
.thumb_func
HackMomoIceTradeHook:
	push {r0, r1, r2, lr}
	mov r1, r2
	ldr r3, =HackMomoIce_TradeBlocked
	bl HackMomoIce_CallR3
	cmp r0, #0
	pop {r0, r1, r2}
	pop {r3}
	mov lr, r3
	bne HackMomoIceTrade_Blocked
	push {r4, r5, r6, r7, lr}
	mov r7, r9
	mov r6, r8
	push {r6, r7}
	ldr r3, =0x0804F1EC | 1
	bx r3

HackMomoIceTrade_Blocked:
	mov r0, #4
	bx lr

HackMomoIce_CallR3:
	bx r3
.pool
