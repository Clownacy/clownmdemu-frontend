#include "disassembler.h"

#include <array>
#include <string>

#include "../common/core/clown68000/disassembler/disassembler.h"
#include "../libraries/imgui/imgui.h"

#include "../emulator-instance.h"
#include "../frontend.h"

long Disassembler::ReadCallback()
{
	const ClownMDEmu_State &clownmdemu = Frontend::emulator->CurrentState().clownmdemu;

	long value;

	switch (current_memory)
	{
		default:
		case 0:
		{
			// ROM
			const auto &rom_buffer = Frontend::emulator->GetROMBuffer();

			if (rom_buffer.empty())
			{
				value = 0;
			}
			else
			{
				address %= std::size(rom_buffer) * 2;
				value = rom_buffer[address / 2];
			}

			break;
		}

		case 1:
			// WORK-RAM
			address %= std::size(clownmdemu.m68k.ram) * 2;
			value = clownmdemu.m68k.ram[address / 2];
			break;

		case 2:
			// PRG-RAM
			address %= std::size(clownmdemu.mega_cd.prg_ram.buffer) * 2;
			value = clownmdemu.mega_cd.prg_ram.buffer[address / 2];
			break;

		case 3:
			// WORD-RAM (1M) Bank 1
			address %= std::size(clownmdemu.mega_cd.word_ram.buffer) * 2 / 2;
			value = clownmdemu.mega_cd.word_ram.buffer[address / 2 * 2 + 0];
			break;

		case 4:
			// WORD-RAM (1M) Bank 2
			address %= std::size(clownmdemu.mega_cd.word_ram.buffer) * 2 / 2;
			value = clownmdemu.mega_cd.word_ram.buffer[address / 2 * 2 + 1];
			break;

		case 5:
			// WORD-RAM (2M)
			address %= std::size(clownmdemu.mega_cd.word_ram.buffer) * 2;
			value = clownmdemu.mega_cd.word_ram.buffer[address / 2];
			break;
	}

	address += 2;

	return value;
}

void Disassembler::PrintCallback(const char* const string)
{
	assembly += string;
	assembly += '\n';
}

void Disassembler::DisplayInternal()
{
	static const std::array<const char*, 6> memories = {"ROM", "WORK-RAM", "PRG-RAM", "WORD-RAM (1M) Bank 1", "WORD-RAM (1M) Bank 2", "WORD-RAM (2M)"};

	ImGui::Combo("Memory", &current_memory, memories.data(), memories.size());

	ImGui::InputInt("Address", &address, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

	if (ImGui::Button("Disassemble"))
	{
		const auto ReadCallback = [](void* const user_data)
		{
			return static_cast<Disassembler*>(user_data)->ReadCallback();
		};

		const auto PrintCallback = [](void* const user_data, const char* const string)
		{
			static_cast<Disassembler*>(user_data)->PrintCallback(string);
		};

		assembly.clear();

		Clown68000_Disassemble(address, 0x1000, ReadCallback, PrintCallback, this);

		if (assembly[assembly.length() - 1] == '\n')
			assembly.pop_back();
	}

	ImGui::PushFont(GetMonospaceFont());
	ImGui::InputTextMultiline("##code", &assembly[0], assembly.length() + 1, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly); // '+1' to count the null character.
	ImGui::PopFont();
}
