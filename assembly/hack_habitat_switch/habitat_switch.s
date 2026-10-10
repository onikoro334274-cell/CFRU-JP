@ せいそくスイッチ（バッグで つかう → src/hack_habitat_switch.c の FieldUseFunc_HabitatSwitch）。切り替えて知らせる。
@ 文は strings/hack_habitat_switch.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.align 2
.global EventScript_HabitatSwitch
EventScript_HabitatSwitch:
	lockall
	callasm HackHabitatSwitch_Toggle
	compare LASTRESULT, 1
	if equal _goto EventScript_HabitatSwitch_Original
	msgbox gText_HabitatSwitch_Shuffled MSG_KEEPOPEN
	closemessage
	releaseall
	end

EventScript_HabitatSwitch_Original:
	msgbox gText_HabitatSwitch_Original MSG_KEEPOPEN
	closemessage
	releaseall
	end
