#include "disassembler.h"

#include <array>
#include <string>

#include "../common/core/clown68000/disassembler/disassembler.h"
#include "../libraries/imgui/imgui.h"

#include "../emulator-instance.h"
#include "../frontend.h"

static unsigned int address;
static int current_memory;
static std::string assembly;

static long ReadCallback(void* const user_data)
{
	const EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);
	const ClownMDEmu_State &clownmdemu = emulator->CurrentState().clownmdemu;

	long value;

	switch (current_memory)
	{
		default:
		case 0:
		{
			// ROM
			const std::vector<unsigned char> &rom_buffer = emulator->GetROMBuffer();

			if (rom_buffer.empty())
			{
				value = 0;
			}
			else
			{
				address %= rom_buffer.size();
				value = (rom_buffer[address + 0] << 8) | rom_buffer[address + 1];
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

static void PrintCallback(void* /*const user_data*/, const char* const string)
{
	assembly += string;
	assembly += '\n';
}

void Disassembler::DisplayInternal()
{
	static const std::array<const char*, 6> memories = {"ROM", "WORK-RAM", "PRG-RAM", "WORD-RAM (1M) Bank 1", "WORD-RAM (1M) Bank 2", "WORD-RAM (2M)"};

	ImGui::Combo("Memory", &current_memory, memories.data(), memories.size());

	static int address_imgui;

	ImGui::InputInt("Address", &address_imgui, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

	if (ImGui::Button("Disassemble"))
	{
		assembly.clear();

		address = address_imgui;
		Clown68000_Disassemble(address, 0x1000, ReadCallback, PrintCallback, &*Frontend::emulator);

		if (assembly[assembly.length() - 1] == '\n')
			assembly.pop_back();
	}

	ImGui::PushFont(GetMonospaceFont());
	ImGui::InputTextMultiline("##code", &assembly[0], assembly.length(), ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
	ImGui::PopFont();
}
