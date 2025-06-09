#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <optional>

#include <QMainWindow>

#include "about.h"
#include "debug-cpu.h"
#include "emulator-widget.h"
#include "ui_main-window.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

private:
	struct EmulatorStuff
	{
		EmulatorWidget emulator;
		Debug::CPU::Dialog debug_cpu;

		EmulatorStuff(QWidget* const parent)
			: emulator(parent)
			, debug_cpu(emulator, parent)
		{}
	};

	Ui::MainWindow ui;
	std::optional<EmulatorStuff> emulator_stuff;
	About about = About(this);
};

#endif // MAINWINDOW_H
