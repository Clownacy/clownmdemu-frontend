#ifndef DEBUG_M68K_H
#define DEBUG_M68K_H

#include "libraries/imgui/imgui.h"
#include "../clownmdemu.h"

void Debug_M68k_RAM(bool *open, ClownMDEmu_State *clownmdemu_state, ImFont *monospace_font);

#endif /* DEBUG_M68K_H */
