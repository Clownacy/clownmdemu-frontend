#include "disassembler.h"

#include <array>
#include <string>

#include <SDL3/SDL.h>

#include "../common/core/clown68000/disassembler/disassembler.h"
#include "../common/core/clownz80/disassembler.h"
#include "../libraries/imgui/imgui.h"

#include "../emulator-instance.h"
#include "../frontend.h"

static constexpr unsigned int maximum_instructions = 0x1000;

cc_u16f Disassembler::ReadMemory()
{
	const ClownMDEmu_State &clownmdemu = Frontend::emulator->CurrentState().clownmdemu;

	cc_u16f value;

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
			// SOUND-RAM
			address %= std::size(clownmdemu.z80.ram);
			value = (static_cast<unsigned int>(clownmdemu.z80.ram[address / 2 * 2 + 0]) << 8) | clownmdemu.z80.ram[address / 2 * 2 + 1];
			break;

		case 3:
			// PRG-RAM
			address %= std::size(clownmdemu.mega_cd.prg_ram.buffer) * 2;
			value = clownmdemu.mega_cd.prg_ram.buffer[address / 2];
			break;

		case 4:
			// WORD-RAM (1M) Bank 1
			address %= std::size(clownmdemu.mega_cd.word_ram.buffer) * 2 / 2;
			value = clownmdemu.mega_cd.word_ram.buffer[address / 2 * 2 + 0];
			break;

		case 5:
			// WORD-RAM (1M) Bank 2
			address %= std::size(clownmdemu.mega_cd.word_ram.buffer) * 2 / 2;
			value = clownmdemu.mega_cd.word_ram.buffer[address / 2 * 2 + 1];
			break;

		case 6:
			// WORD-RAM (2M)
			address %= std::size(clownmdemu.mega_cd.word_ram.buffer) * 2;
			value = clownmdemu.mega_cd.word_ram.buffer[address / 2];
			break;
	}

	return value;
}

void Disassembler::PrintCallback(const char* const string)
{
	assembly += string;
}

long Disassembler::ReadCallback68000(void* const user_data)
{
	auto &disassembler = *static_cast<Disassembler*>(user_data);

	const auto value = disassembler.ReadMemory();
	disassembler.address += 2;
	return value;
}

void Disassembler::PrintCallback68000(void* const user_data, const char* const string)
{
	auto &disassembler = *static_cast<Disassembler*>(user_data);

	disassembler.PrintCallback(string);
	disassembler.PrintCallback("\n");
};

unsigned char Disassembler::ReadCallbackZ80(void* const user_data)
{
	auto &disassembler = *static_cast<Disassembler*>(user_data);

	auto value = disassembler.ReadMemory();
	if (disassembler.address % 2 == 0)
		value >>= 8;
	disassembler.address += 1;
	return value & 0xFF;
}

void Disassembler::PrintCallbackZ80(void* const user_data, const char* const format, ...)
{
	auto &disassembler = *static_cast<Disassembler*>(user_data);

	va_list args;
	char *string;

	va_start(args, format);
	SDL_vasprintf(&string, format, args);
	va_end(args);

	disassembler.PrintCallback(string);
	SDL_free(string);
};

void Disassembler::DisplayInternal()
{
	static const std::array<const char*, 2> cpus = {"68000", "Z80"};
	ImGui::Combo("CPU", &current_cpu, cpus.data(), cpus.size());

	static const std::array<const char*, 7> memories = {"ROM", "WORK-RAM", "SOUND-RAM", "PRG-RAM", "WORD-RAM (1M) Bank 1", "WORD-RAM (1M) Bank 2", "WORD-RAM (2M)"};
	ImGui::Combo("Memory", &current_memory, memories.data(), memories.size());

	ImGui::InputInt("Address", &address_imgui, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

	if (ImGui::Button("Disassemble"))
	{
		assembly.clear();
		address = address_imgui;

		switch (current_cpu)
		{
			case 0:
				// 68000
				Clown68000_Disassemble(address, maximum_instructions, ReadCallback68000, PrintCallback68000, this);
				break;

			case 1:
				// Z80
				ClownZ80_Disassemble(address, maximum_instructions, ReadCallbackZ80, PrintCallbackZ80, this);
				break;
		}

		if (assembly[assembly.length() - 1] == '\n')
			assembly.pop_back();
	}

	ImGui::PushFont(GetMonospaceFont());
	ImGui::InputTextMultiline("##code", &assembly[0], assembly.length() + 1, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly); // '+1' to count the null character.
	ImGui::PopFont();
}
