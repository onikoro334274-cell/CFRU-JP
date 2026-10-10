.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

@ ガチャ (SoulGold のものを移植)
@ マシンやNPCから以下のいずれかを呼び出して使います。
@   EventScript_Gacha1 (ベーシック  さいてい かけきん   50まい)
@   EventScript_Gacha2 (グレート    さいてい かけきん  250まい)
@   EventScript_Gacha3 (ウルトラ    さいてい かけきん 1000まい)
@   EventScript_Gacha4 (マスター    さいてい かけきん 3000まい)
@ プレイヤーのコインを直接かけます。かけきんが多いほど、まだ持っていないポケモンが出やすくなります。
@ StartGacha の結果: LASTRESULT 0 = 手持ちに追加 / 1 = ボックスに転送 / 2 = ガチャを回さずにやめた
@ 受け取ったポケモンの種族は Var 0x8005 に入ります。

.equ ITEM_COIN_CASE_ID, 260

.global EventScript_Gacha1
EventScript_Gacha1:
	lockall
	checkitem ITEM_COIN_CASE_ID 0x1
	compare LASTRESULT 0x0
	if equal _goto Gacha_NoCoinCase
	checkcoins 0x8005
	compare 0x8005 50
	if lessthan _goto Gacha_NotEnoughCoins
	showcoins 0x0 0x0
	msgbox gText_Gacha_Ask1 MSG_YESNO
	compare LASTRESULT 0x1
	if notequal _goto Gacha_Cancel
	hidecoins 0x0 0x0
	setvar 0x8004 0x1
	goto Gacha_Start

.global EventScript_Gacha2
EventScript_Gacha2:
	lockall
	checkitem ITEM_COIN_CASE_ID 0x1
	compare LASTRESULT 0x0
	if equal _goto Gacha_NoCoinCase
	checkcoins 0x8005
	compare 0x8005 250
	if lessthan _goto Gacha_NotEnoughCoins
	showcoins 0x0 0x0
	msgbox gText_Gacha_Ask2 MSG_YESNO
	compare LASTRESULT 0x1
	if notequal _goto Gacha_Cancel
	hidecoins 0x0 0x0
	setvar 0x8004 0x2
	goto Gacha_Start

.global EventScript_Gacha3
EventScript_Gacha3:
	lockall
	checkitem ITEM_COIN_CASE_ID 0x1
	compare LASTRESULT 0x0
	if equal _goto Gacha_NoCoinCase
	checkcoins 0x8005
	compare 0x8005 1000
	if lessthan _goto Gacha_NotEnoughCoins
	showcoins 0x0 0x0
	msgbox gText_Gacha_Ask3 MSG_YESNO
	compare LASTRESULT 0x1
	if notequal _goto Gacha_Cancel
	hidecoins 0x0 0x0
	setvar 0x8004 0x3
	goto Gacha_Start

.global EventScript_Gacha4
EventScript_Gacha4:
	lockall
	checkitem ITEM_COIN_CASE_ID 0x1
	compare LASTRESULT 0x0
	if equal _goto Gacha_NoCoinCase
	checkcoins 0x8005
	compare 0x8005 3000
	if lessthan _goto Gacha_NotEnoughCoins
	showcoins 0x0 0x0
	msgbox gText_Gacha_Ask4 MSG_YESNO
	compare LASTRESULT 0x1
	if notequal _goto Gacha_Cancel
	hidecoins 0x0 0x0
	setvar 0x8004 0x4
	goto Gacha_Start

Gacha_Start:
	callasm StartGacha
	waitstate
	compare LASTRESULT 0x1
	if notequal _goto Gacha_End
	bufferpokemon 0x0 0x8005
	msgbox gText_Gacha_SentToPC MSG_NORMAL
Gacha_End:
	releaseall
	end

Gacha_Cancel:
	hidecoins 0x0 0x0
	releaseall
	end

Gacha_NoCoinCase:
	msgbox gText_Gacha_NoCoinCase MSG_NORMAL
	releaseall
	end

Gacha_NotEnoughCoins:
	msgbox gText_Gacha_NotEnoughCoins MSG_NORMAL
	releaseall
	end
