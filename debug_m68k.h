#ifndef DEBUG_M68K_H
#define DEBUG_M68K_H

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

void Debug_M68k(bool *open, Clown68000_State *m68k, ImFont *monospace_font);

#endif /* DEBUG_M68K_H */
