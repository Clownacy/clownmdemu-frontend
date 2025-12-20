#include "main-window.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QLayout>
#include <QMimeData>

void MainWindow::DoActionEnablement(const bool enabled)
{
	const auto Do = [&](QAction* const action)
	{
		action->setEnabled(enabled);
	};

	Do(ui.actionUnload_Cartridge_File);
	Do(ui.actionPause);
	Do(ui.actionReset);
	Do(ui.actionCPUs);
};

bool MainWindow::LoadCartridgeData(const QString &file_path)
{
	QFile file(file_path);

	if (!file.open(QIODevice::ReadOnly))
		return false;

	LoadCartridgeData(file.readAll());

	return true;
}

void MainWindow::LoadCartridgeData(const QByteArray &file_contents)
{
	emulator.emplace(options, file_contents, this);
	emulator->Pause(ui.actionPause->isChecked());

	if (central_widget == nullptr)
		central_widget = takeCentralWidget();
	setCentralWidget(&*emulator);

	connect(ui.actionPause, &QAction::triggered, &*emulator, &EmulatorWidget::Pause);
	connect(ui.actionReset, &QAction::triggered, &*emulator, &EmulatorWidget::Reset);
	connect(ui.actionCPUs, &QAction::triggered, this,
		[&]()
		{
			emulator->debug_cpu.Open(this, *emulator);
			connect(&*emulator, &EmulatorWidget::NewFrame, &*emulator->debug_cpu, &Dialogs::Debug::CPU::StateChanged);
		}
	);

	DoActionEnablement(true);

	emulator->SetLogCallback(
		[](const std::string &message)
		{
			qDebug() << message;
		}
	);
}

void MainWindow::UnloadCartridgeData()
{
	emulator = std::nullopt;

	DoActionEnablement(false);

	setCentralWidget(central_widget);
	central_widget = nullptr;
}

MainWindow::MainWindow(QWidget* const parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	setAcceptDrops(true);

	connect(ui.actionLoad_Cartridge_Software, &QAction::triggered, this,
		[this]()
		{
			QFileDialog::getOpenFileContent("Mega Drive Cartridge Software (*.bin *.md *.gen)",
				[this]([[maybe_unused]] const QString &file_name, const QByteArray &file_contents)
				{
					LoadCartridgeData(file_contents);
				},
				this
			);
		}
	);

	connect(ui.actionUnload_Cartridge_File, &QAction::triggered, this, &MainWindow::UnloadCartridgeData);

	connect(ui.actionToggles, &QAction::triggered, this, [&](){ debug_toggles.Open(this, options); });

	// TODO: Full-screen the OpenGL widget only!
	connect(ui.actionFullscreen, &QAction::triggered, this, [this](const bool enabled){enabled ? showFullScreen() : showNormal();});
	connect(ui.actionOptions, &QAction::triggered, this,
		[&]()
		{
			options_menu.Open(this, options);
			connect(&*options_menu, &Dialogs::Options::presentationOptionChanged, this,
				[this]()
				{
					if (emulator)
						emulator->update();
				}
			);
		}
	);
	connect(ui.actionAbout, &QAction::triggered, this, [&](){ about_menu.Open(this); });
	connect(ui.actionExit, &QAction::triggered, this, &MainWindow::close);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* const event)
{
	if (!event->mimeData()->hasUrls())
		return;

	event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* const event)
{
	const auto &mime_data = event->mimeData();

	if (!mime_data->hasUrls())
		return;

	const auto &urls = mime_data->urls();

	if (urls.count() != 1)
		return;

	const auto &url = urls[0];

	if (!url.isLocalFile())
		return;

	if (!LoadCartridgeData(url.toLocalFile()))
		return;

	event->acceptProposedAction();
}
