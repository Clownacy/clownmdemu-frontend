#include "debug-log.h"

DebugLog Frontend::debug_log;

void DebugLog::Log(std::string message)
{
	if (logging_enabled || force_console_output)
	{
		try
		{
			if (log_to_console || force_console_output)
				SDL_LogMessage(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN, "%s", message.c_str());

			lines.emplace_back(std::move(message));
		}
		catch (const std::bad_alloc&)
		{
			// Wipe the line buffer to reclaim RAM; it's better than nothing.
			lines.clear();
		}
	}
}

void DebugLog::Log(const char* const format, std::va_list args)
{
	const auto FormatToString = [](const char* const format, std::va_list args)
	{
		std::string message_string;

		std::va_list args2;
		va_copy(args2, args);
		message_string.resize(static_cast<std::size_t>(SDL_vsnprintf(nullptr, 0, format, args2)));
		va_end(args2);

		SDL_vsnprintf(std::data(message_string), std::size(message_string) + 1, format, args);
		return message_string;
	};

	Log(FormatToString(format, args));
}
