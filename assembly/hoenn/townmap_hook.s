@ タウンマップにホウエン（5 枚目）を足すためのフック 2 本。
@ ★ やりたいこと #6。詳細は docs/research/2026-09-16_town-map.md

	.text
	.thumb
	.align	2

@ ★★ ここを armv4t に固定するのが**必須**。
@   CFRU のアセンブル指定は `-mthumb` だけで `-march` を渡さないため、
@   既定では新しい ARM 向けに組まれる。すると
@       ldr r2, =0x1CCC   ->  MOVW r2, #0x1CCC   （Thumb-2）
@       bl  <遠い関数>    ->  Thumb-2 の広い BL
@   になり、**GBA（ARM7TDMI）に無い命令**が出て暴走する。
@   2026-09-16 に実機でクラッシュした（Jumped to invalid address）。
	.arch	armv4t

@ ================================================================
@ 1. 5 枚目を選べるようにする（フック先 080C20B8）
@
@ 素 FRLG は地図を開くときに「選べる最大の地図番号」を書く:
@   080C206C  if (FlagGet(0x846))      最大 = 3
@   080C2090  else if (FlagGet(0x845)) 最大 = 1
@             else                     最大 = 0
@   080C20B6  STRB r1, [r0]
@ そのすぐ後ろで、ハナダのどうくつ前の NPC が消えるフラグ（0x05C）が
@ 立っていれば 4（＝ホウエンまで）に上書きする。
@ このフラグはクチバの船の選択肢の解放条件と同じもの（ユーザー方針）。
@
@ ★ フックは「呼び出し」ではなく「飛び先の差し替え」なので、
@   上書きした 4 命令（080C20B8〜080C20BF）を自分でやり直してから戻る。
@ ★ lr を潰してよい: 元の関数は入口で lr を積んでおり（080C2054）、
@   出口はスタックから pc へ戻すので、生きた lr は使わない。
@ ================================================================
	.global	HoennTownMapUnlockHook
	.thumb_func
HoennTownMapUnlockHook:
	@ ★ 2026-09-29: 最大のページは C（HoennTownMapMaxPage）が決める。
	@   0 = 素のまま / 4 = ホウエン（フラグ 0x5C）/ 5 = シンオウ（SINNOH_TOWNMAP_DEBUG かフラグ 0x13E1）
	bl	HoennTownMapMaxPage
	cmp	r0, #0
	beq	HoennTownMapUnlock_Done
	ldr	r1, =0x02039950		@ タウンマップの状態
	ldr	r1, [r1]
	ldr	r2, =0x00001CCC		@ 選べる最大の地図番号
	add	r1, r1, r2
	strb	r0, [r1]
HoennTownMapUnlock_Done:
	ldr	r4, =0x02039950
	ldr	r2, [r4]
	ldr	r0, =0x000018B8
	add	r1, r2, r0
	ldr	r3, =0x080C20C0 + 1
	bx	r3

	.thumb_func
HoennTownMap_BxR1:
	bx	r1
	.align	2
	.pool

@ ================================================================
@ 2. （削除済み・2026-09-16）
@
@ かつてここに HoennTownMapPageHook（フック先 080C47C0）があった。
@ 「この区分はどの地図に載っているか」を返す関数だと思い込んで
@ ホウエンの区分に 4 を返させていたが、**取り違えだった**。
@
@ 080C47C0 は pokefirered でいう **GetMapsecType**:
@   0 = なし / 1 = 道など / 2 = 訪問済みの街 / 3 = 未訪問の街
@ 呼び出し元 6 箇所（080C4236 080C44A8 080C459E 080C5888 080C5896 080C597A）は
@ すべて「2 か」「3 か」で分岐するか、カーソルの種類として持つだけで、
@ **地図の絵には一切関係しない**。
@ 4 を返すと**ホウエンの街にそらをとぶの行き先が作られない**。
@
@ ホウエンの区分（0x00〜0x57 / 0xC6〜0xFC）は素の既定に落ちて 1 を返す。
@ 街として扱いたくなったら、ここに改めてフックを書くこと（#15 後半）。
@ ================================================================

@ ================================================================
@ 3. 5 枚目の地図の絵を置く場所を作る（フック先 080C1370）
@
@ ★ 素 FRLG の状態の入れ物には「地図の絵」が **4 枚ぶんしか無い**。
@   描くほうは番号から計算で場所を出している（080C1364〜）:
@       場所 = 入れ物 + 0x26 + 1200 * 番号
@   0x26 + 1200*4 = 0x12E6 で、そこは
@   **「そらをとぶ」のときに BG1 に重ねる縁飾り**（元データ 083B8528）。
@   5 枚目を選ぶとその縁飾りを地図として描いてしまい、画面が壊れる。
@   2026-09-16 に実機で確認（ユーザー報告「5 枚目がバグっている」）。
@
@ そこで **入れ物の末尾に 1200 バイト足して**（bytereplacement で
@ AllocZeroed の 0x47C0 -> 0x4C70）、そこを 5 枚目の場所にする。
@ 番号 4 以上のときだけ計算をやめて、その場所を返す。
@
@ ★ 入口の状態: r0 = 選んでいる地図番号 / r1 = 入れ物の先頭 /
@   r2 = 番号*4（フックが潰す。下で作り直す）/ r4 は呼び出し元が使用中。
@   戻り先 080C137A は `ADD r1, r1, r0` なので、r0 に**場所の差**を入れる。
@ ================================================================
	.equ	HOENN_TOWNMAP_SLOT,   0x000047C0	@ 5 枚目の絵を置く場所
	.equ	SINNOH_TOWNMAP_SLOT,  0x00004DA0	@ 6 枚目（シンオウ）の印。★ src/hoenn_townmap.c の TOWNMAP_SINNOH_SLOT と揃える
	.equ	KITAKAMI_TOWNMAP_SLOT, 0x00004DA8	@ ★ 2026-10-02 7 枚目（キタカミ）の印。src/hoenn_townmap.c の TOWNMAP_KITAKAMI_SLOT と揃える
						@   中身は読まない（C が ROM の絵を写す）。見分けるためだけの位置
	@ ★★ DecompressAndCopyTileDataToVram の「ずれ」は **タイル数**。
	@   バイト数ではない。LoadBgTiles（080017D0）が 4bpp のとき
	@       ((元の位置 + ずれ) & 0x7FF) << 5
	@   で書き込み先を出す（8bpp は << 6）。
	@   2026-09-16 にバイト数（16640）を渡して実機で壊した。
	@   16640 & 0x7FF = 256 に丸められ、**素 FRLG のタイル 256〜319 を
	@   壊したうえ、ホウエンの絵が 256 番に載っていた**。
	@ ★★ 0x06004000 から 0x800 バイト（画面ブロック 1 個分）は
	@   何かに上書きされる。2026-09-16 にタイル 520（0x06004100）へ
	@   置いて実機で壊れた（520〜575 が全滅、576 以降は無事）。
	@   犯人は未特定。**0x06004800 より前には置かないこと。**
	.equ	HOENN_TOWNMAP_TILE_INDEX, 576	@ BG0 のタイル 576 番から
	.equ	HOENN_TOWNMAP_PAL_POS,    5 * 16	@ BG パレット 5 番から
	.equ	HOENN_TOWNMAP_PAL_SIZE,   4 * 32	@ 4 本

	.global	HoennTownMapSlotHook
	.thumb_func
HoennTownMapSlotHook:
	cmp	r0, #3
	bhi	HoennTownMapSlot_Fifth
	lsl	r2, r0, #2
	add	r2, r2, r0		@ 5n
	lsl	r0, r2, #4		@ 80n
	sub	r0, r0, r2		@ 75n
	lsl	r0, r0, #4		@ 1200n
	add	r0, #0x26
	b	HoennTownMapSlot_Done
HoennTownMapSlot_Fifth:
	cmp	r0, #5
	beq	HoennTownMapSlot_Fifth_Sinnoh
	cmp	r0, #6
	beq	HoennTownMapSlot_Fifth_Kitakami
	ldr	r0, =HOENN_TOWNMAP_SLOT
	b	HoennTownMapSlot_Done
HoennTownMapSlot_Fifth_Sinnoh:
	ldr	r0, =SINNOH_TOWNMAP_SLOT
	b	HoennTownMapSlot_Done
HoennTownMapSlot_Fifth_Kitakami:
	ldr	r0, =KITAKAMI_TOWNMAP_SLOT
HoennTownMapSlot_Done:
	ldr	r2, =0x080C137A + 1
	bx	r2
	.align	2
	.pool

