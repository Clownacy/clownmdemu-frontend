#include "debug-m68k.h"

#include "../libraries/imgui/imgui.h"
#include "../clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "../clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

void DebugM68k::Registers::DisplayInternal(const Clown68000_State &m68k)
{
	ImGui::PushFont(GetMonospaceFont());

	for (cc_u8f i = 0; i < 8; ++i)
	{
		if (i != 0 && i != 4)
			ImGui::SameLine();

		ImGui::Text("D%" CC_PRIXFAST8 ":%08" CC_PRIXLEAST32, i, m68k.data_registers[i]);
	}

	ImGui::Separator();

	for (cc_u8f i = 0; i < 8; ++i)
	{
		if (i != 0 && i != 4)
			ImGui::SameLine();

		ImGui::Text("A%" CC_PRIXFAST8 ":%08" CC_PRIXLEAST32, i, m68k.address_registers[i]);
	}

	ImGui::Separator();

	ImGui::Text("PC:%08" CC_PRIXLEAST32, m68k.program_counter);
	ImGui::SameLine();
	ImGui::Text("SR:%04" CC_PRIXLEAST16, m68k.status_register);
	ImGui::SameLine();
	ImGui::TextUnformatted("              ");
	ImGui::SameLine();
	if ((m68k.status_register & 0x2000) != 0)
		ImGui::Text("USP:%08" CC_PRIXLEAST32, m68k.user_stack_pointer);
	else
		ImGui::Text("SSP:%08" CC_PRIXLEAST32, m68k.supervisor_stack_pointer);

	ImGui::PopFont();
}
