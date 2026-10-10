@ クチバシティ ポケモンだいすきクラブの「ボールを入れ替えてくれる人」（2026-10-04）。
@ ユーザー「ポケモンの捕まえたときのボールを手持ちの選択したボール変更してくれるNPCつくって配置して」。
@ 人物の表は assembly/kanto/kanto_gible_egg.s の KantoFanClub_Objects（8 人目・localId 8・(2, 7)）。
@ 手持ちを選ぶ（special 0x9F）→ バッグのボールのポケットから選ぶ（special 0xB1・0x8000 = 2）→ src/hack_ball_change.c が入れ替え、ボールを 1 個使う。
@ 文は strings/kanto_ball_change.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ SP_CHOOSE_PARTY_MON, 0x9F
.equ SP_IS_SELECTED_MON_EGG, 0x148
.equ SP_CHOOSE_ITEM_FROM_BAG, 0xB1

.align 2
.global KantoBallChange_EventScript_Talk
KantoBallChange_EventScript_Talk:
	lock
	faceplayer
	msgbox gText_KantoBall_Intro MSG_YESNO
	compare LASTRESULT 1
	if 0x5 _goto KantoBallChange_Cancel
	msgbox gText_KantoBall_WhichMon MSG_KEEPOPEN
	closemessage
	special SP_CHOOSE_PARTY_MON
	waitstate
	callasm HackPartyChoice_RestoreSlot                @ ★ 2026-10-04 0x8004 を手持ちの画面の選んだ番号に戻す（フィールドへ戻るとき ON_RESUME が 0x8004 を書き換える地図がある＝リーグの部屋は 0 にする・src/hack_mystery_gift.c）
	compare 0x8004 6
	if 0x4 _goto KantoBallChange_Cancel                @ 0x4 = 以上（やめた）
	special SP_IS_SELECTED_MON_EGG
	compare LASTRESULT 1
	if 0x1 _goto KantoBallChange_Egg
	msgbox gText_KantoBall_WhichBall MSG_KEEPOPEN
	closemessage
	setvar 0x8000 2                                    @ ボールのポケットから開く
	fadescreen 1                                       @ ★ 黒くしてから開く（special 0xB1 は自分では暗くしない・しないと 1 コマ地図のタイルが崩れて見えた）
	special SP_CHOOSE_ITEM_FROM_BAG
	waitstate
	callasm HackBallChange_Apply
	compare LASTRESULT 0
	if 0x1 _goto KantoBallChange_Done
	compare LASTRESULT 2
	if 0x1 _goto KantoBallChange_NotBall
	compare LASTRESULT 3
	if 0x1 _goto KantoBallChange_Same
	goto KantoBallChange_Cancel

KantoBallChange_Done:
	bufferpartypokemon 0x0 0x8004
	bufferitem 0x1 0x800E
	msgbox gText_KantoBall_Done MSG_KEEPOPEN
	closemessage
	release
	end

KantoBallChange_NotBall:
	msgbox gText_KantoBall_NotBall MSG_KEEPOPEN
	closemessage
	release
	end

KantoBallChange_Same:
	msgbox gText_KantoBall_Same MSG_KEEPOPEN
	closemessage
	release
	end

KantoBallChange_Egg:
	msgbox gText_KantoBall_Egg MSG_KEEPOPEN
	closemessage
	release
	end

KantoBallChange_Cancel:
	msgbox gText_KantoBall_Cancel MSG_KEEPOPEN
	closemessage
	release
	end
