#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "libraries/imgui/imgui.h"

#include "emulator-instance.h"

void Disassembler(bool &open, const EmulatorInstance &emulator, ImFont *monospace_font);

#endif /* DISASSEMBLER_H */
