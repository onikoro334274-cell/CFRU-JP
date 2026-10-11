#include "defines.h"
#include "bw_follower_internal.h"
#include "../include/sprite.h"
#include "../include/decompress.h"
#include "../include/malloc.h"

extern const struct CompressedSpriteSheet gBwFollowerSheets[];
extern const struct CompressedSpriteSheet gMoemonFollowerSheets[];
extern const u16 gBwFollowerCount;
bool8 HackSplit_IsBack(const void *src);
const u8 *HackSplit_Fetch(const void *src, u32 len, void **alloc);

bool8 BwFollowerLoadSheet(u16 gfxId)
{
    struct CompressedSpriteSheet sheet;
    u16 index = BwFollowerIndexOf(gfxId);

    FreeSpriteTilesByTag(BW_FOLLOWER_TILE_TAG);
    if (index >= gBwFollowerCount)
    {
        /* Moémon の連れ歩き（2026-10-08）: LZ77（assembly/hack_moemon_follow/moemon_followers.s） */
        LoadCompressedSpriteSheetUsingHeap(&gMoemonFollowerSheets[index - gBwFollowerCount]);
        return GetSpriteTileStartByTag(BW_FOLLOWER_TILE_TAG) != 0xFFFF;
    }
    sheet = gBwFollowerSheets[index];
    /* ★ 2026-09-28 LZ77 → 自前の F 方式（src/bw_follower_fcomp.c）。展開の入れ物が取れなければ FALSE（次のコマでやり直す） */
    /* ★ 2026-10-11 --split-pics の版はシートのかたまりが ROM の後ろ半分（0x0A000000〜・src/hack_split_pics.c）。SuperCard では
     *   SD から読んだ写しを渡す。1 シートの長さは次のシートとの差（表は位置の順・最後は 8KB まで） */
    if (HackSplit_IsBack(sheet.data))
    {
        void *alloc;
        u32 len = (index + 1 < gBwFollowerCount && gBwFollowerSheets[index + 1].data > sheet.data)
                ? (u32) (gBwFollowerSheets[index + 1].data - sheet.data) : 0x2000;
        const u8 *data = HackSplit_Fetch(sheet.data, len, &alloc);
        bool8 ok;

        if (data == NULL)
            return FALSE;
        ok = BwFollowerLoadFSheet(data, sheet.size, BW_FOLLOWER_TILE_TAG);
        if (alloc != NULL)
            Free(alloc);
        if (!ok)
            return FALSE;
        return GetSpriteTileStartByTag(BW_FOLLOWER_TILE_TAG) != 0xFFFF;
    }
    if (!BwFollowerLoadFSheet(sheet.data, sheet.size, BW_FOLLOWER_TILE_TAG))
        return FALSE;
    return GetSpriteTileStartByTag(BW_FOLLOWER_TILE_TAG) != 0xFFFF;
}
