#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include "SDL.h"

#include "../clownmdemu.h"

void Debug_PlaneA(bool *open, const ClownMDEmu_State *clownmdemu_state, Uint32 colours[16 * 4 * 3]);
void Debug_PlaneB(bool *open, const ClownMDEmu_State *clownmdemu_state, Uint32 colours[16 * 4 * 3]);
void Debug_VRAM(bool *open, const ClownMDEmu_State *clownmdemu_state, Uint32 colours[16 * 4 * 3]);
void Debug_CRAM(bool *open, Uint32 colours[16 * 4 * 3]);

#endif /* DEBUG_VDP_H */
