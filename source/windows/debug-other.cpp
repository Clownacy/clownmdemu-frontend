#include "debug-other.h"

#include "../frontend.h"

void DebugOther::DisplayInternal()
{
	const auto monospace_font = GetMonospaceFont();
	const ClownMDEmu_State &clownmdemu_state = frontend->emulator->GetState();

	DoTable("##Settings", [&]()
		{
			DoProperty(monospace_font, "Z80 Bank", "0x{:06X}-0x{:06X}", clownmdemu_state.z80.bank * 0x8000, (clownmdemu_state.z80.bank + 1) * 0x8000);
			DoProperty(nullptr, "Main 68000 Has Z80 Bus", "{}", clownmdemu_state.z80.bus_requested ? "Yes" : "No");
			DoProperty(nullptr, "Z80 Reset Held", "{}", clownmdemu_state.z80.reset_held ? "Yes" : "No");
			DoProperty(nullptr, "Main 68000 Has Sub 68000 Bus", "{}", clownmdemu_state.mega_cd.m68k.bus_requested ? "Yes" : "No");
			DoProperty(nullptr, "Sub 68000 Reset", "{}", clownmdemu_state.mega_cd.m68k.reset_held ? "Yes" : "No");
			DoProperty(monospace_font, "PRG-RAM Bank", "0x{:05X}-0x{:05X}", clownmdemu_state.mega_cd.prg_ram.bank * 0x20000, (clownmdemu_state.mega_cd.prg_ram.bank + 1) * 0x20000);
			DoProperty(monospace_font, "PRG-RAM Write-Protect", "0x00000-0x{:05X}", clownmdemu_state.mega_cd.prg_ram.write_protect * 0x200);
			DoProperty(nullptr, "WORD-RAM Mode", "{}", clownmdemu_state.mega_cd.word_ram.in_1m_mode ? "1M" : "2M");
			DoProperty(nullptr, "DMNA Bit", "{}", clownmdemu_state.mega_cd.word_ram.dmna ? "Set" : "Clear");
			DoProperty(nullptr, "RET Bit", "{}", clownmdemu_state.mega_cd.word_ram.ret ? "Set" : "Clear");
			DoProperty(nullptr, "Cartridge Inserted", "{}", clownmdemu_state.cartridge_inserted ? "Yes" : "No");
			DoProperty(nullptr, "CD Inserted", "{}", clownmdemu_state.mega_cd.cd_inserted ? "Yes" : "No");
			DoProperty(monospace_font, "68000 Communication Flag", "0x{:04X}", clownmdemu_state.mega_cd.communication.flag);

			DoProperty(monospace_font, "68000 Communication Command",
				[&]()
				{
					for (std::size_t i = 0; i < std::size(clownmdemu_state.mega_cd.communication.command); i += 2)
						ImGui::TextFormatted("0x{:04X} 0x{:04X}", clownmdemu_state.mega_cd.communication.command[i + 0], clownmdemu_state.mega_cd.communication.command[i + 1]);
				}
			);

			DoProperty(monospace_font, "68000 Communication Status",
				[&]()
				{
					for (std::size_t i = 0; i < std::size(clownmdemu_state.mega_cd.communication.status); i += 2)
						ImGui::TextFormatted("0x{:04X} 0x{:04X}", clownmdemu_state.mega_cd.communication.status[i + 0], clownmdemu_state.mega_cd.communication.status[i + 1]);
				}
			);

			DoProperty(nullptr, "SUB-CPU Graphics Interrupt", "{}", clownmdemu_state.mega_cd.irq.enabled[0] ? "Enabled" : "Disabled");
			DoProperty(nullptr, "SUB-CPU Mega Drive Interrupt", "{}", clownmdemu_state.mega_cd.irq.enabled[1] ? "Enabled" : "Disabled");
			DoProperty(nullptr, "SUB-CPU Timer Interrupt", "{}", clownmdemu_state.mega_cd.irq.enabled[2] ? "Enabled" : "Disabled");
			DoProperty(nullptr, "SUB-CPU CDD Interrupt", "{}", clownmdemu_state.mega_cd.irq.enabled[3] ? "Enabled" : "Disabled");
			DoProperty(nullptr, "SUB-CPU CDC Interrupt", "{}", clownmdemu_state.mega_cd.irq.enabled[4] ? "Enabled" : "Disabled");
			DoProperty(nullptr, "SUB-CPU Sub-code Interrupt", "{}", clownmdemu_state.mega_cd.irq.enabled[5] ? "Enabled" : "Disabled");
		}
	);

	DoTable("Graphics Transformation",
		[&]()
		{
			const auto &graphics = clownmdemu_state.mega_cd.rotation;
			DoProperty(nullptr, "Stamp Size", "{} pixels", graphics.large_stamp ? "32x32" : "16x16");
			DoProperty(nullptr, "Stamp Map Size", "{} pixels", graphics.large_stamp_map ? "4096x4096" : "256x256");
			DoProperty(nullptr, "Repeating Stamp Map", "{}", graphics.repeating_stamp_map ? "Yes" : "No");
			DoProperty(nullptr, "Stamp Map Address", "0x{:X}", graphics.stamp_map_address * 4);
			DoProperty(nullptr, "Image Buffer Height in Tiles", "{}", graphics.image_buffer_height_in_tiles + 1);
			DoProperty(nullptr, "Image Buffer Address", "0x{:X}", graphics.image_buffer_address * 4);
			DoProperty(nullptr, "Image Buffer X Offset", "{}", graphics.image_buffer_x_offset);
			DoProperty(nullptr, "Image Buffer Y Offset", "{}", graphics.image_buffer_y_offset);
			DoProperty(nullptr, "Image Buffer Width", "{}", graphics.image_buffer_width);
			DoProperty(nullptr, "Image Buffer Height", "{}", graphics.image_buffer_height);
		}
	);
}
