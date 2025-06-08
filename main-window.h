#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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
	Ui::MainWindow ui;
	EmulatorWidget emulator = EmulatorWidget(this);
	About about = About(this);
	Debug::CPU::Dialog debug_cpu = Debug::CPU::Dialog(emulator, this);
};
#endif // MAINWINDOW_H
