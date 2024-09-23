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
	ImGui::InputTextMultiline("##log", &Frontend::debug_log.lines[0], Frontend::debug_log.lines.length(), ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);

	// When scrolled to the bottom, stay that way.
	if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		ImGui::SetScrollHereY(1.0f);

	ImGui::PopFont();
}
