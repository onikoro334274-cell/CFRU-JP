@ BW 風スタートメニューのスクリプト 2 本（#19）。元: 参考/github/start-menu-bw/assembly/script.s
@ 根拠は docs/research/2026-09-19_bw-start-menu.md
@
@ ★ special 0x5D は BPRJ でも「フィールドでレポートを書くか聞く」（0806EE6C・
@   セーブの窓のタスクを作る）。書き終えて LASTRESULT != 1 ならメニューを開き直す。
@ ★ サファリのリタイアは素の SafariZoneRetirePrompt（080A2210）を C から呼ぶ。

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ SPECIAL_FIELD_ASK_SAVE, 0x5D

.global BwSmScript_SaveGame
BwSmScript_SaveGame:
	pause 0x20
	special SPECIAL_FIELD_ASK_SAVE
	waitstate
	compare LASTRESULT 0x1
	if 0x5 _goto BwSmScript_ReopenMenu    @ 0x5 = not equal（CFRU の xse_defines には EQUAL 系の名前が無い）
	closeonkeypress
	end

.global BwSmScript_ReopenMenu
BwSmScript_ReopenMenu:
	callasm BwShowStartMenu
	end

.global BwSmScript_Retire
BwSmScript_Retire:
	callasm BwSmSafariRetire
	end
