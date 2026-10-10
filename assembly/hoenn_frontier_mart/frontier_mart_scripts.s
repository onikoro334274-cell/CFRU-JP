@ バトルフロンティアのフレンドリィショップ（エメラルドの BattleFrontier_Mart・群 69 の 55）の人（2026-10-05）。
@ ★ エメラルドの地図の人は入れていなかった（BattleFrontier_ は人を置かない地図）。tools/rom/build_hoenn.py EXTRA_NPCS で 3 人:
@   1. 店員（エメラルドと同じ品・(1,3)）
@   2. 対戦の持ち物の店員（ユーザー「対戦系のもちもの売ってくれる店員追加」→ お金・値段は道具の表のまま・(1,2)）
@   3. なんでも おしえる ひと（ユーザー「選択したポケモンが覚えるすべての技を教えてくれる技教えのひと」→ 無料・(8,4)）
@      素の技思い出し（special 0xDB / 0xE0）＋ 印 HACK_FLAG_FULL_TUTOR の間だけ全部の技（src/hack_full_tutor.c・patches/0171）
@ 文は strings/hack_frontier_mart.string（店員の文はエメラルドの文 HoennScr_gText_*）

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ HACK_FLAG_FULL_TUTOR, 0x12EE     @ build_hoenn_scripts.RESERVED_FLAGS
.equ HACK_VAR_FULL_TUTOR_PAGE, 0x4009 @ 一時変数（src/hack_full_tutor.c）
.equ SP_CHOOSE_MON_FOR_MOVE_RELEARNER, 0xDB
.equ SP_TEACH_MOVE_RELEARNER_MOVE, 0xE0
.equ SP_IS_SELECTED_MON_EGG, 0x148
.equ SP_ADD_MENU, 0x25
.equ MENU_2, 0x20
.equ MENU_6, 0x24

.align 2
.global HoennFrontierMart_EventScript_Clerk
HoennFrontierMart_EventScript_Clerk:
	lock
	faceplayer
	message HoennScr_gText_HowMayIServeYou
	waitmessage
	pokemart HoennFrontierMart_Items
	msgbox HoennScr_gText_PleaseComeAgain MSG_KEEPOPEN
	closemessage
	release
	end

.align 2
.global HoennFrontierMart_EventScript_BattleItems
HoennFrontierMart_EventScript_BattleItems:
	lock
	faceplayer
	msgbox gText_FrontierMart_BattleItemsIntro MSG_KEEPOPEN
	message HoennScr_gText_HowMayIServeYou
	waitmessage
	pokemart HoennFrontierMart_BattleItems
	msgbox HoennScr_gText_PleaseComeAgain MSG_KEEPOPEN
	closemessage
	release
	end

.macro page_item n:req, text:req
	setvar 0x8006, \n
	loadpointer 0, \text
	special SP_ADD_MENU
.endm

.align 2
.global HoennFrontierMart_EventScript_Tutor
HoennFrontierMart_EventScript_Tutor:
	lock
	faceplayer
	msgbox gText_FullTutor_Intro MSG_YESNO
	compare LASTRESULT 1
	if 0x5 _goto HoennFrontierMart_TutorBye
