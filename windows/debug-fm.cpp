#include "debug-fm.h"

#include <array>
#include <cstddef>

#include "../frontend.h"

void DebugFM::Registers::DisplayInternal()
{
	const std::array<std::array<const char*, 2>, 2> pannings = {{
		{"Mute", "R"},
		{"L", "L+R"}
	}};

	const auto &fm = Frontend::emulator->GetFMState();
	const auto monospace_font = GetMonospaceFont();

	ImGui::SeparatorText("FM Channels");

	if (ImGui::BeginTable("FM Channel Table", 1 + std::size(fm.channels), ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame))
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
		ImGui::TextUnformatted("Frequency");

		ImGui::PushFont(monospace_font);
		for (const auto &channel : fm.channels)
		{
			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:04X}", channel.state.operators[0].phase.f_number_and_block);
		}
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Feedback");

		ImGui::PushFont(monospace_font);
		for (const auto &channel : fm.channels)
		{
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", 9 - channel.state.feedback_divisor);
		}
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Algorithm");

		ImGui::PushFont(monospace_font);
		for (const auto &channel : fm.channels)
		{
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", channel.state.algorithm);
			DoToolTip(
				[&]() -> std::string_view
				{
					switch (channel.state.algorithm)
					{
						case 0: return "Four serial connection mode.";
						case 1: return "Three double modulation serial connection mode.";
						case 2: return "Double modulation mode (1).";
						case 3: return "Double modulation mode (2).";
						case 4: return "Two serial connection and two parallel modes.";
						case 5: return "Common modulation 3 parallel mode.";
						case 6: return "Two serial connection + two sine mode.";
						case 7: return "Four parallel sine synthesis mode.";
						default: return "ERROR";
					}
				}()
			);
		}
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Panning");

		for (const auto &channel : fm.channels)
		{
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(pannings[channel.pan_left][channel.pan_right]);
		}

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Amplitude Modulation Sensitivity");

		ImGui::PushFont(monospace_font);
		for (const auto &channel : fm.channels)
		{
			ImGui::TableNextColumn();
			switch (channel.state.amplitude_modulation_shift)
			{
				case 0:
					ImGui::TextUnformatted("3");
					break;
				case 1:
					ImGui::TextUnformatted("2");
					break;
				case 3:
					ImGui::TextUnformatted("1");
					break;
				case 7:
					ImGui::TextUnformatted("0");
					break;
				default:
					ImGui::TextUnformatted("ERROR");
					break;
			}
		}
		ImGui::PopFont();

		ImGui::TableNextColumn();
		ImGui::TextUnformatted("Phase Modulation Sensitivity");

		ImGui::PushFont(monospace_font);
		for (const auto &channel : fm.channels)
		{
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", channel.state.phase_modulation_sensitivity);
		}
		ImGui::PopFont();

		ImGui::EndTable();
	}

	ImGui::SeparatorText("FM Operators");

	if (ImGui::BeginTabBar("FM Channels Tab Bar"))
	{
		char window_name_buffer[] = "FM1";

		for (std::size_t channel_index = 0; channel_index != std::size(fm.channels); ++channel_index)
		{
			const auto &channel = fm.channels[channel_index];

			window_name_buffer[2] = '1' + channel_index;

			if (ImGui::BeginTabItem(window_name_buffer))
			{
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

					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(op.key_on ? "On" : "Off");
					}

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Detune");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("{}", op.phase.detune);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Multiplier");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("{}", op.phase.multiplier / 2);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Total Level");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("0x{:02X}", op.total_level >> 3);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Key Scale");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("{}", 3 - op.key_scale);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Attack Rate");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("0x{:02X}", op.rates[FM_OPERATOR_ENVELOPE_MODE_ATTACK]);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Decay Rate");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("0x{:02X}", op.rates[FM_OPERATOR_ENVELOPE_MODE_DECAY]);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Sustain Rate");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("0x{:02X}", op.rates[FM_OPERATOR_ENVELOPE_MODE_SUSTAIN]);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Release Rate");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("0x{:X}", op.rates[FM_OPERATOR_ENVELOPE_MODE_RELEASE] >> 1);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Sustain Level");

					ImGui::PushFont(monospace_font);
					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextFormatted("0x{:X}", (op.sustain_level / 0x20) & 0xF);
					}
					ImGui::PopFont();

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted("Amplitude Modulation");

					for (const auto &op : channel.state.operators)
					{
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(op.amplitude_modulation_on ? "On" : "Off");
					}

					const auto DoSSGEG = [&](const char* const label, const std::function<bool(const FM_Operator &op)> &function)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted(label);

						ImGui::PushFont(monospace_font);
						for (const auto &op : channel.state.operators)
						{
							ImGui::TableNextColumn();
							ImGui::TextUnformatted(function(op) ? "Yes" : "No");
						}
						ImGui::PopFont();
					};

					DoSSGEG("SSG-EG Enabled", [](const FM_Operator &op) { return op.ssgeg.enabled; });
					DoSSGEG("SSG-EG Attack", [](const FM_Operator &op) { return op.ssgeg.attack; });
					DoSSGEG("SSG-EG Alternate", [](const FM_Operator &op) { return op.ssgeg.alternate; });
					DoSSGEG("SSG-EG Hold", [](const FM_Operator &op) { return op.ssgeg.hold; });

					if (channel_index == 2)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn();
						ImGui::TextUnformatted("Frequency");

						ImGui::PushFont(monospace_font);
						for (const auto &frequency : fm.channel_3_metadata.frequencies)
						{
							ImGui::TableNextColumn();
							ImGui::TextFormatted("0x{:04X}", frequency);
						}
						ImGui::PopFont();
					}

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
			ImGui::TextFormatted("0x{:03X}", fm.dac_sample);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Test");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted(fm.dac_test ? "On" : "Off");

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

			for (const auto &timer : fm.timers)
			{
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(timer.enabled ? "Yes" : "No");
			}

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Cached Counter");

			ImGui::PushFont(monospace_font);
			for (const auto &timer : fm.timers)
			{
				ImGui::TableNextColumn();
				ImGui::TextFormatted("0x{:03X}", timer.value);
			}
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Active Counter");

			ImGui::PushFont(monospace_font);
			for (const auto &timer : fm.timers)
			{
				ImGui::TableNextColumn();
				ImGui::TextFormatted("0x{:03X}", timer.counter);
			}
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Flag");

			for (cc_u8f i = 0; i < std::size(fm.timers); ++i)
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
			ImGui::TextFormatted("0x{:02X}", fm.address);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Latched Port");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", fm.port / 3);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Status");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:02X}", fm.status);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("FM3 Mode");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(fm.channel_3_metadata.csm_mode_enabled ? "CSM" : fm.channel_3_metadata.per_operator_frequencies_enabled ? "Multifrequency" : "Normal");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Low Frequency Oscillator");
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(fm.lfo.enabled ? "Enabled" : "Disabled");

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("LFO Frequency");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::TextFormatted("{}", fm.lfo.frequency);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Frequency Cache");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:02X}", fm.cached_upper_frequency_bits);
			ImGui::PopFont();

			ImGui::TableNextColumn();
			ImGui::TextUnformatted("Multi-Frequency Cache");

			ImGui::PushFont(monospace_font);
			ImGui::TableNextColumn();
			ImGui::TextFormatted("0x{:02X}", fm.cached_upper_frequency_bits_fm3_multi_frequency);
			ImGui::PopFont();

			ImGui::EndTable();
		}

		ImGui::EndTable();
	}
}
