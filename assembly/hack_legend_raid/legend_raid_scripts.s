@ 伝説レイド（2026-10-06）: バトルフロンティアの広場（BattleFrontier_OutsideWest (23,47)）の人。
@ ユーザー「バトルフロンティアに話しかけるたびに異なる伝説とレイドバトルが始まるところ作ってほしい」。
@ 流れは CFRU の RaidScript_Start（assembly/overworld_scripts/system_scripts.s）と同じ。相手・星・助っ人は src/hack_legend_raid.c。
@ 文は strings/hack_legend_raid.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ SP_START_RAID_INTRO, 0x116
.equ SP_CREATE_RAID_MON, 0x117
.equ SP_START_RAID_BATTLE, 0x118
.equ FLAG_TAG_BATTLE, 0x908

.align 2
.global HackLegendRaidScr_Talk
HackLegendRaidScr_Talk:
	lock
	faceplayer
	msgbox gText_LegendRaid_Intro MSG_YESNO
	compare LASTRESULT 0x0
	if equal _goto HackLegendRaidScr_Decline
	callasm HackLegendRaid_Begin
	fadescreen FADEOUT_BLACK
	special SP_START_RAID_INTRO
	waitstate
	compare LASTRESULT 0x0
	if equal _goto HackLegendRaidScr_Cancel
	setflag FLAG_TAG_BATTLE
	special SP_CREATE_RAID_MON
	callasm HackLegendRaid_StrengthenBoss
	special SP_START_RAID_BATTLE
	waitstate
	callasm HackLegendRaid_End
	msgbox gText_LegendRaid_After MSG_KEEPOPEN
	closemessage
	release
	end

HackLegendRaidScr_Cancel:
	callasm HackLegendRaid_End
HackLegendRaidScr_Decline:
	msgbox gText_LegendRaid_Decline MSG_KEEPOPEN
	closemessage
	release
	end
