#include "debug-m68k.h"

#include "../libraries/imgui/imgui.h"
#include "../common/clownmdemu/clowncommon/clowncommon.h"
#include "../common/clownmdemu/clownmdemu.h"

void DebugM68k::Registers::DisplayInternal(const Clown68000_State &m68k)
{
	ImGui::PushFont(GetMonospaceFont());

	for (cc_u8f i = 0; i < 8; ++i)
	{
		if (i != 0 && i != 4)
			ImGui::SameLine();

		ImGui::TextFormatted("D{:X}:{:08X}", i, m68k.data_registers[i]);
	}

	ImGui::Separator();

	for (cc_u8f i = 0; i < 8; ++i)
	{
		if (i != 0 && i != 4)
			ImGui::SameLine();

		ImGui::TextFormatted("A{:X}:{:08X}", i, m68k.address_registers[i]);
	}

	ImGui::Separator();

	ImGui::TextFormatted("PC:{:08X}", m68k.program_counter);
	ImGui::SameLine();
	ImGui::TextFormatted("SR:{:04X}", m68k.status_register);
	ImGui::SameLine();
	ImGui::TextUnformatted("              ");
	ImGui::SameLine();
	if ((m68k.status_register & 0x2000) != 0)
		ImGui::TextFormatted("USP:{:08X}", m68k.user_stack_pointer);
	else
		ImGui::TextFormatted("SSP:{:08X}", m68k.supervisor_stack_pointer);

	ImGui::PopFont();
}
