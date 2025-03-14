#ifndef DEBUG_LOG_H
#define DEBUG_LOG_H

#include <cstdarg>
#include <cstddef>
#include <functional>
#include <string>

#include <fmt/core.h>

#include <SDL3/SDL.h>

#include "libraries/imgui/imgui.h"
#include "common/core/clowncommon/clowncommon.h"

class DebugLog
{
private:
	void Log(const std::function<std::size_t()> &GetSize, const std::function<void(char*, std::size_t)> &WriteBuffer);

public:
	std::string lines;
	bool logging_enabled = false, log_to_console = false, force_console_output = true;

	DebugLog()
	{
		SDL_SetLogPriority(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR);
	}

	void ForceConsoleOutput(const bool forced)
	{
		force_console_output = forced;
	}

	void Log(const char *format, std::va_list args);

	template <typename... T>
	void Log(fmt::format_string<T...> format, T&&... args)
	{
		const auto GetSize = [&]()
		{
			return fmt::formatted_size(format, std::forward<T>(args)...);
		};

		const auto WriteBuffer = [&](char* const message_buffer, const std::size_t message_buffer_size)
		{
			fmt::format_to_n(message_buffer, message_buffer_size, format, std::forward<T>(args)...);
		};

		Log(GetSize, WriteBuffer);
	}
};

namespace Frontend
{
	extern DebugLog debug_log;
}

#endif /* DEBUG_LOG_H */
