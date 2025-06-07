#include "debug-cpu.h"
#include "ui_debug-cpu.h"

DebugCPU::DebugCPU(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::DebugCPU)
{
	ui->setupUi(this);
}

DebugCPU::~DebugCPU()
{
	delete ui;
}
