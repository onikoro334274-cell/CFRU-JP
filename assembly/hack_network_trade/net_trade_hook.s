@ ネット交換（src/hack_network_trade.c・pm-start/enchanted-network-trade の src/hooks.s の ENT_BlockHook）。hooks 08009F44（r3）。
@ 素の SetBlockReceivedFlag（08009F44）の頭: 塊を受け取ったら NetTrade_BlockReceived(r0 = 送り手) を呼び、潰した 8 バイト
@ （push {lr} / lsl r0,#24 / lsr r1,r0,#24 / ldr r0,=gWirelessCommType）をここでやって 08009F4C へ戻る
.text
.thumb
.align 2
.global NetTrade_BlockHook
NetTrade_BlockHook:
	push {r0-r4, lr}
	bl NetTrade_BlockReceived
	pop {r0-r4}
	pop {r3}
	mov lr, r3
	push {lr}
	lsl r0, r0, #24
	lsr r1, r0, #24
	ldr r0, =0x03003F7C
	ldr r3, =0x08009F4C | 1
	bx r3
.pool
