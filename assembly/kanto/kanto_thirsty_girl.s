@ タマムシデパート屋上の のどが かわいた女の子（飲み物をあげる）の選択肢（2026-10-04）。
@ ユーザー「タマムシデパート屋上の女の子にジュース上げるイベントで選択肢がバグるCFRUのバグ」。
@ CFRU（CUSTOM_MULTICHOICE）は素の選択肢の表（083A557C）の 0x20〜0x25 番を自分の入れ物 gMultiChoice（0203E000・数 2〜7）に差し替える。
@ 女の子の選択肢（素の MULTICHOICE_THIRSTY_GIRL_* = 32〜37）がちょうどそこで、入れ物に残った文字（SELECT の登録道具の一覧など）が出た。
@ 直し: 素のスクリプトの switch の飛び先（0x8183FC0 ほか 6 語・repoints）をここへ向け、素の飲み物の並びを入れ物に写してから
@ CFRU の番号（2 つ = 0x20・3 つ = 0x21）で選択肢を出し、素の switch VAR_RESULT（素の multichoice の 5 バイト後ろ）へ戻る。
@ 3 つ全部持っているとき（素の 38 = 0x26）は差し替えを受けていないので素のまま

.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.macro thirsty_ask label, vanilla_id, cfru_id, back
.global \label
\label:
	setvar 0x8004 \vanilla_id
	callasm HackThirstyGirl_LoadChoices
	multichoice 0x0 0x0 \cfru_id 0x0
	goto \back
.endm

	thirsty_ask KantoThirstyGirl_Ask20, 0x20, 0x20, 0x0818400C	@ おいしいみず
	thirsty_ask KantoThirstyGirl_Ask21, 0x21, 0x20, 0x08184038	@ サイコソーダ
	thirsty_ask KantoThirstyGirl_Ask22, 0x22, 0x21, 0x08184064	@ おいしいみず・サイコソーダ
	thirsty_ask KantoThirstyGirl_Ask23, 0x23, 0x20, 0x0818409B	@ ミックスオレ
	thirsty_ask KantoThirstyGirl_Ask24, 0x24, 0x21, 0x081840C7	@ おいしいみず・ミックスオレ
	thirsty_ask KantoThirstyGirl_Ask25, 0x25, 0x21, 0x081840FE	@ サイコソーダ・ミックスオレ
