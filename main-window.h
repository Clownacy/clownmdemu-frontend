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

		template<typename... Args>
		EmulatorStuff(QWidget* const parent, Args... args)
			: emulator(std::forward<Args>(args)..., parent)
			, debug_cpu(emulator, parent)
		{}
	};

	Ui::MainWindow ui;
	std::optional<EmulatorStuff> emulator_stuff;
	About about = About(this);
	QWidget *central_widget = nullptr;
};

#endif // MAINWINDOW_H
