#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include <cstddef>

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

class DebugMemory
{
private:
	ImFont* const &monospace_font;

public:
	DebugMemory(
		ImFont* const &monospace_font
	) :
		monospace_font(monospace_font)
	{}
	void Display(bool &open, const char *window_name, const cc_u8l *buffer, std::size_t buffer_length);
	void Display(bool &open, const char *window_name, const cc_u16l *buffer, std::size_t buffer_length);
};

#endif /* DEBUG_MEMORY_H */
