#include "debug_fm.h"

#include <stddef.h>

#include "SDL.h"
#include "libraries/imgui/imgui.h"
#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"
#include "clownmdemu-frontend-common/clownmdemu/clownmdemu.h"

void Debug_FM(bool *open, const ClownMDEmu *clownmdemu, ImFont *monospace_font)
{
	if (ImGui::Begin("FM", open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const FM_State* const fm = &clownmdemu->state->fm;

		ImGui::SeparatorText("DAC Channel");

		if (ImGui::BeginTable("DAC Register Table", 2, ImGuiTableFlags_Borders))
		{
			ImGui::TableSetupColumn("Register");
			ImGui::TableSetupColumn("Value");
			ImGui::TableHeadersRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("DAC Enabled");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(fm->dac_enabled ? "Yes" : "No");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("DAC Sample");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::Text("0x%02" CC_PRIXFAST16, (fm->dac_sample / (0x100 / FM_VOLUME_DIVIDER)) + 0x80);
			ImGui::PopFont();

			ImGui::EndTable();
		}

		ImGui::SeparatorText("FM Channels");

		char window_name_buffer[] = "FM1";

		for (size_t i = 0; i < CC_COUNT_OF(fm->channels); ++i)
		{
			if (ImGui::BeginTabBar("FM Channels Tab Bar"))
			{
				window_name_buffer[2] = '1' + i;

				if (ImGui::BeginTabItem(window_name_buffer))
				{
					const FM_ChannelMetadata* const channel = &fm->channels[i];

					if (ImGui::BeginTable("FM Channel Table", 2, ImGuiTableFlags_Borders))
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

					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}
		}

		ImGui::End();
	}
}
