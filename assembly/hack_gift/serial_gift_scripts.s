@ シリアルコードの ふしぎなおくりもの: ポケセン 2 階の配達員が受け取り待ちの品を渡す（2026-10-11・src/hack_serial_gift.c・patches/0329）。
@ 文は strings/hack_serial_gift.string・あいさつと別れは strings/hack_mystery_gift.string（今までの配達員と同じ）

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.align 2
.global HackSerial_EventScript_Deliveryman
HackSerial_EventScript_Deliveryman:
	lock
	faceplayer
	msgbox gText_HackMG_DeliverymanHello MSG_KEEPOPEN
	callasm HackSerial_Prepare
	compare 0x8004 0
	if equal _goto HackSerial_Deliveryman_Item
	compare 0x8004 1
	if equal _goto HackSerial_Deliveryman_Mon
	goto HackSerial_Deliveryman_Bye

HackSerial_Deliveryman_Item:
	obtainitem 0x8005 0x8006
	compare LASTRESULT 0
	if equal _goto HackSerial_Deliveryman_BagFull
	goto HackSerial_Deliveryman_Done

HackSerial_Deliveryman_BagFull:
	msgbox gText_HackSerial_BagFull MSG_KEEPOPEN
	closemessage
	release
	end

HackSerial_Deliveryman_Mon:
	callasm HackSerial_GiveMon
	compare LASTRESULT 2
	if equal _goto HackSerial_Deliveryman_NoRoom
	fanfare 0x101
	msgbox gText_HackSerial_GotMon MSG_KEEPOPEN
	waitfanfare
	compare LASTRESULT 1
	if notequal _goto HackSerial_Deliveryman_Done
	msgbox gText_HackSerial_SentToPC MSG_KEEPOPEN
	goto HackSerial_Deliveryman_Done

HackSerial_Deliveryman_NoRoom:
	msgbox gText_HackSerial_NoRoom MSG_KEEPOPEN
	closemessage
	release
	end

@ 渡したあと: 受け取り待ちを消してレポートを書く・今までの おくりもの がまだあれば続けて渡す（配達員を消さない）
HackSerial_Deliveryman_Done:
	callasm HackSerial_Finish
	compare LASTRESULT 1
	if equal _goto HackMG_Deliveryman_Gifts

HackSerial_Deliveryman_Bye:
	msgbox gText_HackMG_DeliverymanBye MSG_KEEPOPEN
	closemessage
	removeobject LASTTALKED
	release
	end
