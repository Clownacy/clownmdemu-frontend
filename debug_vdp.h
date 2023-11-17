#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include "SDL.h"

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

void Debug_WindowPlane(bool &open);
void Debug_PlaneA(bool &open);
void Debug_PlaneB(bool &open);
void Debug_VRAM(bool &open);
void Debug_CRAM(bool &open);
void Debug_VDP(bool &open);

#endif /* DEBUG_VDP_H */
