#include "debug_m68k.h"

#include "libraries/imgui/imgui.h"
#include "../clowncommon.h"
#include "../clownmdemu.h"

void Debug_M68k(bool *open, M68k_State *m68k, ImFont *monospace_font)
{
	if (ImGui::Begin("68000 Status", open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushFont(monospace_font);

		for (cc_u8f i = 0; i < 8; ++i)
		{
			if (i != 0 && i != 4)
				ImGui::SameLine();

			ImGui::Text("D%X:%08" CC_PRIXLEAST32, i, m68k->data_registers[i]);
		}

		ImGui::Separator();

		for (cc_u8f i = 0; i < 8; ++i)
		{
			if (i != 0 && i != 4)
				ImGui::SameLine();

			ImGui::Text("A%X:%08" CC_PRIXLEAST32, i, m68k->address_registers[i]);
		}

		ImGui::Separator();

		ImGui::Text("PC:%08" CC_PRIXLEAST32, m68k->program_counter);
		ImGui::SameLine();
		ImGui::Text("SR:%04" CC_PRIXLEAST16, m68k->status_register);
		ImGui::SameLine();
		ImGui::TextUnformatted("              ");
		ImGui::SameLine();
		if ((m68k->status_register & 0x2000) != 0)
			ImGui::Text("USP:%08" CC_PRIXLEAST32, m68k->user_stack_pointer);
		else
			ImGui::Text("SSP:%08" CC_PRIXLEAST32, m68k->supervisor_stack_pointer);

		ImGui::PopFont();

		ImGui::End();
	}
}
