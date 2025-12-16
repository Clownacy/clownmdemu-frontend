#include "debug-z80.h"

#include "../libraries/imgui/imgui.h"

#include "../frontend.h"

void DebugZ80::Registers::DisplayInternal()
{
	const ClownZ80_State &z80 = Frontend::emulator->CurrentState().clownmdemu.z80.state;

	ImGui::PushFont(GetMonospaceFont());

	ImGui::TextFormatted("Normal: A:{:02X} B:{:02X} C:{:02X} D:{:02X} E:{:02X} F:{:02X} H:{:02X} L:{:02X}", z80.a, z80.b, z80.c, z80.d, z80.e, z80.f, z80.h, z80.l);
	ImGui::Separator();
	ImGui::TextFormatted("Shadow: A:{:02X} B:{:02X} C:{:02X} D:{:02X} E:{:02X} F:{:02X} H:{:02X} L:{:02X}", z80.a_, z80.b_, z80.c_, z80.d_, z80.e_, z80.f_, z80.h_, z80.l_);
	ImGui::Separator();
	ImGui::TextFormatted("Other:  I:{:02X} R:{:02X} IX:{:04X} IY:{:04X} SP:{:04X} PC:{:04X}", z80.i, z80.r, (static_cast<cc_u16f>(z80.ixh) << 8) | z80.ixl, (static_cast<cc_u16f>(z80.iyh) << 8) | z80.iyl, z80.stack_pointer, z80.program_counter);

	ImGui::PopFont();
}
