#ifndef DEBUG_CPU_H
#define DEBUG_CPU_H

#include <QDialog>

#include "ui_debug-cpu.h"

class DebugCPU : public QDialog
{
	Q_OBJECT

public:
	explicit DebugCPU(QWidget *parent = nullptr);

private:
	Ui::DebugCPU ui;
};

#endif // DEBUG_CPU_H
