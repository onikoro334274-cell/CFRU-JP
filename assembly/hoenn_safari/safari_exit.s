@ ホウエンのサファリゾーンから出る（2026-10-04・ユーザー「ホウエンのサファリゾーンも実装して」）。
@ 素 FRLG のサファリの終わり（歩数切れ・ボール切れ・やめる）は SafariZone_EventScript_Exit（081A5DAC）と
@ SafariZone_EventScript_OutOfBallsMidBattle（081A5D9B）でセキチクのゲートへ戻す。どちらも頭の setvar（5 バイト）を
@ goto に書き換えて（bytereplacement・repoints）ここへ来させ、ホウエンにいれば 121 ばんどうろのゲートへ戻す。
@ ホウエンでなければ素の setvar をここで行い、素の続き（+5）へ戻る。
@ ゲートは VAR_SAFARI_ZONE_STATE = 1 で主人公を外へ歩かせる（エメラルドの Route121_SafariZoneEntrance_OnFrame）。

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ VAR_SCENE_FUCHSIA_SAFARI, 0x406E   @ 素の VAR_MAP_SCENE_FUCHSIA_CITY_SAFARI_ZONE_ENTRANCE
.equ VAR_HOENN_SAFARI_STATE, 0x5085     @ tools/rom/hoenn_scripts.json の VAR_SAFARI_ZONE_STATE（verify_hoenn_safari.py が見る）
.equ SP_EXIT_SAFARI_MODE, 0xCE          @ 素の special ExitSafariMode（素のスクリプトの 25 CE 00）
.equ GATE_GROUP, 66                     @ Route121_SafariZoneEntrance（エメラルドの 23.0 + 43）
.equ GATE_MAP, 0
.equ VANILLA_EXIT_REST, 0x081A5DB1      @ 素の Exit の setvar のあと（special ExitSafariMode; warp セキチク）
.equ VANILLA_MIDBATTLE_REST, 0x081A5DA0 @ 素の OutOfBallsMidBattle の setvar のあと（special; setwarp セキチク）

.global HoennSafari_EventScript_Exit
HoennSafari_EventScript_Exit:
	callasm HoennSafari_InHoenn
	compare LASTRESULT, 1
	if equal _goto HoennSafari_EventScript_ExitHoenn
	setvar VAR_SCENE_FUCHSIA_SAFARI, 1
	goto VANILLA_EXIT_REST

HoennSafari_EventScript_ExitHoenn:
	setvar VAR_HOENN_SAFARI_STATE, 1
	special SP_EXIT_SAFARI_MODE
	warp GATE_GROUP, GATE_MAP, 0xFF, 2, 5
	waitstate
	end

.global HoennSafari_EventScript_OutOfBallsMidBattle
HoennSafari_EventScript_OutOfBallsMidBattle:
	callasm HoennSafari_InHoenn
	compare LASTRESULT, 1
	if equal _goto HoennSafari_EventScript_OutOfBallsMidBattleHoenn
	setvar VAR_SCENE_FUCHSIA_SAFARI, 3
	goto VANILLA_MIDBATTLE_REST

HoennSafari_EventScript_OutOfBallsMidBattleHoenn:
	setvar VAR_HOENN_SAFARI_STATE, 1
	special SP_EXIT_SAFARI_MODE
	setwarp GATE_GROUP, GATE_MAP, 0xFF, 2, 5
	end
