@ 殿堂入りの部屋でボールを置く位置（2026-09-29・src/hoenn_league.c HoennHofFirstBallY）。
@ 素の FldEff_HallOfFameRecord（080838CC）の 080838F4〜080838FB:
@     movs r0,#60 / strh r0,[r1,#14]（tFirstBallY）/ movs r0,#0 / pop {r4}
@ を潰して、y を C から取り、潰した 4 命令をやり直して素の 080838FC（pop {r1} / bx r1）へ戻る。
@ r1 = &gTasks[taskId]（data は +8）。r0 はこの直後に上書きされるのでフックの飛び先に使える
	.arch armv4t
	.text
	.thumb

	.global	HoennHofBallsHook
	.thumb_func
HoennHofBallsHook:
	push	{r1}
	ldr	r0, =(HoennHofFirstBallY + 1)
	bl	HoennHofCallR0
	pop	{r1}
	strh	r0, [r1, #14]
	mov	r0, #0
	pop	{r4}
	ldr	r1, =0x080838FD
	bx	r1

HoennHofCallR0:
	bx	r0

	.pool
