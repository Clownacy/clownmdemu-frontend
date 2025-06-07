#ifndef DEBUG_CPU_H
#define DEBUG_CPU_H

#include <QDialog>

#include "debug-cpu-m68k.h"
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

			Ui::DebugCPU ui;
			const Emulator &emulator;
			M68k main_cpu, sub_cpu;

		public:
			Dialog(const Emulator &emulator);

		public slots:
			void StateChanged();
		};
	}
}

#endif // DEBUG_CPU_H
