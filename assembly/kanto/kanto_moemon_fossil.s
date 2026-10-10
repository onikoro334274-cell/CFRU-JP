@ グレンの研究所（ポケモンけんきゅうじょ じっけんしつ・群 12 の 4）の かせきの研究員（素の FossilScientist 08189234）の前に、
@ Moemonかせき（ITEM_MOEMON_FOSSIL 0x34B）を その場で よみがえらせる話を足す（2026-10-10・src/hack_moemon_fossil.c）。
@ 研究員の人物の表の台本の語（083807D4・素の値 08189234）を repoints でここへ。持っていなければ・いいえ なら素の台本へ。
@ 文は strings/kanto_moemon_fossil.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ ITEM_MOEMON_FOSSIL, 0x34B
.equ VANILLA_FOSSIL_SCIENTIST, 0x08189234

.global KantoMoemonFossil_EventScript_Scientist
KantoMoemonFossil_EventScript_Scientist:
	checkitem ITEM_MOEMON_FOSSIL 1
	compare LASTRESULT 0
	if equal _goto VANILLA_FOSSIL_SCIENTIST
	lock
	faceplayer
	msgbox gText_MoeFossil_Ask MSG_YESNO
	compare LASTRESULT 0
	if equal _goto KantoMoemonFossil_Vanilla
	callasm HackMoemonFossil_Revive
	compare LASTRESULT 2
	if equal _goto KantoMoemonFossil_Full
	removeitem ITEM_MOEMON_FOSSIL 1
	copyvar 0x8001 LASTRESULT
	msgbox gText_MoeFossil_Wait MSG_KEEPOPEN
	fanfare 0x101
	msgbox gText_MoeFossil_Revived MSG_KEEPOPEN
	waitfanfare
	compare 0x8001 1
	if equal _goto KantoMoemonFossil_ToPC
	release
	end

KantoMoemonFossil_ToPC:
	msgbox gText_MoeFossil_ToPC MSG_NORMAL
	release
	end

KantoMoemonFossil_Full:
	msgbox gText_MoeFossil_Full MSG_NORMAL
	release
	end

KantoMoemonFossil_Vanilla:
	release
	goto VANILLA_FOSSIL_SCIENTIST
