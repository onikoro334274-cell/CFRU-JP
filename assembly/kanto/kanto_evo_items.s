@ タマムシデパート 4F の 進化の道具の売り場（2026-10-03）。
@ ユーザー「ポケモンの進化の道具を売るショップ店員をタマムシデパートに配置して　プロテクターとか」。
@ 素の人物は 3 人（0837F2D8・地図のイベント 0837F344）。人物の表を写して 4 人目（localId 4・店員・(5, 13)・上向き＝カウンターの向こう）を足し、
@ 地図のイベントの人数（0837F344）を 4 に・表の語（0837F348）を repoints で向ける。進化の石とつながりのヒモは同じ階の素の店員（kanto_mart.s）。
@ 品はレベル・つうしん以外で進化に使う道具（値段はどれも 0 でない＝個数の画面で割り算 0 にならない・6.59）。文は strings/kanto_evo_items.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ OBJ_EVENT_GFX_CLERK, 68

@ ---- 人物の表（素の 3 人をそのまま写す + 進化の道具の店員）----
.align 2
.global KantoCeladonDept4F_Objects
KantoCeladonDept4F_Objects:
	.byte 0x01, 0x19, 0x00, 0x00, 0x06, 0x00, 0x06, 0x00, 0x03, 0x08, 0x11, 0x00
	.byte 0x00, 0x00, 0x00, 0x00, 0xAD, 0x3C, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00
	.byte 0x02, 0x12, 0x00, 0x00, 0x0A, 0x00, 0x0B, 0x00, 0x03, 0x02, 0x11, 0x00
	.byte 0x00, 0x00, 0x00, 0x00, 0xB6, 0x3C, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00
	.byte 0x03, 0x44, 0x00, 0x00, 0x03, 0x00, 0x0D, 0x00, 0x00, 0x07, 0x13, 0x00
	.byte 0x00, 0x00, 0x00, 0x00, 0xC8, 0x3C, 0x18, 0x08, 0x00, 0x00, 0x00, 0x00
	.byte 4, OBJ_EVENT_GFX_CLERK, 0, 0
	.2byte 5, 13
	.byte 3, 7, 0x11, 0          @ 高さ 3・上を向く（MOVEMENT_TYPE_FACE_UP = 7）
	.2byte 0, 0
	.4byte KantoEvoItems_EventScript_Clerk
	.2byte 0, 0

.align 2
.global KantoEvoItems_EventScript_Clerk
KantoEvoItems_EventScript_Clerk:
	lock
	faceplayer
	preparemsg gText_KantoEvoItems_Welcome
	waitmsg
	pokemart KantoEvoItems_Items
	msgbox gText_KantoEvoItems_Bye MSG_KEEPOPEN
	closemessage
	release
	end

@ ---- 品（.2byte の並び・0 で終わり）----
.align 1
KantoEvoItems_Items:
	.2byte 88	@ ITEM_PROTECTOR プロテクター
	.2byte 89	@ ITEM_ELECTIRIZER エレキブースター
	.2byte 90	@ ITEM_MAGMARIZER マグマブースター
	.2byte 91	@ ITEM_DUBIOUS_DISC あやしいパッチ
	.2byte 218	@ ITEM_UP_GRADE アップグレード
	.2byte 92	@ ITEM_REAPER_CLOTH れいかいのぬの
	.2byte 119	@ ITEM_RAZOR_CLAW するどいツメ
	.2byte 120	@ ITEM_RAZOR_FANG するどいキバ
	.2byte 201	@ ITEM_DRAGON_SCALE りゅうのウロコ
	.2byte 187	@ ITEM_KINGS_ROCK おうじゃのしるし
	.2byte 199	@ ITEM_METAL_COAT メタルコート
	.2byte 176	@ ITEM_PRISM_SCALE きれいなウロコ
	.2byte 118	@ ITEM_OVAL_STONE まんまるいし
	.2byte 177	@ ITEM_SACHET においぶくろ
	.2byte 178	@ ITEM_WHIPPED_DREAM ホイップポップ
	.2byte 192	@ ITEM_DEEP_SEA_TOOTH しんかいのキバ
	.2byte 193	@ ITEM_DEEP_SEA_SCALE しんかいのウロコ
	.2byte 226	@ ITEM_STRAWBERRY_SWEET いちごアメざいく
	.2byte 233	@ ITEM_SWEET_APPLE あまーいりんご
	.2byte 234	@ ITEM_TART_APPLE すっぱいりんご
	.2byte 0x2E1	@ ITEM_SYRUPY_APPLE みついりりんご
	.2byte 235	@ ITEM_CRACKED_POT われたポット
	.2byte 236	@ ITEM_CHIPPED_POT かけたポット
	.2byte 749	@ ITEM_UNREMARKABLE_TEACUP ボンサクのちゃわん
	.2byte 748	@ ITEM_MASTERPIECE_TEACUP ケッサクのちゃわん
	.2byte 237	@ ITEM_GALARICA_CUFF ガラナツブレス
	.2byte 238	@ ITEM_GALARICA_WREATH ガラナツリース
	.2byte 0x2DC	@ ITEM_BLACK_AUGURITE くろのきせき
	.2byte 0x2DE	@ ITEM_PEAT_BLOCK ピートブロック
	.2byte 0x2DA	@ ITEM_AUSPICIOUS_ARMOR イワイノヨロイ
	.2byte 0x2DB	@ ITEM_MALICIOUS_ARMOR ノロイノヨロイ
	.2byte 0x2DD	@ ITEM_METAL_ALLOY ふくごうきんぞく
	.2byte 0
