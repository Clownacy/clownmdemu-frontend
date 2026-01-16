#include "debug-cpu.h"

Dialogs::Debug::CPU::CPU(const Widgets::Emulator &emulator, QWidget* const parent)
	: Base(parent)
	, emulator(emulator)
	, main_cpu(emulator.GetM68kState())
	, sub_cpu(emulator.GetSubM68kState())
	, sound_cpu(emulator.GetZ80State())
{
	ui.setupUi(this);

	layout()->setSizeConstraint(QLayout::SetFixedSize);

	ui.main_cpu->layout()->addWidget(&main_cpu);
	ui.sub_cpu->layout()->addWidget(&sub_cpu);
	ui.sound_cpu->layout()->addWidget(&sound_cpu);
}

void Dialogs::Debug::CPU::StateChanged()
{
	main_cpu.StateChanged(emulator.GetM68kState());
	sub_cpu.StateChanged(emulator.GetSubM68kState());
	sound_cpu.StateChanged(emulator.GetZ80State());
}
