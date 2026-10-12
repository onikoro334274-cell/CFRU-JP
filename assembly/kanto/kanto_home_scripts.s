@ カントーの自宅 1 階: 初めて 1 階に下りたとき、お母さんから贈り物（2026-09-24 ユーザー要望）。
@ 根拠は docs/research/2026-09-24_kanto-home-gift.md
@
@   Z パワーリング / テラスタルオーブ / メガリング / ダイマックスバンド / きのみプランター /
@   オボンのみ x4 / ランニングシューズ（素 FRLG では道具ではなくフラグ FLAG_SYS_B_DASH）
@   おこづかい 30 万円（2026-10-11 ユーザー「ママから所持金」・addmoney・所持金の上限 9,999,999 は AddMoney が切る）
@
@ ★ 素の 1 階のマップスクリプトは空（0817BCBA の `.byte 0`）。地図のヘッダ（08314E00）の
@   +0x08（マップスクリプトの番地）を repoints でこの KantoHome1F_MapScripts へ向ける
@   （tools/kanto/build_kanto_home.py が repoints に書く）。
@ ★ 済んだ印は予約変数 KANTO_VAR_MOM_GIFT（build_hoenn_scripts.RESERVED_VARS）。
@   ON_FRAME は変数でしか見られないので旗ではなく変数。
@ ★ ふつうは 2 階から階段で下りた直後（主人公は (10,2)）。お母さん（NPC 1・(8,4)）が
@   右・右・上と歩いて主人公の真下 (10,3) に来て、渡したあと元に戻る。
@   すでに遊んでいるセーブで外から入ったときは主人公が階段にいないので、歩かずにその場で渡す。

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ KANTO_VAR_MOM_GIFT, 0x514C        @ build_hoenn_scripts.RESERVED_VARS と同じ（検算が見る）
.equ LOCALID_MOM, 1
.equ LOCALID_PLAYER, 0xFF
.equ FLAG_SYS_B_DASH, 0x82F
.equ MUS_OBTAIN_KEY_ITEM, 318          @ pokefirered include/constants/songs.h
.equ MUS_OBTAIN_ITEM, 258
.equ MOM_ALLOWANCE, 300000             @ おこづかい（円・2026-10-11 ユーザー「300,000 円」・文 gText_KantoMom_GotMoney の額もそろえる）
.equ TEXT_COLOR_FEMALE, 1              @ NPC_TEXT_COLOR_FEMALE
.equ TEXT_COLOR_NEUTRAL, 3             @ NPC_TEXT_COLOR_NEUTRAL
.equ HACK_FLAG_PHONE_MOM, 0x12CA       @ 電話帳のママの番号（src/hack_phone.c・build_hoenn_scripts.HOENN_FLAG_END の下）
.equ ITEM_SITRUS_BERRY, 142            @ include/constants/items.h
.equ ITEM_Z_POWER_RING, 267
.equ ITEM_DYNAMAX_BAND, 347
.equ ITEM_MEGA_RING, 353
.equ ITEM_TERA_ORB, 0x2EE
.equ ITEM_BERRY_PLANTER, 0x308
.equ ITEM_MUSIC_PLAYER, 0x337            @ #28（2026-09-27）
.equ ITEM_TIME_TV, 0x33D                 @ タイムテレビ（2026-10-01・src/play_clock.c）
.equ MAP_SCRIPT_ON_FRAME_TABLE, 2
@ ★ 最初のポケモンのあとお母さんに話しかけるとピカチュウ（2026-09-24・src/kanto_mom_pikachu.c）
.equ KANTO_FLAG_MOM_PIKACHU, 0x13EC    @ build_hoenn_scripts.RESERVED_FLAGS と同じ（検算が見る）
.equ FLAG_SYS_POKEMON_GET, 0x828       @ SYS_FLAGS + 0x28（pokefirered flags.h）
.equ MUS_LEVEL_UP, 257                 @ ポケモンをもらうときのファンファーレ（素のシルフのラプラスと同じ）
.equ KantoHome1F_EventScript_MomOriginal, 0x0817BCBB   @ 素のお母さんのスクリプト（人物データ 0837D720 の +0x10）
@ ★ ピカチュウのあとにもう一度話しかけると まるいおまもり・ひかるおまもり（2026-09-25 ユーザー要望「わたすのを わすれてた」）。
@   印は増やさず、バッグに無いほうだけ渡す（両方あれば素のお母さんへ）。効き目は CFRU が持っている
@   （ひかる＝ src/build_pokemon.c の色違いの引き直し 2 回 / まるい＝ src/daycare.c のタマゴの見つかりやすさ）
.equ ITEM_OVAL_CHARM, 0x1D6
.equ ITEM_SHINY_CHARM, 0x1D7
@ ★ 2026-10-04 両方あれば かがやくおまもり（ひかるおまもりの上位版・色違い 1/500・ON/OFF・src/hack_shining_charm.c）。受け取ると ON
.equ ITEM_SHINING_CHARM, 0x341       @ include/constants/items.h（build_za_megas.HACK_ITEMS）
.equ KANTO_FLAG_SHINING_CHARM_ON, 0x12F8   @ build_hoenn_scripts.RESERVED_FLAGS
.equ ITEM_METEORITE, 280                 @ include/constants/items.h（2026-10-09 デオキシスのフォルムチェンジ）
.equ ITEM_ROTOM_CATALOG, 0x307
@ ★ 2026-10-11 ユーザー「いでんしのくさびとかをママが最初にわたすように」: 伝説のすがたを変える たいせつなもの 9 つ（include/constants/items.h・
@   ほかでは手に入らない）。1 階の贈り物に足し、前の版のセーブはピカチュウ／おまもりの会話で無いものだけ（KeyItemsBody）
.equ ITEM_GRACIDEA, 0x1DF
.equ ITEM_DNA_SPLICERS, 0x1E0
.equ ITEM_REVEAL_GLASS, 0x1E1
.equ ITEM_PRISON_BOTTLE, 0x1E2
.equ ITEM_N_SOLARIZER, 0x1E3
.equ ITEM_N_LUNARIZER, 0x1E4
.equ ITEM_RUSTED_SWORD, 0x1E5
.equ ITEM_RUSTED_SHIELD, 0x1E6
.equ ITEM_REINS_OF_UNITY, 0x2E6