HoennFrontierMart_TutorChoose:
	setflag HACK_FLAG_FULL_TUTOR
	setvar HACK_VAR_FULL_TUTOR_PAGE 0
	msgbox gText_FullTutor_WhichMon MSG_KEEPOPEN
	special SP_CHOOSE_MON_FOR_MOVE_RELEARNER
	waitstate
	callasm HackPartyChoice_RestoreSlot
	compare 0x8004 6
	if 0x4 _goto HoennFrontierMart_TutorEnd
	special SP_IS_SELECTED_MON_EGG
	compare LASTRESULT 1
	if 0x1 _goto HoennFrontierMart_TutorEgg
	callasm HackFullTutor_CountPages
	compare LASTRESULT 0
	if 0x1 _goto HoennFrontierMart_TutorNoMove
	compare LASTRESULT 1
	if 0x1 _goto HoennFrontierMart_TutorTeach
	copyvar 0x8007 LASTRESULT
	preparemsg gText_FullTutor_WhichPage
	waitmsg
	page_item 0, gText_FullTutor_Page1
	page_item 1, gText_FullTutor_Page2
	compare 0x8007 3
	if 0x0 _goto HoennFrontierMart_TutorMenu
	page_item 2, gText_FullTutor_Page3
	compare 0x8007 4
	if 0x0 _goto HoennFrontierMart_TutorMenu
	page_item 3, gText_FullTutor_Page4
	compare 0x8007 5
	if 0x0 _goto HoennFrontierMart_TutorMenu
	page_item 4, gText_FullTutor_Page5
	compare 0x8007 6
	if 0x0 _goto HoennFrontierMart_TutorMenu
	page_item 5, gText_FullTutor_Page6
HoennFrontierMart_TutorMenu:
	setvar 0x8006 10
	compare 0x8007 2
	if 0x1 _goto HoennFrontierMart_TutorMenu2
	compare 0x8007 3
	if 0x1 _goto HoennFrontierMart_TutorMenu3
	compare 0x8007 4
	if 0x1 _goto HoennFrontierMart_TutorMenu4
	compare 0x8007 5
	if 0x1 _goto HoennFrontierMart_TutorMenu5
	multichoice 17, 0, MENU_6, 0
	goto HoennFrontierMart_TutorPagePicked
HoennFrontierMart_TutorMenu2:
	multichoice 17, 0, 0x20, 0
	goto HoennFrontierMart_TutorPagePicked
HoennFrontierMart_TutorMenu3:
	multichoice 17, 0, 0x21, 0
	goto HoennFrontierMart_TutorPagePicked
HoennFrontierMart_TutorMenu4:
	multichoice 17, 0, 0x22, 0
	goto HoennFrontierMart_TutorPagePicked
HoennFrontierMart_TutorMenu5:
	multichoice 17, 0, 0x23, 0
HoennFrontierMart_TutorPagePicked:
	compare LASTRESULT 0x7F
	if 0x1 _goto HoennFrontierMart_TutorEnd           @ B（やめた）
	copyvar HACK_VAR_FULL_TUTOR_PAGE LASTRESULT
HoennFrontierMart_TutorTeach:
	msgbox gText_FullTutor_WhichMove MSG_KEEPOPEN
	special SP_TEACH_MOVE_RELEARNER_MOVE
	waitstate
HoennFrontierMart_TutorEnd:
	clearflag HACK_FLAG_FULL_TUTOR
	setvar HACK_VAR_FULL_TUTOR_PAGE 0
HoennFrontierMart_TutorBye:
	msgbox gText_FullTutor_Bye MSG_KEEPOPEN
	closemessage
	release
	end

HoennFrontierMart_TutorEgg:
	msgbox gText_FullTutor_Egg MSG_KEEPOPEN
	goto HoennFrontierMart_TutorEnd

HoennFrontierMart_TutorNoMove:
	msgbox gText_FullTutor_NoMove MSG_KEEPOPEN
	goto HoennFrontierMart_TutorEnd

@ ---- 品物 ----
.align 1
.global HoennFrontierMart_Items
HoennFrontierMart_Items:
	.2byte 0x2	@ ITEM_ULTRA_BALL
	.2byte 0x15	@ ITEM_HYPER_POTION
	.2byte 0x14	@ ITEM_MAX_POTION
	.2byte 0x13	@ ITEM_FULL_RESTORE
	.2byte 0x17	@ ITEM_FULL_HEAL
	.2byte 0x18	@ ITEM_REVIVE
	.2byte 0x19	@ ITEM_MAX_REVIVE げんきのかたまり（2026-10-10 ユーザー「バトルフロンティアショップ 元気の塊も」）
	.2byte 0x54	@ ITEM_MAX_REPEL
	.2byte 0x40	@ ITEM_PROTEIN
	.2byte 0x43	@ ITEM_CALCIUM
	.2byte 0x41	@ ITEM_IRON
	.2byte 0x46	@ ITEM_ZINC
	.2byte 0x42	@ ITEM_CARBOS
	.2byte 0x3F	@ ITEM_HP_UP
	.2byte 0

