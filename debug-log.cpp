#include "debug-log.h"

void DebugLog::Log(const char* const format, std::va_list args)
{
	if (logging_enabled || force_console_output)
	{
		const std::size_t message_buffer_size = static_cast<std::size_t>(SDL_vsnprintf(nullptr, 0, format, args)) + 1;

		try
		{
			lines.emplace_back(message_buffer_size);
			char* const message_buffer = &lines.back()[0];
			SDL_vsnprintf(message_buffer, message_buffer_size, format, args);

			if (log_to_console || force_console_output)
				SDL_LogMessage(SDL_LOG_CATEGORY_ERROR, SDL_LOG_PRIORITY_ERROR, "%s", message_buffer);
		}
		catch (const std::bad_alloc&)
		{
			// Wipe the line buffer to reclaim RAM. It's better than nothing.
			lines.clear();
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

void DebugLog::Display(bool &open)
{
	ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Log", &open))
	{
		ImGui::Checkbox("Enable Logging", &logging_enabled);
		ImGui::SameLine();
		ImGui::Checkbox("Log to Console", &log_to_console);
		ImGui::SameLine();
		if (ImGui::Button("Clear"))
			lines.clear();
		ImGui::Separator();

		if (ImGui::BeginChild("Scrolling Section", ImVec2(0,0), false, ImGuiWindowFlags_HorizontalScrollbar))
		{
			ImGui::PushFont(monospace_font);

			ImGuiListClipper clipper;
			clipper.Begin(lines.size());
			while (clipper.Step())
			{
				for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				{
					const char* const line = &lines[i][0];
					ImGui::TextUnformatted(line, line + lines[i].size() - 1);
				}
			}
			clipper.End();

			// When scrolled to the bottom, stay that way.
			if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);

			ImGui::PopFont();
		}

		ImGui::EndChild();
	}

	ImGui::End();
}