.global KantoHome1F_MapScripts
KantoHome1F_MapScripts:
	.byte MAP_SCRIPT_ON_FRAME_TABLE
	.word KantoHome1F_FrameTable
	.byte 0

.align 1
KantoHome1F_FrameTable:
	.hword KANTO_VAR_MOM_GIFT, 0
	.word KantoHome1F_EventScript_MomGift
	.hword 0

.align 2
.global KantoHome1F_EventScript_MomGift
KantoHome1F_EventScript_MomGift:
	@ ★ たいせつなもの 5 つのどれかをもう持っていたら、イベントを起こさない（ユーザー要望 2026-09-24
	@   「たいせつなものに重複がある場合イベント発生しないように」）。印だけ立てて何も出さずに終わる
	checkitem ITEM_Z_POWER_RING 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHome1F_EventScript_AlreadyHave         @ 0x1 = equal
	checkitem ITEM_TERA_ORB 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHome1F_EventScript_AlreadyHave
	checkitem ITEM_MEGA_RING 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHome1F_EventScript_AlreadyHave
	checkitem ITEM_DYNAMAX_BAND 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHome1F_EventScript_AlreadyHave
	checkitem ITEM_BERRY_PLANTER 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHome1F_EventScript_AlreadyHave
	lockall
	textcolor TEXT_COLOR_FEMALE
	applymovement LOCALID_MOM KantoHome1F_Move_MomNotice
	waitmovement 0
	msgbox gText_KantoMom_Wait MSG_KEEPOPEN
	closemessage
	getplayerpos 0x8004 0x8005
	compare 0x8004 10
	if 0x5 _goto KantoHome1F_EventScript_MomGiftHere      @ 0x5 = not equal
	compare 0x8005 2
	if 0x5 _goto KantoHome1F_EventScript_MomGiftHere
	applymovement LOCALID_MOM KantoHome1F_Move_MomToPlayer
	applymovement LOCALID_PLAYER KantoHome1F_Move_FaceDown
	waitmovement 0
	call KantoHome1F_EventScript_Gifts
	applymovement LOCALID_MOM KantoHome1F_Move_MomBack
	waitmovement 0
	goto KantoHome1F_EventScript_MomGiftDone

KantoHome1F_EventScript_MomGiftHere:
	call KantoHome1F_EventScript_Gifts

KantoHome1F_EventScript_MomGiftDone:
	setvar KANTO_VAR_MOM_GIFT 1
	releaseall
	end

KantoHome1F_EventScript_AlreadyHave:
	setvar KANTO_VAR_MOM_GIFT 1
	end

