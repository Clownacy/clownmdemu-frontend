#include "debug_psg.h"

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "../clowncommon.h"
#include "../clownmdemu.h"

void Debug_PSG(bool *open, ClownMDEmu_State *clownmdemu_state, ImFont *monospace_font)
{
	if (ImGui::Begin("PSG Status", open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// Latched command.
		if (ImGui::TreeNodeEx("Latched Command", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::PushFont(monospace_font);

			if (clownmdemu_state->psg.latched_command.channel == 3)
				ImGui::TextUnformatted("Channel: Noise");
			else
				ImGui::Text("Channel: Tone #%u", clownmdemu_state->psg.latched_command.channel + 1);

			ImGui::TextUnformatted(clownmdemu_state->psg.latched_command.is_volume_command ? "Type:    Attenuation" : "Type:    Frequency");

			ImGui::PopFont();

			ImGui::TreePop();
		}

		// Channels.
		if (ImGui::TreeNodeEx("Channels", ImGuiTreeNodeFlags_DefaultOpen))
		{
			const unsigned long psg_clock = (clownmdemu_state->tv_standard == CLOWNMDEMU_TV_STANDARD_PAL ? CLOWNMDEMU_MASTER_CLOCK_PAL : CLOWNMDEMU_MASTER_CLOCK_NTSC) / 15 / 16;

			// Tone channels.
			for (unsigned int i = 0; i < CC_COUNT_OF(clownmdemu_state->psg.tones); ++i)
			{
				char string_buffer[7 + 1];
				SDL_snprintf(string_buffer, sizeof(string_buffer), "Tone #%d", i + 1);
				if (ImGui::TreeNodeEx(string_buffer, ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::PushFont(monospace_font);

					ImGui::Text("Frequency: 0x%03X (%6uHz)", clownmdemu_state->psg.tones[i].countdown_master, psg_clock / (clownmdemu_state->psg.tones[i].countdown_master + 1) / 2);

					if (clownmdemu_state->psg.tones[i].attenuation == 15)
						ImGui::TextUnformatted("Attenuation: 0xF (Mute)");
					else
						ImGui::Text("Attenuation: 0x%X (%2udb)", clownmdemu_state->psg.tones[i].attenuation, clownmdemu_state->psg.tones[i].attenuation * 2);

					ImGui::PopFont();

					ImGui::TreePop();
				}
			}

			// Noise channel.
			if (ImGui::TreeNodeEx("Noise", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::PushFont(monospace_font);

				switch (clownmdemu_state->psg.noise.type)
				{
					case PSG_NOISE_TYPE_PERIODIC:
						ImGui::TextUnformatted("Type: Periodic Noise");
						break;

					case PSG_NOISE_TYPE_WHITE:
						ImGui::TextUnformatted("Type: White Noise");
						break;
				}

				if (clownmdemu_state->psg.noise.frequency_mode == 3)
					ImGui::TextUnformatted("Frequency Mode: Tone #3");
				else
					ImGui::Text("Frequency Mode: %d (%4uHz)", clownmdemu_state->psg.noise.frequency_mode, psg_clock / (0x10 << clownmdemu_state->psg.noise.frequency_mode) / 2);

				if (clownmdemu_state->psg.noise.attenuation == 15)
					ImGui::TextUnformatted("Attenuation:  0xF (Mute)");
				else
					ImGui::Text("Attenuation:  0x%X (%2udb)", clownmdemu_state->psg.noise.attenuation, clownmdemu_state->psg.noise.attenuation * 2);

				ImGui::PopFont();

				ImGui::TreePop();
			}

			ImGui::TreePop();
		}

		ImGui::End();
	}
}
