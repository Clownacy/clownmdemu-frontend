#include "debug-cpu-m68k.h"

Debug::CPU::M68k::M68k(const Clown68000_State &state)
{
	setLayout(&layout);

	layout.addWidget(&data_registers.group_box);
	layout.addWidget(&address_registers.group_box);
	layout.addWidget(&misc_registers.group_box);

	StateChanged(state);
}

void Debug::CPU::M68k::StateChanged(const Clown68000_State &state)
{
	const auto RegisterToString = [](const int digits, const QString &label, const cc_u32f value)
	{
		return QStringLiteral("%1:%2").arg(label).arg(value, digits, 0x10, '0').toUpper();
	};

	const auto WordRegisterToString = [&](const QString &label, const cc_u32f value)
	{
		return RegisterToString(4, label, value);
	};

	const auto LongRegisterToString = [&](const QString &label, const cc_u32f value)
	{
		return RegisterToString(8, label, value);
	};

	const auto DoRegisters = [&](LabelGrid<4, 2> &label_grid, const QChar prefix, const cc_u32l (&registers)[8])
	{
		for (std::size_t i = 0; i < std::size(registers); ++i)
		{
			const auto x = i % 4;
			const auto y = i / 4;
			label_grid.labels[y][x].setText(LongRegisterToString(QStringLiteral(" %1%2").arg(prefix).arg(i), registers[i]));
		}
	};

	DoRegisters(data_registers, 'D', state.data_registers);
	DoRegisters(address_registers, 'A', state.address_registers);

	misc_registers.labels[0][0].setText(LongRegisterToString(" PC", state.program_counter));
	misc_registers.labels[0][1].setText(WordRegisterToString(" SR", state.status_register));

	// Show the register that is not currently address register 7.
	if ((state.status_register & 0x2000) != 0)
		misc_registers.labels[0][3].setText(LongRegisterToString("USP", state.user_stack_pointer));
	else
		misc_registers.labels[0][3].setText(LongRegisterToString("SSP", state.supervisor_stack_pointer));
}
