#include "defines.h"
#include "../include/overworld.h"
#include "map_region.h"
#include "region_fly.h"
#include "Tables/hoenn_script_flags.h"

/*
region_fly.c
    ジョウト・シンオウのそらをとぶの町と、ポケセンの回復地点（#2・2026-09-30・ユーザー「先にリストの最後のそらをとぶの行き先・回復地点」）。
    ★ 詳細は docs/research/2026-09-30_region-fly-heal.md

    ・飛べるか: タウンマップの「区分の種類」（GetMapsecType 080C47C0 のフック → SinnohMapsecType）が 2（訪問済み）/ 3（まだ）を返す
    ・行き先: HoennSetFlyWarpDestination（080C6460 の置き換え）が RegionFlySetWarp を先に試す。
      素の回復地点の表（gExpandedHealLocations）には足さない（上限 080C0E86 も触らない）
    ・訪問フラグ: その町の地図にいる間、毎フレーム立てる（ホウエンの HoennFlyVisitTick と同じ）
    ・回復地点: ポケセン（ホウエン・ジョウト・シンオウ）にいる間、毎フレーム「最後の回復地点」をその町のドアの前にする
      （素の setrespawn の代わり。ホウエンのポケセンの地図は ON_RESUME だけで setrespawn が無かった）
    ・全滅: 素の SetWhiteoutRespawnWarpAndHealerNpc はカントーの 20 件しか引けない（080C0E4C）。
      見つからなかったとき（0）だけ RegionWhiteoutRespawn（assembly/region_fly_hook.s の RegionWhiteoutHook・080C0F08）
*/

#define gSpecialVarLastTalked (*(u16 *) 0x02037006)

const struct RegionFlyCity *RegionFlyFindCity(u8 region, u8 mapsec)
{
    u32 i;

    for (i = 0; i < gRegionFlyCityCount[0]; i++)
        if (gRegionFlyCities[i].region == region && gRegionFlyCities[i].mapsec == mapsec)
            return &gRegionFlyCities[i];
    return NULL;
}

/* ★ 2026-10-01 ユーザー「デバッグフラグ地方ごとの経っているときは全部のフライングに飛べるように」:
   config.h の JOHTO_TOWNMAP_DEBUG / SINNOH_TOWNMAP_DEBUG が有効なら、その地方の町は全部「訪問済み」あつかい
   （訪問フラグそのものは立てない＝デバッグを外せば元どおり） */
bool8 RegionFlyCityVisited(const struct RegionFlyCity *c)
{
#ifdef JOHTO_TOWNMAP_DEBUG
    if (c->region == MAP_REGION_JOHTO)
        return TRUE;
#endif
#ifdef SINNOH_TOWNMAP_DEBUG
    if (c->region == MAP_REGION_SINNOH)
        return TRUE;
#endif
#ifdef KITAKAMI_TOWNMAP_DEBUG
    if (c->region == MAP_REGION_KITAKAMI)
        return TRUE;        /* ★ 2026-10-02 ユーザー「キタカミの里もデバッグフラグ立てて行けるように そらをとぶで」 */
#endif
    return FlagGet(c->flag);
}

u8 RegionFlyCityType(u8 region, u8 mapsec)
{
    const struct RegionFlyCity *c = RegionFlyFindCity(region, mapsec);

    if (c == NULL)
        return 0;
    return RegionFlyCityVisited(c) ? 2 : 3;
}

bool8 RegionFlySetWarp(u8 region, u8 mapsec)
{
    const struct RegionFlyCity *c = RegionFlyFindCity(region, mapsec);

    if (c == NULL)
        return FALSE;
    SetWarpDestination((s8) c->group, (s8) c->num, -1, (s8) c->x, (s8) c->y);
    return TRUE;
}

