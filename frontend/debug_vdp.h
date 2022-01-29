#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include "../clownmdemu.h"

void Debug_PlaneA(bool *open, const ClownMDEmu_State *clownmdemu_state);
void Debug_PlaneB(bool *open, const ClownMDEmu_State *clownmdemu_state);
void Debug_VRAM(bool *open, const ClownMDEmu_State *clownmdemu_state);
void Debug_CRAM(bool *open);

#endif /* DEBUG_VDP_H */
