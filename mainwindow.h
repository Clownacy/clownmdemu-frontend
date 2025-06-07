#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "debug-cpu.h"
#include "emulator-widget.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private:
	Ui::MainWindow *ui;
	DebugCPU debug_cpu = DebugCPU(this);
	EmulatorWidget emulator = EmulatorWidget(this);
};
#endif // MAINWINDOW_H
