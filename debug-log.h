#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <cstdarg>
#include <cstddef>
#include <vector>

#include "SDL.h"

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

class DebugLog
{
private:
	ImFont* const &monospace_font;

	std::vector<std::vector<char>> lines;
	bool logging_enabled, log_to_console, force_console_output = true;

public:
	DebugLog(ImFont* const &monospace_font) : monospace_font(monospace_font)
	{
		SDL_LogSetPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR);
	}
	void ForceConsoleOutput(bool forced)
	{
		force_console_output = forced;
	}
	void Log(const char *format, std::va_list args);
	CC_ATTRIBUTE_PRINTF(2, 3) void Log(const char *format, ...);
	void Display(bool &open);
};

#endif /* DEBUG_LOG_H */
