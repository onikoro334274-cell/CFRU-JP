@ キンセツの自転車屋: 自転車にモード（通常 / マッハ / ダート）を追加してもらう（2026-09-24 ユーザー要望）。
@ 根拠は docs/research/2026-09-24_bike-modes.md
@
@ ★ 自転車屋（MauvilleCity_BikeShop）はイベントを移植していない地図なので、
@   tools/rom/build_hoenn.py の EXTRA_NPCS が店主と店員をここのスクリプトで置く。
@ ★ 素 FRLG の自転車は 1 台（ITEM_BICYCLE）なので、エメラルドのように 2 台から選ぶのではなく、
@   持っている自転車に「モード」を付ける。済んだ印は予約フラグ HOENN_FLAG_BIKE_MODES
@   （build_hoenn_scripts.RESERVED_FLAGS）。src/hoenn_bike.c が L ボタンでモードを切り替える。
@ ★ gHoennBikeScript_Mode* は L ボタンを押したときの表示（CFRU の SystemScript_EnableBikeTurboBoost と同じ形）。

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ HOENN_FLAG_BIKE_MODES, 0x13ED     @ build_hoenn_scripts.RESERVED_FLAGS と同じ（検算が見る）
.equ ITEM_BICYCLE, 360                 @ include/constants/items.h
.equ MUS_OBTAIN_KEY_ITEM, 318          @ pokefirered include/constants/songs.h
.equ SPECIAL_STOP_SOUNDS, 0x9A         @ assembly/overworld_scripts/system_scripts.s と同じ

.align 2
.global HoennBikeShop_EventScript_Rydel
HoennBikeShop_EventScript_Rydel:
	lock
	faceplayer
	checkflag HOENN_FLAG_BIKE_MODES
	if 0x1 _goto HoennBikeShop_EventScript_Again
	checkitem ITEM_BICYCLE 1
	compare LASTRESULT 0
	if 0x1 _goto HoennBikeShop_EventScript_NoBike
	msgbox gText_HoennBike_Offer MSG_YESNO
	compare LASTRESULT 0
	if 0x1 _goto HoennBikeShop_EventScript_Declined
	msgbox gText_HoennBike_Working MSG_KEEPOPEN
	fanfare MUS_OBTAIN_KEY_ITEM
	preparemsg gText_HoennBike_Got
	waitmsg
	waitfanfare
	setflag HOENN_FLAG_BIKE_MODES
	msgbox gText_HoennBike_Explain MSG_KEEPOPEN
	release
	end

HoennBikeShop_EventScript_Again:
	msgbox gText_HoennBike_Again MSG_KEEPOPEN
	release
	end

HoennBikeShop_EventScript_NoBike:
	msgbox gText_HoennBike_NoBike MSG_KEEPOPEN
	release
	end

HoennBikeShop_EventScript_Declined:
	msgbox gText_HoennBike_Declined MSG_KEEPOPEN
	release
	end

.align 2
.global HoennBikeShop_EventScript_Staff
HoennBikeShop_EventScript_Staff:
	msgbox gText_HoennBike_Staff MSG_FACE
	end

.align 2
.global gHoennBikeScript_ModeNormal
gHoennBikeScript_ModeNormal:
	lockall
	special SPECIAL_STOP_SOUNDS
	sound 0x3
	signmsg
	msgboxsign
	msgbox gText_HoennBike_ModeNormal MSG_SIGN
	releaseall
	end

.align 2
.global gHoennBikeScript_ModeMach
gHoennBikeScript_ModeMach:
	lockall
	special SPECIAL_STOP_SOUNDS
	sound 0x2
	signmsg
	msgboxsign
	msgbox gText_HoennBike_ModeMach MSG_SIGN
	releaseall
	end

.align 2
.global gHoennBikeScript_ModeDirt
gHoennBikeScript_ModeDirt:
	lockall
	special SPECIAL_STOP_SOUNDS
	sound 0x2
	signmsg
	msgboxsign
	msgbox gText_HoennBike_ModeDirt MSG_SIGN
	releaseall
	end
