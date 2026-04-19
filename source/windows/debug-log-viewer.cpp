#include "debug-log-viewer.h"

#include "../frontend.h"

void DebugLogViewer::DisplayInternal()
{
	ImGui::Checkbox("Enable Logging", &Frontend::debug_log.logging_enabled);
	ImGui::SameLine();
	ImGui::Checkbox("Log to Console", &Frontend::debug_log.log_to_console);
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
		Frontend::debug_log.lines.clear();

	ImGui::PushFont(GetMonospaceFont());

	for (const auto &line : Frontend::debug_log.lines)
		ImGui::TextUnformatted(line);

	ImGui::PopFont();
}
