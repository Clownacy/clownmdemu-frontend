#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "libraries/imgui/imgui.h"

#include "emulator-instance.h"

class WindowPopup;

void Disassembler(WindowPopup &window, const EmulatorInstance &emulator, ImFont *monospace_font);

#endif /* DISASSEMBLER_H */
