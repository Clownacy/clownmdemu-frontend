#include "main-window.h"

#include <QFileDialog>
#include <QLayout>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	setCentralWidget(&emulator);

	connect(ui.actionLoad_Cartridge_Software, &QAction::triggered, this,
		[&]()
		{
			QFileDialog::getOpenFileContent("Mega Drive Cartridge Software (*.bin *.md *.gen)",
				[&]([[maybe_unused]] const QString &file_name, const QByteArray &file_contents)
				{
					emulator.LoadCartridgeSoftware(file_contents);
				}, this
			);
		}
	);

	connect(ui.actionCPUs, &QAction::triggered, this, [&](){debug_cpu.show();});

	connect(&emulator, &EmulatorWidget::NewFrame, &debug_cpu, &Debug::CPU::Dialog::StateChanged);

	connect(ui.actionAbout, &QAction::triggered, &about, &About::show);
}
