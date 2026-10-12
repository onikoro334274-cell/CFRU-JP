@ 特別なウルガモス（#95・2026-10-11 ユーザー提案）。えんとつ山の山頂（広場の右の端 (30, 15)・2026-10-12 に いんせきのそばから移した＝ストーリー 2 章の場所）の人が 1 回だけ渡す。
@ 人は build_hoenn.PEOPLE_APPEND の 'MtChimney'（(30, 15)）。渡すのは src/hack_volcarona.c の HackVolcarona_Give（callasm）。
@ 文は strings/hack_volcarona.string。印は HACK_FLAG_VOLCARONA（0x12C9・src/hack_volcarona.h）。
.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ HACK_FLAG_VOLCARONA, 0x12C9
.equ TEXT_COLOR_MALE, 0
.equ TEXT_COLOR_NEUTRAL, 3
.equ MUS_LEVEL_UP, 257

.global HackScr_Volcarona_Talk
HackScr_Volcarona_Talk:
	lock
	faceplayer
	checkflag HACK_FLAG_VOLCARONA
	if 0x1 _goto HackScr_Volcarona_After
	textcolor TEXT_COLOR_MALE
	msgbox gText_Volcarona_Intro MSG_KEEPOPEN
	msgbox gText_Volcarona_Ask MSG_YESNO
	compare LASTRESULT 0x1
	if 0x0 _goto HackScr_Volcarona_No
	callasm HackVolcarona_Give + 1
	compare LASTRESULT 2
	if 0x1 _goto HackScr_Volcarona_NoRoom
	setflag HACK_FLAG_VOLCARONA
	textcolor TEXT_COLOR_NEUTRAL
	fanfare MUS_LEVEL_UP
	preparemsg gText_Volcarona_Got
	waitmsg
	waitfanfare
	compare LASTRESULT 1
	if 0x1 _call HackScr_Volcarona_ToPC
	textcolor TEXT_COLOR_MALE
	msgbox gText_Volcarona_Bye MSG_KEEPOPEN
	closemessage
	release
	end

HackScr_Volcarona_ToPC:
	msgbox gText_Volcarona_ToPC MSG_KEEPOPEN
	return

HackScr_Volcarona_NoRoom:
	msgbox gText_Volcarona_NoRoom MSG_KEEPOPEN
	closemessage
	release
	end

HackScr_Volcarona_No:
	msgbox gText_Volcarona_No MSG_KEEPOPEN
	closemessage
	release
	end

HackScr_Volcarona_After:
	textcolor TEXT_COLOR_MALE
	msgbox gText_Volcarona_After MSG_KEEPOPEN
	closemessage
	release
	end
