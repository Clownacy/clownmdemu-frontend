#include "debug-z80.h"

#include "frontend.h"

void DebugZ80::Registers::Display()
{
	if (Begin())
	{
		const Z80_State &z80 = Frontend::emulator->CurrentState().clownmdemu.z80.state;

		ImGui::PushFont(GetMonospaceFont());

		ImGui::Text("Normal: A:%02" CC_PRIXLEAST8 " B:%02" CC_PRIXLEAST8 " C:%02" CC_PRIXLEAST8 " D:%02" CC_PRIXLEAST8 " E:%02" CC_PRIXLEAST8 " F:%02" CC_PRIXLEAST8 " H:%02" CC_PRIXLEAST8 " L:%02" CC_PRIXLEAST8, z80.a, z80.b, z80.c, z80.d, z80.e, z80.f, z80.h, z80.l);
		ImGui::Separator();
		ImGui::Text("Shadow: A:%02" CC_PRIXLEAST8 " B:%02" CC_PRIXLEAST8 " C:%02" CC_PRIXLEAST8 " D:%02" CC_PRIXLEAST8 " E:%02" CC_PRIXLEAST8 " F:%02" CC_PRIXLEAST8 " H:%02" CC_PRIXLEAST8 " L:%02" CC_PRIXLEAST8, z80.a_, z80.b_, z80.c_, z80.d_, z80.e_, z80.f_, z80.h_, z80.l_);
		ImGui::Separator();
		ImGui::Text("Other:  I:%02" CC_PRIXLEAST8 " R:%02" CC_PRIXLEAST8 " IX:%04" CC_PRIXFAST16 " IY:%04" CC_PRIXFAST16 " SP:%04" CC_PRIXLEAST16 " PC:%04" CC_PRIXLEAST16, z80.i, z80.r, (static_cast<cc_u16f>(z80.ixh) << 8) | z80.ixl, (static_cast<cc_u16f>(z80.iyh) << 8) | z80.iyl, z80.stack_pointer, z80.program_counter);

		ImGui::PopFont();
	}

	End();
}
