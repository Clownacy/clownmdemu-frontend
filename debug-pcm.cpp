#include "debug-pcm.h"

void DebugPCM::Display(bool &open)
{
	if (ImGui::Begin("PCM", &open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::SeparatorText("Channels");
		if (ImGui::BeginTable("Channels", 9, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Channel");
			ImGui::TableSetupColumn("Address");
			ImGui::TableSetupColumn("Address (Whole)");
			ImGui::TableSetupColumn("Disabled");
			ImGui::TableSetupColumn("Frequency");
			ImGui::TableSetupColumn("Loop Address");
			ImGui::TableSetupColumn("Panning");
			ImGui::TableSetupColumn("Start Address");
			ImGui::TableSetupColumn("Volume");
			ImGui::TableHeadersRow();

			const PCM_State &pcm = emulator.CurrentState().clownmdemu.mega_cd.pcm;

			for (cc_u8f i = 0; i < CC_COUNT_OF(pcm.channels); ++i)
			{
				const auto &channel = pcm.channels[i];

				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIuFAST8, i + 1);

				ImGui::PushFont(monospace_font);

				ImGui::TableNextColumn();
				ImGui::Text("0x%" CC_PRIXLEAST32, channel.address >> 11);

				ImGui::TableNextColumn();
				ImGui::Text("0x%" CC_PRIXLEAST32, channel.address);

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(channel.disabled ? "Yes" : "No");

				ImGui::TableNextColumn();
				ImGui::Text("0x%04" CC_PRIXLEAST16 " (%" CC_PRIuLEAST16 "Hz)", channel.frequency, channel.frequency * CLOWNMDEMU_PCM_SAMPLE_RATE / 0x800);

				ImGui::TableNextColumn();
				ImGui::Text("0x%" CC_PRIXLEAST16, channel.loop_address);

				ImGui::TableNextColumn();
				ImGui::Text("0x%" CC_PRIXLEAST8, channel.panning);

				ImGui::TableNextColumn();
				ImGui::Text("0x%" CC_PRIXLEAST8, channel.start_address);

				ImGui::TableNextColumn();
				ImGui::Text("0x%" CC_PRIXLEAST8, channel.volume);

				ImGui::PopFont();
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
}
