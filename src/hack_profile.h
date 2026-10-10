#pragma once
#include "../include/global.h"

#ifdef HACK_PROFILE
/* Stable VBlank-counter/VCOUNT sample, in scanline units. */
u32 HackProfile_Now(void);
#endif
