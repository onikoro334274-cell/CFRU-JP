@ ミナモデパートのエレベーターの係（2026-10-08 ユーザー「ミナモデパートのエレベーターのボタンがない」）。
@ エメラルドの LilycoveCity_DepartmentStoreElevator_EventScript_Attendant（係に話しかけて階を選ぶ）を、素 FRLG のタマムシデパートの
@ エレベーターの部品で書き直した。tools/rom/build_hoenn_scripts.py の PATTERNS で係のスクリプトをここへ向ける。
@ - 今の階の窓: 素の special 0x132 DrawElevatorCurrentFloorWindow（0x8005: 4 = 1F 〜 8 = 5F）／閉じる 0x160
@ - 「なんがいに いきますか？」: 素のタマムシの文 0819318B・選択肢: 素の 31（5F〜1F・やめる）
@ - 揺れと窓の光: src/hoenn_tower.c の HoennDept_MoveElevator（素の AnimateElevator はシルフの窓を描くので使わない）
@ - 着く階: setdynamicwarp（群 56 = エメラルドの 13 + 43・1F〜5F は 16〜20・(2, 1) はエメラルドと同じ）
@ - 屋上はエメラルドどおり 5F の階段から
.text
.thumb
.align 2

.include "../xse_commands.s"
.include "../xse_defines.s"
.include "../asm_defines.s"

.equ DEPT_GROUP, 56
.equ TEXT_WHICH_FLOOR, 0x0819318B
.equ MULTI_DEPT_FLOORS, 31
.equ SPECIAL_DRAW_FLOOR_WINDOW, 0x132
.equ SPECIAL_CLOSE_FLOOR_WINDOW, 0x160
.equ TEXT_COLOR_FEMALE, 1

.align 2
.global HackScr_LilycoveElevator
HackScr_LilycoveElevator:
	lock
	faceplayer
	textcolor TEXT_COLOR_FEMALE
	callasm HoennDept_ElevatorSetup + 1
	special SPECIAL_DRAW_FLOOR_WINDOW
	preparemsg TEXT_WHICH_FLOOR
	waitmsg

	compare LASTRESULT 0
	if 0x1 _goto HackDept_From5F
	compare LASTRESULT 1
	if 0x1 _goto HackDept_From4F
	compare LASTRESULT 2
	if 0x1 _goto HackDept_From3F
	compare LASTRESULT 3
	if 0x1 _goto HackDept_From2F
	compare LASTRESULT 4
	if 0x1 _goto HackDept_From1F
	goto HackDept_From1F

HackDept_From5F:
	multichoicedefault 0 0 MULTI_DEPT_FLOORS 0 0x0
	goto HackDept_Choose

HackDept_From4F:
	multichoicedefault 0 0 MULTI_DEPT_FLOORS 1 0x0
	goto HackDept_Choose

HackDept_From3F:
	multichoicedefault 0 0 MULTI_DEPT_FLOORS 2 0x0
	goto HackDept_Choose

HackDept_From2F:
	multichoicedefault 0 0 MULTI_DEPT_FLOORS 3 0x0
	goto HackDept_Choose

HackDept_From1F:
	multichoicedefault 0 0 MULTI_DEPT_FLOORS 4 0x0
	goto HackDept_Choose

HackDept_Choose:
	compare LASTRESULT 0
	if 0x1 _goto HackDept_To5F
	compare LASTRESULT 1
	if 0x1 _goto HackDept_To4F
	compare LASTRESULT 2
	if 0x1 _goto HackDept_To3F
	compare LASTRESULT 3
	if 0x1 _goto HackDept_To2F
	compare LASTRESULT 4
	if 0x1 _goto HackDept_To1F
	goto HackDept_Exit

HackDept_To5F:
	setvar 0x8006 8
	setdynamicwarp DEPT_GROUP 20 0xFF 2 1
	compare 0x8005 8
	if 0x1 _goto HackDept_Exit
	call HackDept_Move
	goto HackDept_Exit

HackDept_To4F:
	setvar 0x8006 7
	setdynamicwarp DEPT_GROUP 19 0xFF 2 1
	compare 0x8005 7
	if 0x1 _goto HackDept_Exit
	call HackDept_Move
	goto HackDept_Exit

HackDept_To3F:
	setvar 0x8006 6
	setdynamicwarp DEPT_GROUP 18 0xFF 2 1
	compare 0x8005 6
	if 0x1 _goto HackDept_Exit
	call HackDept_Move
	goto HackDept_Exit

HackDept_To2F:
	setvar 0x8006 5
	setdynamicwarp DEPT_GROUP 17 0xFF 2 1
	compare 0x8005 5
	if 0x1 _goto HackDept_Exit
	call HackDept_Move
	goto HackDept_Exit

HackDept_To1F:
	setvar 0x8006 4
	setdynamicwarp DEPT_GROUP 16 0xFF 2 1
	compare 0x8005 4
	if 0x1 _goto HackDept_Exit
	call HackDept_Move
	goto HackDept_Exit

HackDept_Exit:
	special SPECIAL_CLOSE_FLOOR_WINDOW
	release
	end

HackDept_Move:
	special SPECIAL_CLOSE_FLOOR_WINDOW
	closemessage
	applymovement LASTTALKED HackDept_Move_PressButton
	waitmovement 0
	waitse
	callasm HoennDept_MoveElevator + 1
	waitstate
	return

HackDept_Move_PressButton:
	.byte 0x29, 0xFE	@ その場で速く下を向いて歩く・おわり
