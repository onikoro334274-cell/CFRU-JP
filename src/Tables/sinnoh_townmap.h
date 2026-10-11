/* タウンマップのシンオウ。tools/townmap/build_sinnoh_townmap.py が生成。手で編集しないこと。 */
#pragma once

#define SINNOH_TM_W            28   /* マス（区画 x 1〜28） */
#define SINNOH_TM_H            23   /* マス（区画 z 6〜28）★ 窓は 15 行なので縦にスクロールする */
#define SINNOH_TM_ART_W        34
#define SINNOH_TM_ART_H        28
#define SINNOH_TM_NAME_COUNT   78   /* シンオウだけの番号 0〜 */
#define SINNOH_TM_ICON_COUNT   23
#define SINNOH_TM_TILE_COUNT   196

extern const u8 gSinnohTownMapTiles[];
extern const u8 gSinnohTownMapTiles2[];
extern const u16 gSinnohTownMapTilemap[];
extern const u8 gSinnohTownMapLayout[];
extern const u8 gSinnohDungeonIcons[];
extern const u8 *const gSinnohMapSecNames[];
extern const u16 gSinnohMapSecPos[];
extern const u16 gSinnohMapSecSizes[];
