@ かがやくおまもり（バッグで つかう → src/hack_shining_charm.c の FieldUseFunc_ShiningCharm）。ON/OFF を切り替えて知らせる。
@ 文は strings/hack_shining_charm.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.align 2
.global EventScript_ShiningCharm
EventScript_ShiningCharm:
	lockall
	callasm HackShiningCharm_Toggle
	compare LASTRESULT, 1
	if equal _goto EventScript_ShiningCharm_On
	msgbox gText_ShiningCharm_Off MSG_KEEPOPEN
	closemessage
	releaseall
	end

EventScript_ShiningCharm_On:
	msgbox gText_ShiningCharm_On MSG_KEEPOPEN
	closemessage
	releaseall
	end
