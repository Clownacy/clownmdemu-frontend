#include "debug_psg.h"

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

void Debug_PSG(bool *open, ClownMDEmu *clownmdemu, ImFont *monospace_font)
{
	if (ImGui::Begin("PSG", open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// Latched command.
		ImGui::SeparatorText("Latched Command");
		if (ImGui::BeginTable("Latched Command", 2, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Channel");
			ImGui::TableSetupColumn("Type");
			ImGui::TableHeadersRow();

			ImGui::TableNextColumn();

			if (clownmdemu->state->psg.latched_command.channel == 3)
				ImGui::TextUnformatted("Noise");
			else
				ImGui::Text("Tone %" CC_PRIuFAST8, clownmdemu->state->psg.latched_command.channel + 1);

			ImGui::TableNextColumn();

			ImGui::TextUnformatted(clownmdemu->state->psg.latched_command.is_volume_command ? "Attenuation" : "Frequency");

			ImGui::EndTable();
		}

		// Channels.
		const cc_u32f psg_clock = (clownmdemu->configuration->general.tv_standard == CLOWNMDEMU_TV_STANDARD_PAL ? CLOWNMDEMU_MASTER_CLOCK_PAL : CLOWNMDEMU_MASTER_CLOCK_NTSC) / 15 / 16;

		// Tone channels.
		ImGui::SeparatorText("Tone Channels");
		if (ImGui::BeginTable("Tone Channels", 3, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Channel");
			ImGui::TableSetupColumn("Frequency");
			ImGui::TableSetupColumn("Attentuation");
			ImGui::TableHeadersRow();

			for (cc_u8f i = 0; i < CC_COUNT_OF(clownmdemu->state->psg.tones); ++i)
			{
				ImGui::TableNextColumn();
				ImGui::Text("Tone %" CC_PRIuFAST8, i + 1);

				ImGui::PushFont(monospace_font);

				ImGui::TableNextColumn();
				ImGui::Text("0x%03" CC_PRIXFAST16 " (%6" CC_PRIuFAST32 "Hz)", clownmdemu->state->psg.tones[i].countdown_master, psg_clock / (clownmdemu->state->psg.tones[i].countdown_master + 1) / 2);

				ImGui::TableNextColumn();
				if (clownmdemu->state->psg.tones[i].attenuation == 15)
					ImGui::TextUnformatted("0xF (Mute)");
				else
					ImGui::Text("0x%" CC_PRIXFAST8 " (%2" CC_PRIuFAST8 "db)", clownmdemu->state->psg.tones[i].attenuation, clownmdemu->state->psg.tones[i].attenuation * 2);

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

			switch (clownmdemu->state->psg.noise.type)
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

			if (clownmdemu->state->psg.noise.frequency_mode == 3)
				ImGui::TextUnformatted("Tone 3");
			else
				ImGui::Text("%" CC_PRIdFAST8 " (%4" CC_PRIuFAST32 "Hz)", clownmdemu->state->psg.noise.frequency_mode, psg_clock / (0x10 << clownmdemu->state->psg.noise.frequency_mode) / 2);

			ImGui::TableNextColumn();

			if (clownmdemu->state->psg.noise.attenuation == 15)
				ImGui::TextUnformatted("0xF (Mute)");
			else
				ImGui::Text("0x%" CC_PRIXFAST8 " (%2" CC_PRIuFAST8 "db)", clownmdemu->state->psg.noise.attenuation, clownmdemu->state->psg.noise.attenuation * 2);

			ImGui::PopFont();

			ImGui::EndTable();
		}

		ImGui::End();
	}
}
