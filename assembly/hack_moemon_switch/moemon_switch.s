@ Moemonボタン（バッグで つかう → src/hack_moemon_switch.c の FieldUseFunc_MoemonSwitch）。ON/OFF を切り替えて知らせる。
@ 文は strings/hack_moemon_switch.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.align 2
.global EventScript_MoemonSwitch
EventScript_MoemonSwitch:
	lockall
	callasm HackMoemonSwitch_Toggle
	compare LASTRESULT, 1
	if equal _goto EventScript_MoemonSwitch_On
	msgbox gText_MoemonSwitch_Off MSG_KEEPOPEN
	closemessage
	releaseall
	end

EventScript_MoemonSwitch_On:
	msgbox gText_MoemonSwitch_On MSG_KEEPOPEN
	closemessage
	releaseall
	end
