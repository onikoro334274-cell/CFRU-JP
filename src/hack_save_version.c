#include "defines.h"
#include "../include/event_data.h"
#include "../include/load_save.h"
#include "Tables/hoenn_script_flags.h"

struct HackVersionReset
{
	u16 version;
	u16 kind;     /* 0 フラグを下ろす・1 フラグを立てる・2 変数に値 */
	u16 id;
	u16 value;
};
#define HACK_SAVE_VERSION_TABLE
#include "hack_save_version.h"

/*
 * ★ 2026-10-10 ユーザー「セーブデータに版いれられる？」→「はい」: セーブに版を入れる。
 *
 * - 版の数 HACK_SAVE_VERSION は配る版（make_demo_bundle.py の VERSION）から作る生成物（hack_save_version.h・
 *   tools/build/build_save_version.py）。0.<更新>.<バグ修正> → 更新 * 256 + バグ修正（0.35.2 は 0x2302）
 * - HACK_VAR_SAVE_VERSION（0x50EF）: 最後にレポートを書いた版。src/save.c の SaveWriteToFlash の頭で書く（patches/0265）
 * - HACK_VAR_SAVE_VERSION_FIRST（0x50EE）: はじめからを始めた版。NewGameWipeNewSaveData（拡張のセーブを 0 で消す）の直後に書く。
 *   拡張変数（0x5000〜）は拡張のセーブの中なので、そのあとの ClearSav1 では消えない（なんいどと同じ）
 * - どちらも 0 は「書かれていない」＝前の版（0.35.2 より前）のセーブ。前のセーブの始めた版は分からないので 0 のまま
 */

//今の版（★ 2026-10-12 ネット交換のプロファイルの番号・src/hack_network_trade.c。版の見出しを読む C を増やさない＝tools/build/verify_build_speed.py）
u16 HackSaveVersion_Current(void)
{
	return HACK_SAVE_VERSION;
}

void HackSaveVersion_OnSave(void)
{
	VarSet(HACK_VAR_SAVE_VERSION, HACK_SAVE_VERSION);
}

void HackSaveVersion_NewGame(void)
{
	VarSet(HACK_VAR_SAVE_VERSION_FIRST, HACK_SAVE_VERSION);
	VarSet(HACK_VAR_SAVE_VERSION, HACK_SAVE_VERSION);
}

/*
 * ★ 2026-10-11 ユーザー「ポケセンにテレポート（パッチ更新のたびに）」「パッチによってフラグ リセット」→（確認）「最後に使ったポケセン」
 *   「版ごとの表をファイルで持つ」:
 *   つづきから を選んだとき（src/bw_main_menu.c）、レポートの版（0x50EF）が今のビルドより古ければ、
 *   (1) tools/build/version_resets.json の「セーブの版より新しい行」を全部当て（フラグを下ろす／立てる・変数に値・生成物 hack_save_version.h の表）、
 *   (2) 続きからを「最後に使ったポケセン（gSaveBlock1->lastHealLocation＝全滅の戻り先と同じ・ホウエンなどは src/region_fly.c が書く）」への
 *       ワープにする（素の continueGameWarp の仕組み・HoennTower_SaveGame と同じ）。地図に入り直すので一時フラグ・一時変数も 0 に戻る。
 *   版の変数はレポートを書くまで古いままなので、書かずにまた つづきから をすれば同じことがもう一度起きる（フラグの戻しは何度当てても同じ）。
 *   版が 0（0.35.2 より前）のレポートは つづきから に出ないのでここには来ない。
 */
#define SetContinueGameWarp ((void (*)(s8, s8, s8, s8, s8)) (0x08054FB8 | 1))   //素（src/hoenn_tower.c と同じ）

void HackSaveVersion_OnContinue(void)
{
	u16 saved = VarGet(HACK_VAR_SAVE_VERSION);
	const struct WarpData* heal = &gSaveBlock1->lastHealLocation;
	u32 i;

	if (saved == 0 || saved >= HACK_SAVE_VERSION)
		return;
	for (i = 0; i < HACK_VERSION_RESET_COUNT; ++i)
	{
		const struct HackVersionReset* r = &sHackVersionResets[i];

		if (r->version <= saved)
			continue;
		if (r->kind == 0)
			FlagClear(r->id);
		else if (r->kind == 1)
			FlagSet(r->id);
		else if (r->kind == 2)
			VarSet(r->id, r->value);
	}
	if (heal->mapGroup == 0 && heal->mapNum == 0 && heal->x == 0 && heal->y == 0)
		return;   //回復地点がまだ無い（起きないはず）: その場で続ける
	SetContinueGameWarp(heal->mapGroup, heal->mapNum, -1, heal->x, heal->y);
	SetContinueGameWarpStatus();
}
