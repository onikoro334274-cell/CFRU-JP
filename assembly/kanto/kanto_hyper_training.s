@ 2 のしまの家（技を思い出させてくれる人の家・TwoIsland_House）の すごいとっくんの人（2026-10-03）。
@ ユーザー「王冠の使用できるようにNPCを２のしまに配置して」。
@ 素の家の人物は 1 人（0838261C・ふとった おとこ）。人物の表を写して 2 人目（localId 2・からておう・(2,4)）を足し、
@ 地図のイベントの人数（08382638）を 2 に・表の語（0838263C）を repoints で向ける。
@ きたえるのは CFRU の special 0x12F（sp12F_SetHyperTraining・0x8004 = 手持ちの番号・0x8005 = 能力・6 = 全部）。
@ ぎんのおうかん: 能力を 1 つ選ぶ / きんのおうかん: 全部。もう 31（かきたえ済み）なら王冠は減らさない。文は strings/kanto_hyper_training.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ ITEM_BOTTLE_CAP, 0x27F
.equ ITEM_GOLD_BOTTLE_CAP, 0x280
.equ SP_CHOOSE_PARTY_MON, 0x9F
.equ SP_IS_SELECTED_MON_EGG, 0x148
.equ SP_SET_HYPER_TRAINING, 0x12F
.equ SP_ADD_MENU, 0x25
.equ MENU_2, 0x20
.equ MENU_3, 0x21
.equ MENU_6, 0x24
.equ OBJ_EVENT_GFX_FAT_MAN, 27
.equ OBJ_EVENT_GFX_BLACK_BELT, 54

@ ---- 人物の表（素の 1 人目をそのまま写す + すごいとっくんの人）----
.align 2
.global KantoTwoIslandHouse_Objects
KantoTwoIslandHouse_Objects:
	.byte 1, OBJ_EVENT_GFX_FAT_MAN, 0, 0
	.2byte 7, 5
	.byte 3, 8, 0x11, 0
	.2byte 0, 0
	.4byte 0x08190FB9
	.2byte 0, 0
	.byte 2, OBJ_EVENT_GFX_BLACK_BELT, 0, 0
	.2byte 2, 4
	.byte 3, 8, 0x11, 0          @ 高さ 3・下を向く（MOVEMENT_TYPE_FACE_DOWN = 8）
	.2byte 0, 0
	.4byte KantoHyper_EventScript_Talk
	.2byte 0, 0

.macro hyper_item n:req, text:req
	setvar 0x8006, \n
	loadpointer 0, \text
	special SP_ADD_MENU
.endm

.align 2
.global KantoHyper_EventScript_Talk
KantoHyper_EventScript_Talk:
	lock
	faceplayer
	checkitem ITEM_GOLD_BOTTLE_CAP 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHyper_HasGold
	checkitem ITEM_BOTTLE_CAP 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHyper_UseSilver
	msgbox gText_KantoHyper_Intro MSG_KEEPOPEN
	closemessage
	release
	end

KantoHyper_HasGold:
	checkitem ITEM_BOTTLE_CAP 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHyper_ChooseCap
	goto KantoHyper_UseGold

@ どちらの王冠も持っている: えらぶ
KantoHyper_ChooseCap:
	preparemsg gText_KantoHyper_WhichCap
	waitmsg
	hyper_item 0, gText_KantoHyper_Gold
	hyper_item 1, gText_KantoHyper_Silver
	hyper_item 2, gText_KantoHyper_Cancel
	setvar 0x8006, 12
	multichoice 17, 6, MENU_3, 0
	compare LASTRESULT 0
	if 0x1 _goto KantoHyper_UseGold
	compare LASTRESULT 1
	if 0x1 _goto KantoHyper_UseSilver
	goto KantoHyper_Cancel

