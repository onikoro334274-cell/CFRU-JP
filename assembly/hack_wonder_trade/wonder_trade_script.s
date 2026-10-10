.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

@ ミラクル交換（src/hack_wonder_trade.c・2026-10-07）。元は pkhk_BeGiNNeR さんの Wonder_Trades.s（通信交換の場面版）。
@ クチバのポケモンだいすきクラブの 9 人目（けんきゅういん・(9, 8)・assembly/kanto/kanto_gible_egg.s の人物の表）が呼ぶ。
@ special の番号は日本語版（decomp より 1 つ前にずれる）: 0x9F 手持ちを選ぶ・0x147 選んだ子の種族・0xFE 交換の場面

.equ SPECIAL_CHOOSE_PARTY_MON, 0x9F
.equ SPECIAL_GET_PARTY_MON_SPECIES, 0x147
.equ SPECIAL_DO_INGAME_TRADE_SCENE, 0xFE

.global EventScript_Wonder_Trades
EventScript_Wonder_Trades:
	lock
	faceplayer
	@ ★ 2026-10-07 ユーザー「1日5回まで制限はずして」: 回数の確かめ（CheckWonderTradeLimit）をやめた・何回でもできる
	msgbox gText_WonderTrade_Ask MSG_NORMAL
	msgbox gText_WonderTrade_Ask2 MSG_YESNO
	compare LASTRESULT 0x1
	if equal _goto WonderTrade_SelectMon
	msgbox gText_WonderTrade_ComeAgain MSG_NORMAL
	release
	end

WonderTrade_SelectMon:
	msgbox gText_WonderTrade_SelectMon MSG_NORMAL
	special SPECIAL_CHOOSE_PARTY_MON
	waitstate
	special2 0x8003 SPECIAL_GET_PARTY_MON_SPECIES
	compare 0x8004 0x6
	if greaterorequal _goto WonderTrade_Cancel @ やめた（0xFF）
	compare 0x8003 0x19C @ SPECIES_EGG
	if equal _goto WonderTrade_NoEgg
	@ ★ 2026-10-07 ユーザー「ポケモン選択時に確認が無いのが危ない」「持ち物を持ってる場合は…確認があった方が良さそう」: いいえ なら選び直し
	callasm WonderTrade_BufferChosen
	@ ★ 2026-10-09 ユーザー「ももアイスをトレード不可アイテムに」: ももアイスを持った子は出せない（0x8006 = 2・src/hack_wonder_trade.c）
	compare 0x8006 0x2
	if equal _goto WonderTrade_NoMomoIce
	msgbox gText_WonderTrade_Confirm MSG_YESNO
	compare LASTRESULT 0x1
	if notequal _goto WonderTrade_SelectMon
	compare 0x8006 0x0
	if equal _goto WonderTrade_Go
	msgbox gText_WonderTrade_ItemWarn MSG_YESNO
	compare LASTRESULT 0x1
	if notequal _goto WonderTrade_SelectMon
WonderTrade_Go:
	msgbox gText_WonderTrade_Wait MSG_NORMAL
	msgbox gText_WonderTrade_Wait2 MSG_NORMAL
	@ 受け取るポケモンを作って、通信交換の場面を流す
	copyvar 0x8005 0x8004
	callasm CreateWonderTradePokemon @ gEnemyParty[0] に作る・Var8004 をミラクル交換の目印（0xFFFF）にする
	callasm WonderTrade_TrySkipScene @ せっていで みない なら場面を出さずに入れ替える（LASTRESULT = 1）
	compare LASTRESULT 0x1
	if equal _goto WonderTrade_SkippedScene
	special SPECIAL_DO_INGAME_TRADE_SCENE
	waitstate
WonderTrade_AfterScene:
	copyvar 0x8004 0x8005 @ 目印を外す
	callasm WonderTrade_CountTrade @ 2026-10-10 回数を数える（じっせき）
	msgbox gText_WonderTrade_Thanks MSG_NORMAL
	msgbox gText_WonderTrade_ComeAgain MSG_NORMAL
	release
	end

WonderTrade_SkippedScene:
	fanfare 0x101 @ ポケモンを もらったときの曲（MUS_LEVEL_UP＝素の givemon と同じ）
	msgbox gText_WonderTrade_Received MSG_KEEPOPEN
	waitfanfare
	goto WonderTrade_AfterScene

WonderTrade_Cancel:
	msgbox gText_WonderTrade_ComeAgain MSG_NORMAL
	release
	end

WonderTrade_NoMomoIce:
	msgbox gText_MomoIce_NoTrade MSG_NORMAL
	goto WonderTrade_SelectMon

WonderTrade_NoEgg:
	msgbox gText_WonderTrade_NoEgg MSG_NORMAL
	release
	end

