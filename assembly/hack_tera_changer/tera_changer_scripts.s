@ テラスタイプを変えてくれる人（2026-10-06・BattleFrontier_Mart (5,4)・build_hoenn.EXTRA_NPCS）。
@ ユーザー「バトルフロンティアにテラスタルタイプを変えてくれる人導入してほしい」。
@ 手持ちを選ぶ（special 0x9F）→ タイプを選ぶ（src/hack_tera_changer.c のスクロールの一覧）→ CFRU の special 0x131 で書き換え。無料。
@ 文は strings/hack_tera_changer.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ SP_CHOOSE_PARTY_MON, 0x9F
.equ SP_IS_SELECTED_MON_EGG, 0x148

.align 2
.global HackTeraChangerScr_Talk
HackTeraChangerScr_Talk:
	lock
	faceplayer
	msgbox gText_Tera_Intro MSG_YESNO
	compare LASTRESULT 1
	if 0x5 _goto HackTeraChangerScr_Bye
	msgbox gText_Tera_WhichMon MSG_KEEPOPEN
	closemessage
	special SP_CHOOSE_PARTY_MON
	waitstate
	callasm HackPartyChoice_RestoreSlot
	compare 0x8004 6
	if 0x4 _goto HackTeraChangerScr_Bye
	special SP_IS_SELECTED_MON_EGG
	compare LASTRESULT 1
	if 0x1 _goto HackTeraChangerScr_Egg
	preparemsg gText_Tera_WhichType
	waitmsg
	callasm HackTeraChanger_ShowList
	waitstate
	compare LASTRESULT 0x7F
	if 0x1 _goto HackTeraChangerScr_Bye
	copyvar 0x8005 LASTRESULT
	callasm HackTeraChanger_Apply
	compare LASTRESULT 1
	if 0x1 _goto HackTeraChangerScr_Same
	compare LASTRESULT 2
	if 0x1 _goto HackTeraChangerScr_Banned
	bufferpartypokemon 0x0 0x8004
	fanfare 0x101
	msgbox gText_Tera_Done MSG_KEEPOPEN
	waitfanfare
	closemessage
	release
	end

HackTeraChangerScr_Same:
	msgbox gText_Tera_Same MSG_KEEPOPEN
	closemessage
	release
	end

HackTeraChangerScr_Banned:
	msgbox gText_Tera_Banned MSG_KEEPOPEN
	closemessage
	release
	end

HackTeraChangerScr_Egg:
	msgbox gText_Tera_Egg MSG_KEEPOPEN
	closemessage
	release
	end

HackTeraChangerScr_Bye:
	msgbox gText_Tera_Bye MSG_KEEPOPEN
	closemessage
	release
	end
