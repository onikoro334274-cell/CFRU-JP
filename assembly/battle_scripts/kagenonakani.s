.text
.thumb
.align 2
.include "../asm_defines.s"
.include "../battle_script_macros.s"
.global BattleScript_UrameshiyaActivates
BattleScript_UrameshiyaActivates:
	call BattleScript_AbilityPopUp
	setword BATTLE_STRING_LOADER gText_UrameshiyaActivates
	printstring 0x184
	waitmessage DELAY_1SECOND
	call BattleScript_AbilityPopUpRevert
	return
