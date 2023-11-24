#ifndef DEBUG_M68K_H
#define DEBUG_M68K_H

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

class DebugM68k
{
private:
	ImFont* const &monospace_font;

public:
	DebugM68k(
		ImFont* const &monospace_font
	) :
		monospace_font(monospace_font)
	{}
	void Display(bool &open, const char* const name, const Clown68000_State &m68k);
};

#endif /* DEBUG_M68K_H */
