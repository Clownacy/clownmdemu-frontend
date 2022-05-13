#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include "SDL.h"

#include "libraries/imgui/imgui.h"
#include "../clownmdemu.h"

void Debug_PlaneA(bool *open, const ClownMDEmu *clownmdemu, const Uint32 colours[16 * 4 * 3]);
void Debug_PlaneB(bool *open, const ClownMDEmu *clownmdemu, const Uint32 colours[16 * 4 * 3]);
void Debug_VRAM(bool *open, const ClownMDEmu *clownmdemu, const Uint32 colours[16 * 4 * 3]);
void Debug_CRAM(bool *open, const ClownMDEmu *clownmdemu, const Uint32 colours[16 * 4 * 3], ImFont *monospace_font);

#endif /* DEBUG_VDP_H */
