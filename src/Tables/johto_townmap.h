/* タウンマップのジョウト。tools/townmap/build_johto_townmap.py が生成。手で編集しないこと。 */
#pragma once

#define JOHTO_TM_TILE_INDEX   320   /* BG0 のタイル番号（素の 320 枚の後ろ） */
#define JOHTO_TM_TILE_INDEX2  808   /* 2 つ目の置き場（ホウエンの後ろ） */
#define JOHTO_TM_TILE_COUNT   187
#define JOHTO_TM_SRC_W        60   /* つないだ絵の列 */
#define JOHTO_TM_SRC_OFS      30   /* 絵の列 = 配置表の x + 4 + これ */
#define JOHTO_TM_LAYOUT_W     30   /* ジョウトの配置表の列（x = -W〜-1） */
#define JOHTO_TM_LAYOUT_H     15
#define JOHTO_TM_MIN_X        (-30)
#define JOHTO_TM_EXTRA_COUNT  2   /* カントー側に足すマス（28 番道路） */
#define JOHTO_TM_ICON_COUNT   17   /* ジョウトのダンジョンの印 */

extern const u8 gJohtoTownMapTiles[];
extern const u8 gJohtoTownMapTiles2[];
extern const u16 gJohtoKantoTilemap[];
extern const u8 gJohtoTownMapLayout[];
extern const u8 gJohtoTownMapExtraCells[];
extern const s8 gJohtoDungeonIcons[];
