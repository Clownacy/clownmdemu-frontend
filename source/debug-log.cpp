#include "debug-log.h"

DebugLog Frontend::debug_log;

void DebugLog::Log(std::string message)
{
	if (logging_enabled || force_console_output)
	{
		try
		{
			lines.emplace_back(std::move(message));

			if (log_to_console || force_console_output)
				SDL_LogMessage(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR, "%s", message.c_str());
		}
		catch (const std::bad_alloc&)
		{
			// Wipe the line buffer to reclaim RAM. It's better than nothing.
			lines.clear();
		}
	}
}

void DebugLog::Log(const char* const format, std::va_list args)
{
	const auto GetSize = [&]()
	{
		std::va_list args2;
		va_copy(args2, args);
		const std::size_t message_buffer_size = static_cast<std::size_t>(SDL_vsnprintf(nullptr, 0, format, args2));
		va_end(args2);
		return message_buffer_size;
	};

	const auto WriteBuffer = [&](char* const message_buffer, const std::size_t message_buffer_size)
	{
		SDL_vsnprintf(message_buffer, message_buffer_size + 1, format, args);
	};

	std::string message_string;
	message_string.resize(GetSize());
	WriteBuffer(std::data(message_string), std::size(message_string));

	Log(std::move(message_string));
}
