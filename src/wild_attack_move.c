#include "defines.h"
#include "defines_battle.h"
#include "../include/pokemon.h"
#include "../include/constants/moves.h"
#include "../include/new/learn_move.h"
#include "../include/new/wild_attack_move.h"

void WildMon_EnsureAttackMove(struct Pokemon* mon)
{
    u8 slot = MAX_MON_MOVES - 1;
    for (u8 i = 0; i < MAX_MON_MOVES; ++i)
    {
        u16 move = GetMonData(mon, MON_DATA_MOVE1 + i, NULL);
        if (move != MOVE_NONE && move < MOVES_COUNT && gBattleMoves[move].split != SPLIT_STATUS)
        {
            if (GetMonData(mon, MON_DATA_PP1 + i, NULL) == 0)
                SetMonMoveSlot(mon, move, i);
            return;
        }
        if (move == MOVE_NONE)
            slot = i;
    }
    SetMonMoveSlot(mon, MOVE_TACKLE, slot);
}
