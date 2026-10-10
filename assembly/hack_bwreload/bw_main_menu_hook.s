@ タイトルのあとのメニュー（src/bw_main_menu.c・2026-10-10）。hooks 0800C1AC（r0）。
@ 素の 0800C1A0 の中で、CFRU の時計の注意のフック（0800C1A4）が「注意なし」で戻る所。r7 = タスクの番号。
@ BW のメニューを作ったら、素の関数の後片付け（0800C3E2: add sp,#16 / pop r8 / pop {r4-r7,pc}）へ
.text
.thumb
.align 2
.global BwMainMenuHook
BwMainMenuHook:
	mov r0, r7
	bl BwMainMenuPresent
	ldr r0, =0x0800C3E2 | 1
	bx r0
.pool
