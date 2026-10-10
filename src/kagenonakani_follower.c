/* MAKIO419 氏の連れ歩き。下・上・左・右各3コマ。40件の並びは既存と共通。 */
#include "defines.h"
#include "../include/sprite.h"

#define FACE(name, tile) \
static const union AnimCmd name[] = { ANIMCMD_FRAME(tile, 16), ANIMCMD_FRAME(tile, 16), \
    ANIMCMD_FRAME(tile, 16), ANIMCMD_FRAME(tile, 16), ANIMCMD_JUMP(0) }
#define WALK(name, a, b, c, dur) \
static const union AnimCmd name[] = { ANIMCMD_FRAME(a, dur), ANIMCMD_FRAME(b, dur), \
    ANIMCMD_FRAME(c, dur), ANIMCMD_FRAME(b, dur), ANIMCMD_JUMP(0) }
#define DIRECTION(dir, first) \
FACE(sFace##dir, (first + 1) * 16); \
WALK(sGo##dir, first * 16, (first + 1) * 16, (first + 2) * 16, 8); \
WALK(sFast##dir, first * 16, (first + 1) * 16, (first + 2) * 16, 6); \
WALK(sFaster##dir, first * 16, (first + 1) * 16, (first + 2) * 16, 4); \
WALK(sFastest##dir, first * 16, (first + 1) * 16, (first + 2) * 16, 3)
DIRECTION(South, 0);
DIRECTION(North, 3);
DIRECTION(West, 6);
DIRECTION(East, 9);
#define FOUR(p) p##South, p##North, p##West, p##East
const union AnimCmd *const gBwFollowerAnims_Kagenonakani[40] = {
    FOUR(sFace), FOUR(sGo), FOUR(sFast), FOUR(sFaster), FOUR(sFastest),
    FOUR(sFastest), FOUR(sFace), FOUR(sGo), FOUR(sFastest), FOUR(sFastest),
};
