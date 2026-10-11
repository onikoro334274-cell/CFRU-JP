@ お母さんの貯金（金銀の「ははおやの ちょきん」・2026-10-06・src/hack_mom_savings.c）。
@ ユーザー「金銀の母親貯金みたいなの実装できそう？」→ 割合は自分で決める・電話の演出・道具は家に帰ったとき渡す。
@   EventScript_MomSavingsCall … フィールドの電話（src/hack_mom_savings.c HackMomSavings_FieldTick が始める・gStringVar1 = 道具の名前）
@   KantoHome1F_EventScript_MomSavings … 家のお母さん（kanto_home_scripts.s の いつもの会話の手前から）
@ 文は strings/kanto_mom_savings.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ TEXT_COLOR_FEMALE, 1
.equ TEXT_COLOR_NEUTRAL, 3
.equ SE_POKENAV_TWO_BEEPS, 0x66
.equ SP_ADD_MENU, 0x25
.equ MENU_4, 0x22
.equ MENU_5, 0x23
.equ MENU_7, 0x25
.equ KantoHome1F_EventScript_MomOriginal, 0x0817BCBB   @ 素のお母さんのスクリプト（kanto_home_scripts.s と同じ）

.macro menu_item n:req, text:req
	setvar 0x8006, \n
	loadpointer 0, \text
	special SP_ADD_MENU
.endm

.align 2
.global EventScript_MomSavingsCall
EventScript_MomSavingsCall:
	lockall
	playse SE_POKENAV_TWO_BEEPS
	waitse
	playse SE_POKENAV_TWO_BEEPS
	waitse
	textcolor TEXT_COLOR_NEUTRAL
	msgbox gText_MomCall_Ring MSG_KEEPOPEN
	textcolor TEXT_COLOR_FEMALE
	compare 0x8004 1                                 @ ★ 2026-10-11 1 = 買ったものは全部パソコンへ（src/hack_mom_savings.c の FieldTick が置く）
	if 0x1 _goto EventScript_MomSavingsCallPc
	msgbox gText_MomCall_Talk MSG_KEEPOPEN
	closemessage
	releaseall
	end
EventScript_MomSavingsCallPc:
	msgbox gText_MomCall_TalkPc MSG_KEEPOPEN
	closemessage
	releaseall
	end

.align 2
.global KantoHome1F_EventScript_MomSavings
KantoHome1F_EventScript_MomSavings:
	lock
	faceplayer
	textcolor TEXT_COLOR_FEMALE
	@ ★ 2026-10-10 ユーザー「ママのかいものを一気に渡すように」: まとめてバッグに入れる（HackMomSavings_GiveAll）
	callasm HackMomSavings_GiveAll
	compare LASTRESULT 0
	if 0x1 _goto KantoMomSav_Menu
	compare LASTRESULT 3
	if 0x1 _goto KantoMomSav_GiveFull
	copyvar 0x8001 LASTRESULT
	msgbox gText_MomSav_Gift MSG_KEEPOPEN
	fanfare 0x102 @ MUS_OBTAIN_ITEM
	textcolor TEXT_COLOR_NEUTRAL
	msgbox gText_MomSav_GotAll MSG_KEEPOPEN
	waitfanfare
	textcolor TEXT_COLOR_FEMALE
	compare 0x8001 2
	if 0x1 _goto KantoMomSav_GiveFull
	goto KantoMomSav_Menu
KantoMomSav_GiveFull:
	msgbox gText_MomSav_BagFull MSG_KEEPOPEN
	goto KantoMomSav_Menu
KantoMomSav_Menu:
	callasm HackMomSavings_BufferStatus
	compare LASTRESULT 1
	if 0x1 _goto KantoMomSav_StatusOn
	msgbox gText_MomSav_StatusOff MSG_KEEPOPEN
	goto KantoMomSav_Ask
KantoMomSav_StatusOn:
	msgbox gText_MomSav_StatusOn MSG_KEEPOPEN
KantoMomSav_Ask:
	callasm HackMomSavings_BufferStatus
	preparemsg gText_MomSav_What
	waitmsg
	menu_item 0, gText_MomSav_Withdraw
	menu_item 1, gText_MomSav_Percent
	compare LASTRESULT 1
	if 0x1 _goto KantoMomSav_ItemStop
	menu_item 2, gText_MomSav_Start
	goto KantoMomSav_ItemEnd
KantoMomSav_ItemStop:
	menu_item 2, gText_MomSav_Stop
KantoMomSav_ItemEnd:
	menu_item 3, gText_MomSav_Cancel
	setvar 0x8006, 8                                   @ 窓の幅（タイル・自前の選択肢は出す直前に 0x8006 で渡す）
	multichoice 21, 0, MENU_4, 0
	compare LASTRESULT 0
	if 0x1 _goto KantoMomSav_DoWithdraw
	compare LASTRESULT 1
	if 0x1 _goto KantoMomSav_DoPercent
	compare LASTRESULT 2
	if 0x1 _goto KantoMomSav_DoToggle
	goto KantoMomSav_Bye

KantoMomSav_DoWithdraw:
	callasm HackMomSavings_Withdraw
	compare LASTRESULT 0
	if 0x1 _goto KantoMomSav_NoWithdraw
	msgbox gText_MomSav_Withdrew MSG_KEEPOPEN
	goto KantoMomSav_Bye
KantoMomSav_NoWithdraw:
	msgbox gText_MomSav_NoWithdraw MSG_KEEPOPEN
	goto KantoMomSav_Bye

KantoMomSav_DoPercent:
	preparemsg gText_MomSav_WhichPercent
	waitmsg
	menu_item 0, gText_MomSav_P10
	menu_item 1, gText_MomSav_P20
	menu_item 2, gText_MomSav_P30
	menu_item 3, gText_MomSav_P40
	menu_item 4, gText_MomSav_P50
	menu_item 5, gText_MomSav_P75
	menu_item 6, gText_MomSav_P100
	setvar 0x8006, 12
	multichoice 17, 0, MENU_7, 0
	compare LASTRESULT 0x7F
	if 0x1 _goto KantoMomSav_Bye
	callasm HackMomSavings_SetPercent
	msgbox gText_MomSav_PercentSet MSG_KEEPOPEN
	goto KantoMomSav_Bye

KantoMomSav_DoToggle:
	callasm HackMomSavings_ToggleOn
	compare LASTRESULT 1
	if 0x1 _goto KantoMomSav_Started
	msgbox gText_MomSav_Stopped MSG_KEEPOPEN
	goto KantoMomSav_Bye
KantoMomSav_Started:
	callasm HackMomSavings_BufferStatus
	msgbox gText_MomSav_Started MSG_KEEPOPEN

KantoMomSav_Bye:
	msgbox gText_MomSav_Bye MSG_KEEPOPEN
	closemessage
	release
	end
