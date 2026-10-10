@ ヤマブキシティ かくとう道場（SaffronCity_Dojo・群 14 の 2）の きんこんしきの夫婦（2026-10-09）。
@ ユーザーの表（かくとうポケモン 13 組）「各3:3で手持ちがあるとしたら … のテーブルが選ばれるみたいな」→ 2 人組のダブル・新しい 2 人・
@ Smogon の型・レベルは手持ちに合わせる。置き場は Claude が かくとう道場に決めた（左の小部屋 (2, 4)／(3, 4)・下向き）。
@ 素の道場の人物は 7 人（08380C68）。人物の表を写して 8・9 人目を足し、地図のイベントの人数（08380D78）を 9 に・表の語（08380D7C）を repoints で向ける。
@ 手持ち: トレーナー 936（ゲンゾウ）・937（ハナ）は 13 組の左・右を 13 匹ずつ持ち、戦うたびに src/hack_golden_wedding.c が 3 組を選ぶ。
@ 何度でも戦える（戦う前にトレーナーの印を消す）。文は strings/kanto_golden_wedding.string

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ TRAINER_GOLDEN_WEDDING_M, 936   @ tools/kanto/kanto_trainer_ids.json
.equ TRAINER_GOLDEN_WEDDING_F, 937
.equ LOCALID_GOLDEN_WEDDING_M, 8
.equ LOCALID_GOLDEN_WEDDING_F, 9
.equ OBJ_EVENT_GFX_EXPERT_M, 0x0726  @ CFRU の表 7（tools/rom/hoenn_ow.json）
.equ OBJ_EVENT_GFX_EXPERT_F, 0x073C

.align 2
.global KantoDojo_Objects
KantoDojo_Objects:
	@ 素の 7 人（08380C68 をそのまま）
	.byte 0x01, 0x36, 0x00, 0x00, 0x0B, 0x00, 0x0A, 0x00, 0x03, 0x09, 0x11, 0x00, 0x01, 0x00, 0x07, 0x00
	.4byte 0x0818A41A
	.2byte 0, 0
	.byte 0x02, 0x36, 0x00, 0x00, 0x01, 0x00, 0x0A, 0x00, 0x03, 0x0A, 0x11, 0x00, 0x01, 0x00, 0x07, 0x00
	.4byte 0x0818A431
	.2byte 0, 0
	.byte 0x03, 0x36, 0x00, 0x00, 0x0B, 0x00, 0x08, 0x00, 0x03, 0x09, 0x11, 0x00, 0x01, 0x00, 0x07, 0x00
	.4byte 0x0818A448
	.2byte 0, 0
	.byte 0x04, 0x36, 0x00, 0x00, 0x01, 0x00, 0x08, 0x00, 0x03, 0x0A, 0x11, 0x00, 0x01, 0x00, 0x07, 0x00
	.4byte 0x0818A45F
	.2byte 0, 0
	.byte 0x05, 0x36, 0x00, 0x00, 0x06, 0x00, 0x05, 0x00, 0x03, 0x08, 0x11, 0x00, 0x01, 0x00, 0x02, 0x00
	.4byte 0x0818A476
	.2byte 0, 0
	.byte 0x06, 0x5C, 0x00, 0x00, 0x05, 0x00, 0x03, 0x00, 0x03, 0x08, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00
	.4byte 0x0818A2D4
	.2byte 0x60, 0
	.byte 0x07, 0x5C, 0x00, 0x00, 0x07, 0x00, 0x03, 0x00, 0x03, 0x08, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00
	.4byte 0x0818A31A
	.2byte 0x61, 0
	@ 8 人目: ゲンゾウ（おじいさん）(2, 4)・下向き
	.byte LOCALID_GOLDEN_WEDDING_M, OBJ_EVENT_GFX_EXPERT_M & 0xFF, 0, OBJ_EVENT_GFX_EXPERT_M >> 8   @ 見た目の上の 8 ビットは 4 バイト目（tools/rom/build_hoenn.py と同じ）
	.2byte 2, 4
	.byte 3, 8, 0x11, 0
	.2byte 0, 0
	.4byte KantoGoldenWedding_EventScript
	.2byte 0, 0
	@ 9 人目: ハナ（おばあさん）(3, 4)・下向き
	.byte LOCALID_GOLDEN_WEDDING_F, OBJ_EVENT_GFX_EXPERT_F & 0xFF, 0, OBJ_EVENT_GFX_EXPERT_F >> 8
	.2byte 3, 4
	.byte 3, 8, 0x11, 0
	.2byte 0, 0
	.4byte KantoGoldenWedding_EventScript
	.2byte 0, 0

.align 2
.global KantoGoldenWedding_EventScript
KantoGoldenWedding_EventScript:
	lock
	faceplayer
	msgbox gText_KantoGoldenWedding_Hello MSG_YESNO
	compare LASTRESULT 0x0
	if 0x1 _goto KantoGoldenWedding_No
	cleartrainerflag TRAINER_GOLDEN_WEDDING_M
	cleartrainerflag TRAINER_GOLDEN_WEDDING_F
	trainerbattle11 0xB TRAINER_GOLDEN_WEDDING_M TRAINER_GOLDEN_WEDDING_F LOCALID_GOLDEN_WEDDING_M LOCALID_GOLDEN_WEDDING_F 0x0 gText_KantoGoldenWedding_IntroM gText_KantoGoldenWedding_IntroF gText_KantoGoldenWedding_LossM gText_KantoGoldenWedding_LossF gText_KantoGoldenWedding_Cannot gText_KantoGoldenWedding_Cannot
	msgbox gText_KantoGoldenWedding_After MSG_KEEPOPEN
	closemessage
	release
	end

KantoGoldenWedding_No:
	msgbox gText_KantoGoldenWedding_No MSG_KEEPOPEN
	closemessage
	release
	end
