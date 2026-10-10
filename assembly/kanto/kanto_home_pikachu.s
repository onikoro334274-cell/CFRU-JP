@ カントーの自宅 1 階: お母さんの横のピカチュウ（2026-10-06）。
@ ユーザー「ピカチュウへの導線がほしい　ママの横にピカチュウ置くとか　ポケモンもらったら渡すものがあるからとか」。
@ 素の 1 階の人物は 1 人（0837D720・お母さん）。人物の表を写して 2 人目（localId 2・(9,5)＝お母さんの右下・左向き（(8,3) はお母さんの頭に重なった）・最初の贈り物で
@ お母さんが歩く道（右・右・上）の外）にピカチュウ（OBJ_EVENT_GFX_PIKACHU 120）を足し、地図のイベントの人数（0837D764）を 2 に・
@ 表の語（0837D768）を repoints で向ける。ピカチュウは もらった印 KANTO_FLAG_MOM_PIKACHU で消える。
@ 1 人目（お母さん）は今の ROM と同じ（絵 0x22・+3 は 07 = bytereplacement 0837D721／0837D723 と同じ・スクリプトは MomTalk）。

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ OBJ_EVENT_GFX_PIKACHU, 120         @ pokefirered include/constants/event_objects.h
.equ KANTO_FLAG_MOM_PIKACHU, 0x13EC     @ build_hoenn_scripts.RESERVED_FLAGS（kanto_home_scripts.s と同じ）
.equ FLAG_SYS_POKEMON_GET, 0x828
.equ SPECIES_PIKACHU, 25

.align 2
.global KantoHome1F_Objects
KantoHome1F_Objects:
	.byte 1, 0x22, 0, 7
	.2byte 8, 4
	.byte 3, 9, 0, 0
	.2byte 0, 0
	.4byte KantoHome1F_EventScript_MomTalk
	.2byte 0, 0
	.byte 2, OBJ_EVENT_GFX_PIKACHU, 0, 0
	.2byte 9, 5
	.byte 3, 9, 0, 0
	.2byte 0, 0
	.4byte KantoHome1F_EventScript_Pikachu
	.2byte KANTO_FLAG_MOM_PIKACHU, 0

.align 2
.global KantoHome1F_EventScript_Pikachu
KantoHome1F_EventScript_Pikachu:
	lock
	faceplayer
	cry SPECIES_PIKACHU 0
	msgbox gText_KantoHomePika_Cry MSG_KEEPOPEN
	waitcry
	checkflag FLAG_SYS_POKEMON_GET
	if 0x1 _goto KantoHome1F_PikachuHasMon
	msgbox gText_KantoHomePika_NoMon MSG_KEEPOPEN
	closemessage
	release
	end

KantoHome1F_PikachuHasMon:
	msgbox gText_KantoHomePika_HasMon MSG_KEEPOPEN
	closemessage
	release
	end
