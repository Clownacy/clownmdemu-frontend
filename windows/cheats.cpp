#include "cheats.h"

#include <fmt/core.h>

#include "../libraries/imgui/misc/cpp/imgui_stdlib.h"

#include "../common/cheat.h"

Cheats::CodeSlot::CodeSlot(std::string code)
	: code(std::move(code))
{
	if (!Cheat_DecodeCheat(&decoded_cheat, this->code.c_str()))
		status = Status::Invalid;
	else
		status = Status::Pending;
}

void Cheats::DisplayInternal(EmulatorInstance &emulator)
{
	if (ImGui::BeginTable("Cheats", 2, ImGuiTableFlags_Borders))
	{
		ImGui::TableSetupColumn("Code");
		ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60 * GetWindow().GetDPIScale());
		ImGui::TableHeadersRow();

		const auto &DisplayCode = [](const char* const hint, std::string &code)
		{
			ImGui::TableNextColumn();
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::InputTextWithHint("##Code", hint, &code, ImGuiInputTextFlags_CharsUppercase);

			return ImGui::IsItemDeactivatedAfterEdit();
		};

		for (auto &slot : codes)
		{
			ImGui::PushID(&slot);

			ImGui::PushFont(GetMonospaceFont());

			if (DisplayCode("This cheat will be deleted.", slot.code))
				slot = CodeSlot(slot.code);

			if (slot.status != CodeSlot::Status::Invalid)
			{
				if (ImGui::BeginItemTooltip())
				{
					ImGui::TextFormatted("Address: {:06X}\nValue:     {:04X}", slot.decoded_cheat.address, slot.decoded_cheat.value);
					ImGui::EndTooltip();
				}
			}

			ImGui::PopFont();

			ImGui::TableNextColumn();
			switch (slot.status)
			{
				case CodeSlot::Status::Invalid:
					ImGui::TextUnformatted("Invalid");
					break;
				case CodeSlot::Status::Error:
					ImGui::TextUnformatted("Error");
					break;
				case CodeSlot::Status::Pending:
					ImGui::TextUnformatted("Pending");
					break;
				case CodeSlot::Status::Applied:
					ImGui::TextUnformatted("Applied");
					break;
			}

			ImGui::PopID();
		}

		ImGui::PushFont(GetMonospaceFont());

		std::string new_code;
		if (DisplayCode("Input cheat code here.", new_code))
			if (!new_code.empty())
				codes.emplace_back(new_code);

		ImGui::PopFont();

		ImGui::EndTable();
	}

	if (ImGui::Button("Apply"))
	{
		// Remove empty codes.
		codes.remove_if([](const auto &slot){return slot.code.empty();});

		emulator.ResetCheats();

		unsigned int index = 0;
		for (auto &slot : codes)
		{
			if (slot.status != CodeSlot::Status::Invalid)
			{
				if (emulator.AddDecodedCheat(index++, true, slot.decoded_cheat))
					slot.status = CodeSlot::Status::Applied;
				else
					slot.status = CodeSlot::Status::Error;
			}
		}
	}
}
