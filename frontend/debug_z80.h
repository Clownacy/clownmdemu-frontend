#ifndef DEBUG_Z80_H
#define DEBUG_Z80_H

#include "libraries/imgui/imgui.h"
#include "../clownmdemu.h"

void Debug_Z80(bool *open, Z80_State *z80, ImFont *monospace_font);

#endif /* DEBUG_Z80_H */
