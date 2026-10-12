@ ふしぎなおくりものの配達員（ポケセン 2 階）と おもいだしノート のスクリプト（2026-10-03・src/hack_mystery_gift.c）。
@ 文は strings/hack_mystery_gift.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ ITEM_EXP_SHARE, 182
.equ ITEM_LUCKY_EGG, 197                @ include/constants/items.h（★ 2026-10-04 ユーザー「しあわせタマゴふしぎなおくりものに追加して」）
.equ KANTO_FLAG_MG_LUCKY_EGG, 0x12EF    @ 渡した印（持たせられるので 1 回だけ・build_hoenn_scripts.RESERVED_FLAGS）
.equ ITEM_MOMO_ICE, 0x343               @ ★ 2026-10-05 ももアイス（include/constants/items.h）
.equ KANTO_FLAG_MG_MOMO_ICE, 0x12ED     @ 渡した印（1 回だけ）
.equ ITEM_MOVE_NOTE, 0x33E              @ include/constants/items.h（tools/megas/build_za_megas.HACK_ITEMS）
.equ SP_CHOOSE_MON_FOR_MOVE_RELEARNER, 0xDB    @ 素 FRLG の ChooseMonForMoveRelearner（2 のしまの技思い出しの人と同じ）
.equ SP_TEACH_MOVE_RELEARNER_MOVE, 0xE0        @ TeachMoveRelearnerMove
.equ SP_IS_SELECTED_MON_EGG, 0x148             @ IsSelectedMonEgg

@ ---- 配達員（素の GetSavedRamScriptIfValid の代わりに HackMG_GetDeliverymanScript が返す）----
@ がくしゅうそうち・おもいだしノートのうち持っていないもの と しあわせタマゴ（1 回だけ）を渡して、消える
.align 2
.global HackMG_EventScript_Deliveryman
HackMG_EventScript_Deliveryman:
	lock
	faceplayer
	msgbox gText_HackMG_DeliverymanHello MSG_KEEPOPEN
.global HackMG_Deliveryman_Gifts
HackMG_Deliveryman_Gifts:                           @ シリアルコードの品のあとはここから（serial_gift_scripts.s）
	checkitem ITEM_EXP_SHARE 1
	compare LASTRESULT 1
	if equal _goto HackMG_Deliveryman_Note
	obtainitem ITEM_EXP_SHARE 1
HackMG_Deliveryman_Note:
	checkitem ITEM_MOVE_NOTE 1
	compare LASTRESULT 1
	if equal _goto HackMG_Deliveryman_LuckyEgg
	obtainitem ITEM_MOVE_NOTE 1
HackMG_Deliveryman_LuckyEgg:
	checkflag KANTO_FLAG_MG_LUCKY_EGG
	if 0x1 _goto HackMG_Deliveryman_MomoIce
	obtainitem ITEM_LUCKY_EGG 1
	compare LASTRESULT 1
	if 0x5 _goto HackMG_Deliveryman_Bye            @ バッグがいっぱい: 印を立てず、また次に
	setflag KANTO_FLAG_MG_LUCKY_EGG
HackMG_Deliveryman_MomoIce:
	checkflag KANTO_FLAG_MG_MOMO_ICE
	if 0x1 _goto HackMG_Deliveryman_Bye
	obtainitem ITEM_MOMO_ICE 1
	compare LASTRESULT 1
	if 0x5 _goto HackMG_Deliveryman_Bye
	setflag KANTO_FLAG_MG_MOMO_ICE
HackMG_Deliveryman_Bye:
	msgbox gText_HackMG_DeliverymanBye MSG_KEEPOPEN
	closemessage
	removeobject LASTTALKED
	release
	end

@ ---- おもいだしノート（バッグで つかう → src/hack_mystery_gift.c の FieldUseFunc_MoveNote）----
@ 素の 2 のしまの技思い出しの人（TwoIsland_House_EventScript_ChooseMonToTutor）と同じ流れで、キノコを払う所だけ無い
.align 2
.global EventScript_MoveNote
EventScript_MoveNote:
	lockall
EventScript_MoveNote_Choose:
	msgbox gText_MoveNote_WhichMon MSG_KEEPOPEN
	special SP_CHOOSE_MON_FOR_MOVE_RELEARNER
	waitstate
	callasm HackPartyChoice_RestoreSlot                @ ★ 2026-10-04 0x8004 を手持ちの画面の選んだ番号に戻す（フィールドへ戻るとき ON_RESUME が 0x8004 を書き換える地図がある＝リーグの部屋は 0 にする・src/hack_mystery_gift.c）
	compare 0x8004, 6
	if greaterorequal _goto EventScript_MoveNote_End
	special SP_IS_SELECTED_MON_EGG
	compare LASTRESULT, 1
	if equal _goto EventScript_MoveNote_Egg
	compare 0x8005, 0
	if equal _goto EventScript_MoveNote_NoMove
	msgbox gText_MoveNote_WhichMove MSG_KEEPOPEN
	special SP_TEACH_MOVE_RELEARNER_MOVE
	waitstate
	compare 0x8004, 0
	if equal _goto EventScript_MoveNote_Choose
EventScript_MoveNote_End:
	releaseall
	end

EventScript_MoveNote_Egg:
	msgbox gText_MoveNote_Egg MSG_KEEPOPEN
	closemessage
	goto EventScript_MoveNote_Choose

EventScript_MoveNote_NoMove:
	msgbox gText_MoveNote_NoMove MSG_KEEPOPEN
	closemessage
	goto EventScript_MoveNote_Choose
