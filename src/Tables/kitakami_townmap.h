/* タウンマップのキタカミ。tools/townmap/build_kitakami_townmap.py が生成。手で編集しないこと。 */
#pragma once

#define KITAKAMI_TM_W            22
#define KITAKAMI_TM_H            15
#define KITAKAMI_TM_ART_W        30
#define KITAKAMI_TM_ART_H        20
#define KITAKAMI_TM_NAME_COUNT   4   /* キタカミだけの番号 0〜 */
#define KITAKAMI_TM_TILE_COUNT   2
/* キタカミの地図＝ジョウトの群（JOHTO_MAP_GROUP_FIRST）の番号 148〜164（tools/sinnoh/sinnoh_maps.JOHTO_MAPS） */
#define KITAKAMI_MAP_NUM_FIRST   148
#define KITAKAMI_MAP_NUM_LAST    164

extern const u8 gKitakamiTownMapTiles[];
extern const u8 gKitakamiTownMapTiles2[];
extern const u16 gKitakamiTownMapTilemap[];
extern const u8 gKitakamiTownMapLayout[];
extern const u8 *const gKitakamiMapSecNames[];
extern const u16 gKitakamiMapSecPos[];
extern const u16 gKitakamiMapSecSizes[];
