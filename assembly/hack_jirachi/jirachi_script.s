@ トクサネシティの白い岩のジラーチ（2026-10-07・src/hack_jirachi.c）。
@ ユーザー「トクサネシティ　白い石　ジラーチ　白い岩に7回連続で話しかけるとジラーチプレシャスボール入が手に入る」
@ 白い岩（エメラルドの看板 (57, 21)）のスクリプトを tools/rom/build_hoenn_scripts.py の PATTERNS でここへ向ける。
@ 続けて 7 回目で渡す（間に歩く・向きを変える・人に話しかける・地図を出ると数え直し＝src/hack_jirachi.c の HackJirachi_CountRock）・
@ 1 度だけ。文は strings/hack_jirachi.string
.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ HOENN_FLAG_JIRACHI, 0x12EA      @ build_hoenn_scripts.RESERVED_FLAGS と同じ
.equ TEXT_COLOR_NEUTRAL, 3
.equ MUS_LEVEL_UP, 257
.equ SPECIES_JIRACHI, 0x199
.equ CRY_NORMAL, 0

.align 2
.global HackScr_MossdeepWhiteRock
HackScr_MossdeepWhiteRock:
	checkflag HOENN_FLAG_JIRACHI
	if 0x1 _goto HackJirachi_Plain
	callasm HackJirachi_CountRock + 1
	compare LASTRESULT 0
	if 0x1 _goto HackJirachi_Plain
	lockall
	textcolor TEXT_COLOR_NEUTRAL
	msgbox gText_HackJirachi_Shine MSG_KEEPOPEN
	checksound
	cry SPECIES_JIRACHI CRY_NORMAL
	waitcry
	callasm HackJirachi_Give + 1
	compare LASTRESULT 2
	if 0x1 _goto HackJirachi_NoRoom
	setflag HOENN_FLAG_JIRACHI
	fanfare MUS_LEVEL_UP
	preparemsg gText_HackJirachi_Got
	waitmsg
	waitfanfare
	compare LASTRESULT 1
	if 0x1 _call HackJirachi_ToPC
	closemessage
	releaseall
	end

HackJirachi_ToPC:
	msgbox gText_HackJirachi_ToPC MSG_KEEPOPEN
	return

HackJirachi_NoRoom:
	msgbox gText_HackJirachi_NoRoom MSG_KEEPOPEN
	closemessage
	releaseall
	end

HackJirachi_Plain:
	msgbox gText_HackJirachi_WhiteRock MSG_SIGN
	end
