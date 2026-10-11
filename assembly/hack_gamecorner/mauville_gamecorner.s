@ キンセツシティのゲームコーナー（2026-10-10 ユーザー「ミニゲーム導入ファイル これキンセツシティに実装」）
@ 左のルーレットの台 = けいば（assembly/overworld_scripts/derby.s の EventScript_Derby）
@ 右のルーレットの台 = ガチャガチャ（assembly/overworld_scripts/gacha.s の EventScript_Gacha1〜4 をここで選ぶ）
@ 台に話しかける看板は tools/rom/build_hoenn.py の EXTRA_SIGNS（MauvilleCity_GameCorner）

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ SP_ADD_MENU, 0x25

@ ★ 2026-10-11 コイン売り（tools/rom/build_hoenn.py の EXTRA_NPCS）: 素のタマムシのゲームコーナーの店員の台本（CeladonCity_GameCorner_EventScript_CoinsClerk）の選択肢から先を使う
.global HackScr_MauvilleCoinsClerk
HackScr_MauvilleCoinsClerk:
	lock
	faceplayer
	showmoneybox 0 0 0
	showcoinsbox 0 5
	message gText_MauvilleCoins_Welcome
	waitmessage
	goto 0x081850B9   @ 素の台本の multichoice から（あいさつの文だけ「ロケット ゲーム コーナー」をキンセツに差し替え）

.global HackScr_MauvilleGachaMenu
HackScr_MauvilleGachaMenu:
	lockall
	message gText_MauvilleGacha_Which
	waitmessage
	setvar 0x8006 0
	loadpointer 0 gText_MauvilleGacha_Menu1
	special SP_ADD_MENU
	setvar 0x8006 1
	loadpointer 0 gText_MauvilleGacha_Menu2
	special SP_ADD_MENU
	setvar 0x8006 2
	loadpointer 0 gText_MauvilleGacha_Menu3
	special SP_ADD_MENU
	setvar 0x8006 3
	loadpointer 0 gText_MauvilleGacha_Menu4
	special SP_ADD_MENU
	setvar 0x8006 4
	loadpointer 0 gText_MauvilleGacha_Cancel
	special SP_ADD_MENU
	setvar 0x8006 15	@ 窓の幅（タイル・11 だと「50まい」が切れた）
	multichoice 0, 0, 0x23, 0
	compare LASTRESULT 0
	if 0x1 _goto EventScript_Gacha1
	compare LASTRESULT 1
	if 0x1 _goto EventScript_Gacha2
	compare LASTRESULT 2
	if 0x1 _goto EventScript_Gacha3
	compare LASTRESULT 3
	if 0x1 _goto EventScript_Gacha4
	closeonkeypress
	releaseall
	end
