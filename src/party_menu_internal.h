#pragma once

#include "../include/global.h"
#include "../include/main.h"
#include "../include/task.h"

struct PartyMenuInternal
{
	TaskFunc task;
	MainCallback exitCallback;
	u32 chooseHalf:1;
	u32 lastSelectedSlot:3;
	u32 spriteIdConfirmPokeball:7;
	u32 spriteIdCancelPokeball:7;
	u32 messageId:14;
	u8 windowId[3];
	u8 actions[8];
	u8 numActions;
	u16 palBuffer[BG_PLTT_SIZE / sizeof(u16)];
	s16 data[16];
};

extern struct PartyMenuInternal* sPartyMenuInternal;

#if defined(__arm__)
_Static_assert(__builtin_offsetof(struct PartyMenuInternal, data) == 0x218,
	"BPRJ native level-up stats buffer layout");
#endif
