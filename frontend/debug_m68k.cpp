#include "debug_m68k.h"

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "../clowncommon.h"
#include "../clownmdemu.h"

void Debug_M68k_RAM(bool *open, ClownMDEmu *clownmdemu, ImFont *monospace_font)
{
	ImGui::PushFont(monospace_font);

	// Fit window's width to text, and disable horizontal resizing.
	const ImGuiStyle &style = ImGui::GetStyle();
	const float width = style.ScrollbarSize + style.WindowPadding.x * 2 + ImGui::CalcTextSize("0000: 0001 0203 0405 0607 0809 0A0B 0C0D 0E0F").x;
	ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0), ImVec2(width, FLT_MAX));

	ImGui::PopFont();

	if (ImGui::Begin("68000 RAM Viewer", open))
	{
		ImGui::PushFont(monospace_font);

		ImGuiListClipper clipper;
		clipper.Begin(CC_COUNT_OF(clownmdemu->state->m68k_ram) / 0x10);
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				const size_t offset = i * 0x10;
				const unsigned char* const bytes = &clownmdemu->state->m68k_ram[offset];

				ImGui::Text("%04zX: %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X %02X%02X", offset,
					bytes[0x0], bytes[0x1], bytes[0x2], bytes[0x3], bytes[0x4], bytes[0x5], bytes[0x6], bytes[0x7],
					bytes[0x8], bytes[0x9], bytes[0xA], bytes[0xB], bytes[0xC], bytes[0xD], bytes[0xE], bytes[0xF]);
			}
		}
		
		ImGui::PopFont();

		ImGui::End();
	}
}
