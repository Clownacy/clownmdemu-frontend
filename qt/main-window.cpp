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
	emulator_stuff.emplace(this, file_contents);
	emulator_stuff->emulator.Pause(ui.actionPause->isChecked());

	if (central_widget == nullptr)
		central_widget = takeCentralWidget();
	setCentralWidget(&emulator_stuff->emulator);

	connect(ui.actionPause, &QAction::triggered, &emulator_stuff->emulator, &EmulatorWidget::Pause);
	connect(ui.actionReset, &QAction::triggered, &emulator_stuff->emulator, &EmulatorWidget::Reset);
	connect(ui.actionCPUs, &QAction::triggered, &emulator_stuff->debug_cpu, &Debug::CPU::Dialog::show);
	connect(&emulator_stuff->emulator, &EmulatorWidget::NewFrame, &emulator_stuff->debug_cpu, &Debug::CPU::Dialog::StateChanged);

	DoActionEnablement(true);

	emulator_stuff->emulator.SetLogCallback(
		[](const std::string &message)
		{
			qDebug() << message;
		}
	);
}

void MainWindow::UnloadCartridgeData()
{
	emulator_stuff = std::nullopt;

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

	// TODO: Full-screen the OpenGL widget only!
	connect(ui.actionFullscreen, &QAction::triggered, this, [this](const bool enabled){enabled ? showFullScreen() : showNormal();});

	connect(ui.actionAbout, &QAction::triggered, &about, &About::show);
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
