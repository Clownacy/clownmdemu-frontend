#ifndef DEBUG_Z80_H
#define DEBUG_Z80_H

#include "libraries/imgui/imgui.h"

#include "emulator-instance.h"

class DebugZ80
{
private:
	const EmulatorInstance &emulator;
	ImFont* const monospace_font;

public:
	DebugZ80(
		const EmulatorInstance &emulator,
		ImFont* const monospace_font
	) :
		emulator(emulator),
		monospace_font(monospace_font)
	{}
	void Display(bool &open);
};

#endif /* DEBUG_Z80_H */
