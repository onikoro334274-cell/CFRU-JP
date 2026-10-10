@ ライバルのお姉さん（ナナミ）: タウンマップのあとに特別なジグザグマをくれる（2026-10-03・src/hack_daisy_zigzagoon.c）。
@ 素のナナミ（PalletTown_RivalsHouse の人物データ 0837D7B8 の +0x10 = 0817BFDE）を repoints（## KANTODAISY）でここへ向ける。
@ もらえないとき（殿堂入りのあと・タウンマップの前・もうもらった）は素のスクリプトへ。文は strings/kanto_daisy.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ KANTO_FLAG_DAISY_ZIGZAGOON, 0x12FD      @ build_hoenn_scripts.RESERVED_FLAGS と同じ
.equ FLAG_SYS_GAME_CLEAR, 0x82C
.equ VAR_MAP_SCENE_PALLET_TOWN_RIVALS_HOUSE, 0x4058   @ 2 = タウンマップをもらった（pokefirered）
.equ DAISY_ORIGINAL_SCRIPT, 0x0817BFDE
.equ TEXT_COLOR_FEMALE, 1
.equ TEXT_COLOR_NEUTRAL, 3
.equ MUS_LEVEL_UP, 257

.align 2
.global KantoDaisy_EventScript_Talk
KantoDaisy_EventScript_Talk:
	checkflag FLAG_SYS_GAME_CLEAR
	if 0x1 _goto DAISY_ORIGINAL_SCRIPT
	compare VAR_MAP_SCENE_PALLET_TOWN_RIVALS_HOUSE 2
	if 0x0 _goto DAISY_ORIGINAL_SCRIPT           @ 0x0 = より小さい（まだタウンマップをもらっていない）
	checkflag KANTO_FLAG_DAISY_ZIGZAGOON
	if 0x1 _goto DAISY_ORIGINAL_SCRIPT
	lock
	faceplayer
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoDaisy_ZigzagoonOffer MSG_KEEPOPEN
	callasm HackDaisy_GiveZigzagoon + 1
	compare LASTRESULT 2
	if 0x1 _goto KantoDaisy_EventScript_NoRoom
	setflag KANTO_FLAG_DAISY_ZIGZAGOON
	textcolor TEXT_COLOR_NEUTRAL
	fanfare MUS_LEVEL_UP
	preparemsg gText_KantoDaisy_GotZigzagoon
	waitmsg
	waitfanfare
	compare LASTRESULT 1
	if 0x1 _call KantoDaisy_EventScript_ToPC
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoDaisy_ZigzagoonBye MSG_KEEPOPEN
	closemessage
	release
	end

KantoDaisy_EventScript_ToPC:
	msgbox gText_KantoDaisy_ZigzagoonToPC MSG_KEEPOPEN
	return

KantoDaisy_EventScript_NoRoom:
	msgbox gText_KantoDaisy_NoRoom MSG_KEEPOPEN
	closemessage
	release
	end
