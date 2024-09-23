#include "debug-pcm.h"

#include "frontend.h"

void DebugPCM::Registers::DisplayInternal()
{
	if (ImGui::BeginTable("Channels", 8, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Channel");
		ImGui::TableSetupColumn("Enabled");
		ImGui::TableSetupColumn("Frequency");
		ImGui::TableSetupColumn("Volume");
		ImGui::TableSetupColumn("Panning");
		ImGui::TableSetupColumn("Address");
		ImGui::TableSetupColumn("Start Address");
		ImGui::TableSetupColumn("Loop Address");
		ImGui::TableHeadersRow();

		const PCM_State &pcm = Frontend::emulator->CurrentState().clownmdemu.mega_cd.pcm;

		for (cc_u8f i = 0; i < CC_COUNT_OF(pcm.channels); ++i)
		{
			const auto &channel = pcm.channels[i];

			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST8, static_cast<cc_u8f>(i + 1));

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(channel.disabled ? "No" : "Yes");

			ImGui::PushFont(GetMonospaceFont());

			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16 " (%5" CC_PRIuFAST32 "Hz)", channel.frequency, static_cast<cc_u32f>(channel.frequency * CLOWNMDEMU_PCM_SAMPLE_RATE / 0x800));

			ImGui::TableNextColumn();
			ImGui::Text("0x%02" CC_PRIXLEAST8, channel.volume);

			ImGui::TableNextColumn();
			ImGui::Text("0x%02" CC_PRIXFAST8, static_cast<cc_u8f>((channel.panning[0] << 0) | (channel.panning[1] << 4)));

			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXFAST32 ".%03" CC_PRIXFAST32, static_cast<cc_u32f>(channel.address / (1 << 11)), static_cast<cc_u32f>((channel.address % (1 << 11)) * 2));

			ImGui::TableNextColumn();
			ImGui::Text("0x%02" CC_PRIXLEAST8 "00", channel.start_address);

			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXLEAST16, channel.loop_address);

			ImGui::PopFont();
		}

		ImGui::EndTable();
	}
}
