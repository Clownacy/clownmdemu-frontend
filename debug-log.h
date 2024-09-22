#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <cstdarg>
#include <cstddef>
#include <string>

#include "SDL.h"

#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

class WindowPopup;

class DebugLog
{
private:
	std::string lines;
	bool logging_enabled = false, log_to_console = false, force_console_output = true;

public:
	DebugLog()
	{
		SDL_LogSetPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR);
	}
	void ForceConsoleOutput(bool forced)
	{
		force_console_output = forced;
	}
	void Log(const char *format, std::va_list args);
	CC_ATTRIBUTE_PRINTF(2, 3) void Log(const char *format, ...);
	void Display(WindowPopup &window);
};

#endif /* DEBUG_LOG_H */
