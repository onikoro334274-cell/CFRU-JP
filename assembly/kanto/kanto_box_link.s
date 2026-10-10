@ ボックスリンク（ITEM_BOX_LINK・src/hack_box_link.c・2026-10-04）。
@ ・バッグで つかう → EventScript_BoxLink: 素の「ポケモンあずかりシステム」（special 0x3C ShowPokemonStorageSystemPC）。
@   素のパソコンと同じ（あずけると回復する）。ポケモンリーグ・バトルフロンティアでは使えない（src/hack_box_link.c が断る）
@ ・マサキ（25 ばんどうろ みさきのこや・人もピッピも同じスクリプト 0818E7F3）: サントアンヌ号のチケット（FLAG_GOT_SS_TICKET 0x234）のあと話すと渡す。
@   人物の表のスクリプトの語 08381FDC / 08381FF4 を repoints でここへ向ける。渡したあと（持っている）は素のスクリプトへ
@ 文は strings/kanto_box_link.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ ITEM_BOX_LINK, 0x342                 @ include/constants/items.h
.equ FLAG_GOT_SS_TICKET, 0x234            @ pokefirered include/constants/flags.h
.equ SP_SHOW_POKEMON_STORAGE_SYSTEM_PC, 0x3C
.equ BILL_ORIGINAL_SCRIPT, 0x0818E7F3     @ 素の Route25_SeaCottage_EventScript_Bill

.align 2
.global EventScript_BoxLink
EventScript_BoxLink:
	lockall
	special SP_SHOW_POKEMON_STORAGE_SYSTEM_PC
	waitstate
	releaseall
	end

.align 2
.global KantoBoxLink_EventScript_Bill
KantoBoxLink_EventScript_Bill:
	checkflag FLAG_GOT_SS_TICKET
	if 0x0 _goto KantoBoxLink_BillOriginal
	checkitem ITEM_BOX_LINK 1
	compare LASTRESULT 1
	if 0x1 _goto KantoBoxLink_BillOriginal
	lock
	faceplayer
	msgbox gText_BoxLink_BillOffer MSG_KEEPOPEN
	obtainitem ITEM_BOX_LINK 1
	compare LASTRESULT 1
	if 0x5 _goto KantoBoxLink_BillEnd
	msgbox gText_BoxLink_BillExplain MSG_KEEPOPEN
KantoBoxLink_BillEnd:
	closemessage
	release
	end

KantoBoxLink_BillOriginal:
	goto BILL_ORIGINAL_SCRIPT
