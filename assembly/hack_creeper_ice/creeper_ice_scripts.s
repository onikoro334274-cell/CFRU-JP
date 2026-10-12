.text
.thumb
.align 2
@ くりーぱーアイス（src/hack_creeper_ice.c・2026-10-11・patches/0333）: ターンの終わりの残りの文と、5 ターン目の爆発

.include "../asm_defines.s"
.include "../battle_script_macros.s"

.global BattleScript_HackCreeperIceFuse
.global BattleScript_HackCreeperIceExplode
.global BattleScript_HackCreeperIceExplodeLoop

@ 残り（文は C が BATTLE_STRING_LOADER に入れてある）
BattleScript_HackCreeperIceFuse:
	playanimation BANK_SCRIPTING ANIM_WAITING_WAGGLE 0x0
	printstring 0x184
	waitmessage DELAY_1SECOND
	end2

@ 爆発: 持ち主（と ダブルの味方）の HP を 0 にして、素の ひんし の流れへ（場に出ていない手持ちは C が先に 0 にしてある）
BattleScript_HackCreeperIceExplode:
	setword BATTLE_STRING_LOADER gText_HackCreeperIce_Explode
	printstring 0x184
	waitmessage DELAY_1SECOND
BattleScript_HackCreeperIceExplodeLoop:
	orword HIT_MARKER HITMARKER_IGNORE_SUBSTITUTE | HITMARKER_NON_ATTACK_DMG
	graphicalhpupdate BANK_SCRIPTING
	datahpupdate BANK_SCRIPTING
	faintpokemon BANK_SCRIPTING 0x0 0x0
	callasm HackCreeperIce_NextVictim
	end2