void RegionFlyVisitTick(void)
{
    u8 region = GetCurrentMapRegion();
    const struct RegionFlyCity *c;

    if (region != MAP_REGION_JOHTO && region != MAP_REGION_SINNOH && region != MAP_REGION_KITAKAMI)
        return;
    /* ★ 2026-10-12 タウンマップのそのページを開く旗（前は立てる所が無く、config.h の *_TOWNMAP_DEBUG で見せていた）:
       その地方の地図に入ったら立てる（船・そらをとぶ・ワープのどれで入っても同じ）。シンオウ 0x13E1・ジョウト 0x13E2 */
    if (region == MAP_REGION_SINNOH && !FlagGet(SINNOH_FLAG_TOWNMAP))
        FlagSet(SINNOH_FLAG_TOWNMAP);
    else if (region == MAP_REGION_JOHTO && !FlagGet(JOHTO_FLAG_TOWNMAP))
        FlagSet(JOHTO_FLAG_TOWNMAP);
    c = RegionFlyFindCity(region, gMapHeader.regionMapSectionId);
    if (c != NULL && c->group == (u8) gSaveBlock1->location.mapGroup)
        FlagSet(c->flag);
}

static const struct RegionPokeCenter *FindPokeCenterHere(void)
{
    u8 g = (u8) gSaveBlock1->location.mapGroup;
    u8 n = (u8) gSaveBlock1->location.mapNum;
    u32 i;

    if (g < HOENN_MAP_GROUP_FIRST)
        return NULL;                /* カントー・ななしま（素の setrespawn に任せる） */
    for (i = 0; i < gRegionPokeCenterCount[0]; i++)
        if (gRegionPokeCenters[i].pcGroup == g && gRegionPokeCenters[i].pcNum == n)
            return &gRegionPokeCenters[i];
    return NULL;
}

void RegionHealTick(void)
{
    const struct RegionPokeCenter *p = FindPokeCenterHere();
    struct WarpData *w;

    if (p == NULL)
        return;
    w = &gSaveBlock1->lastHealLocation;
    w->mapGroup = (s8) p->healGroup;
    w->mapNum = (s8) p->healNum;
    w->warpId = -1;
    w->x = p->healX;
    w->y = p->healY;
}

/*
    全滅したときの戻り先（素の表で見つからなかったときだけ・RegionWhiteoutHook から）。
    「最後の回復地点」と群・番号・x・y が同じ行 → 無ければ群・番号だけ同じ行のポケセンの看護師の前へ。
    戻り 0 = 決めた / 1 = 見つからない（素の 1 番＝マサラタウンの家で続ける。いままでは表の外を読んでいた）
*/
u8 RegionWhiteoutRespawn(struct WarpData *warp)
{
    const struct WarpData *h = &gSaveBlock1->lastHealLocation;
    const struct RegionPokeCenter *p = NULL;
    u32 i;

    for (i = 0; i < gRegionPokeCenterCount[0]; i++)
    {
        const struct RegionPokeCenter *e = &gRegionPokeCenters[i];

        if (e->healGroup != (u8) h->mapGroup || e->healNum != (u8) h->mapNum)
            continue;
        if (e->healX == h->x && e->healY == h->y)
        {
            p = e;
            break;
        }
        if (p == NULL)
            p = e;
    }
    if (p == NULL)
        return 1;
    warp->mapGroup = (s8) p->pcGroup;
    warp->mapNum = (s8) p->pcNum;
    warp->warpId = -1;
    warp->x = p->respX;
    warp->y = p->respY;
    gSpecialVarLastTalked = p->nurse;
    return 0;
}

/* ★ 2026-10-01 デモ版（カントー＋ナナシマ＋ホウエン・build_rom.py --demo が -DHACK_DEMO_BUILD を渡す）か。
   クチバの船（tools/sinnoh/build_sinnoh.py の SinnohFerry_AskWhere）が callasm で呼び、LASTRESULT が 1 なら
   キッサキ・アサギの選択肢を出さずに「ホウエンへ いきますか」だけにする */
void Region_IsDemoBuild(void)
{
#ifdef HACK_DEMO_BUILD
	gSpecialVar_LastResult = TRUE;
#else
	gSpecialVar_LastResult = FALSE;
#endif
}
