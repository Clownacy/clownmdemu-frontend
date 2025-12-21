#include "debug-psg.h"

#include <algorithm>

#include "../frontend.h"

void DebugPSG::Registers::DisplayInternal()
{
	const PSG_State &psg = Frontend::emulator->GetState().psg;
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
			ImGui::TextFormatted("Tone {}", psg.latched_command.channel + 1);

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

		for (std::size_t i = 0; i != std::size(psg.tones); ++i)
		{
			const auto &tone = psg.tones[i];

			ImGui::TableNextColumn();
			ImGui::TextFormatted("Tone {}", i + 1);

			ImGui::PushFont(monospace_font);

			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:03X} ({:6}Hz)", tone.countdown_master, tone.countdown_master == 0 ? 0 : psg_clock / tone.countdown_master / 2);

			ImGui::TableNextColumn();
			if (tone.attenuation == 15)
				ImGui::TextUnformatted("0xF (Mute)");
			else
				ImGui::TextFormatted("0x{:X} ({:2}db)", tone.attenuation, tone.attenuation * 2);

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
			ImGui::TextFormatted("{} ({:4}Hz)", psg.noise.frequency_mode, psg_clock / (0x10 << psg.noise.frequency_mode) / 2);

		ImGui::TableNextColumn();

		if (psg.noise.attenuation == 15)
			ImGui::TextUnformatted("0xF (Mute)");
		else
			ImGui::TextFormatted("0x{:X} ({:2}db)", psg.noise.attenuation, psg.noise.attenuation * 2);

		ImGui::PopFont();

		ImGui::EndTable();
	}
}
