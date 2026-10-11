@ オリジナルトレーナー ヒガン（作者 ふたーら 氏・2026-10-11 ユーザー「これモブトレーナーとして実装できる？」→「フエンタウンの近くの道路」
@ 「ほかのホウエンのモブと同じ 何度でも」）。113 ばんどうろ (11, 8)・人は build_hoenn.EXTRA_NPCS（エメラルドの人のうしろ）。
@ 話しかけるたびに戦う（勝ったあと印を下ろす＝モモカ博士と同じ）。トレーナーは TRAINER_HIGAN（938・tools/kanto/kanto_trainer_ids.json）
.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ TRAINER_HIGAN, 938
.equ TEXT_COLOR_FEMALE, 1

.global HackScr_Higan_Talk
HackScr_Higan_Talk:
	lock
	faceplayer
	textcolor TEXT_COLOR_FEMALE
	msgbox gText_Higan_Ask MSG_YESNO
	compare LASTRESULT 0x1
	if 0x0 _goto HackScr_Higan_No
	msgbox gText_Higan_Go MSG_KEEPOPEN
	closemessage
	cleartrainerflag TRAINER_HIGAN
	trainerbattle3 0x3 TRAINER_HIGAN 0x0 gText_Higan_Defeat
	msgbox gText_Higan_After MSG_KEEPOPEN
	closemessage
	release
	end

HackScr_Higan_No:
	msgbox gText_Higan_No MSG_KEEPOPEN
	closemessage
	release
	end