@ ================================================================
@ 4. 5 枚目の絵を読み込む（フック先 080C1594）
@
@ 素 FRLG の読み込みは状態機械で、最後の状態が縁飾りを展開している:
@   080C1594  LZ77UnCompWram(083B8528, 入れ物 + 0x12E6)
@ そこに **5 枚目の展開を 1 つ足す**。縁飾りは素のまま残す。
@
@ ★ 081C7A90 は `SWI 0x11`（LZ77UnCompWram。元 r0 / 先 r1）。
@ ★ r4 は呼び出し元が使っているので退避する。
@ ★ 出口の 080C15BC が `ADD sp,#4 / POP {r4} / POP {r1} / BX r1` なので、
@   積んだものは必ず戻してから 080C159E へ飛ぶこと。
@ ================================================================
	.global	HoennTownMapLoadHook
	.thumb_func
HoennTownMapLoadHook:
	push	{r4, lr}
	ldr	r4, =0x0203994C
	ldr	r4, [r4]
	ldr	r0, =0x083B8528		@ 素 FRLG の縁飾り（やり直し）
	ldr	r1, =0x000012E6
	add	r1, r4, r1
	bl	HoennTownMap_Lz77
	ldr	r0, =gHoennTownMapTilemap	@ 段階 C: 本物のホウエンの地図
	ldr	r1, =HOENN_TOWNMAP_SLOT
	add	r1, r4, r1
	bl	HoennTownMap_Lz77
	pop	{r4}
	pop	{r1}			@ 積んだ lr を捨てる
	ldr	r1, =0x080C159E + 1
	bx	r1

	.thumb_func
HoennTownMap_Lz77:
	ldr	r2, =0x081C7A90 + 1
	bx	r2

	.thumb_func
HoennTownMap_BxR2:
	bx	r2

	.thumb_func
HoennTownMap_BxR3:
	bx	r3

	.thumb_func
HoennTownMap_BxR4:
	bx	r4
	.align	2
	.pool

@ ================================================================
@ 5. 切り替えメニューからの描き直しも 5 枚目に対応させる
@    （フック先 080C27AC と 080C27F8。どちらもレジスタ r1）
@
@ ★★ 「1200 × 番号」の計算は **ROM 全体で 3 箇所**ある。
@   2026-09-16 に `r2` を一時に使う形だけで探し、`r1` を使う下の 2 つを
@   取りこぼした。そのため 5 枚目に切り替えた直後の描き直しが
@   `入れ物 + 0x26 + 1200*4`（= 縁飾り）を読み、画面が白くなっていた。
@   **1 つの命令の形だけで探してはいけない**（MAPSEC でも同じ失敗をしている）。
@
@   080C136E  一時=r2   … 地図を開いたとき（4 のところ）
@   080C27AC  一時=r1   … 切り替えメニューで決定したとき
@   080C27F8  一時=r1   … 切り替えメニューを閉じたとき
@
@ ★ どちらも直後に `LSL r0,r0,#4` と `ADD r0,#0x26` が続くが、
@   フックの 8 バイトがそこまで届かないので**飛ばして**、
@   こちらで**差の全部**を作ってから戻る。
@ ★ 生き残らせるレジスタ: A は r3（入れ物）、B は r2（入れ物）。
@ ================================================================
	.global	HoennTownMapSwitchSlotHookA
	.thumb_func
HoennTownMapSwitchSlotHookA:		@ 080C27AC -> 080C27B8
	cmp	r0, #3
	bhi	HoennTownMapSwitchA_Fifth
	lsl	r1, r0, #2
	add	r1, r1, r0
	lsl	r0, r1, #4
	sub	r0, r0, r1
	lsl	r0, r0, #4
	add	r0, #0x26
	b	HoennTownMapSwitchA_Done
HoennTownMapSwitchA_Fifth:
	cmp	r0, #5
	beq	HoennTownMapSwitchA_Fifth_Sinnoh
	cmp	r0, #6
	beq	HoennTownMapSwitchA_Fifth_Kitakami
	ldr	r0, =HOENN_TOWNMAP_SLOT
	b	HoennTownMapSwitchA_Done
HoennTownMapSwitchA_Fifth_Sinnoh:
	ldr	r0, =SINNOH_TOWNMAP_SLOT
	b	HoennTownMapSwitchA_Done
HoennTownMapSwitchA_Fifth_Kitakami:
	ldr	r0, =KITAKAMI_TOWNMAP_SLOT
HoennTownMapSwitchA_Done:
	ldr	r1, =0x080C27B8 + 1
	bx	r1
	.align	2
	.pool

	.global	HoennTownMapSwitchSlotHookB
	.thumb_func
HoennTownMapSwitchSlotHookB:		@ 080C27F8 -> 080C2804
	cmp	r0, #3
	bhi	HoennTownMapSwitchB_Fifth
	lsl	r1, r0, #2
	add	r1, r1, r0
	lsl	r0, r1, #4
	sub	r0, r0, r1
	lsl	r0, r0, #4
	add	r0, #0x26
	b	HoennTownMapSwitchB_Done
HoennTownMapSwitchB_Fifth:
	cmp	r0, #5
	beq	HoennTownMapSwitchB_Fifth_Sinnoh
	cmp	r0, #6
	beq	HoennTownMapSwitchB_Fifth_Kitakami
	ldr	r0, =HOENN_TOWNMAP_SLOT
	b	HoennTownMapSwitchB_Done
HoennTownMapSwitchB_Fifth_Sinnoh:
	ldr	r0, =SINNOH_TOWNMAP_SLOT
	b	HoennTownMapSwitchB_Done
HoennTownMapSwitchB_Fifth_Kitakami:
	ldr	r0, =KITAKAMI_TOWNMAP_SLOT
HoennTownMapSwitchB_Done:
	ldr	r1, =0x080C2804 + 1
	bx	r1
	.align	2
	.pool

@ ================================================================
@ 6. ホウエンにいるときは 5 枚目から開く（フック先 080C1240・r2）
@
@ 素 FRLG は開いた瞬間に「どの地図から見せるか」を決めている:
@   080C1240  r0 = gMapHeader.regionMapSectionId （0x02036D30 + 20）
@   080C1248  if (r0 <= 0x8E) -> 0（カントー）
@             else 083B862C の 3 行（30 バイト・0xC5 終端）から探し、
@                  見つかった行 n -> n + 1（ななしま）
@   080C12BC  [+0x479B] = [+0x479C] = その番号
@
@ **ホウエンの区分は 0x00〜0x57 なので「0x8E 以下」に落ちてカントーになる。**
@ 2026-09-16 に実機で確認（トウカシティで開いてもカントーのままだった）。
@
@ ★ あわせて「地図を切り替えてよいか」の権限も開ける。
@   080C1224 で FlagGet(0x845)（ななしまの地図 1-3）が偽なら
@   [+0x4797 + 0] = 0 にされ、**切り替えボタン自体が出なくなる**。
@   ホウエンはこのフラグと無関係なので、
@   「ホウエンにいる」か「0x05C が立っている」なら 1 に戻す。
@
@ ★ 入口で生きているもの: r1 = 0 / r7 = 0（080C123C〜080C123E で作られる）。
@   両方とも出口までに元どおりにすること。r2・r3 は潰してよい。
@ ★ 戻り先は 2 つ: 素のまま続けるなら 080C1248、
@   ホウエンなら判定を飛ばして格納だけする 080C12BC（r7 に番号を入れて渡す）。
@ ================================================================
	.global	HoennTownMapInitialPageHook
	.thumb_func
HoennTownMapInitialPageHook:
	push	{r1, lr}
	@ ★ 2026-09-29: 番号の範囲ではなく**今いるマップの地方**で決める（C の HoennTownMapInitialPage）。
	@   シンオウの番号（0〜77）もジョウトの番号（0x4E〜・0xD1〜）も「0x58 未満か 0xC5 より上」に入るため。
	@   0 = 素のまま（カントー・ななしま）/ 4 = ホウエン / 5 = シンオウ / 0xFF = ジョウト（素の判定を飛ばしてカントー）
	bl	HoennTownMapInitialPage
	mov	r3, r0
HoennTownMapInitial_CheckFlag:
	push	{r3}
	mov	r0, #0x5C		@ FLAG_HIDE_CERULEAN_CAVE_GUARD
	ldr	r1, =0x0806DEC4 + 1	@ FlagGet（遠いので bx で呼ぶ）
	bl	HoennTownMap_BxR1
	pop	{r3}
	cmp	r3, #0
	beq	HoennTownMapInitial_ByFlag
	cmp	r3, #0xFF		@ ジョウト = カントーのページ（切り替えの許可はカントーと同じくフラグで）
	bne	HoennTownMapInitial_Allow
HoennTownMapInitial_ByFlag:
	cmp	r0, #0
	beq	HoennTownMapInitial_Done
