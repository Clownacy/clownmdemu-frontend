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

	ImGui::PushFont(GetMonospaceFont(), GetMonospaceFont()->LegacySize);
	ImGui::InputTextMultiline("##log", &Frontend::debug_log.lines[0], Frontend::debug_log.lines.length() + 1, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);

	ImGui::PopFont();
}