KantoHome1F_EventScript_Gifts:
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_Gift MSG_KEEPOPEN
	obtainitem ITEM_Z_POWER_RING 1
	obtainitem ITEM_TERA_ORB 1
	obtainitem ITEM_MEGA_RING 1
	obtainitem ITEM_DYNAMAX_BAND 1
	obtainitem ITEM_BERRY_PLANTER 1
	obtainitem ITEM_TIME_TV 1			@ 2026-10-01 タイムテレビ（プレイ時間の時計・ユーザー「最初から持っている」）
	obtainitem ITEM_SITRUS_BERRY 4
	call KantoHome1F_EventScript_KeyItemsBody   @ 2026-10-11 伝説の すがたを変える たいせつなもの 9 つ
	@ ★ 2026-10-11 おこづかい（ユーザー「ママから所持金」）。道具のあと・くつの前
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_Money MSG_KEEPOPEN
	addmoney MOM_ALLOWANCE 0
	textcolor TEXT_COLOR_NEUTRAL
	fanfare MUS_OBTAIN_ITEM
	preparemsg gText_KantoMom_GotMoney
	waitmsg
	waitfanfare
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_Shoes MSG_KEEPOPEN
	textcolor TEXT_COLOR_NEUTRAL
	fanfare MUS_OBTAIN_KEY_ITEM
	preparemsg gText_KantoMom_GotShoes
	waitmsg
	waitfanfare
	setflag FLAG_SYS_B_DASH
	@ ★ MSG_SIGN は使わない。素の Std_MsgboxSign は中で releaseall を呼ぶので、
	@   この先（お母さんが戻る）の前に主人公が動けてしまう。ニビの原本と同じく signmsg で挟む
	signmsg
	msgbox gText_KantoMom_ExplainShoes MSG_KEEPOPEN
	normalmsg
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_PikachuHint MSG_KEEPOPEN     @ ★ 2026-10-06 ピカチュウへの導線（ユーザー「ポケモンもらったら渡すものがあるからとか」）
	msgbox gText_KantoMom_Bye MSG_KEEPOPEN
	closemessage
	return

KantoHome1F_Move_MomNotice:
	.byte exclaim
	.byte end_m

KantoHome1F_Move_MomToPlayer:
	.byte walk_right
	.byte walk_right
	.byte walk_up
	.byte look_up
	.byte end_m

KantoHome1F_Move_MomBack:
	.byte walk_down
	.byte walk_left
	.byte walk_left
	.byte look_left
	.byte end_m

KantoHome1F_Move_FaceDown:
	.byte look_down
	.byte end_m

@ ---- お母さんに話しかけたとき（人物データ 0837D730 の語を repoints でここへ向ける）----
@ 最初のポケモンをもらっていて、まだピカチュウをもらっていなければ渡す。それ以外は素のスクリプトへ
.align 2
.global KantoHome1F_EventScript_MomTalk
KantoHome1F_EventScript_MomTalk:
	@ ★ 2026-10-11 電話帳（pm-start/poke-call・src/hack_phone.c）: はじめて話しかけたときにママの番号を登録して、いつもの流れへ
	checkflag HACK_FLAG_PHONE_MOM
	if 0x1 _goto KantoHome1F_EventScript_MomTalkMain
	lock
	faceplayer
	textcolor TEXT_COLOR_NEUTRAL
	msgbox gText_HackPhone_MomExchange MSG_KEEPOPEN
	setflag HACK_FLAG_PHONE_MOM
	closeonkeypress
	release
KantoHome1F_EventScript_MomTalkMain:
	checkflag FLAG_SYS_POKEMON_GET
	if 0x0 _goto KantoHome1F_EventScript_MomOriginal       @ 0x0 = まだ（旗が立っていない）
	checkflag KANTO_FLAG_MOM_PIKACHU
	if 0x1 _goto KantoHome1F_EventScript_MomCharms
	lock
	faceplayer
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_PikachuOffer MSG_KEEPOPEN
	callasm KantoMomGivePikachu + 1
	compare LASTRESULT 2
	if 0x1 _goto KantoHome1F_EventScript_MomPikachuNoRoom
	setflag KANTO_FLAG_MOM_PIKACHU
	textcolor TEXT_COLOR_NEUTRAL
	fanfare MUS_LEVEL_UP
	preparemsg gText_KantoMom_GotPikachu
	waitmsg
	waitfanfare
	compare LASTRESULT 1
	if 0x1 _call KantoHome1F_EventScript_MomPikachuToPC
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_PikachuBye MSG_KEEPOPEN
	@ ★ 2026-10-11 ユーザー「最初に渡すママのアイテムはまとめてするように」: ピカチュウと同じ会話で おまもり 3 つ（まるい・ひかる・かがやく）も渡す
	msgbox gText_KantoMom_CharmsToo MSG_KEEPOPEN
	call KantoHome1F_EventScript_CharmsBody
	closemessage
	release
	end

