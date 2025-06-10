#include "debug-cpu.h"

Debug::CPU::Dialog::Dialog(const Emulator &emulator, QWidget* const parent)
	: Base(parent)
	, emulator(emulator)
	, main_cpu(emulator.GetState().m68k.state)
	, sub_cpu(emulator.GetState().mega_cd.m68k.state)
	, sound_cpu(emulator.GetState().z80.state)
{
	ui.setupUi(this);

	layout()->setSizeConstraint(QLayout::SetFixedSize);

	ui.main_cpu->layout()->addWidget(&main_cpu);
	ui.sub_cpu->layout()->addWidget(&sub_cpu);
	ui.sound_cpu->layout()->addWidget(&sound_cpu);
}

void Debug::CPU::Dialog::StateChanged()
{
	main_cpu.StateChanged(emulator.GetState().m68k.state);
	sub_cpu.StateChanged(emulator.GetState().mega_cd.m68k.state);
	sound_cpu.StateChanged(emulator.GetState().z80.state);
}
