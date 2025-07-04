#ifndef DEBUG_CPU_H
#define DEBUG_CPU_H

#include <QDialog>

#include "debug-cpu-m68k.h"
#include "debug-cpu-z80.h"
#include "emulator.h"
#include "ui_debug-cpu.h"

namespace Debug
{
	namespace CPU
	{
		class Dialog : public QDialog
		{
			Q_OBJECT

		protected:
			using Base = QDialog;

			Ui::DebugCPU ui;
			const Emulator &emulator;
			M68k main_cpu, sub_cpu;
			Z80 sound_cpu;

		public:
			explicit Dialog(const Emulator &emulator, QWidget *parent = nullptr);

		public slots:
			void StateChanged();
		};
	}
}

#endif // DEBUG_CPU_H
