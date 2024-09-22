#ifndef DEBUG_PCM_H
#define DEBUG_PCM_H

#include "libraries/imgui/imgui.h"

#include "emulator-instance.h"

class WindowPopup;

class DebugPCM
{
private:
	const EmulatorInstance &emulator;
	ImFont* const &monospace_font;

public:
	DebugPCM(
		const EmulatorInstance &emulator,
		ImFont* const &monospace_font
	) :
		emulator(emulator),
		monospace_font(monospace_font)
	{}
	void Display(WindowPopup &window);
};

#endif /* DEBUG_PCM_H */
