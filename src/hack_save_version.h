/* 生成物（tools/build/build_save_version.py・元は tools/patch/make_demo_bundle.py の VERSION と version_resets.json）。手で直さない */
#pragma once
#define HACK_SAVE_VERSION 0x2A00  /* PokemonStart v0.42.0 */
#define HACK_SAVE_MIN_VERSION 0x2402  /* これより古いレポートは つづきから を出さない（v0.36.2）*/
#define HACK_VERSION_RESET_COUNT 7
#ifdef HACK_SAVE_VERSION_TABLE
/* 版（u16）・種類（0 フラグを下ろす・1 立てる・2 変数に値）・番号・値。版の古い順。最後は番兵 */
static const struct HackVersionReset sHackVersionResets[HACK_VERSION_RESET_COUNT + 1] = {
	{0x2601, 2, 0x5140, 0x0000},  /* v0.38.1 */
	{0x2601, 2, 0x5141, 0x0000},  /* v0.38.1 */
	{0x2601, 2, 0x5142, 0x0000},  /* v0.38.1 */
	{0x2601, 2, 0x5143, 0x0000},  /* v0.38.1 */
	{0x2601, 2, 0x5144, 0x0000},  /* v0.38.1 */
	{0x2601, 2, 0x5145, 0x0000},  /* v0.38.1 */
	{0x2601, 2, 0x5146, 0x0000},  /* v0.38.1 */
	{0, 0, 0, 0},
};
#endif
