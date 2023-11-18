#ifndef DEBUG_FM_H
#define DEBUG_FM_H

#include "libraries/imgui/imgui.h"

#include "emulator-instance.h"

void Debug_FM(bool &open, const EmulatorInstance &emulator, ImFont *monospace_font);

#endif /* DEBUG_FM_H */
