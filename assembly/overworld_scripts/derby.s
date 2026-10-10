.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

@ けいば (SoulGold から移植)
@ マシンやNPCから EventScript_Derby を呼び出して使います。
@ ゲーム内でコインをかけ、当たるとコインが増えます (勝ち分は DERBY_VAR_WINNINGS に入り、このスクリプトで受け取ります)。

.equ DERBY_VAR_WINNINGS, 0x50EC @ [hack] 2026-10-10 0x512E → 0x50EC（チューブの予約とぶつかる） @ src/derby.c の GAME_CORNER_VAR_WINNINGS と同じ値
.equ DERBY_MIN_COINS, 10
.equ ITEM_COIN_CASE_ID, 260

.global EventScript_Derby
EventScript_Derby:
	lockall
	checkitem ITEM_COIN_CASE_ID 0x1
	compare LASTRESULT 0x0
	if equal _goto Derby_NoCoinCase
	checkcoins 0x8004
	compare 0x8004 DERBY_MIN_COINS
	if lessthan _goto Derby_NotEnoughCoins
	msgbox gText_Derby_Ask MSG_YESNO
	compare LASTRESULT 0x1
	if notequal _goto Derby_Cancel
	closeonkeypress
	setvar DERBY_VAR_WINNINGS 0x0
	callasm StartDerby
	waitstate
	fadedefaultbgm
	compare DERBY_VAR_WINNINGS 0x0
	if equal _goto Derby_NoWin
	showcoins 0x0 0x0
	buffernumber 0x0 DERBY_VAR_WINNINGS
	msgbox gText_Derby_YouWon MSG_NORMAL
	addcoins DERBY_VAR_WINNINGS
	setvar DERBY_VAR_WINNINGS 0x0
	updatecoins 0x0 0x0
	playse 248 @ SE_MONEY
	pause 0x28
	hidecoins 0x0 0x0
	releaseall
	end

Derby_NoWin:
	msgbox gText_Derby_NoWin MSG_NORMAL
	releaseall
	end

Derby_Cancel:
	releaseall
	end

Derby_NoCoinCase:
	msgbox gText_Derby_NoCoinCase MSG_NORMAL
	releaseall
	end

Derby_NotEnoughCoins:
	msgbox gText_Derby_NotEnoughCoins MSG_NORMAL
	releaseall
	end
