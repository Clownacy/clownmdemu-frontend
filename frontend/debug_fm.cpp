#include "debug_fm.h"

#include <stddef.h>

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "../clowncommon.h"
#include "../clownmdemu.h"

void Debug_DAC_Channel(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font)
{
	if (ImGui::Begin("DAC", open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const FM_State* const fm = &clownmdemu->state->fm;

		if (ImGui::BeginTable("Channel Table", 2, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Register");
			ImGui::TableSetupColumn("All Operators");
			ImGui::TableHeadersRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("DAC Enabled");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(fm->dac_enabled ? "Yes" : "No");
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("DAC Sample");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%02" CC_PRIXFAST16, (fm->dac_sample / (0x100 / FM_VOLUME_DIVIDER)) + 0x80);
			ImGui::PopFont();

			ImGui::EndTable();
		}

		ImGui::End();
	}
}

void Debug_FM_Channel(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font, cc_u16f channel_index)
{
	char window_name_buffer[sizeof("FM Channel 0")];
	SDL_snprintf(window_name_buffer, sizeof(window_name_buffer), "FM Channel %" CC_PRIuFAST16, channel_index + 1);

	if (ImGui::Begin(window_name_buffer, open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const FM_ChannelMetadata* const channel = &clownmdemu->state->fm.channels[channel_index];

		if (ImGui::BeginTable("Channel Table", 2, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Register");
			ImGui::TableSetupColumn("All Operators");
			ImGui::TableHeadersRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Cached Upper Frequency Bits");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%02" CC_PRIXFAST16, channel->cached_upper_frequency_bits);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Frequency");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%04" CC_PRIXFAST16, channel->state.operators[0].phase.f_number_and_block);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Feedback");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();

			cc_u16f bit_index = 0;

			for (cc_u16f temp = channel->state.feedback_divisor; temp != 1; temp >>= 1)
				++bit_index;

			ImGui::Text("%" CC_PRIuFAST16, 9 - bit_index);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Algorithm");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("%" CC_PRIuFAST16, channel->state.algorithm);
			ImGui::PopFont();

			ImGui::EndTable();
		}

		if (ImGui::BeginTable("Operator Table", 5, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Register");
			ImGui::TableSetupColumn("Operator 1");
			ImGui::TableSetupColumn("Operator 2");
			ImGui::TableSetupColumn("Operator 3");
			ImGui::TableSetupColumn("Operator 4");
			ImGui::TableHeadersRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Key On");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(channel->state.operators[operator_index].envelope.key_on ? "On" : "Off");
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Detune");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIuFAST16, channel->state.operators[operator_index].phase.detune);
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Multiplier");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIuFAST16, channel->state.operators[operator_index].phase.multiplier / 2);
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Total Level");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::Text("0x%02" CC_PRIXFAST16, channel->state.operators[operator_index].envelope.total_level >> 3);
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Key Scale");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				static const cc_u16f decode[8] = {3, 2, 0xFF, 1, 0xFF, 0xFF, 0xFF, 0};

				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIuFAST16, decode[channel->state.operators[operator_index].envelope.key_scale - 1]);
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Attack Rate");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::Text("0x%02" CC_PRIXFAST16, channel->state.operators[operator_index].envelope.rates[FM_ENVELOPE_MODE_ATTACK]);
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Decay Rate");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::Text("0x%02" CC_PRIXFAST16, channel->state.operators[operator_index].envelope.rates[FM_ENVELOPE_MODE_DECAY]);
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Sustain Rate");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::Text("0x%02" CC_PRIXFAST16, channel->state.operators[operator_index].envelope.rates[FM_ENVELOPE_MODE_SUSTAIN]);
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Release Rate");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::Text("0x%" CC_PRIXFAST16, channel->state.operators[operator_index].envelope.rates[FM_ENVELOPE_MODE_RELEASE] >> 1);
			}
			ImGui::PopFont();

			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Sustain Level");

			ImGui::PushFont(monospace_font);
			for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel->state.operators); ++operator_index)
			{
				ImGui::TableNextColumn();
				ImGui::Text("0x%" CC_PRIXFAST16, (channel->state.operators[operator_index].envelope.sustain_level / 0x20) & 0xF);
			}
			ImGui::PopFont();

			ImGui::EndTable();
		}

		ImGui::End();
	}
}