HoennTownMapInitial_Allow:
	ldr	r0, =0x0203994C
	ldr	r0, [r0]
	ldr	r1, =0x00004797
	add	r0, r0, r1
	mov	r1, #1
	strb	r1, [r0, #0]		@ 地図の切り替えを許可する
HoennTownMapInitial_Done:
	mov	r7, r3			@ 0 = カントー / 4 = ホウエン
	pop	{r1}			@ 0 に戻す
	pop	{r2}			@ 積んだ lr を捨てる
	ldr	r2, =0x02036D30		@ 素の 4 命令をやり直す
	ldrb	r0, [r2, #20]
	ldr	r3, =0x0203994C
	mov	r8, r3
	cmp	r7, #0xFF		@ ★ 2026-09-30 ジョウト: 素のななしまの探索（080C1248）を飛ばしてカントー（0）で格納へ。
	bne	HoennTownMapInitial_NotJohto	@   ジョウトの番号 0xC9〜0xF7 はななしまの表に無く、素は表の外を探し続けて真っ暗になった
	mov	r7, #0
	b	HoennTownMapInitial_Fifth
HoennTownMapInitial_NotJohto:
	cmp	r7, #0
	bne	HoennTownMapInitial_Fifth
	ldr	r3, =0x080C1248 + 1
	bx	r3
HoennTownMapInitial_Fifth:
	ldr	r3, =0x080C12BC + 1
	bx	r3
	.align	2
	.pool

@ ================================================================
@ 7. ホウエンの絵と色を VRAM に載せる（フック先 080C14E8・r2）
@
@ 素 FRLG は状態 3 で自分の絵を積む:
@   080C14E6  LDR r1, =083B61A4       （320 タイル）
@   080C14E8  MOV r4,#0 / STR r4,[sp] / MOV r0,#0 / MOV r2,#0
@   080C14F0  MOV r3,#0 / BL 080F78D0  DecompressAndCopyTileDataToVram
@ 次の状態 4（080C1524）が FreeTempTileDataBuffersIfPossible で
@ **積み終わるのを待つ**ので、ホウエンの絵もここで一緒に積む。
@ 別の状態で積むとこの待ちに入らない。
@
@ ★ 置き場所: BG0 の絵は 0x06000000 から（BG 設定は 083B85D8）。
@   FRLG が 320 タイル使い、**BG1 の絵が 0x06004000 から始まる**が
@   BG1 は 5 タイルしか使わない。そこで **タイル 520 番（0x06004100）**から
@   232 枚置く。次にぶつかるのは BG1 の画面 0x06007800（タイル 960）。
@ ★ 色: 素は BG パレット 0〜4・12〜15 を使う。5〜11 が空いているので 5〜8。
@
@ ★ 5 つ目の引数は **呼ぶ瞬間の [sp,#0]** に置く。
@   push した後は自分で 4 バイト空けること。
@ ★ 入口で r1 = 0x083B61A4 が生きている（080C14E6 で入る）。
@   潰すので、戻る前に置き直す。
@ ================================================================
	.global	HoennTownMapTilesHook
	.thumb_func
HoennTownMapTilesHook:
	push	{r4, lr}
	sub	sp, #4				@ 5 つ目の引数の置き場
	mov	r4, #0
	str	r4, [sp, #0]			@ やり方 0
	mov	r0, #0				@ bg0
	ldr	r1, =gHoennTownMapTiles
	mov	r2, #0				@ 大きさ 0 = 全部
	ldr	r3, =HOENN_TOWNMAP_TILE_INDEX	@ ★タイル数（バイトではない）
	ldr	r4, =0x080F78D0 + 1		@ DecompressAndCopyTileDataToVram
	bl	HoennTownMap_BxR4
	add	sp, #4
	ldr	r0, =gHoennTownMapPal
	mov	r1, #HOENN_TOWNMAP_PAL_POS
	mov	r2, #HOENN_TOWNMAP_PAL_SIZE
	ldr	r3, =0x0806FB90 + 1		@ LoadPalette
	bl	HoennTownMap_BxR3
	@ ★ ジョウト（カントーの西）の地形のタイルも同じ状態で積む（2026-09-29）。
	@   置き場は 2 か所: 320〜511 と 808〜959（tools/townmap/build_johto_townmap.py）。
	@   フラグが立っていなくても積む（見えないだけで害は無い）
	sub	sp, #4
	mov	r4, #0
	str	r4, [sp, #0]
	mov	r0, #0
	ldr	r1, =gJohtoTownMapTiles
	mov	r2, #0
	ldr	r3, =JOHTO_TM_TILE_INDEX_A	@ ★タイル数（バイトではない）
	ldr	r4, =0x080F78D0 + 1		@ DecompressAndCopyTileDataToVram
	bl	HoennTownMap_BxR4
	mov	r4, #0
	str	r4, [sp, #0]
	mov	r0, #0
	ldr	r1, =gJohtoTownMapTiles2
	mov	r2, #0
	ldr	r3, =JOHTO_TM_TILE_INDEX_B
	ldr	r4, =0x080F78D0 + 1
	bl	HoennTownMap_BxR4
	add	sp, #4
	pop	{r4}
	pop	{r3}				@ 積んだ lr を捨てる
	ldr	r1, =0x083B61A4			@ 素の 4 命令をやり直す
	mov	r4, #0
	str	r4, [sp, #0]
	mov	r0, #0
	mov	r2, #0
	ldr	r3, =0x080C14F0 + 1
	bx	r3
	.align	2
	.pool

@ ================================================================
@ 8. 枠を 28 列に広げる（段階 2）— 原点とカーソルの範囲
@
@ ★ なぜ原点を動かすのか
@   素 FRLG はマス (x,y) を **画面タイル (x+4, y+4)** に描く
@   （`LSL #3 / ADD #0x24`。36 = 4*8 + 4（スプライトの中心ぶん））。
@   原点 4 のままだと 28 列はタイル 4〜31 になり **画面（30 タイル）に入らない**。
@   ホウエンだけ原点を **1**（画素 8）にすると 1〜28 で収まる。
@
@ ★ スクロールの器
@   原点は定数ではなく `HoennTownMapOriginX` が出す。
@   そこで **スクロール量（入れ物 +0x4C70・画素）を引いている**ので、
@   30 列を超える地図（カントー＋ジョウトを 1 枚に繋ぐなど）を足すときは
@   この変数を動かす追従処理を書けばよい。今は常に 0。
@
@ ★ 触ってはいけないもの
@   `080C59AC` / `080C5A6E` は **マスを順に走って
@   `[0x02039964] + 0x148 + 通し番号*16` を埋めるループ**。
@   22×15 = 330 件ぶんしか無いので、広げるとはみ出して隣を壊す。
@   2026-09-16 に一度それで壊している。
@ ================================================================
	.equ	HOENN_TM_PAGE,          4	@ ホウエンは 5 枚目
	.equ	HOENN_TM_ORIGIN,        4 * 8	@ 画面タイル 4（全ページ共通）

	.equ	HOENN_TM_SCROLL_X,      0x00004D10	@ 入れ物の中のスクロール量（画素）
	.equ	REG_BG0HOFS,            0x04000010
	.equ	HOENN_TM_MAX_HOENN,     26	@ 右へ動けるのは x <= これ（= 27 まで）
	.equ	HOENN_TM_MAX_KANTO,     20	@ 素のまま

@ 戻り r3 = x の原点（画素）= 32 - スクロール量。r0〜r2・r4 以上は保つ。
@ ★ **BG はずらさない**（タイルマップを書き直す方式）。
@   画面の左右 2 列を常に空けるため、BG0HOFS は 0 に固定する。
@   位置を出すたび（＝毎フレーム）呼ばれるので、ここで 0 を書いておく。
@ ★ ホウエン以外ではスクロール量を 0 に戻す（地図を切り替えた直後の保険）。
	.thumb_func
HoennTownMapOriginX:
@ ★ 飛べる場所のアイコンをここで毎フレーム貼り直す。
@   絵を描き直したときだけでは そらをとぶ で効かなかった。
@   （エミュレータでは正しく動いたので、呼ばれる条件の問題だった）
@ ★ スクロール量は C（HoennTownMapScrollPx）から**符号つき**で受け取る（2026-09-29）。
@   ジョウトが見えるカントーは負（-240〜0）なので、ldrh で読むと壊れる。
@   スクロールしないページでは C が 0 に戻して 0 を返す（素の「カントー: スクロールなし」と同じ）。
	push	{r0, r1, r2, lr}
	bl	HoennFlyIconsTick
	bl	HoennTownMapScrollPx	@ r0 = スクロール量（画素・符号つき）
	mov	r3, #HOENN_TM_ORIGIN
	sub	r3, r3, r0
	ldr	r0, =REG_BG0HOFS
	mov	r1, #0
	strh	r1, [r0, #0]		@ ★ BG はずらさない（常に 0）
	pop	{r0, r1, r2, pc}

@ 戻り r3 = 右へ動ける x の上限。r0〜r2 は保つ。
	.thumb_func
HoennTownMapMaxX:
	push	{r0, r1, lr}
	ldr	r0, =0x0203994C
	ldr	r0, [r0]
	ldr	r1, =0x0000479B
	ldrb	r1, [r0, r1]
	cmp	r1, #HOENN_TM_PAGE
	beq	HoennTownMapMaxX_Wide
	cmp	r1, #5			@ シンオウ（6 枚目）も 28 列
	bne	HoennTownMapMaxX_Kanto
HoennTownMapMaxX_Wide:
	mov	r3, #HOENN_TM_MAX_HOENN
	pop	{r0, r1, pc}
HoennTownMapMaxX_Kanto:
	mov	r3, #HOENN_TM_MAX_KANTO
	pop	{r0, r1, pc}
	.align	2
	.pool

@ ---- 8a. カーソルのスプライト（フック先 080C41D8・r3）-------------
@   素: ADD r0,#0x24 / STRH r0,[r1,#32] / LDR r1,[r2,#28] / MOV r3,#0
	.global	HoennTownMapOriginHook1
	.thumb_func
HoennTownMapOriginHook1:
	push	{lr}
	bl	HoennTownMapOriginX
	add	r0, r0, r3
	add	r0, #4			@ スプライトの中心ぶん
	strh	r0, [r1, #32]
	ldr	r1, [r2, #28]
	mov	r3, #2			@ ★ 素の 080C41DE は #2（y の索引）。
					@   #0 にすると戻り先の LDRSH が y でなく x を
					@   読み、縦位置が横位置と同じになる。
					@   2026-09-16 に実機で「右を押すと右下へ動く」
	pop	{r0}			@ 積んだ lr を捨てる
	ldr	r0, =0x080C41E0 + 1
	bx	r0
	.align	2
	.pool

@ ---- 8b. もう 1 つの位置（フック先 080C4222・r2）-------------------
@   素: ADD r0,#0x24 / STRH r0,[r1,#4] / MOV r3,#2 / LDRSH r0,[r1,r3]
@       / LSL r0,r0,#3    … 戻り先 080C422C は y 側（触らない）
	.global	HoennTownMapOriginHook2
	.thumb_func
HoennTownMapOriginHook2:
	push	{r0, r1, lr}
@ ★ ここはカーソルを作る関数（080C41EC）の中にしか無い。
@   直前の 080C4216 `BL 080C4F24` で**カーソルの地図上の位置が決まった**
@   ばかりなので、スクロール量を正しく決め直せるのはここが最初。
@
@   絵を描くのは 080C137E で、**カーソルが作られるより先**。
@   そのとき `ComputeTownMapScroll` は前回のタウンマップのカーソル
@   （解放済みメモリ）を読んでしまう。だから
@   「サイユウシティ（右端）で閉じて、カナズミシティ（左端）で開くと
@   右にスクロールしたまま」になっていた（2026-09-17 にユーザーが発見）。
	bl	HoennTownMapScrollUpdate
	pop	{r0, r1}
	bl	HoennTownMapOriginX
	add	r0, r0, r3
	add	r0, #4
	strh	r0, [r1, #4]
	mov	r3, #2
	ldrsh	r0, [r1, r3]
	lsl	r0, r0, #3
	push	{r0, r1}
	bl	HoennTownMapScrollPy	@ ★ 縦のスクロール量（シンオウだけ・2026-09-29）
	mov	r3, r0
	pop	{r0, r1}
	sub	r0, r0, r3
	pop	{r3}
	ldr	r3, =0x080C422C + 1
	bx	r3
	.align	2
	.pool

@ ---- 8c. 主人公アイコン（フック先 080C46B0・r3）-------------------
@   素: ADD r0,#0x24 / STRH r0,[r2,#32] / LDR r2,[r1,#28] / MOV r5,#2
	.global	HoennTownMapOriginHook3
	.thumb_func
HoennTownMapOriginHook3:
	push	{lr}
	bl	HoennTownMapOriginX
	add	r0, r0, r3
	add	r0, #4
	strh	r0, [r2, #32]
	ldr	r2, [r1, #28]
	mov	r5, #2
	pop	{r3}
	ldr	r3, =0x080C46B8 + 1
	bx	r3
	.align	2
	.pool

@ ---- 8d. 選択枠の矩形（フック先 080C30C0・r2）---------------------
@   素: ADD r4,#0x20 / LDR r2,=0x3E16 / ADD r1,r5,r2 / STRH r4,[r1,#0]
@   ★ ここだけ「中心ぶん +4」が無い（矩形の左上なので）
	.global	HoennTownMapOriginHook4
	.thumb_func
HoennTownMapOriginHook4:
	push	{lr}
	bl	HoennTownMapOriginX
	add	r4, r4, r3
	ldr	r2, =0x00003E16
	add	r1, r5, r2
	strh	r4, [r1, #0]
	pop	{r3}
	ldr	r3, =0x080C30C8 + 1
	bx	r3
	.align	2
	.pool

@ ---- 8e. 右へ動ける上限（フック先 080C43F6・r3）-------------------
@   素: CMP r0,#0x14 / BGT 080C4400 / MOV r0,#2 / STRH r0,[r1,#8]
@       / MOV r5,#1
	.global	HoennTownMapRightHook
	.thumb_func
HoennTownMapRightHook:
	push	{lr}
	bl	HoennTownMapMaxX
	cmp	r0, r3
	bgt	HoennTownMapRight_Done
	mov	r0, #2
	strh	r0, [r1, #8]
	mov	r5, #1
HoennTownMapRight_Done:
	pop	{r3}
	ldr	r3, =0x080C4400 + 1
	bx	r3
	.align	2
	.pool

@ ---- 8f（削除済み・2026-09-16）------------------------------------
@   切り替えボタンの絵を動かすフックがあったが、**不要になった**。
@   ボタンは BG の (24,14) に描かれ、マスは (21,11)。
@   原点を全ページ 4 に戻してスクロール方式にしたので、
@   **絵もカーソルも同じだけスクロールして必ず一致する**。

@ ================================================================
@ 9. スクロールの追従（フック先 080C457E・r3）
@
@ カーソルの位置が決まった直後（080C454C 以降の 4 つの増減の後）に
@ スクロール量を決め直す。窓は 22 列なので
@       スクロール量 s は  x - 21 <= s <= x  に収める
@ そのうえで 0 〜 6 に丸める。
@
@ 素: BL 080C2004 / LSL r0,#24 / LSR r0,#24 / LDR r1,[r4,#0]
@     戻り先 080C4588 は MOV r3,#2（y の索引）
@ ★ 戻る前に r0 = 今の地図番号（u8）、r1 = [r4] を作り直すこと。
@ ================================================================

@ ★ スクロール量の計算と絵の書き直しは **C に集約**した
@   （src/hoenn_townmap.c の HoennTownMapScrollUpdate）。
@   アセンブリと C で同じ計算を二重に持つと必ずずれるため。

	.global	HoennTownMapFollowHook
	.thumb_func
HoennTownMapFollowHook:
	push	{lr}
	bl	HoennTownMapScrollUpdate	@ C 側（計算＋書き直し）
	ldr	r3, =0x080C2004 + 1		@ GetSelectedRegionMap
	bl	HoennTownMap_BxR3
	lsl	r0, r0, #24
	lsr	r0, r0, #24
	ldr	r1, [r4, #0]
	pop	{r3}
	ldr	r3, =0x080C4588 + 1
	bx	r3
	.align	2
	.pool

@ ================================================================
@ 10. ホウエンの区分の「種類」を返す（フック先 080C47C0・r3）
@
@ そらをとぶには `GetMapsecType` が **2** を返す必要がある
@   0 = なし / 1 = 道など / 2 = 訪問済みの街（飛べる）/ 3 = 未訪問の街
@ 素は街ごとの**訪問フラグ**を見て 2 か 3 を返す（080C4A3E。
@ `0x8A2` / `0x8A3` … と場所ごとに別の番号を積んでいる）。
@
@ ★★★ **2026-09-22 から こちらも街ごとに見る**（ユーザー決定）。
@   それまでは `0x5C`（ななしまの終わりで立つフラグ）1 つで
@   **17 都市が一気に解放**されていた。
@
@ 区分の種類は `gHoennMapsecKind`（tools/rom/build_hoenn_fly.py が生成）:
@   0 = 素の処理に任せる（カントー・ななしま）/ 1 = 道など /
@   **2 + 街の番号** = 街（0〜16）
@ 訪問済みフラグは `gHoennVisitFlagBase`（0x13EF）+ 街の番号。
@ 旗を立てるのは `src/hoenn_steps.c` の `HoennFlyVisitTick()`（毎フレーム）。
@
@ ★★ **必ず 0〜3 の範囲で返すこと。**
@   2026-09-16 に 4 を返して、ホウエンの街にそらをとぶの行き先が
@   作られなくなった（`080C5934` の `CMP r0,#0x02` を通らない）。
@
@ ★ 素の入口は PUSH {lr} なので、素へ流すときは
@   **積んだものを lr だけに戻してから** 080C47C8 へ飛ぶこと。
@ ================================================================
	.global	HoennMapsecTypeHook
	.thumb_func
HoennMapsecTypeHook:
	push	{lr}
	lsl	r0, r0, #24
	lsr	r0, r0, #24
	push	{r0}			@ 区分を退避
	@ ★ シンオウのページ（6 枚目）では番号がシンオウだけのもの。C が 0 / 1 を返す（0xFF = いつもどおり）
	bl	SinnohMapsecType
	cmp	r0, #0xFF
	beq	HoennMapsecType_NotSinnoh
	add	sp, #4
	pop	{pc}
HoennMapsecType_NotSinnoh:
	ldr	r0, [sp, #0]		@ 区分を戻す
	ldr	r1, =gHoennMapsecKind
	ldrb	r1, [r1, r0]
	cmp	r1, #0
	beq	HoennMapsecType_Vanilla
	cmp	r1, #1
	beq	HoennMapsecType_Route
	@ ---- 街 ---- r1 = 2 + 街の番号。訪問済みフラグを見る
	sub	r1, #2
	ldr	r0, =gHoennVisitFlagBase
	ldrh	r0, [r0, #0]
	add	r0, r0, r1		@ フラグの番号
	ldr	r1, =0x0806DEC4 + 1	@ FlagGet
	bl	HoennTownMap_BxR1
	add	sp, #4			@ 退避した区分を捨てる
	cmp	r0, #0
	beq	HoennMapsecType_NotYet
	mov	r0, #2			@ 訪問済みの街（飛べる）
	pop	{pc}
HoennMapsecType_NotYet:
	mov	r0, #3			@ 街だがまだ飛べない
	pop	{pc}
HoennMapsecType_Route:
	add	sp, #4
	mov	r0, #1
	pop	{pc}
HoennMapsecType_Vanilla:
	pop	{r0}			@ 区分を戻す（積みは lr だけになる）
	sub	r0, #0x58
	ldr	r1, =0x080C47C8 + 1
	bx	r1
	.align	2
	.pool

@ ================================================================
@ 12. 主人公アイコンを作るときにスクロール量を引く
@
@ ★ 素 FRLG はアイコンの画面 x を **作るときに一度だけ** 出して、
@   あとは動かさない（スプライトのコールバックは 0x08007188 =
@   `BX lr` の空関数）。
@
@ ★ 位置の式が **カーソルと違う書き方**をされている:
@     080C54A0  LSL r1,r1,#19
@     080C54A2  MOV r3,#0x90
@     080C54A4  LSL r3,r3,#14      @ 0x240000
@     080C54A6  ADD r1,r1,r3
@     080C54A8  ASR r1,r1,#16      @ ＝ x * 8 + 36
@   カーソルは `LSL #3` / `ADD #0x24`。**同じ計算でも形が違う**ので、
@   形だけで探すと取りこぼす（CLAUDE.md 6.12）。
@
@ ★ ここを直さないと、地図を開いた直後（カーソルを動かす前）に
@   スクロール量が引かれず、**アイコンが地図の枠の外**に出る
@   （2026-09-17 にユーザーが発見。主人公がスクロール先の街にいるとき）。
@
@ 8 バイトぶん（080C54A0〜080C54A7）を潰し、r1 に最終的な画素 x を入れて
@ **080C54AA へ戻る**（080C54A8 の ASR は通さない）。
@ r0（アイコンの入れ物）と r2 は戻り先で使うので保つ。
@ ================================================================
	.global	HoennPlayerIconXHook
	.thumb_func
HoennPlayerIconXHook:
	push	{r0, r2, lr}
	mov	r2, #0
	ldrsh	r1, [r0, r2]		@ アイコンの x（マス）
	lsl	r1, r1, #3
	add	r1, #36			@ 素と同じ x * 8 + 36
	push	{r1}
	bl	HoennTownMapScrollPx	@ ★ 符号つき（ジョウトが見えるカントーは負）・入れ物が無ければ 0
	pop	{r1}
	sub	r1, r1, r0
	pop	{r0, r2}
	pop	{r3}			@ 積んだ lr を捨てる
	ldr	r3, =0x080C54AA + 1
	bx	r3
	.align	2
	.pool

@ ================================================================
@ 13. 飛べる場所のアイコンをホウエンにも出す
@
@ ★ 素のアイコン作成ループ（080C5934）は
@     x: CMP r4,#0x15（0〜21） / y: CMP r5,#0x0E / 地図: CMP r1,#0x03
@   で回る。**ホウエン（5 枚目）が最初から除外**されている。
@   上限 2 つは bytereplacement で 0x1B（27 列）と 0x04（5 枚）に広げた。
@
@ ★★ ただしアイコンの入れ物は **25 件**しかない
@   （後始末のループが 080C5C28 / 080C5C6C で `CMP r5,#0x18`）。
@   素が 14 個使い、ホウエンは 23 マスあるので、全部の地図ぶんを
@   一度に作ると 37 個で溢れる。
@   そこで **「今見ている地図」のぶんだけ作る**。
@   地図を切り替えたら C 側が作り直す。
@
@ ★ 0xC5（地名なし）は GetMapsecType が 0 を返す（飛び先 080C4A30）。
@   だから x を 27 まで広げてもカントー側に余計なアイコンは出ない。
@ ================================================================

@ ---- 13a. 地図 1 枚ごとに「作るか飛ばすか」を決める ----------------
@   フック先 080C5950（地図ループの先頭）。潰すのは 8 バイト:
@     MOV r5,#0 / LSL r0,r1,#24 / ADD r1,#1 / MOV r10,r1
	.global	HoennFlyIconMapHook
	.thumb_func
HoennFlyIconMapHook:
	mov	r5, #0			@ 素どおり（y = 0）
	lsl	r0, r1, #24
	add	r1, #1
	mov	r10, r1
	lsr	r0, r0, #24		@ r0 = この地図の番号
	push	{r0, r1, lr}
	bl	HoennFlyIconWantMap	@ C。0 ならこの地図は飛ばす
	mov	r2, r0
	pop	{r0, r1}
	pop	{r3}			@ 積んだ lr を捨てる
	cmp	r2, #0
	beq	HoennFlyIconMapHook_Skip
	ldr	r3, =0x080C595A + 1	@ 素の STR r0,[sp,#8] へ戻る
	bx	r3
HoennFlyIconMapHook_Skip:
	ldr	r3, =0x080C59BA + 1	@ 地図ループの次へ
	bx	r3
	.align	2
	.pool

@ ---- 13b. 作り終えたら位置をスクロールに合わせる -------------------
@   フック先 080C59CC（関数の出口）。潰すのは 8 バイト:
@     MOV r10,r5 / POP {r4,r5,r6,r7} / POP {r0} / BX r0
@   ★ r3 はここでは死んでいる（080C59C8 で r8 へ移したあと）。
	.global	HoennFlyIconDoneHook
	.thumb_func
HoennFlyIconDoneHook:
	mov	r10, r5			@ 素どおり
	pop	{r4, r5, r6, r7}
	pop	{r0}			@ 戻り先
	push	{r0}
	bl	HoennFlyIconsPlace	@ C。位置合わせと窓の外の非表示
	pop	{r0}
	bx	r0
	.align	2
	.pool

@ ---- 13c. 同じ街に 2 個アイコンを作らない ---------------------------
@   フック先 080C5980。潰すのは 8 バイト:
@     LSR r0,r0,#24 / CMP r0,#0x02 / BNE 080C59A6 / ADD r0,r7,#0
@   ★ ループの持ち物: r4 = x / r8 = y<<16 / [sp,#8] = 地図番号 /
@     r7 = 作ったアイコンの通し番号。
@   ★ ホウエンは 2 マスにまたがる街が 6 つある。サイユウ以外は 1 個でよい。
	.global	HoennFlyIconCellHook
	.thumb_func
HoennFlyIconCellHook:
	lsr	r0, r0, #24		@ 素どおり（区分の種類）
	cmp	r0, #2
	bne	HoennFlyIconCellHook_Skip
	push	{r1, r2, lr}
	ldr	r0, [sp, #20]		@ [sp,#8] + 積んだ 3 語 = 地図番号
	mov	r1, r8
	asr	r1, r1, #16		@ y
	lsl	r2, r4, #16
	lsr	r2, r2, #16		@ x
	mov	r3, r7			@ 置き場の番号（＝通し番号）
	bl	HoennFlyIconWantCell	@ C。0 ならこのマスは作らない
	mov	r3, r0
	pop	{r1, r2}
	pop	{r0}			@ 積んだ lr を捨てる
	cmp	r3, #0
	beq	HoennFlyIconCellHook_Skip
	add	r0, r7, #0		@ 素どおり
	ldr	r3, =0x080C5988 + 1
	bx	r3
HoennFlyIconCellHook_Skip:
	ldr	r3, =0x080C59A6 + 1	@ 次のマスへ
	bx	r3
	.align	2
	.pool

@ ---- 13d. アイコンを作る前に必ず全部消す -------------------------
@   フック先 080C5940（作成関数 080C5934 の入口）。潰すのは 8 バイト:
@     MOV r7,#0 / MOV r0,#3 / BL 080C1FE8
@   ★ 素は「一度作ったら作り直さない」前提。消す処理を作り直し側だけに
@     置いたら、地図を切り替えるたびにアイコンが積み上がった。
@     **入口で消せば、誰が何度作っても積み上がらない。**
	.global	HoennFlyIconClearHook
	.thumb_func
HoennFlyIconClearHook:
	push	{lr}
	bl	HoennFlyIconsClear
	mov	r7, #0			@ 素どおり（通し番号）
	mov	r0, #3			@ 素どおり（権限の 4 本目）
	ldr	r3, =0x080C1FE8 + 1
	bl	HoennTownMap_BxR3	@ 素の BL 080C1FE8 の代わり
	pop	{r3}
	mov	lr, r3
	ldr	r3, =0x080C5948 + 1
	bx	r3
	.align	2
	.pool


@ ================================================================
@ 14. 地図の切り替え画面を 2 列 5 枚にする
@
@ ★ 詳細は src/hoenn_townmap.c の末尾と
@   docs/research/2026-09-17_switch-map-menu.md
@
@ 素は縦 1 列しか想定しておらず、5 項目だと 24 + 32*5 = 184 画素で
@ 画面（160）からはみ出す。左 3 枚・右 2 枚の 2 列に組み直す。
@
@ ★ 縦位置は **開始行 `[入れ物+0x1CCE]`** 1 つで全部決まるので、
@   選択が動くたびに入れ替えれば枠も矢印もついてくる。
@   横位置だけは定数が 4 箇所に焼き込んであるので、そこを差し替える。
@
@ ★ 潰すバイト数は **フック先が 4 の倍数なら 8、そうでなければ 10**
@   （scripts/insert.py の Hook）。10 のほうは 5 命令目までやり直すこと。
@ ================================================================

@ ---- 14a. 一覧の絵とタイルマップを読み込む ----------------------
@   フック先 080C2130（項目数 0 または 3 以上のときに来る枝）。
@   潰すのは 8 バイト: LDR r0,=0x02039950 / LDR r1,[r0] /
@                      LDR r3,=0x1CCE / ADD r2,r1,r3
@   ★ 素はこの先 080C2138 で開始行 3 を書き、080C213C で 4 枚ぶんの
@     タイルマップを、080C2148 で絵を読み込む。**両方こちらで済ませて**
@     080C2152 へ飛ぶ。r4 は素が 080C214A で入れる値を自分で入れる。
@   ★ r6 ＝ 開いたときの項目番号（080C205A で作り、080C2162 で書く）。
	.global	HoennSwitchMenuInitHook
	.thumb_func
HoennSwitchMenuInitHook:
	add	r0, r6, #0
	bl	HoennSwitchMenuInit	@ C
	ldr	r4, =0x02039950		@ 080C2152 以降が使う
	ldr	r0, =0x080C2152 + 1
	bx	r0
	.align	2
	.pool

@ ---- 14b. 選択枠の左端（開いたとき・080C257C）-------------------
@   潰すのは 8 バイト: LDR r1,=0x1CD4 / ADD r0,r2,r1 /
@                      MOV r1,#0x48 / STRH r1,[r0]
@   ★ `[+0x1CD4]` は ROM 全体で誰も読まないので書き戻さない。
@     効くのは直後に積まれるスタック上の窓の矩形（r1）だけ。
@   ★ r2 ＝ 入れ物。C が潰すので退避する。
	.global	HoennSwitchMenuLeftHookA
	.thumb_func
HoennSwitchMenuLeftHookA:
	push	{r2}
	mov	r0, #0
	bl	HoennSwitchMenuEdgeX	@ C
	add	r1, r0, #0
	pop	{r2}
	ldr	r0, =0x080C2584 + 1
	bx	r0
	.align	2
	.pool

@ ---- 14c. 選択枠の右端（開いたとき・080C25B2）-------------------
@   ★ 080C25B2 は 4 の倍数ではないので **10 バイト**潰れる。
@     5 命令目の LDR r0,[sp,#4]（080C25BA）までやり直すこと。
@   ★ r2 / r3 が生きている（r3 は 0xFFFF）。r4・r5 は C が守る。
	.global	HoennSwitchMenuRightHookA
	.thumb_func
HoennSwitchMenuRightHookA:
	push	{r2, r3}
	mov	r0, #1
	bl	HoennSwitchMenuEdgeX	@ C
	add	r1, r0, #0
	pop	{r2, r3}
	ldr	r0, =0x080C25BC + 1
	mov	lr, r0			@ 素の関数は入口で lr を積んである
	ldr	r0, [sp, #4]		@ 潰した 080C25BA をやり直す
	bx	lr
	.align	2
	.pool

@ ---- 14d. 選択枠の左端（毎フレーム・080C266C）------------------
@   ここでは r2 / r3 はまだ使われていない（080C267E 以降で入る）。
	.global	HoennSwitchMenuLeftHookB
	.thumb_func
HoennSwitchMenuLeftHookB:
	mov	r0, #0
	bl	HoennSwitchMenuEdgeX	@ C
	add	r1, r0, #0
	ldr	r0, =0x080C2674 + 1
	bx	r0
	.align	2
	.pool

@ ---- 14e. 選択枠の右端（毎フレーム・080C26A2）------------------
@   ★ ここも 4 の倍数ではないので 10 バイト。
@     r2 ＝ 0xFFFF / r3 ＝ 入れ物+0x1CD6 が生きている。
	.global	HoennSwitchMenuRightHookB
	.thumb_func
HoennSwitchMenuRightHookB:
	push	{r2, r3}
	mov	r0, #1
	bl	HoennSwitchMenuEdgeX	@ C
	add	r1, r0, #0
	pop	{r2, r3}
	ldr	r0, =0x080C26AC + 1
	mov	lr, r0
	ldr	r0, [sp, #4]		@ 潰した 080C26AA をやり直す
	bx	lr
	.align	2
	.pool

@ ---- 14f. 上下左右で選択を動かす（080C26C8）--------------------
@   素の上キー（080C26C8〜080C26F0）と下キー（080C26F2〜080C2724）を
@   まとめて置き換え、080C2726（A ボタンの処理）へ飛ぶ。
@   ★ r6 ＝「描き直しが要る」の印。素と同じく 1 を立てる。
@     r5（入れ物）・r7・r8 は C が守る。
	.global	HoennSwitchMenuMoveHook
	.thumb_func
HoennSwitchMenuMoveHook:
	bl	HoennSwitchMenuMove	@ C。戻り 1 なら動いた
	cmp	r0, #0
	beq	HoennSwitchMenuMove_Done
	mov	r6, #1
HoennSwitchMenuMove_Done:
	ldr	r0, =0x080C2726 + 1
	bx	r0
	.align	2
	.pool

@ ---- 14g. 左右の矢印スプライトを置き直す（080C289C）------------
@   素のコールバックは y しか直していない。2 列では x も動く。
@   ★ ここは**スプライトのコールバック**で、r0 に相手が入って来る。
@     だからフックに使うレジスタは r0 以外（hooks の 3 列目を 3 にする）。
@   ★ lr は呼び出し元へ戻るために生きている。bl で潰すので積んでおく。
	.global	HoennSwitchMenuArrowHook
	.thumb_func
HoennSwitchMenuArrowHook:
	push	{lr}
	bl	HoennSwitchMenuArrowMove	@ C
	pop	{r0}
	bx	r0
	.align	2
	.pool

@ ================================================================
@ 15. ボタンを画面に固定する（2026-09-18・作り直し）
@
@ ★ 詳細は src/hoenn_townmap.c の末尾と
@   docs/research/2026-09-16_town-map.md
@
@ ボタンの絵は画面の列 24〜26 に固定した（btnX）。
@ **当たり判定のマスのほうを窓の右端（21 + スクロール量）へ動かす。**
@
@ ★★ 判定は **9 箇所**あり、形が 3 種類ある。**全部押さえること。**
@   2026-09-18 に 2 番目の形（32 ビットまとめ読み）を取りこぼして
@   「ボタンは付いてくるが A が効かない」になった。CLAUDE.md 6.12。
@
@ ★ 潰れるバイト数はフック先が 4 の倍数かで変わる（CLAUDE.md 6.14）。
@ ★ 潰した範囲の**途中**へ飛んでくる分岐が無いことも確かめること。
@   `080C4664` の `B 080C46A4` があるので、下の 15c は
@   **`080C46A0` ではなく `080C469C`** に置いてある。
@   `verify_townmap.py` の「フックの途中へ飛ぶ分岐」が毎回見る。
@ ================================================================

@ ---- 15a. 「カーソルの x」を画面での列に差し替える ---------------
@   素: BL 080C46EC / LSL r0,r0,#16 / LSR r0,r0,#16 / CMP r0,#0x15
@   ここは素が BL を置いていた場所なので r0〜r3 と lr は自由に使える。
	.global	HoennTownMapBtnXHook1
	.thumb_func
HoennTownMapBtnXHook1:			@ 080C1678（8 バイト）
	bl	HoennTownMapCursorScreenX
	ldr	r3, =0x080C1680 + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapBtnXHook3
	.thumb_func
HoennTownMapBtnXHook3:			@ 080C188C（8 バイト）
	bl	HoennTownMapCursorScreenX
	ldr	r3, =0x080C1894 + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapBtnXHook4
	.thumb_func
HoennTownMapBtnXHook4:			@ 080C18C0（8 バイト）
	bl	HoennTownMapCursorScreenX
	ldr	r3, =0x080C18C8 + 1
	bx	r3
	.align	2
	.pool

@   ★ 4 の倍数でないので 10 バイト潰れる ＝ CMP まで消える。
@     自分で比べてから BNE へ戻る（LDR も BX もフラグを変えない）。
	.equ	HOENN_TM_BTN_X, 21	@ ★ src/hoenn_townmap.c の
					@   TOWNMAP_BUTTON_X と揃えること

	.global	HoennTownMapBtnXHook2
	.thumb_func
HoennTownMapBtnXHook2:			@ 080C16A6（10 バイト）
	bl	HoennTownMapCursorScreenX
	cmp	r0, #HOENN_TM_BTN_X
	ldr	r3, =0x080C16B0 + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapBtnXHook5
	.thumb_func
HoennTownMapBtnXHook5:			@ 080C627E（10 バイト）
	bl	HoennTownMapCursorScreenX
	cmp	r0, #HOENN_TM_BTN_X
	ldr	r3, =0x080C6288 + 1
	bx	r3
	.align	2
	.pool

@ ---- 15b. ★★ A の判定（x と y を 32 ビットまとめて比べている）------
@   素: LDR r1,[r0,#0] / LDR r0,=0x000?0015 / CMP r1,r0 / BNE ...
@   ★ r2（0x03003130）と r3（押されたキー）が生きている。C が潰すので退避。
@   ★ フックが潰すレジスタは **1**（080C4428 / 080C443A で r1 は書かれる）。

	.global	HoennTownMapBtnExitHook
	.thumb_func
HoennTownMapBtnExitHook:		@ 080C4428（8 バイト。BNE まで潰れる）
	push	{r2, r3}
	bl	HoennTownMapCursorPacked
	pop	{r2, r3}
	ldr	r1, =0x000D0015		@ y=13, x=21 ＝「おわる」
	cmp	r0, r1
	bne	HoennTownMapBtnExit_No
	ldr	r1, =0x080C4430 + 1	@ 一致（音を鳴らして 6 を返す）
	bx	r1
HoennTownMapBtnExit_No:
	ldr	r1, =0x080C4438 + 1	@ 不一致（次の判定へ）
	bx	r1
	.align	2
	.pool

	.global	HoennTownMapBtnSwitchHook
	.thumb_func
HoennTownMapBtnSwitchHook:		@ 080C443A（10 バイト。MOV r0,#0 まで）
	push	{r2, r3}
	bl	HoennTownMapCursorPacked
	pop	{r2, r3}
	ldr	r1, =0x000B0015		@ y=11, x=21 ＝「きりかえ」
	cmp	r0, r1
	bne	HoennTownMapBtnSwitch_No
	mov	r0, #0			@ 潰した 080C4442 をやり直す
	ldr	r1, =0x080C4444 + 1
	bx	r1
HoennTownMapBtnSwitch_No:
	ldr	r1, =0x080C44FE + 1
	bx	r1
	.align	2
	.pool

@ ---- 15c. カーソルをボタンへ寄せる 2 箇所 ------------------------
@   素はマス番号 21 を直接書いている。窓の右端に動くので作り直す。
@   ★ r1 ＝ カーソルの入れ物。C が潰すので退避する。

@   フック先 080C465C（8 バイト）。素:
@     MOV r0,#0x15 / STRH r0,[r1,#0] / MOV r0,#0x0B / STRH r0,[r1,#2]
	.global	HoennTownMapBtnSnapHook1
	.thumb_func
HoennTownMapBtnSnapHook1:
	push	{r1}
	bl	HoennTownMapButtonCellX
	pop	{r1}
	strh	r0, [r1, #0]
	push	{r1}
	mov	r0, #0x0B		@ 切り替えボタンの行（シンオウは縦スクロールのぶん下へ）
	bl	HoennTownMapButtonCellY
	pop	{r1}
	strh	r0, [r1, #2]
	ldr	r3, =0x080C4664 + 1
	bx	r3
	.align	2
	.pool

@   フック先 **080C469C**（8 バイト）。素:
@     MOV r0,#0x0D / STRH r0,[r1,#2] / MOV r0,#0x15 / STRH r0,[r1,#0]
@   ★ 080C46A0 に置くと、`080C4664` の `B 080C46A4` が
@     **フックの飛び先ポインタ語の中**へ飛んでしまう。1 つ前に置くこと。
	.global	HoennTownMapBtnSnapHook2
	.thumb_func
HoennTownMapBtnSnapHook2:
	push	{r1}
	bl	HoennTownMapButtonCellX
	pop	{r1}
	strh	r0, [r1, #0]
	push	{r1}
	mov	r0, #0x0D		@ おわるボタンの行（シンオウは縦スクロールのぶん下へ）
	bl	HoennTownMapButtonCellY
	pop	{r1}
	strh	r0, [r1, #2]
	ldr	r3, =0x080C46A4 + 1
	bx	r3
	.align	2
	.pool

@ ================================================================
@ 16. ジョウト（カントーの西）へカーソルを動かせるようにする（フック先 080C440C・r3）
@
@ 素（左ボタン）:
@   080C440C  MOV r3,#0 / LDRSH r0,[r1,r3] / CMP r0,#0 / BLE 080C441A
@   080C4414  （左へ 1 マス動かす）
@ 「x が 0 なら動かない」の 0 を、C の HoennTownMapMinCursorX に置き換える
@ （ジョウトが見えるカントーだけ -30・ほかは素と同じ 0）。
@ ★ 戻り先で生きているのは r1（カーソル）・r2（gMain）・r5・r6。r0 と r3 は捨ててよい
@   （080C4414 は r0 を作り直し、080C441A は r3 を読み直す）。
@ ★ 080C440C〜080C4413 の途中へ飛んでくる分岐は無い（080C4408 の BEQ は 080C441A へ）。
@ ================================================================
	.global	HoennTownMapLeftHook
	.thumb_func
HoennTownMapLeftHook:
	push	{r1, r2, lr}
	bl	HoennTownMapMinCursorX	@ r0 = 左へ動ける x の下限
	pop	{r1, r2}
	pop	{r3}			@ 積んだ lr を捨てる
	mov	r3, r0
	mov	r0, #0
	ldrsh	r0, [r1, r0]		@ カーソルの x
	cmp	r0, r3
	ble	HoennTownMapLeft_No
	ldr	r3, =0x080C4414 + 1
	bx	r3
HoennTownMapLeft_No:
	ldr	r3, =0x080C441A + 1
	bx	r3
	.align	2
	.pool

@ ================================================================
@ 17. タウンマップを閉じるときにジョウトの印も消す（フック先 080C5C70・r1）
@
@ 素の FreeMapIcons（そらをとぶ・タウンマップの両方の後始末）の終わり:
@   080C5C70  LDR r4,=0x02039964 / LDR r0,[r4] / CMP r0,#0 / BEQ 080C5C80
@   080C5C78  BL 08002BC4（Free）/ ...
@ 印の絵のタグは直前のループで解放されている。借りていたジョウトの印をここで消す。
@ ★ 080C5C70〜080C5C77 の途中へ飛んでくる分岐は無い（080C5C6E の BLS は 080C5C30 へ）。
@ ================================================================
	.global	JohtoTownMapFreeIconsHook
	.thumb_func
JohtoTownMapFreeIconsHook:
	bl	JohtoTownMapFreeIcons
	ldr	r4, =0x02039964
	ldr	r0, [r4, #0]
	cmp	r0, #0
	beq	JohtoTownMapFree_Skip
	ldr	r1, =0x080C5C78 + 1
	bx	r1
JohtoTownMapFree_Skip:
	ldr	r1, =0x080C5C80 + 1
	bx	r1
	.align	2
	.pool

@ ================================================================
@ 18. ジョウトのダンジョンの区分の種類（フック先 080C4A5C・r1）
@
@ 素の GetDungeonMapsecType（080C4A5C）:
@   PUSH {lr} / LSL r0,#24 / LSR r0,#24 / SUB r0,#0x7E / CMP r0,#0x47 / BLS 表へ
@   表の外は 080C4CA4 で 1（道）を返す。ダンジョン名の色は「表[種類 - 2]」なので 1 は表の手前を読む。
@ ジョウトの区分なら C が 3 を返し、そのまま戻る。それ以外は素のまま。
@ ★ r1 は素の表の外の出口（080C4CA4 MOV r0,#1 / POP {r1} / BX r1）まで読まれない。
@ ================================================================
	.global	JohtoDungeonTypeHook
	.thumb_func
JohtoDungeonTypeHook:
	push	{lr}
	lsl	r0, r0, #24
	lsr	r0, r0, #24
	push	{r0}
	bl	JohtoDungeonMapsecType	@ r0 = 0（ジョウトではない）/ 3
	cmp	r0, #0
	beq	JohtoDungeonType_Vanilla
	add	sp, #4			@ 退避した区分を捨てる
	pop	{r1}
	bx	r1
JohtoDungeonType_Vanilla:
	pop	{r0}			@ 区分を戻す（積みは素と同じ lr だけ）
	sub	r0, #0x7E
	ldr	r1, =0x080C4A64 + 1
	bx	r1
	.align	2
	.pool

@ ================================================================
@ 19. シンオウ（6 枚目）の縦スクロール（2026-09-29）
@
@ シンオウは 28 × 23 マスで、カーソルの窓（15 行）に入らない。横と同じく
@ **絵を書き直す**方式で縦にもずらす（src/hoenn_townmap.c の ComputeTownMapScrollY）。
@ ここでは y を使う所を押さえる（素の ROM を逆アセンブルして数えた・カーソルの y を読む 080C46F8 の呼び出しは 8 か所）:
@   19a 080C41E0 カーソルのスプライトの y（素: LDRSH r0,[r2,r3] / LSL #3 / ADD #0x24 / STRH [r1,#34]）
@   19b 080C43D8 下へ動ける限界（素: MOV r3,#2 / LDRSH / CMP r0,#0x0D / BGT）
@   19c 080C30B6 選択枠の y（素: BL 080C46F8 / LSL r0,#16 / LDR r5,[r5] / LSL r4,#3）★ 4 の倍数でない＝10 バイト
@   19d ボタンの判定の y 5 か所（BL 080C46F8 + LSL + LSR［+ CMP］・横の 15a と対）
@   （8b カーソルを作るときの y・ボタンへ寄せる y・A の判定（HoennTownMapCursorPacked）は上で直した）
@   080C1600 / 080C540C はマスの番号として使う（地名を引く・主人公の位置）ので触らない
@ ★ シンオウ以外は縦のスクロール量が 0 なので、素とまったく同じ値になる
@ ================================================================
	.global	HoennTownMapOriginYHook
	.thumb_func
HoennTownMapOriginYHook:		@ 080C41E0（8 バイト・r0）
	@ ★★ 飛ぶのに使うレジスタは r0。r3 は y の添字（2）として生きている（OriginHook1 が入れる）。
	@   最初 r3 にしてフックの飛び先の番地で添字を潰し、カーソルが 0 行目に貼り付いた
	@   （2026-09-29 実機「一番下の行から上にいけない」）。念のため添字も入れ直す
	mov	r3, #2
	ldrsh	r0, [r2, r3]
	lsl	r0, r0, #3
	push	{r0, r1, r2}
	bl	HoennTownMapScrollPy
	mov	r3, r0
	pop	{r0, r1, r2}
	sub	r0, r0, r3
	add	r0, #0x24
	strh	r0, [r1, #34]
	ldr	r3, =0x080C41E8 + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapDownHook
	.thumb_func
HoennTownMapDownHook:			@ 080C43D8（8 バイト・r3）
	push	{r1, r2, lr}
	bl	HoennTownMapMaxCursorY	@ r0 = 下へ動ける y の上限（素は 13）
	pop	{r1, r2}
	pop	{r3}			@ 積んだ lr を捨てる
	mov	r3, r0
	mov	r0, #2
	ldrsh	r0, [r1, r0]		@ カーソルの y
	cmp	r0, r3
	bgt	HoennTownMapDown_No
	ldr	r3, =0x080C43E0 + 1
	bx	r3
HoennTownMapDown_No:
	ldr	r3, =0x080C43E6 + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapRectYHook
	.thumb_func
HoennTownMapRectYHook:			@ 080C30B6（10 バイト・r1）
	bl	HoennTownMapCursorScreenY
	lsl	r0, r0, #16
	ldr	r5, [r5, #0]
	lsl	r4, r4, #3
	ldr	r1, =0x080C30C0 + 1	@ 080C30C0 は 8d のフック（原点の x）
	bx	r1
	.align	2
	.pool

	.equ	HOENN_TM_BTN_Y_SWITCH, 11
	.equ	HOENN_TM_BTN_Y_EXIT,   13

	.global	HoennTownMapBtnYHook1
	.thumb_func
HoennTownMapBtnYHook1:			@ 080C1684（8 バイト）
	bl	HoennTownMapCursorScreenY
	ldr	r3, =0x080C168C + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapBtnYHook2
	.thumb_func
HoennTownMapBtnYHook2:			@ 080C16B2（10 バイト。CMP まで消える）
	bl	HoennTownMapCursorScreenY
	cmp	r0, #HOENN_TM_BTN_Y_EXIT
	ldr	r3, =0x080C16BC + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapBtnYHook3
	.thumb_func
HoennTownMapBtnYHook3:			@ 080C1898（8 バイト）
	bl	HoennTownMapCursorScreenY
	ldr	r3, =0x080C18A0 + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapBtnYHook4
	.thumb_func
HoennTownMapBtnYHook4:			@ 080C18CC（8 バイト）
	bl	HoennTownMapCursorScreenY
	ldr	r3, =0x080C18D4 + 1
	bx	r3
	.align	2
	.pool

	.global	HoennTownMapBtnYHook5
	.thumb_func
HoennTownMapBtnYHook5:			@ 080C628A（10 バイト。CMP まで消える）
	bl	HoennTownMapCursorScreenY
	cmp	r0, #HOENN_TM_BTN_Y_EXIT
	ldr	r3, =0x080C6294 + 1
	bx	r3
	.align	2
	.pool

@ ================================================================
@ 20. シンオウのページの地名（2026-09-29）
@
@ 地名の表を引くのは 2 か所だけ（表の番地 083B8834 のリテラルは 080C1E78 / 080C5F9C の 2 つ）:
@   20a 080C5F88 GetMapName（素: LDR r0,=表 / LSL r1,r5,#2 / ADD r1,r0 / LDR r1,[r1]）→ r1 = 文字
@   20b 080C1DE8 ダンジョン名（素: LDR r0,=表 / LSL r4,#2 / ADD r4,r0 / LDR r4,[r4]）→ r4 = 文字
@ C の HoennTownMapNameFor が、シンオウのページを見ているときだけシンオウの表を引く。
@ ★ r4〜r6 は C が保つ。r0〜r3 は戻り先で作り直される
@ ================================================================
	.global	HoennTownMapNameHook
	.thumb_func
HoennTownMapNameHook:			@ 080C5F88（8 バイト・r0）
	add	r0, r5, #0
	bl	HoennTownMapNameFor
	add	r1, r0, #0
	ldr	r0, =0x080C5F90 + 1
	bx	r0
	.align	2
	.pool

	.global	HoennTownMapDungeonNameHook
	.thumb_func
HoennTownMapDungeonNameHook:		@ 080C1DE8（8 バイト・r0）
	add	r0, r4, #0
	bl	HoennTownMapNameFor
	add	r4, r0, #0
	ldr	r0, =0x080C1DF0 + 1
	bx	r0
	.align	2
	.pool

@ ================================================================
@ 21. 主人公の位置の表（シンオウのマップにいるときはシンオウの表・2026-09-29）
@
@ 素の GetPlayerPositionOnRegionMap（普通のマップの場合）は、地名の番号で
@ 大きさの表（080C4E88 のリテラル 080C4F1C）と座標の表（080C4EEA のリテラル 080C4F20）を引く。
@ シンオウの番号はシンオウだけのもので、ホウエンの番号とぶつかるので、表の頭を C に決めさせる
@ （HoennTownMapSizeTable / HoennTownMapPosTable・今いるマップの地方で選ぶ）。
@ ★ 座標の表は +2（y）でもう一度引かれる（080C4EFA `ADD r1,#2`）ので、r1 に表の頭を残す
@ ================================================================
	.global	HoennTownMapSizeTableHook
	.thumb_func
HoennTownMapSizeTableHook:		@ 080C4E88（8 バイト・r0）
	push	{r1}			@ r1 = マップの幅（080C4E96 で使う）
	bl	HoennTownMapSizeTable
	pop	{r1}
	mov	r10, r0
	ldrh	r0, [r7, #20]
	lsl	r0, r0, #2
	ldr	r2, =0x080C4E90 + 1
	bx	r2
	.align	2
	.pool

	.global	HoennTownMapPosTableHook
	.thumb_func
HoennTownMapPosTableHook:		@ 080C4EEA（10 バイト・r1）
	bl	HoennTownMapPosTable
	mov	r1, r0
	mov	r2, r9
	add	r0, r2, r1
	ldrh	r0, [r0, #0]
	add	r0, r4, r0
	ldr	r2, =0x080C4EF4 + 1
	bx	r2
	.align	2
	.pool
