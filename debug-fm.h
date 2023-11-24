#ifndef DEBUG_FM_H
#define DEBUG_FM_H

#include "libraries/imgui/imgui.h"

#include "emulator-instance.h"

class DebugFM
{
private:
	const EmulatorInstance &emulator;
	ImFont* const &monospace_font;

public:
	DebugFM(
		const EmulatorInstance &emulator,
		ImFont* const &monospace_font
	) :
		emulator(emulator),
		monospace_font(monospace_font)
	{}
	void Display(bool &open);
};

#endif /* DEBUG_FM_H */