.align 1
.global HoennFrontierMart_BattleItems
HoennFrontierMart_BattleItems:
	.2byte 0xBA	@ ITEM_CHOICE_BAND
	.2byte 0x2B7	@ ITEM_CHOICE_SCARF
	.2byte 0x2B8	@ ITEM_CHOICE_SPECS
	.2byte 0x2A6	@ ITEM_LIFE_ORB
	.2byte 0xC8	@ ITEM_LEFTOVERS
	.2byte 0x2AB	@ ITEM_FOCUS_SASH
	.2byte 0x2C7	@ ITEM_ASSAULT_VEST
	.2byte 0x2A4	@ ITEM_EXPERT_BELT
	.2byte 0x2BE	@ ITEM_ROCKY_HELMET
	.2byte 0x2D5	@ ITEM_HEAVY_DUTY_BOOTS
	.2byte 0x2C6	@ ITEM_WEAKNESS_POLICY
	.2byte 0x2BF	@ ITEM_AIR_BALLOON
	.2byte 0x2BC	@ ITEM_EVIOLITE
	.2byte 0x2B1	@ ITEM_BLACK_SLUDGE
	.2byte 0x2A8	@ ITEM_TOXIC_ORB
	.2byte 0x2A9	@ ITEM_FLAME_ORB
	.2byte 0x2C5	@ ITEM_EJECT_BUTTON
	.2byte 0x2D2	@ ITEM_EJECT_PACK
	.2byte 0x2C0	@ ITEM_RED_CARD
	.2byte 0x2A5	@ ITEM_LIGHT_CLAY
	.2byte 0xC6	@ ITEM_SCOPE_LENS
	.2byte 0x2A1	@ ITEM_WIDE_LENS
	.2byte 0xB9	@ ITEM_MENTAL_HERB
	.2byte 0xB4	@ ITEM_WHITE_HERB
	.2byte 0x2A7	@ ITEM_POWER_HERB
	.2byte 0x2CA	@ ITEM_SAFETY_GOGGLES
	.2byte 0x2E3	@ ITEM_COVERT_CLOAK
	.2byte 0x2E2	@ ITEM_CLEAR_AMULET
	.2byte 0x2E4	@ ITEM_LOADED_DICE
	.2byte 0x2D7	@ ITEM_THROAT_SPRAY
	.2byte 0x2D3	@ ITEM_ROOM_SERVICE
	.2byte 0x2D4	@ ITEM_BLUNDER_POLICY
	.2byte 0x2D6	@ ITEM_UTILITY_UMBRELLA
	.2byte 0x2CC	@ ITEM_TERRAIN_EXTENDER
	.2byte 0x2B5	@ ITEM_DAMP_ROCK
	.2byte 0x2B4	@ ITEM_HEAT_ROCK
	.2byte 0x2B3	@ ITEM_SMOOTH_ROCK
	.2byte 0x2B2	@ ITEM_ICY_ROCK
	.2byte 0x8D	@ ITEM_LUM_BERRY
	.2byte 0x8E	@ ITEM_SITRUS_BERRY
	.2byte 0x2BA	@ ITEM_SHED_SHELL
	.2byte 0x2EF	@ ITEM_MIRROR_HERB
	.2byte 0x2E5	@ ITEM_PUNCHING_GLOVE
	.2byte 0x2F0	@ ITEM_ABILITY_SHIELD
	.2byte 0xB7	@ ITEM_QUICK_CLAW
	.2byte 0xBB	@ ITEM_KINGS_ROCK
	.2byte 0xB3	@ ITEM_BRIGHT_POWDER
	.2byte 0xC4	@ ITEM_FOCUS_BAND
	.2byte 0
