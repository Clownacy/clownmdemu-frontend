#include "disassembler.h"

#include "clownmdemu-frontend-common/clownmdemu/clown68000/disassembler/disassembler.h"

static unsigned int address;
static int current_memory;

static long ReadCallback(void* const user_data)
{
	EmulatorInstance* const emulator = static_cast<EmulatorInstance*>(user_data);

	long value;

	switch (current_memory)
	{
		default:
		case 0:
			if (emulator->rom_buffer_size == 0)
			{
				value = 0;
			}
			else
			{
				address %= emulator->rom_buffer_size;
				value = (emulator->rom_buffer[address + 0] << 8) | emulator->rom_buffer[address + 1];
			}

			break;

		case 1:
			address %= CC_COUNT_OF(emulator->state->clownmdemu.m68k_ram) * 2;
			value = emulator->state->clownmdemu.m68k_ram[address / 2];
			break;

		case 2:
			address %= CC_COUNT_OF(emulator->state->clownmdemu.word_ram) * 2;
			value = emulator->state->clownmdemu.word_ram[address / 2];
			break;

		case 3:
			address %= CC_COUNT_OF(emulator->state->clownmdemu.prg_ram) * 2;
			value = emulator->state->clownmdemu.prg_ram[address / 2];
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
		static const char* const memories[] = {"ROM", "WORK-RAM", "WORD-RAM", "PRG-RAM"};

		ImGui::Combo("Memory", &current_memory, memories, CC_COUNT_OF(memories));

		static int address_master, address_imgui;

		bool update = false;

		update |= ImGui::InputInt("Address", &address_imgui, 0, 0, ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue);
		update |= ImGui::Button("Disassemble");

		if (update)
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