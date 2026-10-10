.text
.thumb
.align 2
@ なつき度で戦いに効く効果の文（src/hack_affection.c・2026-10-09）。効果そのものは C が先に済ませてある（ここは文と絵の更新だけ）

.include "../asm_defines.s"
.include "../battle_script_macros.s"

.global BattleScript_AffectionEndured
.global BattleScript_AffectionCured
.global BattleScript_AffectionLastMon
.global BattleScript_AffectionBond
.global BattleScript_AffectionCover
.global BattleScript_AffectionWokeUp
.global BattleScript_AffectionUnfroze
.global BattleScript_AffectionShookParalysis
.global BattleScript_AffectionStatBlock

@ ふんばる（技のダメージの文のところ・src/general_bs_commands.c が呼ぶ）
BattleScript_AffectionEndured:
	setword BATTLE_STRING_LOADER gText_Affection_Endured
	printstring 0x184
	waitmessage DELAY_1SECOND
	return

@ ターンの終わりに状態異常を治す（BattleScriptExecute）
BattleScript_AffectionCured:
	setword BATTLE_STRING_LOADER gText_Affection_Cured
	printstring 0x184
	waitmessage DELAY_1SECOND
	refreshhpbar BANK_SCRIPTING
	end2

@ さいごの いっぴき（技を出す前・1 戦闘 1 回）
BattleScript_AffectionLastMon:
	setword BATTLE_STRING_LOADER gText_Affection_LastMon
	printstring 0x184
	waitmessage DELAY_1SECOND
	return

@ きずなのいちげき（damagecalc の前・技の名前の文のあと）
BattleScript_AffectionBond:
	waitmessage DELAY_1SECOND   @ 技の名前の文（attackstring は待たない）を読めるように
	setword BATTLE_STRING_LOADER gText_Affection_Bond
	printstring 0x184
	waitmessage DELAY_1SECOND
	return

@ かばう（damagecalc の後・相手を変えてもう一度 damagecalc）
BattleScript_AffectionCover:
	waitmessage DELAY_1SECOND   @ 技の名前の文（attackstring は待たない）を読めるように
	setword BATTLE_STRING_LOADER gText_Affection_Cover
	printstring 0x184
	waitmessage DELAY_1SECOND
	return

@ ふりかえる（ねむり・こおりは C が治してある・まひは止まらずに動く）
BattleScript_AffectionWokeUp:
	setword BATTLE_STRING_LOADER gText_Affection_WokeUp
	printstring 0x184
	waitmessage DELAY_1SECOND
	refreshhpbar BANK_ATTACKER
	return

BattleScript_AffectionUnfroze:
	setword BATTLE_STRING_LOADER gText_Affection_Unfroze
	printstring 0x184
	waitmessage DELAY_1SECOND
	refreshhpbar BANK_ATTACKER
	return

BattleScript_AffectionShookParalysis:
	setword BATTLE_STRING_LOADER gText_Affection_ShookParalysis
	printstring 0x184
	waitmessage DELAY_1SECOND
	return

@ なかよしの まもり（src/stat_buffs.c・クリアアミュレットと同じ形）
BattleScript_AffectionStatBlock:
	setword BATTLE_STRING_LOADER gText_Affection_StatBlock
	printstring 0x184
	waitmessage DELAY_1SECOND
	return
