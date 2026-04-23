#include "debug-log-viewer.h"

void DebugLogViewer::DisplayInternal()
{
	ImGui::Checkbox("Enable Logging", &debug_log.logging_enabled);
	ImGui::SameLine();
	ImGui::Checkbox("Log to Console", &debug_log.log_to_console);
	ImGui::SameLine();
	if (ImGui::Button("Clear"))
		debug_log.lines.clear();

	ImGui::Separator();

	if (ImGui::BeginChild("Log", {}, 0, ImGuiWindowFlags_NoMove))
	{
		ImGui::PushFont(GetMonospaceFont());

		ImGuiListClipper clipper;
		clipper.Begin(std::size(debug_log.lines));
		while (clipper.Step())
			for (auto i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
				ImGui::TextUnformatted(debug_log.lines[i]);

		text_select.update();

		ImGui::PopFont();

		// Create context menu for copying text.
		if (ImGui::BeginPopupContextWindow()) {
			ImGui::BeginDisabled(debug_log.lines.empty());

			ImGui::BeginDisabled(!text_select.hasSelection());
			if (ImGui::MenuItem("Copy", "Ctrl+C"))
				text_select.copy();
			ImGui::EndDisabled();

			if (ImGui::MenuItem("Select All", "Ctrl+A"))
				text_select.selectAll();

			ImGui::EndDisabled();

			ImGui::EndPopup();
		}
	}

	ImGui::EndChild();
}
