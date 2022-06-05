#include "debug_memory.h"

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "../clowncommon.h"

void Debug_Memory(bool *open, ImFont *monospace_font, const char *window_name, const unsigned char *buffer, size_t buffer_length)
{
	ImGui::PushFont(monospace_font);

	// Fit window's width to text, and disable horizontal resizing.
	const ImGuiStyle &style = ImGui::GetStyle();
	const float width = style.ScrollbarSize + style.WindowPadding.x * 2 + ImGui::CalcTextSize("0000: 0001 0203 0405 0607 0809 0A0B 0C0D 0E0F").x;
	ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, FLT_MAX));

	ImGui::PopFont();

	if (ImGui::Begin(window_name, open))
	{
		ImGui::PushFont(monospace_font);

		ImGuiListClipper clipper;
		clipper.Begin(buffer_length / 0x10);
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				const size_t offset = i * 0x10;
				const unsigned char* const bytes = &buffer[offset];

				ImGui::Text("%04zX: %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X", offset,
					bytes[0x0], bytes[0x1], bytes[0x2], bytes[0x3], bytes[0x4], bytes[0x5], bytes[0x6], bytes[0x7],
					bytes[0x8], bytes[0x9], bytes[0xA], bytes[0xB], bytes[0xC], bytes[0xD], bytes[0xE], bytes[0xF]);
			}
		}
		
		ImGui::PopFont();

		ImGui::End();
	}
}
