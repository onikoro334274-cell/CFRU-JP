@ マサラのオーキド研究所: ポケモン図鑑をもらうときに、ずかんナビと、最初のポケモンに合わせたメガストーンも
@ もらう（2026-09-24 ユーザー要望「図鑑もらうときに図鑑ナビ入手、最初のポケモンに対応するメガストーンゲット
@ ヒトカゲの場合は XY 両方」）。根拠は docs/research/2026-09-24_kanto-oak-dex-gifts.md
@
@ ★ 素の図鑑の場面（pokefirered PalletTown_ProfessorOaksLab の ReceivedPokedexFromOak のあと）は
@     playfanfare / message / waitmessage / waitfanfare /
@     call EventScript_RestorePrevTextColor      <- 0817CD83（ポインタの語は 0817CD84）
@     setflag FLAG_SYS_POKEDEX_GET ...
@   この call の飛び先を repoints でここへ向ける（tools/kanto/build_kanto_home.py が書く）。
@   ここで素の EventScript_RestorePrevTextColor（08193F43）を最後に呼び直すので、流れは素と同じ。
@ ★ ずかんナビは道具ではなく旗 FLAG_SYS_DEXNAV（0x91E）。src/hoenn_steps.c の HoennDexNavUnlockTick も
@   図鑑の旗を見て立てるが、ここで立てれば「手に入れた」が出る

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ EventScript_RestorePrevTextColor, 0x08193F43   @ 素（奇数だがスクリプトの番地。そのまま call する）
.equ VAR_STARTER_MON, 0x4031           @ 0 フシギダネ / 1 ゼニガメ / 2 ヒトカゲ（pokefirered vars.h）
.equ FLAG_SYS_DEXNAV, 0x91E            @ CFRU src/config.h
.equ MUS_OBTAIN_KEY_ITEM, 318          @ pokefirered include/constants/songs.h
.equ TEXT_COLOR_MALE, 0                @ NPC_TEXT_COLOR_MALE（オーキド）
.equ TEXT_COLOR_NEUTRAL, 3
.equ ITEM_VENUSAURITE, 0x215           @ include/constants/items.h
.equ ITEM_CHARIZARDITE_X, 0x216
.equ ITEM_CHARIZARDITE_Y, 0x217
.equ ITEM_BLASTOISINITE, 0x218

.align 2
.global KantoOakLab_EventScript_DexExtras
KantoOakLab_EventScript_DexExtras:
	textcolor TEXT_COLOR_MALE
	msgbox gText_KantoOak_DexNav MSG_KEEPOPEN
	textcolor TEXT_COLOR_NEUTRAL
	fanfare MUS_OBTAIN_KEY_ITEM
	preparemsg gText_KantoOak_GotDexNav
	waitmsg
	waitfanfare
	setflag FLAG_SYS_DEXNAV
	msgbox gText_KantoOak_ExplainDexNav MSG_KEEPOPEN
	textcolor TEXT_COLOR_MALE
	msgbox gText_KantoOak_MegaStone MSG_KEEPOPEN
	compare VAR_STARTER_MON 0
	if 0x1 _call KantoOakLab_EventScript_GiveVenusaurite     @ 0x1 = equal
	compare VAR_STARTER_MON 1
	if 0x1 _call KantoOakLab_EventScript_GiveBlastoisinite
	compare VAR_STARTER_MON 2
	if 0x1 _call KantoOakLab_EventScript_GiveCharizardites
	call EventScript_RestorePrevTextColor
	return

KantoOakLab_EventScript_GiveVenusaurite:
	obtainitem ITEM_VENUSAURITE 1
	return

KantoOakLab_EventScript_GiveBlastoisinite:
	obtainitem ITEM_BLASTOISINITE 1
	return

KantoOakLab_EventScript_GiveCharizardites:
	obtainitem ITEM_CHARIZARDITE_X 1
	obtainitem ITEM_CHARIZARDITE_Y 1
	return
