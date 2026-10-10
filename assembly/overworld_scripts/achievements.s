.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

@ じっせき解除メッセージ + マイルストーン報酬
@ src/achievements.c の Achievements_FieldFrameUpdate から開始されます。

.equ MUS_FANFA1, 0x101 @ アイテムゲットのファンファーレ

.global EventScript_AchievementUnlocked
EventScript_AchievementUnlocked:
	lockall
	callasm AchievementsPreparePopup @ LASTRESULT: 0=1つだけ / 1=まとめて / 2=解除なし(報酬だけ)
	compare LASTRESULT 0x2
	if equal _goto AchievementUnlocked_Reward
	playfanfare MUS_FANFA1
	compare LASTRESULT 0x1
	if equal _goto AchievementUnlocked_Many
	msgbox gText_Ach_Unlocked MSG_NORMAL
	goto AchievementUnlocked_Wait

AchievementUnlocked_Many:
	msgbox gText_Ach_UnlockedMany MSG_NORMAL

AchievementUnlocked_Wait:
	waitfanfare

AchievementUnlocked_Reward:
	callasm AchievementsPrepareAutoReward @ LASTRESULT: 0=なし / 1=もらえた
	compare LASTRESULT 0x1
	if notequal _goto AchievementUnlocked_Done
	playfanfare MUS_FANFA1
	msgbox gText_Ach_Reward MSG_NORMAL
	waitfanfare

AchievementUnlocked_Done:
	callasm AchievementsPrepareAutoGift @ LASTRESULT: 1=ピカチュウを渡す番 (ACH_REWARDS_VIA_NPC が有効なら常に0 = NPCから受け取る)
	compare LASTRESULT 0x1
	if notequal _goto AchievementUnlocked_End2
	call AchievementGift_GivePikachu

AchievementUnlocked_End2:
	releaseall
	end

@ ピカチュウのプレゼント (ともにあゆむ解除時のテスト用)。call で呼び出します
AchievementGift_GivePikachu:
	callasm AchievementsMarkGiftDone
	givepokemon SPECIES_DITTO 5 ITEM_NONE @ [hack] 2026-10-10 ピカチュウ → メタモン（お母さんのピカチュウと重なるため・ユーザー「別なポケモンで」→ メタモン）。種族(SPECIES_xxx) / レベル / もちもの(ITEM_xxx)。ここを変えると別のポケモンになります
	compare LASTRESULT 0x0
	if equal _goto AchievementGift_Party
	compare LASTRESULT 0x1
	if equal _goto AchievementGift_PC
	msgbox gText_Ach_GiftFail MSG_NORMAL
	return

AchievementGift_Party:
	playfanfare MUS_FANFA1
	msgbox gText_Ach_GiftParty MSG_NORMAL
	waitfanfare
	return

AchievementGift_PC:
	playfanfare MUS_FANFA1
	msgbox gText_Ach_GiftPC MSG_NORMAL
	waitfanfare
	return

@ ---------------------------------------------------------------------------
@ マイルストーン報酬を渡すNPC用スクリプト
@   NPCのスクリプトから  call EventScript_AchievementNPC  (または goto) で呼び出してください。
@   lock / faceplayer / release はこのスクリプトの中で行います。
@   src/config.h の ACH_REWARDS_VIA_NPC を有効にすると、報酬 (ピカチュウも含む) は自動では渡されず、このNPCからだけ受け取れます。
@   (無効のままでも、このNPCで解除数の確認ができます)
@ ---------------------------------------------------------------------------
.global EventScript_AchievementNPC
EventScript_AchievementNPC:
	lock
	faceplayer
	callasm AchievementsPrepareNpcDialog @ [BUFFER1]=解除数 [BUFFER2]=つぎの報酬の必要数
	msgbox gText_AchNpc_Greeting MSG_NORMAL
	callasm AchievementsPrepareGift @ LASTRESULT: 1=ピカチュウを渡す番
	compare LASTRESULT 0x1
	if notequal _goto AchievementNPC_ClaimLoop
	call AchievementGift_GivePikachu

AchievementNPC_ClaimLoop:
	callasm AchievementsPrepareReward @ LASTRESULT: 0=なし / 1=もらえた / 2=バッグがいっぱい
	compare LASTRESULT 0x1
	if notequal _goto AchievementNPC_AfterClaim
	playfanfare MUS_FANFA1
	msgbox gText_Ach_Reward MSG_NORMAL
	waitfanfare
	goto AchievementNPC_ClaimLoop

AchievementNPC_AfterClaim:
	compare LASTRESULT 0x2
	if equal _goto AchievementNPC_BagFull
	callasm AchievementsPrepareNpcDialog
	compare LASTRESULT 0x1
	if equal _goto AchievementNPC_Next
	msgbox gText_AchNpc_AllDone MSG_NORMAL
	release
	end

AchievementNPC_Next:
	msgbox gText_AchNpc_Next MSG_NORMAL
	release
	end

AchievementNPC_BagFull:
	msgbox gText_AchNpc_BagFull MSG_NORMAL
	release
	end
