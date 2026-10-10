.text
.thumb
.align 2
@ お母さんの貯金: 賞金のあとに「ママに おくった」の文（src/hack_mom_savings.c HackMomSavings_GetMoneyReward が呼ぶ・2026-10-09）
@ ★ 素の台本の「printstring STRINGID_PLAYERGOTMONEY / waitmessage」の代わりにここを通り、終わったらその次へ戻る（return）

.include "../asm_defines.s"
.include "../battle_script_macros.s"

.global BattleScript_HackMomSavingsSent

BattleScript_HackMomSavingsSent:
	printstring 30			@ STRINGID_PLAYERGOTMONEY（素と同じ文）
	waitmessage DELAY_1SECOND
	setword BATTLE_STRING_LOADER gText_MomSav_BattleSent
	printstring 0x184
	waitmessage DELAY_1SECOND
	return
