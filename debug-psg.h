#ifndef DEBUG_PSG_H
#define DEBUG_PSG_H

#include "libraries/imgui/imgui.h"

#include "emulator-instance.h"

class DebugPSG
{
private:
	const EmulatorInstance &emulator;
	ImFont* const monospace_font;

public:
	DebugPSG(
		const EmulatorInstance &emulator,
		ImFont* const monospace_font
	) :
		emulator(emulator),
		monospace_font(monospace_font)
	{}
	void Display(bool &open);
};

#endif /* DEBUG_PSG_H */
