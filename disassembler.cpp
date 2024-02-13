#include "disassembler.h"

#include "clownmdemu-frontend-common/clownmdemu/clown68000/disassembler/disassembler.h"

static unsigned int address;
static int current_memory;

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
			address %= CC_COUNT_OF(clownmdemu.m68k_ram) * 2;
			value = clownmdemu.m68k_ram[address / 2];
			break;

		case 2:
			// PRG-RAM
			address %= CC_COUNT_OF(clownmdemu.prg_ram) * 2;
			value = clownmdemu.prg_ram[address / 2];
			break;

		case 3:
			// WORD-RAM (1M) Bank 1
			address %= CC_COUNT_OF(clownmdemu.word_ram) * 2 / 2;
			value = clownmdemu.word_ram[address / 2 * 2 + 0];
			break;

		case 4:
			// WORD-RAM (1M) Bank 2
			address %= CC_COUNT_OF(clownmdemu.word_ram) * 2 / 2;
			value = clownmdemu.word_ram[address / 2 * 2 + 1];
			break;

		case 5:
			// WORD-RAM (2M)
			address %= CC_COUNT_OF(clownmdemu.word_ram) * 2;
			value = clownmdemu.word_ram[address / 2];
			break;
	}

	address += 2;

	return value;
}

static void PrintCallback(void* /*const user_data*/, const char* const string)
{
	ImGui::TextUnformatted(string);
}

void Disassembler(bool &open, const EmulatorInstance &emulator, ImFont* const monospace_font)
{
	ImGui::SetNextWindowSize(ImVec2(580, 620), ImGuiCond_FirstUseEver);

	if (ImGui::Begin("68000 Disassembler", &open))
	{
		static const std::array<const char*, 6> memories = {"ROM", "WORK-RAM", "PRG-RAM", "WORD-RAM (1M) Bank 1", "WORD-RAM (1M) Bank 2", "WORD-RAM (2M)"};

		ImGui::Combo("Memory", &current_memory, memories.data(), memories.size());

		static int address_master, address_imgui;

		ImGui::InputInt("Address", &address_imgui, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);

		if (ImGui::Button("Disassemble"))
			address_master = address_imgui;

		ImGui::Separator();

		if (ImGui::BeginChild("#code"))
		{
			ImGui::PushFont(monospace_font);

			address = address_master;
			Clown68000_Disassemble(address, 0x1000, ReadCallback, PrintCallback, &emulator);

			ImGui::PopFont();
		}

		ImGui::EndChild();
	}

	ImGui::End();
}
