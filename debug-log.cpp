#include "debug-log.h"

DebugLog Frontend::debug_log;

void DebugLog::Log(const char* const format, std::va_list args)
{
	if (logging_enabled || force_console_output)
	{
		std::va_list args2;
		va_copy(args2, args);
		const std::size_t message_buffer_size = static_cast<std::size_t>(SDL_vsnprintf(nullptr, 0, format, args2));
		va_end(args2);

		try
		{
			if (lines.length() != 0)
				lines += '\n';

			const auto length = lines.length();
			lines.resize(length + message_buffer_size);
			char* const message_buffer = &lines[length];
			SDL_vsnprintf(message_buffer, message_buffer_size + 1, format, args);

			if (log_to_console || force_console_output)
				SDL_LogMessage(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR, "%s", message_buffer);
		}
		catch (const std::bad_alloc&)
		{
			// Wipe the line buffer to reclaim RAM. It's better than nothing.
			lines.clear();
			lines.shrink_to_fit();
		}
	}
}

void DebugLog::Log(const char* const format, ...)
{
	std::va_list args;
	va_start(args, format);
	Log(format, args);
	va_end(args);
}
