.text
.thumb
.align 2
/*
second_ability_scripts.s（2026-10-01・自作）
	2 つ目の特性（src/second_ability.c）の いかく と、新しい特性 おどかし（ABILITY_SCARE・いかくの とくこう版）の台本。
	素の BattleScript_IntimidateActivatesEnd3（ability_battle_scripts.s）を写したもの。素の台本は
	「その場のポケモンの特性が いかく か」を jumpifability で見て あまいミツ と分かれるので、
	特性が 2 つ目のときに使えない → 分かれ道を除いて、下げる能力だけ変えた 2 本にした。
	始めるのは ability_battle_effects.c の Hack_StartIntimidateLike（intimidateBank・intimidateActive を立てる）。
*/

.include "../asm_defines.s"
.include "../battle_script_macros.s"

.global BattleScript_HackIntimidateEnd3
.global BattleScript_HackScareEnd3

@;@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ いかく（2 つ目の特性）: あいての こうげきを 1 さげる
BattleScript_HackIntimidateEnd3:
	call BattleScript_AbilityPopUp
	setbyte TARGET_BANK 0x0
HackIntimidate_Loop:
	setstatchanger STAT_ATK | DECREASE_1
	trygetintimidatetarget HackIntimidate_Return
	jumpifbehindsubstitute BANK_TARGET HackIntimidate_Next
	statbuffchange STAT_TARGET | STAT_NOT_PROTECT_AFFECTED | STAT_BS_PTR HackIntimidate_Next
	jumpifbyte EQUALS MULTISTRING_CHOOSER 0x2 HackIntimidate_Prevented
	setgraphicalstatchangevalues
	playanimation BANK_TARGET ANIM_STAT_BUFF ANIM_ARG_1
	printfromtable gStatDownStringIds
	waitmessage DELAY_1SECOND
	jumpifhelditemeffect BANK_TARGET ITEM_EFFECT_ADRENALINE_ORB HackIntimidate_AdrenalineOrb
	goto HackIntimidate_Next

HackIntimidate_AdrenalineOrb:
	setstatchanger STAT_SPD | INCREASE_1
	statbuffchange STAT_TARGET | STAT_NOT_PROTECT_AFFECTED | STAT_BS_PTR, HackIntimidate_Next
	jumpifbyte EQUALS MULTISTRING_CHOOSER 0x2 HackIntimidate_Next
	playanimation BANK_TARGET ANIM_ITEM_USE 0x0
	setgraphicalstatchangevalues
	playanimation BANK_TARGET ANIM_STAT_BUFF ANIM_ARG_1
	printfromtable gStatUpStringIds
	waitmessage DELAY_1SECOND
	removeitem BANK_TARGET
	goto HackIntimidate_Next

HackIntimidate_Prevented:
	pause DELAY_HALFSECOND
	printfromtable gStatDownStringIds
	waitmessage DELAY_1SECOND

HackIntimidate_Next:
	jumpifword NOTANDS BATTLE_TYPE BATTLE_DOUBLE HackIntimidate_Return
	addbyte TARGET_BANK 0x1
	trygetintimidatetarget HackIntimidate_Return
	callasm TryReactiveIntimidatePopUp
	goto HackIntimidate_Loop

HackIntimidate_Return:
	callasm TryRemoveIntimidateAbilityPopUp
	callasm RemoveIntimidateActive
	callasm RemoveCottonDownActive
	end3

@;@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
@ おどかし（1 つ目でも 2 つ目でも）: あいての とくこうを 1 さげる
BattleScript_HackScareEnd3:
	call BattleScript_AbilityPopUp
	setbyte TARGET_BANK 0x0
HackScare_Loop:
	setstatchanger STAT_SPATK | DECREASE_1
	trygetintimidatetarget HackScare_Return
	jumpifbehindsubstitute BANK_TARGET HackScare_Next
	statbuffchange STAT_TARGET | STAT_NOT_PROTECT_AFFECTED | STAT_BS_PTR HackScare_Next
	jumpifbyte EQUALS MULTISTRING_CHOOSER 0x2 HackScare_Prevented
	setgraphicalstatchangevalues
	playanimation BANK_TARGET ANIM_STAT_BUFF ANIM_ARG_1
	printfromtable gStatDownStringIds
	waitmessage DELAY_1SECOND
	goto HackScare_Next

HackScare_Prevented:
	pause DELAY_HALFSECOND
	printfromtable gStatDownStringIds
	waitmessage DELAY_1SECOND

HackScare_Next:
	jumpifword NOTANDS BATTLE_TYPE BATTLE_DOUBLE HackScare_Return
	addbyte TARGET_BANK 0x1
	trygetintimidatetarget HackScare_Return
	callasm TryReactiveIntimidatePopUp
	goto HackScare_Loop

HackScare_Return:
	callasm TryRemoveIntimidateAbilityPopUp
	callasm RemoveIntimidateActive
	callasm RemoveSupersweetSyrupActive
	end3
