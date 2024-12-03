#include "debug-pcm.h"

#include "../libraries/imgui/imgui.h"

#include "../frontend.h"

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

		for (auto channel = std::cbegin(pcm.channels); channel != std::cend(pcm.channels); ++channel)
		{
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", static_cast<cc_u8f>(std::distance(std::cbegin(pcm.channels), channel) + 1));

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(channel->disabled ? "No" : "Yes");

			ImGui::PushFont(GetMonospaceFont());

			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:04X} ({:5}Hz)", channel->frequency, static_cast<cc_u32f>(channel->frequency * CLOWNMDEMU_PCM_SAMPLE_RATE / 0x800));

			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:02X}", channel->volume);

			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:02X}", static_cast<cc_u8f>((channel->panning[0] << 0) | (channel->panning[1] << 4)));

			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:04X}.{:03X}", static_cast<cc_u32f>(channel->address / (1 << 11)), static_cast<cc_u32f>((channel->address % (1 << 11)) * 2));

			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:02X}00", channel->start_address);

			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:04X}", channel->loop_address);

			ImGui::PopFont();
		}

		ImGui::EndTable();
	}
}