@ ---- きんのおうかん: 全部 ----
KantoHyper_UseGold:
	msgbox gText_KantoHyper_GoldOffer MSG_YESNO
	compare LASTRESULT 1
	if 0x0 _goto KantoHyper_Cancel
	call KantoHyper_ChooseMon
	compare 0x8004 6
	if 0x4 _goto KantoHyper_Cancel                     @ 0x4 = 以上（やめた）
	setvar 0x8005 6
	special2 LASTRESULT SP_SET_HYPER_TRAINING
	compare LASTRESULT 0
	if 0x1 _goto KantoHyper_AlreadyMax
	removeitem ITEM_GOLD_BOTTLE_CAP 1
	goto KantoHyper_Done

@ ---- ぎんのおうかん: 1 つ ----
KantoHyper_UseSilver:
	msgbox gText_KantoHyper_SilverOffer MSG_YESNO
	compare LASTRESULT 1
	if 0x0 _goto KantoHyper_Cancel
	call KantoHyper_ChooseMon
	compare 0x8004 6
	if 0x4 _goto KantoHyper_Cancel
	preparemsg gText_KantoHyper_WhichStat
	waitmsg
	hyper_item 0, gText_KantoHyper_HP
	hyper_item 1, gText_KantoHyper_Atk
	hyper_item 2, gText_KantoHyper_Def
	hyper_item 3, gText_KantoHyper_SpAtk
	hyper_item 4, gText_KantoHyper_SpDef
	hyper_item 5, gText_KantoHyper_Spe
	setvar 0x8006, 10
	multichoice 19, 0, MENU_6, 0
	@ 選択肢の並び（HP・こうげき・ぼうぎょ・とくこう・とくぼう・すばやさ）→ CFRU の能力の番号（HP 0・攻 1・防 2・素早 3・特攻 4・特防 5）
	compare LASTRESULT 6
	if 0x4 _goto KantoHyper_Cancel                     @ B（0x7F）
	copyvar 0x8005 LASTRESULT
	compare LASTRESULT 3
	if 0x1 _call KantoHyper_StatSpAtk
	compare LASTRESULT 4
	if 0x1 _call KantoHyper_StatSpDef
	compare LASTRESULT 5
	if 0x1 _call KantoHyper_StatSpe
	special2 LASTRESULT SP_SET_HYPER_TRAINING
	compare LASTRESULT 0
	if 0x1 _goto KantoHyper_AlreadyMax
	removeitem ITEM_BOTTLE_CAP 1
	goto KantoHyper_Done

KantoHyper_StatSpAtk:
	setvar 0x8005 4
	return
KantoHyper_StatSpDef:
	setvar 0x8005 5
	return
KantoHyper_StatSpe:
	setvar 0x8005 3
	return

@ 手持ちを選ぶ（0x8004 = 番号・6 以上 = やめた・タマゴはやめた扱い）
KantoHyper_ChooseMon:
	msgbox gText_KantoHyper_WhichMon MSG_KEEPOPEN
	closemessage
	special SP_CHOOSE_PARTY_MON
	waitstate
	callasm HackPartyChoice_RestoreSlot                @ ★ 2026-10-04 0x8004 を手持ちの画面の選んだ番号に戻す（フィールドへ戻るとき ON_RESUME が 0x8004 を書き換える地図がある＝リーグの部屋は 0 にする・src/hack_mystery_gift.c）
	compare 0x8004 6
	if 0x4 _goto KantoHyper_ChooseMonEnd
	special SP_IS_SELECTED_MON_EGG
	compare LASTRESULT 1
	if 0x1 _call KantoHyper_EggCancel
KantoHyper_ChooseMonEnd:
	return

KantoHyper_EggCancel:
	msgbox gText_KantoHyper_Egg MSG_KEEPOPEN
	setvar 0x8004 6
	return

KantoHyper_Done:
	bufferpartypokemon 0x0 0x8004
	msgbox gText_KantoHyper_Done MSG_KEEPOPEN
	closemessage
	release
	end

KantoHyper_AlreadyMax:
	msgbox gText_KantoHyper_AlreadyMax MSG_KEEPOPEN
	closemessage
	release
	end

KantoHyper_Cancel:
	msgbox gText_KantoHyper_Bye MSG_KEEPOPEN
	closemessage
	release
	end
