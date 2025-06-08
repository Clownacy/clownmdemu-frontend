#include "debug-cpu-m68k.h"

Debug::CPU::M68k::M68k(const Clown68000_State &state)
{
	layout.addWidget(&data_registers.group_box);
	layout.addWidget(&address_registers.group_box);
	layout.addWidget(&misc_registers.group_box);

	StateChanged(state);
}

void Debug::CPU::M68k::StateChanged(const Clown68000_State &state)
{
	const auto DoRegisters = [](auto &label_grid, const QChar prefix, const cc_u32l (&registers)[8])
	{
		for (std::size_t i = 0; i < std::size(registers); ++i)
		{
			const auto x = i % label_grid.width;
			const auto y = i / label_grid.width;
			label_grid.labels[y][x].setText(LongRegisterToString(QStringLiteral("%1%2").arg(prefix).arg(i), registers[i]));
		}
	};

	DoRegisters(data_registers, 'D', state.data_registers);
	DoRegisters(address_registers, 'A', state.address_registers);

	misc_registers.labels[0][0].setText(LongRegisterToString("PC", state.program_counter));
	misc_registers.labels[0][1].setText(WordRegisterToString("SR", state.status_register));

	// Show the register that is not currently address register 7.
	if ((state.status_register & 0x2000) != 0)
		misc_registers.labels[0][2].setText(LongRegisterToString("USP", state.user_stack_pointer));
	else
		misc_registers.labels[0][2].setText(LongRegisterToString("SSP", state.supervisor_stack_pointer));
}
