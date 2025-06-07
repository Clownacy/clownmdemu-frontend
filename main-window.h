#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "debug-cpu.h"
#include "emulator-widget.h"
#include "ui_main-window.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

private:
	Ui::MainWindow ui;
	DebugCPU debug_cpu = DebugCPU(this);
	EmulatorWidget emulator = EmulatorWidget(this);
};
#endif // MAINWINDOW_H