@ ピカチュウのあと: おまもり 3 つ（まるい・ひかる・かがやく）のどれかが無ければ、無いものを全部 1 回の会話で渡す（2026-10-11 まとめた）。
@ 全部あれば いんせき・カタログ → お母さんの貯金
KantoHome1F_EventScript_MomCharms:
	@ ★ 2026-10-12 ユーザー「ママからたいせつなものにもらっていないのがあったらママに話しかければもらえるように」:
	@   おまもり 3 つ・1 階の贈り物の たいせつなもの 6 つ・伝説の 9 つの 18 個を全部調べ、1 つでも無ければ無いものを全部渡す（MomGiveCharms）
	call KantoHome1F_EventScript_CheckMissing
	compare 0x8009 0
	if 0x5 _goto KantoHome1F_EventScript_MomGiveCharms   @ 0x5 = not equal
	callasm HackMeteorite_MomCheck + 1                   @ ★ 2026-10-09 デオキシス・ロトムを持っているのに いんせき・カタログが無ければ（src/hack_meteorite.c）
	compare LASTRESULT 1
	if 0x1 _goto KantoHome1F_EventScript_MomFormItems
	goto KantoHome1F_EventScript_MomSavings              @ ★ 2026-10-06 お母さんの貯金（assembly/kanto/kanto_mom_savings.s）

KantoHome1F_EventScript_MomFormItems:
	lock
	faceplayer
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_FormItems MSG_KEEPOPEN
	compare 0x8004 1
	if 0x1 _call KantoHome1F_EventScript_MomGiveMeteorite
	compare 0x8005 1
	if 0x1 _call KantoHome1F_EventScript_MomGiveCatalog
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_FormItemsBye MSG_KEEPOPEN
	closemessage
	release
	end

KantoHome1F_EventScript_MomGiveMeteorite:
	obtainitem ITEM_METEORITE 1
	return

KantoHome1F_EventScript_MomGiveCatalog:
	obtainitem ITEM_ROTOM_CATALOG 1
	return

@ かがやくおまもり（CharmsBody から call）
KantoHome1F_EventScript_MomGiveShining:
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_ShiningOffer MSG_KEEPOPEN
	obtainitem ITEM_SHINING_CHARM 1
	setflag KANTO_FLAG_SHINING_CHARM_ON
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_ShiningBye MSG_KEEPOPEN
	return

@ あとから話しかけたとき（ピカチュウはもらったが おまもりが無い＝前の版のセーブなど）
KantoHome1F_EventScript_MomGiveCharms:
	lock
	faceplayer
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_CharmsForgot MSG_KEEPOPEN
	call KantoHome1F_EventScript_CharmsBody
	closemessage
	release
	end

@ 無い おまもりを全部渡す（まるい・ひかる → 説明・かがやく → 説明）。0x8008 = まるい か ひかる を渡したか
KantoHome1F_EventScript_CharmsBody:
	setvar 0x8008 0
	checkitem ITEM_OVAL_CHARM 1
	compare LASTRESULT 0
	if 0x1 _call KantoHome1F_EventScript_MomGiveOval
	checkitem ITEM_SHINY_CHARM 1
	compare LASTRESULT 0
	if 0x1 _call KantoHome1F_EventScript_MomGiveShiny
	compare 0x8008 0
	if 0x5 _call KantoHome1F_EventScript_CharmsExplain      @ 0x5 = not equal
	checkitem ITEM_SHINING_CHARM 1
	compare LASTRESULT 0
	if 0x1 _call KantoHome1F_EventScript_MomGiveShining
	call KantoHome1F_EventScript_KeyItemsBody
	call KantoHome1F_EventScript_StartItemsBody   @ 2026-10-12 1 階の贈り物の たいせつなもの 6 つのうち無いもの
	return

KantoHome1F_EventScript_CharmsExplain:
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_CharmsBye MSG_KEEPOPEN
	return

@ 伝説の すがたを変える たいせつなもの 9 つのうち、無いものを渡す（1 つでも渡したら説明を 1 回）
KantoHome1F_EventScript_KeyItemsBody:
	setvar 0x8009 0
	setvar 0x8000 ITEM_GRACIDEA
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_DNA_SPLICERS
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_REVEAL_GLASS
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_PRISON_BOTTLE
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_N_SOLARIZER
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_N_LUNARIZER
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_RUSTED_SWORD
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_RUSTED_SHIELD
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_REINS_OF_UNITY
	call KantoHome1F_EventScript_KeyItem
	compare 0x8009 0
	if 0x5 _call KantoHome1F_EventScript_KeyItemsExplain      @ 0x5 = not equal
	return

