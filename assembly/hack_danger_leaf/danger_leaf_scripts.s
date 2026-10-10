.text
.thumb
.align 2
@ あぶないはっぱ（src/hack_danger_leaf.c・2026-10-09）: 場に出たとき。能力と こんらんは C が先に変えてある（ここは絵と文だけ）

.include "../asm_defines.s"
.include "../battle_script_macros.s"

.global BattleScript_DangerLeafRet
.global BattleScript_DangerLeafEnd2

BattleScript_DangerLeafEnd2:
	call BattleScript_DangerLeafRet
	end2

BattleScript_DangerLeafRet:
	playanimation BANK_SCRIPTING ANIM_ITEM_USE 0x0
	setbyte 0x2023F3F 0x0
	playstatchangeanimation BANK_SCRIPTING STAT_ANIM_ATK | STAT_ANIM_DEF | STAT_ANIM_SPD | STAT_ANIM_SPATK | STAT_ANIM_SPDEF STAT_ANIM_UP | STAT_ANIM_IGNORE_ABILITIES | STAT_ANIM_BY_TWO
	setword BATTLE_STRING_LOADER gText_DangerLeaf_StatsUp
	printstring 0x184
	waitmessage DELAY_1SECOND
	chosenstatusanimation BANK_SCRIPTING TRUE STATUS2_CONFUSION
	setword BATTLE_STRING_LOADER gText_DangerLeaf_Confused
	printstring 0x184
	waitmessage DELAY_1SECOND
	removeitem BANK_SCRIPTING   @ メンタルハーブと同じく使ったらなくなる（戦いのあと src/end_battle.c が戻す）
	return
