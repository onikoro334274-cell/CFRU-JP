@ タマムシデパート 4F（進化の石の売り場）の品ぞろえに「つきのいし・たいようのいし・つながりのヒモ」を足す
@ （2026-09-25 ユーザー要望「あるならタマムシデパートで販売してほしい」「ついでにつきのいし、たいようのいしもデパートについか」）。
@
@ ★ 素の売り場（pokefirered CeladonCity_DepartmentStore_4F_EventScript_Clerk）は
@     pokemart CeladonCity_DepartmentStore_4F_Items     <- 命令 08183CDE（0x86）、ポインタの語は 08183CDF
@   素の表は 08183CF0: ピッピにんぎょう / レトロメール / ほのお / かみなり / みず / リーフのいし / 0
@   その語を repoints でこの表へ向ける（素の表は書き換えない）。
@ ★ つながりのヒモは道具 87（ITEM_LINK_CABLE・値段 3000・src/Tables/item_tables.c）。
@   DPE の進化の表でユンゲラー・ゴーリキー・ゴースト・ゴローンが EVO_ITEM ITEM_LINK_CABLE を持つ

.text
.thumb
.align 2

.global KantoCeladonDept4F_Items
KantoCeladonDept4F_Items:
	.2byte 80	@ ITEM_POKE_DOLL
	.2byte 132	@ ITEM_RETRO_MAIL
	.2byte 95	@ ITEM_FIRE_STONE
	.2byte 96	@ ITEM_THUNDER_STONE
	.2byte 97	@ ITEM_WATER_STONE
	.2byte 98	@ ITEM_LEAF_STONE
	.2byte 94	@ ITEM_MOON_STONE
	.2byte 93	@ ITEM_SUN_STONE
	.2byte 99	@ ITEM_SHINY_STONE ひかりのいし（★ 2026-10-07 ユーザー「進化の石系売ってないのタマムシデパートに売って」）
	.2byte 100	@ ITEM_DUSK_STONE やみのいし
	.2byte 101	@ ITEM_DAWN_STONE めざめいし
	.2byte 102	@ ITEM_ICE_STONE こおりのいし
	.2byte 87	@ ITEM_LINK_CABLE
	.2byte 0	@ ITEM_NONE

@ ---- 序盤の店（トキワ・ニビ・ハナダ）にピーピーミニ（ITEM_PP_MINI 0x33F・PP を 5 回復）を足す（2026-10-03）----
@ ユーザー「消費アイテムでpp5だけ回復する」「序盤のショップでならぶように」。素の表は書き換えず、pokemart の語を repoints で向ける
@   トキワ 0817EF9C（素の表 0817EFAC）・ニビ 0817FE17（素の表 0817FE28）・ハナダ 081810C0（素の表 081810D0）
.equ ITEM_PP_MINI, 0x33F

.align 1
.global KantoViridianMart_Items
KantoViridianMart_Items:
	.2byte 4	@ ITEM_POKE_BALL
	.2byte 13	@ ITEM_POTION
	.2byte ITEM_PP_MINI
	.2byte 14	@ ITEM_ANTIDOTE
	.2byte 18	@ ITEM_PARALYZE_HEAL
	.2byte 0

.global KantoPewterMart_Items
KantoPewterMart_Items:
	.2byte 4	@ ITEM_POKE_BALL
	.2byte 13	@ ITEM_POTION
	.2byte ITEM_PP_MINI
	.2byte 14	@ ITEM_ANTIDOTE
	.2byte 18	@ ITEM_PARALYZE_HEAL
	.2byte 17	@ ITEM_AWAKENING
	.2byte 15	@ ITEM_BURN_HEAL
	.2byte 85	@ ITEM_ESCAPE_ROPE
	.2byte 86	@ ITEM_REPEL
	.2byte 0

.global KantoCeruleanMart_Items
KantoCeruleanMart_Items:
	.2byte 4	@ ITEM_POKE_BALL
	.2byte 22	@ ITEM_SUPER_POTION
	.2byte 13	@ ITEM_POTION
	.2byte ITEM_PP_MINI
	.2byte 14	@ ITEM_ANTIDOTE
	.2byte 18	@ ITEM_PARALYZE_HEAL
	.2byte 17	@ ITEM_AWAKENING
	.2byte 15	@ ITEM_BURN_HEAL
	.2byte 85	@ ITEM_ESCAPE_ROPE
	.2byte 86	@ ITEM_REPEL
	.2byte 0

@ ---- タマムシデパート 5F の薬の店員に どりょくリセット（ITEM_EV_RESET 0x340・努力値を全部 0）を足す（2026-10-03）----
@ ユーザー「努力値リセットアイテムを導入してほしい　消費アイテムでタマムシデパートで販売」
@   pokemart CeladonCity_DepartmentStore_5F_Vitamins の語 08183E31（素の表 08183E40）を repoints で向ける
.equ ITEM_EV_RESET, 0x340
.equ ITEM_LEVEL_DOWN, 0x344   @ 2026-10-06 レベルダウン（ユーザー「レベルダウンアイテム作れる？」）

.global KantoCeladonDept5F_Vitamins
KantoCeladonDept5F_Vitamins:
	.2byte 63	@ ITEM_HP_UP
	.2byte 64	@ ITEM_PROTEIN
	.2byte 65	@ ITEM_IRON
	.2byte 67	@ ITEM_CALCIUM
	.2byte 70	@ ITEM_ZINC
	.2byte 66	@ ITEM_CARBOS
	.2byte ITEM_EV_RESET
	.2byte ITEM_LEVEL_DOWN
	@ 2026-10-06 ユーザー「ポイントマックスととくせいぱっちカプセル販売するように」・値段は CFRU の表どおり
	.2byte 71	@ ITEM_PP_MAX（10000）
	.2byte 0x2D8	@ ITEM_ABILITY_CAPSULE（10000）
	.2byte 0x2D9	@ ITEM_ABILITY_PATCH（20000）
	@ ミント 21 種（2026-10-03 ユーザー「ミントの販売もタマムシデパートに」・値段は CFRU の表どおり 20000）
	.2byte 0x2F1	@ ITEM_LONELY_MINT
	.2byte 0x2F2	@ ITEM_ADAMANT_MINT
	.2byte 0x2F3	@ ITEM_NAUGHTY_MINT
	.2byte 0x2F4	@ ITEM_BRAVE_MINT
	.2byte 0x2F5	@ ITEM_BOLD_MINT
	.2byte 0x2F6	@ ITEM_IMPISH_MINT
	.2byte 0x2F7	@ ITEM_LAX_MINT
	.2byte 0x2F8	@ ITEM_RELAXED_MINT
	.2byte 0x2F9	@ ITEM_MODEST_MINT
	.2byte 0x2FA	@ ITEM_MILD_MINT
	.2byte 0x2FB	@ ITEM_RASH_MINT
	.2byte 0x2FC	@ ITEM_QUIET_MINT
	.2byte 0x2FD	@ ITEM_CALM_MINT
	.2byte 0x2FE	@ ITEM_GENTLE_MINT
	.2byte 0x2FF	@ ITEM_CAREFUL_MINT
	.2byte 0x300	@ ITEM_SASSY_MINT
	.2byte 0x301	@ ITEM_TIMID_MINT
	.2byte 0x302	@ ITEM_HASTY_MINT
	.2byte 0x303	@ ITEM_JOLLY_MINT
	.2byte 0x304	@ ITEM_NAIVE_MINT
	.2byte 0x305	@ ITEM_SERIOUS_MINT
	.2byte 0