KantoHome1F_EventScript_KeyItemsExplain:
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_KeyItems MSG_KEEPOPEN
	return

@ ★ 2026-10-12 ママからもらう たいせつなもの 18 個（おまもり 3・1 階の贈り物 6・伝説の 9）のどれかが無ければ 0x8009 = 1
KantoHome1F_EventScript_CheckMissing:
	setvar 0x8009 0
	setvar 0x8000 ITEM_OVAL_CHARM
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_SHINY_CHARM
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_SHINING_CHARM
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_Z_POWER_RING
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_TERA_ORB
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_MEGA_RING
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_DYNAMAX_BAND
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_BERRY_PLANTER
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_TIME_TV
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_GRACIDEA
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_DNA_SPLICERS
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_REVEAL_GLASS
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_PRISON_BOTTLE
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_N_SOLARIZER
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_N_LUNARIZER
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_RUSTED_SWORD
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_RUSTED_SHIELD
	call KantoHome1F_EventScript_MarkIfMissing
	setvar 0x8000 ITEM_REINS_OF_UNITY
	call KantoHome1F_EventScript_MarkIfMissing
	return

KantoHome1F_EventScript_MarkIfMissing:
	checkitem 0x8000 1
	compare LASTRESULT 0
	if 0x1 _goto KantoHome1F_EventScript_MarkMissing
	return

KantoHome1F_EventScript_MarkMissing:
	setvar 0x8009 1
	return

@ ★ 2026-10-12 1 階の贈り物の たいせつなもの 6 つ（Zパワーリング・テラスタルオーブ・メガリング・ダイマックスバンド・きのみプランター・タイムテレビ）のうち無いものを渡す（1 つでも渡したら説明を 1 回）
KantoHome1F_EventScript_StartItemsBody:
	setvar 0x8009 0
	setvar 0x8000 ITEM_Z_POWER_RING
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_TERA_ORB
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_MEGA_RING
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_DYNAMAX_BAND
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_BERRY_PLANTER
	call KantoHome1F_EventScript_KeyItem
	setvar 0x8000 ITEM_TIME_TV
	call KantoHome1F_EventScript_KeyItem
	compare 0x8009 0
	if 0x5 _call KantoHome1F_EventScript_StartItemsExplain      @ 0x5 = not equal
	return

KantoHome1F_EventScript_StartItemsExplain:
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_KantoMom_StartItems MSG_KEEPOPEN
	return

@ 0x8000 の道具を持っていなければ 1 つ渡す（giveitem と同じ 0x8000／0x8001 → callstd MSG_OBTAIN）
KantoHome1F_EventScript_KeyItem:
	checkitem 0x8000 1
	compare LASTRESULT 0
	if 0x1 _goto KantoHome1F_EventScript_KeyItemGive
	return

KantoHome1F_EventScript_KeyItemGive:
	setvar 0x8001 1
	callstd MSG_OBTAIN
	setvar 0x8009 1
	return

KantoHome1F_EventScript_MomGiveOval:
	obtainitem ITEM_OVAL_CHARM 1
	setvar 0x8008 1
	return

KantoHome1F_EventScript_MomGiveShiny:
	obtainitem ITEM_SHINY_CHARM 1
	setvar 0x8008 1
	return

KantoHome1F_EventScript_MomPikachuToPC:
	msgbox gText_KantoMom_PikachuToPC MSG_KEEPOPEN
	return

KantoHome1F_EventScript_MomPikachuNoRoom:
	msgbox gText_KantoMom_PikachuNoRoom MSG_KEEPOPEN
	closemessage
	release
	end

@ ---- 自宅 2F のファミコン（#28・2026-09-27 ユーザー要望「自宅のファミコンから入手できるようにしておいて」）----
@ 素の看板（bg_event 0837D780 の +0x08 = 0817BF05）を repoints（## KANTOHOME）でここへ向ける。
@ まだ持っていなければミュージックプレイヤーを渡し、持っていれば素のファミコンの文へ戻す。
.equ KANTO_NES_SCRIPT_VANILLA, 0x0817BF05

.align 2
.global KantoHome2F_EventScript_NES
KantoHome2F_EventScript_NES:
	checkitem ITEM_MUSIC_PLAYER 1
	compare LASTRESULT 1
	if 0x1 _goto KantoHome2F_EventScript_NESVanilla
	lockall
	msgbox gText_KantoHome2F_NESFound MSG_KEEPOPEN
	obtainitem ITEM_MUSIC_PLAYER 1
	releaseall
	end

KantoHome2F_EventScript_NESVanilla:
	goto KANTO_NES_SCRIPT_VANILLA
