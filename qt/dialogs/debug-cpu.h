#ifndef DIALOGS_DEBUG_CPU_H
#define DIALOGS_DEBUG_CPU_H

#include <QDialog>

#include "widgets/debug-cpu-m68k.h"
#include "widgets/debug-cpu-z80.h"
#include "../emulator-widget.h"
#include "ui_debug-cpu.h"

namespace Dialogs::Debug
{
	class CPU : public QDialog
	{
		Q_OBJECT

	protected:
		using Base = QDialog;

		Ui::DebugCPU ui;
		const EmulatorWidget &emulator;
		Widgets::Debug::CPU::M68k main_cpu, sub_cpu;
		Widgets::Debug::CPU::Z80 sound_cpu;

	public:
		explicit CPU(const EmulatorWidget &emulator, QWidget *parent = nullptr);

	public slots:
		void StateChanged();
	};
}

#endif // DIALOGS_DEBUG_CPU_H
