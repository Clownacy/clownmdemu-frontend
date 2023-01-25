#ifndef DEBUG_M68K_H
#define DEBUG_M68K_H

#include "libraries/imgui/imgui.h"
#include "../clownmdemu.h"

void Debug_M68k(bool *open, M68k_State *m68k, ImFont *monospace_font);

#endif /* DEBUG_M68K_H */
