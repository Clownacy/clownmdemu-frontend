#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <cstdarg>
#include <cstddef>
#include <functional>
#include <list>
#include <string>
#include <string_view>

#include <fmt/core.h>
#include <SDL3/SDL.h>

#include "../libraries/imgui/imgui.h"
#include "../common/core/libraries/clowncommon/clowncommon.h"

class DebugLog
{
public:
	std::list<std::string> lines;
	bool logging_enabled = false, log_to_console = false, force_console_output = true;

	DebugLog()
	{
		SDL_SetLogPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR);
	}

	void ForceConsoleOutput(const bool forced)
	{
		force_console_output = forced;
	}

	void Log(std::string string);
	void Log(const char *format, std::va_list args);

	template <typename... Ts>
	void Log(const fmt::format_string<Ts...> &format, Ts &&...args)
	{
		Log(fmt::format(format, std::forward<Ts>(args)...));
	}

	static auto GetSDLErrorMessage(const std::string_view &function_name)
	{
		return fmt::format("{} failed with the following message - '{}'", function_name, SDL_GetError());
	}

	void SDLError(const std::string_view &function_name)
	{
		Log("{}", GetSDLErrorMessage(function_name));
	}
};

namespace Frontend
{
	extern DebugLog debug_log;
}

#endif /* DEBUG_LOG_H */
