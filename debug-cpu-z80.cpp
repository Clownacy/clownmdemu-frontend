#include "debug-cpu-z80.h"

Debug::CPU::Z80::Z80(const Z80_State &state)
{
	DefaultSetup(regular_registers);
	DefaultSetup(shadow_registers);
	DefaultSetup(misc_registers);

	StateChanged(state);
}

void Debug::CPU::Z80::StateChanged(const Z80_State &state)
{
	regular_registers.labels[0][0].setText(ByteRegisterToString("A", state.a));
	regular_registers.labels[0][1].setText(ByteRegisterToString("B", state.b));
	regular_registers.labels[0][2].setText(ByteRegisterToString("C", state.c));
	regular_registers.labels[0][3].setText(ByteRegisterToString("D", state.d));
	regular_registers.labels[0][4].setText(ByteRegisterToString("E", state.e));
	regular_registers.labels[0][5].setText(ByteRegisterToString("F", state.f));
	regular_registers.labels[0][6].setText(ByteRegisterToString("H", state.h));
	regular_registers.labels[0][7].setText(ByteRegisterToString("L", state.l));

	shadow_registers.labels[0][0].setText(ByteRegisterToString("A", state.a_));
	shadow_registers.labels[0][1].setText(ByteRegisterToString("B", state.b_));
	shadow_registers.labels[0][2].setText(ByteRegisterToString("C", state.c_));
	shadow_registers.labels[0][3].setText(ByteRegisterToString("D", state.d_));
	shadow_registers.labels[0][4].setText(ByteRegisterToString("E", state.e_));
	shadow_registers.labels[0][5].setText(ByteRegisterToString("F", state.f_));
	shadow_registers.labels[0][6].setText(ByteRegisterToString("H", state.h_));
	shadow_registers.labels[0][7].setText(ByteRegisterToString("L", state.l_));

	misc_registers.labels[0][0].setText(WordRegisterToString("IX", static_cast<cc_u16f>(state.ixh) << 8 | state.ixl));
	misc_registers.labels[0][1].setText(WordRegisterToString("IY", static_cast<cc_u16f>(state.iyh) << 8 | state.iyl));
	misc_registers.labels[0][2].setText(WordRegisterToString("PC", state.program_counter));
	misc_registers.labels[0][3].setText(WordRegisterToString("SP", state.stack_pointer));
}
