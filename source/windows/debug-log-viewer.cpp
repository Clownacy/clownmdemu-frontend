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

	ImGuiListClipper clipper;
	clipper.Begin(std::size(Frontend::debug_log.lines));
	while (clipper.Step())
		for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			ImGui::TextUnformatted(Frontend::debug_log.lines[i]);

	ImGui::PopFont();
}
