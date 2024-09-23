#include "debug-psg.h"

#include <algorithm>

#include "frontend.h"

bool DebugPSG::Registers::Display()
{
	return BeginAndEnd(0,
		[&]()
		{
			const PSG_State &psg = Frontend::emulator->CurrentState().clownmdemu.psg;
			const auto monospace_font = GetMonospaceFont();

			// Latched command.
			ImGui::SeparatorText("Latched Command");
			if (ImGui::BeginTable("Latched Command", 2, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Channel");
				ImGui::TableSetupColumn("Type");
				ImGui::TableHeadersRow();

				ImGui::TableNextColumn();

				if (psg.latched_command.channel == 3)
					ImGui::TextUnformatted("Noise");
				else
					ImGui::Text("Tone %" CC_PRIuLEAST8, psg.latched_command.channel + 1);

				ImGui::TableNextColumn();

				ImGui::TextUnformatted(psg.latched_command.is_volume_command ? "Attenuation" : "Frequency");

				ImGui::EndTable();
			}

			// Channels.
			const cc_u32f psg_clock = Frontend::emulator->GetPALMode() ? CLOWNMDEMU_PSG_SAMPLE_RATE_PAL : CLOWNMDEMU_PSG_SAMPLE_RATE_NTSC;

			// Tone channels.
			ImGui::SeparatorText("Tone Channels");
			if (ImGui::BeginTable("Tone Channels", 3, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Channel");
				ImGui::TableSetupColumn("Frequency");
				ImGui::TableSetupColumn("Attentuation");
				ImGui::TableHeadersRow();

				for (cc_u8f i = 0; i < CC_COUNT_OF(psg.tones); ++i)
				{
					ImGui::TableNextColumn();
					ImGui::Text("Tone %" CC_PRIuFAST8, i + 1);

					ImGui::PushFont(monospace_font);

					ImGui::TableNextColumn();
					ImGui::Text("0x%03" CC_PRIXLEAST16 " (%6" CC_PRIuFAST32 "Hz)", psg.tones[i].countdown_master, psg.tones[i].countdown_master == 0 ? 0 : psg_clock / psg.tones[i].countdown_master / 2);

					ImGui::TableNextColumn();
					if (psg.tones[i].attenuation == 15)
						ImGui::TextUnformatted("0xF (Mute)");
					else
						ImGui::Text("0x%" CC_PRIXLEAST8 " (%2" CC_PRIuLEAST8 "db)", psg.tones[i].attenuation, psg.tones[i].attenuation * 2);

					ImGui::PopFont();
				}

				ImGui::EndTable();
			}

			// Noise channel.
			ImGui::SeparatorText("Noise Channel");
			if (ImGui::BeginTable("Noise Channel", 3, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Noise Type");
				ImGui::TableSetupColumn("Frequency Mode");
				ImGui::TableSetupColumn("Attentuation");
				ImGui::TableHeadersRow();

				ImGui::TableNextColumn();

				switch (psg.noise.type)
				{
					case PSG_NOISE_TYPE_PERIODIC:
						ImGui::TextUnformatted("Periodic");
						break;

					case PSG_NOISE_TYPE_WHITE:
						ImGui::TextUnformatted("White");
						break;
				}

				ImGui::PushFont(monospace_font);

				ImGui::TableNextColumn();

				if (psg.noise.frequency_mode == 3)
					ImGui::TextUnformatted("Tone 3");
				else
					ImGui::Text("%" CC_PRIdLEAST8 " (%4" CC_PRIuFAST32 "Hz)", psg.noise.frequency_mode, psg_clock / (0x10 << psg.noise.frequency_mode) / 2);

				ImGui::TableNextColumn();

				if (psg.noise.attenuation == 15)
					ImGui::TextUnformatted("0xF (Mute)");
				else
					ImGui::Text("0x%" CC_PRIXLEAST8 " (%2" CC_PRIuLEAST8 "db)", psg.noise.attenuation, psg.noise.attenuation * 2);

				ImGui::PopFont();

				ImGui::EndTable();
			}
		}
	);
}
