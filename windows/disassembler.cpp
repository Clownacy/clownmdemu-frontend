#include "disassembler.h"

#include <array>
#include <string>

#include <SDL3/SDL.h>

#include "../common/core/clown68000/disassembler/disassembler.h"
#include "../common/core/clownz80/disassembler.h"
#include "../libraries/imgui/imgui.h"

#include "../emulator-instance.h"
#include "../frontend.h"

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

cc_u16f Disassembler::ReadCallback16Bit()
{
	const auto value = ReadMemory();
	address += 2;
	return value;
}

cc_u8f Disassembler::ReadCallback8Bit()
{
	auto value = ReadMemory();
	if (address % 2 == 0)
		value >>= 8;
	address += 1;
	return value & 0xFF;
}

void Disassembler::PrintCallback(const char* const string)
{
	assembly += string;
}

void Disassembler::DisplayInternal()
{
	static const std::array<const char*, 7> memories = {"ROM", "WORK-RAM", "SOUND-RAM", "PRG-RAM", "WORD-RAM (1M) Bank 1", "WORD-RAM (1M) Bank 2", "WORD-RAM (2M)"};

	ImGui::Combo("Memory", &current_memory, memories.data(), memories.size());

	ImGui::InputInt("Address", &address_imgui, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

	if (ImGui::Button("Disassemble"))
	{
		assembly.clear();

		address = address_imgui;
		Disassemble(address);

		if (assembly[assembly.length() - 1] == '\n')
			assembly.pop_back();
	}

	ImGui::PushFont(GetMonospaceFont());
	ImGui::InputTextMultiline("##code", &assembly[0], assembly.length() + 1, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly); // '+1' to count the null character.
	ImGui::PopFont();
}

void Disassembler68000::Disassemble(const unsigned long address)
{
		const auto ReadCallback = [](void* const user_data) -> long
		{
			return static_cast<Disassembler68000*>(user_data)->ReadCallback16Bit();
		};

		const auto PrintCallback = [](void* const user_data, const char* const string)
		{
			static_cast<Disassembler68000*>(user_data)->PrintCallback(string);
			static_cast<Disassembler68000*>(user_data)->PrintCallback("\n");
		};

		Clown68000_Disassemble(address, 0x1000, ReadCallback, PrintCallback, this);
}

void DisassemblerZ80::Disassemble(const unsigned long address)
{
		const auto ReadCallback = [](void* const user_data) -> unsigned char
		{
			return static_cast<DisassemblerZ80*>(user_data)->ReadCallback8Bit();
		};

		const auto PrintCallback = [](void* const user_data, const char* const format, ...)
		{
			va_list args;
			char *string;

			va_start(args, format);
			SDL_vasprintf(&string, format, args);
			va_end(args);

			static_cast<DisassemblerZ80*>(user_data)->PrintCallback(string);
			SDL_free(string);
		};

		ClownZ80_Disassemble(address, 0x1000, ReadCallback, PrintCallback, this);
}
