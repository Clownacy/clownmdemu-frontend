#include "debug-fm.h"

#include <cstddef>

void Debug_FM(bool &open, const EmulatorInstance &emulator, ImFont *monospace_font)
{
	if (ImGui::Begin("FM", &open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const char* const pannings[2][2] = {
			{"Mute", "R"},
			{"L", "L+R"}
		};

		const FM_State &fm = emulator.state->clownmdemu.fm;

		ImGui::SeparatorText("FM Channels");

		if (ImGui::BeginTable("FM Channel Table", 1 + CC_COUNT_OF(fm.channels), ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("FM1");
			ImGui::TableSetupColumn("FM2");
			ImGui::TableSetupColumn("FM3");
			ImGui::TableSetupColumn("FM4");
			ImGui::TableSetupColumn("FM5");
			ImGui::TableSetupColumn("FM6");
			ImGui::TableHeadersRow();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Frequency Cache");

			ImGui::PushFont(monospace_font);
			for (cc_u8f i = 0; i < CC_COUNT_OF(fm.channels); ++i)
			{
				ImGui::TableNextColumn();
				ImGui::Text("0x%02" CC_PRIXLEAST8, fm.channels[i].cached_upper_frequency_bits);
			}
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Frequency");

			ImGui::PushFont(monospace_font);
			for (cc_u8f i = 0; i < CC_COUNT_OF(fm.channels); ++i)
			{
				ImGui::TableNextColumn();
				ImGui::Text("0x%04" CC_PRIXLEAST16, fm.channels[i].state.operators[0].phase.f_number_and_block);
			}
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Feedback");

			ImGui::PushFont(monospace_font);
			for (cc_u8f i = 0; i < CC_COUNT_OF(fm.channels); ++i)
			{
				ImGui::TableNextColumn();

				cc_u16f bit_index = 0;

				for (cc_u16f temp = fm.channels[i].state.feedback_divisor; temp != 1; temp >>= 1)
					++bit_index;

				ImGui::Text("%" CC_PRIuFAST16, 9 - bit_index);
			}
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Algorithm");

			ImGui::PushFont(monospace_font);
			for (cc_u8f i = 0; i < CC_COUNT_OF(fm.channels); ++i)
			{
				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIuLEAST16, fm.channels[i].state.algorithm);
			}
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Panning");

			for (cc_u8f i = 0; i < CC_COUNT_OF(fm.channels); ++i)
			{
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pannings[fm.channels[i].pan_left][fm.channels[i].pan_right]);
			}

			ImGui::EndTable();
		}

		ImGui::SeparatorText("FM Operators");

		if (ImGui::BeginTabBar("FM Channels Tab Bar"))
		{
			char window_name_buffer[] = "FM1";

			for (std::size_t i = 0; i < CC_COUNT_OF(fm.channels); ++i)
			{
				window_name_buffer[2] = '1' + i;

				if (ImGui::BeginTabItem(window_name_buffer))
				{
					const FM_ChannelMetadata &channel = fm.channels[i];

					if (ImGui::BeginTable("FM Operator Table", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame))
					{
						ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed);
						ImGui::TableSetupColumn("Operator 1");
						ImGui::TableSetupColumn("Operator 2");
						ImGui::TableSetupColumn("Operator 3");
						ImGui::TableSetupColumn("Operator 4");
						ImGui::TableHeadersRow();

						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Key On");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(channel.state.operators[operator_index].envelope.key_on ? "On" : "Off");
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Detune");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::Text("%" CC_PRIuLEAST16, channel.state.operators[operator_index].phase.detune);
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Multiplier");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::Text("%" CC_PRIuLEAST16, channel.state.operators[operator_index].phase.multiplier / 2);
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Total Level");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::Text("0x%02" CC_PRIXLEAST16, channel.state.operators[operator_index].envelope.total_level >> 3);
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Key Scale");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							static const cc_u8l decode[8] = {3, 2, 0xFF, 1, 0xFF, 0xFF, 0xFF, 0};

							ImGui::TableNextColumn();
							ImGui::Text("%" CC_PRIuLEAST8, decode[channel.state.operators[operator_index].envelope.key_scale - 1]);
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Attack Rate");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::Text("0x%02" CC_PRIXLEAST16, channel.state.operators[operator_index].envelope.rates[FM_ENVELOPE_MODE_ATTACK]);
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Decay Rate");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::Text("0x%02" CC_PRIXLEAST16, channel.state.operators[operator_index].envelope.rates[FM_ENVELOPE_MODE_DECAY]);
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Sustain Rate");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::Text("0x%02" CC_PRIXLEAST16, channel.state.operators[operator_index].envelope.rates[FM_ENVELOPE_MODE_SUSTAIN]);
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Release Rate");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::Text("0x%" CC_PRIXLEAST16, channel.state.operators[operator_index].envelope.rates[FM_ENVELOPE_MODE_RELEASE] >> 1);
						}
						ImGui::PopFont();

						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Sustain Level");

						ImGui::PushFont(monospace_font);
						for (cc_u16f operator_index = 0; operator_index < CC_COUNT_OF(channel.state.operators); ++operator_index)
						{
							ImGui::TableNextColumn();
							ImGui::Text("0x%" CC_PRIXLEAST16, (channel.state.operators[operator_index].envelope.sustain_level / 0x20) & 0xF);
						}
						ImGui::PopFont();

						ImGui::EndTable();
					}

					ImGui::EndTabItem();
				}
			}

			ImGui::EndTabBar();
		}

		if (ImGui::BeginTable("Table Table", 2, ImGuiTableFlags_SizingStretchSame))
		{
			ImGui::TableNextColumn();
			ImGui::SeparatorText("DAC Channel");

			if (ImGui::BeginTable("DAC Register Table", 2, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Register");
				ImGui::TableSetupColumn("Value");
				ImGui::TableHeadersRow();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Enabled");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(fm.dac_enabled ? "Yes" : "No");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Sample");

				ImGui::PushFont(monospace_font);
				ImGui::TableNextColumn();
				ImGui::Text("0x%02" CC_PRIXLEAST16, (fm.dac_sample / (0x100 / FM_VOLUME_DIVIDER)) + 0x80);
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Panning");

				ImGui::TableNextColumn();
				ImGui::TextUnformatted(pannings[fm.channels[5].pan_left][fm.channels[5].pan_right]);

				ImGui::EndTable();
			}

			ImGui::TableNextColumn();
			ImGui::SeparatorText("Timers");

			if (ImGui::BeginTable("Timer Table", 3, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Property");
				ImGui::TableSetupColumn("Timer A");
				ImGui::TableSetupColumn("Timer B");
				ImGui::TableHeadersRow();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Enabled");

				for (cc_u8f i = 0; i < CC_COUNT_OF(fm.timers); ++i)
				{
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(fm.timers[i].enabled ? "Yes" : "No");
				}

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Cached Counter");

				ImGui::PushFont(monospace_font);
				for (cc_u8f i = 0; i < CC_COUNT_OF(fm.timers); ++i)
				{
					ImGui::TableNextColumn();
					ImGui::Text("0x%03" CC_PRIXLEAST32, CC_DIVIDE_CEILING(fm.timers[i].value, FM_SAMPLE_RATE_DIVIDER) - 1);
				}
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Active Counter");

				ImGui::PushFont(monospace_font);
				for (cc_u8f i = 0; i < CC_COUNT_OF(fm.timers); ++i)
				{
					ImGui::TableNextColumn();
					ImGui::Text("0x%03" CC_PRIXLEAST32, CC_DIVIDE_CEILING(fm.timers[i].counter, FM_SAMPLE_RATE_DIVIDER) - 1);
				}
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Flag");

				for (cc_u8f i = 0; i < CC_COUNT_OF(fm.timers); ++i)
				{
					ImGui::TableNextColumn();
					ImGui::TextUnformatted((fm.status & (1 << i)) != 0 ? "Set" : "Cleared");
				}

				ImGui::EndTable();
			}

			ImGui::TableNextColumn();
			ImGui::SeparatorText("Other");

			if (ImGui::BeginTable("Other Table", 2, ImGuiTableFlags_Borders))
			{
				ImGui::TableSetupColumn("Property");
				ImGui::TableSetupColumn("Value");
				ImGui::TableHeadersRow();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Latched Address");

				ImGui::PushFont(monospace_font);
				ImGui::TableNextColumn();
				ImGui::Text("0x%02" CC_PRIXLEAST8, fm.address);
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Latched Port");

				ImGui::PushFont(monospace_font);
				ImGui::TableNextColumn();
				ImGui::Text("%" CC_PRIXLEAST8, fm.port / 3);
				ImGui::PopFont();

				ImGui::TableNextColumn();
				ImGui::TextUnformatted("Status");

				ImGui::PushFont(monospace_font);
				ImGui::TableNextColumn();
				ImGui::Text("0x%02" CC_PRIXLEAST8, fm.status);
				ImGui::PopFont();

				ImGui::EndTable();
			}

			ImGui::EndTable();
		}
	}

	ImGui::End();
}
