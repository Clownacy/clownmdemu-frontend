#include "main-window.h"

#include <QFileDialog>
#include <QLayout>

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	const auto DoActionEnablement = [this](const bool enabled)
	{
		const auto Do = [&](QAction* const action)
		{
			action->setEnabled(enabled);
		};

		Do(ui.actionUnload_Cartridge_File);
		Do(ui.actionCPUs);
	};

	connect(ui.actionLoad_Cartridge_Software, &QAction::triggered, this,
		[this, DoActionEnablement]()
		{
			QFileDialog::getOpenFileContent("Mega Drive Cartridge Software (*.bin *.md *.gen)",
				[this, DoActionEnablement]([[maybe_unused]] const QString &file_name, const QByteArray &file_contents)
				{
					emulator_stuff.emplace(this);

					setCentralWidget(&emulator_stuff->emulator);
					emulator_stuff->emulator.LoadCartridgeSoftware(file_contents);

					connect(ui.actionCPUs, &QAction::triggered, &emulator_stuff->debug_cpu, &Debug::CPU::Dialog::show);
					connect(&emulator_stuff->emulator, &EmulatorWidget::NewFrame, &emulator_stuff->debug_cpu, &Debug::CPU::Dialog::StateChanged);

					DoActionEnablement(true);
				}, this
			);
		}
	);

	connect(ui.actionUnload_Cartridge_File, &QAction::triggered, this,
		[this, DoActionEnablement]()
		{
			emulator_stuff = std::nullopt;

			DoActionEnablement(false);
		}
	);

	connect(ui.actionAbout, &QAction::triggered, &about, &About::show);
}
