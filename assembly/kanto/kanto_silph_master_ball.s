@ シルフカンパニー 1F（SilphCo_1F・群 1 の 47）の マスターボールを売る人（2026-10-05）。
@ ユーザー「シルフカンパニーにマスターボール売る人作成して」→ 6 万 5535 円・殿堂入りのあと・1F 受付の横。
@ 素の 1F の人物は 1 人（08373CC4・受付）。人物の表を写して 2 人目（localId 2・(5,7)・受付と同じカウンターの奥・下向き）を足し、
@ 地図のイベントの人数（08373D10）を 2 に・表の語（08373D14）を repoints で向ける。
@ ユーザー「スクリプトで変えてほしい」→「お店の画面のまま・値段だけ売り場で」: 道具の表の値段は 0 のまま（売れない）。
@ この売り場（KantoSilph_MasterBallItems）の買う画面のときだけ 65535 円（src/mega_shop.c の MegaShop_ItemIdGetPrice）。
@ 文は strings/kanto_silph_master_ball.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ FLAG_SYS_GAME_CLEAR, 0x82C
.equ ITEM_MASTER_BALL, 1
.equ OBJ_EVENT_GFX_WORKER_F, 48
.equ OBJ_EVENT_GFX_CLERK, 68

.align 2
.global KantoSilph1F_Objects
KantoSilph1F_Objects:
	.byte 1, OBJ_EVENT_GFX_WORKER_F, 0, 0
	.2byte 3, 7
	.byte 3, 8, 0x11, 0
	.2byte 0, 0
	.4byte 0x0816685A
	.2byte 0x3F, 0
	.byte 2, OBJ_EVENT_GFX_CLERK, 0, 0
	.2byte 5, 7
	.byte 3, 8, 0x11, 0
	.2byte 0, 0
	.4byte KantoSilph_EventScript_MasterBall
	.2byte 0, 0

.align 2
.global KantoSilph_EventScript_MasterBall
KantoSilph_EventScript_MasterBall:
	lock
	faceplayer
	checkflag FLAG_SYS_GAME_CLEAR
	if 0x0 _goto KantoSilph_MasterBallNotYet
	message gText_KantoSilph_MasterBallHello
	waitmessage
	pokemart KantoSilph_MasterBallItems
	msgbox gText_KantoSilph_MasterBallBye MSG_KEEPOPEN
	closemessage
	release
	end

KantoSilph_MasterBallNotYet:
	msgbox gText_KantoSilph_MasterBallNotYet MSG_KEEPOPEN
	closemessage
	release
	end

.align 1
.global KantoSilph_MasterBallItems
KantoSilph_MasterBallItems:
	.2byte ITEM_MASTER_BALL
	.2byte 0
