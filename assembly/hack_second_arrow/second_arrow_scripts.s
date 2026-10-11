.text
.thumb
.align 2
@ にのや（src/hack_second_arrow.c・2026-10-10）: 技が失敗したあと。能力は C が先に上げてある（ここは絵と文と、道具をなくすだけ）

.include "../asm_defines.s"
.include "../battle_script_macros.s"

.global BattleScript_SecondArrowRet

BattleScript_SecondArrowRet:
	playanimation BANK_SCRIPTING ANIM_ITEM_USE 0x0
	setbyte 0x2023F3F 0x0
	playstatchangeanimation BANK_SCRIPTING STAT_ANIM_ATK | STAT_ANIM_SPD | STAT_ANIM_SPATK STAT_ANIM_UP | STAT_ANIM_IGNORE_ABILITIES
	setword BATTLE_STRING_LOADER gText_SecondArrow_StatsUp
	printstring 0x184
	waitmessage DELAY_1SECOND
	removeitem BANK_SCRIPTING
	return
