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
	std::vector<std::vector<char>> lines;
	bool logging_enabled, log_to_console, force_console_output = true;

public:
	DebugLog()
	{
		SDL_LogSetPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR);
	}
	void ForceConsoleOutput(bool forced)
	{
		force_console_output = forced;
	}
	void Log(const char *message, std::va_list args);
	void Display(bool &open, ImFont *monospace_font);
};

#endif /* DEBUG_LOG_H */
