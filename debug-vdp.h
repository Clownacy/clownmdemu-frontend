#ifndef DEBUG_VDP_H
#define DEBUG_VDP_H

#include "SDL.h"

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

#include "emulator-instance.h"

void Debug_WindowPlane(bool &open, const EmulatorInstance &emulator);
void Debug_PlaneA(bool &open, const EmulatorInstance &emulator);
void Debug_PlaneB(bool &open, const EmulatorInstance &emulator);
void Debug_VRAM(bool &open, const EmulatorInstance &emulator);
void Debug_CRAM(bool &open, const EmulatorInstance &emulator);
void Debug_VDP(bool &open, const EmulatorInstance &emulator);

#endif /* DEBUG_VDP_H */
