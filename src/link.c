#include "defines.h"
#include "defines_battle.h"

#include "../include/new/util.h"
#include "../include/new/link.h"
/*
link.c
	handles data transfer
*/

#define DATA_TRANSFER_HEADER_SIZE 7

void EmitDataTransfer(u8 bufferId, void* dst, u16 size, void* data)
{
	s32 i;
	const u8 *src = data;

	if (size > sizeof(gBattleBuffersTransferData) - DATA_TRANSFER_HEADER_SIZE
	 || (size != 0 && (dst == NULL || data == NULL)))
		return;

	gBattleBuffersTransferData[0] = CONTROLLER_DATATRANSFER;
	gBattleBuffersTransferData[1] = (u32)(dst);
	gBattleBuffersTransferData[2] = ((u32)(dst) & 0x0000FF00) >> 8;
	gBattleBuffersTransferData[3] = ((u32)(dst) & 0x00FF0000) >> 16;
	gBattleBuffersTransferData[4] = ((u32)(dst) & 0xFF000000) >> 24;
	gBattleBuffersTransferData[5] = size;
	gBattleBuffersTransferData[6] = (size & 0xFF00) >> 8;
	for (i = 0; i < size; i++)
		gBattleBuffersTransferData[DATA_TRANSFER_HEADER_SIZE + i] = src[i];
	PrepareBufferDataTransfer(bufferId, gBattleBuffersTransferData, size + DATA_TRANSFER_HEADER_SIZE);
}

void HandleDataTransfer(void)
{
	if (gActiveBattler >= MAX_BATTLERS_COUNT)
		return;

	u32 dstArg = gBattleBufferA[gActiveBattler][1]
			  | (gBattleBufferA[gActiveBattler][2] << 8)
			  | (gBattleBufferA[gActiveBattler][3] << 16)
			  | ((u32) gBattleBufferA[gActiveBattler][4] << 24);

	u16 sizeArg = gBattleBufferA[gActiveBattler][5] | (gBattleBufferA[gActiveBattler][6] << 8);

	u8* src = &gBattleBufferA[gActiveBattler][DATA_TRANSFER_HEADER_SIZE];
	u8* dst = (u8*)(dstArg);

	/* A controller packet cannot contain more than this one buffer. Drop
	 * malformed payloads but still acknowledge them below to avoid a stall. */
	if (sizeArg != 0 && dstArg != 0
	 && sizeArg <= sizeof(gBattleBufferA[0]) - DATA_TRANSFER_HEADER_SIZE)
		Memcpy(dst, src, sizeArg);

	if (SIDE(gActiveBattler) == B_SIDE_OPPONENT)
	{
		if (gBattleTypeFlags & BATTLE_TYPE_LINK)
			LinkOpponentBufferExecCompleted();
		else
			OpponentBufferExecCompleted();
	}
	else
		PlayerBufferExecCompleted();
}
