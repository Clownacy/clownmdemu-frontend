#ifndef DEBUG_PSG_H
#define DEBUG_PSG_H

#include "libraries/imgui/imgui.h"

#include "emulator-instance.h"

void Debug_PSG(bool &open, const EmulatorInstance &emulator, ImFont *monospace_font);

#endif /* DEBUG_PSG_H */
