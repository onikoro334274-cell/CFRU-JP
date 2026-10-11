#pragma once

#include "../include/sprite.h"

/* Internal contracts shared by the hand-written follower modules.
 * Keep tags in sync with tools/bwfollow/build_bw_follower.py. */
#define BW_FOLLOWER_TILE_TAG 0x4F10
#define BW_FOLLOWER_FIRST_TABLE 1
#define BW_FOLLOWER_PAL_SLOT 10
#define BW_PAL_TAG_BASE 0x2000

struct Pokemon;

struct Pokemon *BwFollowerLeadMon(void);
u16 BwFollowerGfxIdForSpecies(u16 species);
u16 BwFollowerGfxIdForMon(struct Pokemon *mon);
u16 BwFollowerIndexOf(u16 gfxId);
bool8 BwFollowerValidIndex(u16 index);
bool8 BwFollowerLeadShinyFor(u16 index);
u16 BwFollowerPalPos(u16 index, bool8 shiny);
bool8 BwFollowerLoadSheet(u16 gfxId);
bool8 BwFollowerLoadFSheet(const u8 *data, u16 rawSize, u16 tag);

extern const struct SpritePalette gBwFollowerOwPalTable[];
