#pragma once

/*
    ジョウト・シンオウのそらをとぶの町と、ポケセンの回復地点（#2・2026-09-30）。
    表は assembly/hoenn/region_fly.s（tools/rom/build_region_fly.py が生成）・処理は src/region_fly.c
    ★ 詳細は docs/research/2026-09-30_region-fly-heal.md
*/

struct RegionFlyCity
{
    u16 flag;       /* 訪問フラグ（立っていれば飛べる） */
    u8 region;      /* MAP_REGION_JOHTO / MAP_REGION_SINNOH / MAP_REGION_KITAKAMI */
    u8 mapsec;      /* 地名の番号（シンオウはシンオウだけの番号） */
    u8 group;
    u8 num;
    s16 x, y;       /* 降りる所 */
    s8 iconX, iconY; /* タウンマップのページの上の印のマス */
};

struct RegionPokeCenter
{
    u8 pcGroup, pcNum;      /* ポケセンの地図 */
    u8 healGroup, healNum;  /* 回復地点（外・ドアの 1 つ下） */
    s16 healX, healY;
    u8 respX, respY;        /* 全滅したときに立つ所（看護師の 2 つ下） */
    u8 nurse;               /* 看護師の localId */
    u8 pad;
};

extern const struct RegionFlyCity gRegionFlyCities[];
extern const u8 gRegionFlyCityCount[];
extern const struct RegionPokeCenter gRegionPokeCenters[];
extern const u8 gRegionPokeCenterCount[];

const struct RegionFlyCity *RegionFlyFindCity(u8 region, u8 mapsec);
bool8 RegionFlyCityVisited(const struct RegionFlyCity *c);   /* 訪問済みか（地方のデバッグ設定なら常に TRUE） */
u8 RegionFlyCityType(u8 region, u8 mapsec);     /* 町なら 2（訪問済み）/ 3（まだ）・違えば 0 */
bool8 RegionFlySetWarp(u8 region, u8 mapsec);   /* 町なら行き先を決めて TRUE */
void RegionFlyVisitTick(void);
void RegionHealTick(void);
