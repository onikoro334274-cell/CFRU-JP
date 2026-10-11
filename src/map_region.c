#include "defines.h"
#include "map_region.h"
#include "Tables/sinnoh_townmap.h"
#include "Tables/kitakami_townmap.h"

/*
map_region.c
    地方と地名の番号の読み替え（2026-09-29・ユーザー「先に地名問題片付けよう」）。
    ★ 考え方は src/map_region.h の頭・詳細は docs/research/2026-09-29_mapsec-regions.md
*/

extern const u8 *const gExpandedMapSecNames[];   /* assembly/hoenn/mapsec.s（0x00〜0xFC） */

static bool8 InGroupRange(u8 group, u8 first, u8 count)
{
    return count != 0 && group >= first && group < first + count;
}

u8 GetMapGroupRegion(u8 group)
{
    if (InGroupRange(group, HOENN_MAP_GROUP_FIRST, HOENN_MAP_GROUP_COUNT))
        return MAP_REGION_HOENN;
    if (InGroupRange(group, JOHTO_MAP_GROUP_FIRST, JOHTO_MAP_GROUP_COUNT))
        return MAP_REGION_JOHTO;
    if (InGroupRange(group, SINNOH_MAP_GROUP_FIRST, SINNOH_MAP_GROUP_COUNT) || group == SINNOH_MAP_GROUP_SECOND)
        return MAP_REGION_SINNOH;
    return MAP_REGION_KANTO;
}

/* ★ 2026-10-02 キタカミ（ユーザー「空きの地名の拡張はできないか？」→ キタカミだけの番号）: ジョウトの群の 148〜164 */
u8 GetMapRegionOf(u8 group, u8 num)
{
    u8 r = GetMapGroupRegion(group);

    if (r == MAP_REGION_JOHTO && num >= KITAKAMI_MAP_NUM_FIRST && num <= KITAKAMI_MAP_NUM_LAST)
        return MAP_REGION_KITAKAMI;
    return r;
}

u8 GetCurrentMapRegion(void)
{
    return GetMapRegionOf((u8) gSaveBlock1->location.mapGroup, (u8) gSaveBlock1->location.mapNum);
}

/* 番号 → 地名の文字。シンオウはシンオウだけの番号として引く（範囲の外はいつもの表） */
const u8 *MapRegion_GetName(u8 mapsec, u8 region)
{
    if (region == MAP_REGION_SINNOH && mapsec < SINNOH_TM_NAME_COUNT)
        return gSinnohMapSecNames[mapsec];
    if (region == MAP_REGION_KITAKAMI && mapsec < KITAKAMI_TM_NAME_COUNT)
        return gKitakamiMapSecNames[mapsec];
    return gExpandedMapSecNames[mapsec];
}

/* ここで作ったポケモンの親の版（CreateBoxMon・タマゴがかえったとき）。シンオウだけ 12 */
u8 MapRegion_MetGameHere(void)
{
    switch (GetCurrentMapRegion())
    {
    case MAP_REGION_SINNOH:
        return MET_GAME_SINNOH;
    case MAP_REGION_KITAKAMI:
        return MET_GAME_KITAKAMI;
    default:
        return gGameVersion;
    }
}

u8 MapRegion_FromMetGame(u8 metGame)
{
    if (metGame == MET_GAME_KITAKAMI)
        return MAP_REGION_KITAKAMI;
    return (metGame == MET_GAME_SINNOH) ? MAP_REGION_SINNOH : MAP_REGION_KANTO;
}

/* このゲームで出会ったか（FR・LG・シンオウの印）。ステータス画面の文の選び分けに使う */
bool8 MapRegion_IsOurMetGame(u8 metGame)
{
    return metGame == VERSION_FIRE_RED || metGame == VERSION_LEAF_GREEN || metGame == MET_GAME_SINNOH || metGame == MET_GAME_KITAKAMI;
}
