#include "main-window.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QLayout>
#include <QMimeData>

#include "message-box.h"

void MainWindow::DoActionEnablement(const bool enabled)
{
	const auto Do = [&](QAction* const action)
	{
		action->setEnabled(enabled);
	};

	Do(ui.actionPause);
	Do(ui.actionReset);

	Do(ui.actionQuick_Save);
	ui.actionQuick_Load->setEnabled(false);

	Do(ui.actionSave_to_File);
	Do(ui.actionLoad_from_File);

	Do(ui.actionCPUs);
};

void MainWindow::CreateEmulator()
{
	emulator.emplace(options, cartridge_rom_buffer, SDL::IOStream(SDL_IOFromConstMem(std::data(cd_buffer), std::size(cd_buffer))), cd_file_path, this);
	emulator->Pause(ui.actionPause->isChecked());

	if (central_widget == nullptr)
		central_widget = takeCentralWidget();
	setCentralWidget(&*emulator);

	connect(ui.actionPause, &QAction::triggered, &*emulator, &Widgets::Emulator::Pause);
	connect(ui.actionReset, &QAction::triggered, &*emulator, &Widgets::Emulator::SoftReset);

	connect(ui.actionQuick_Save, &QAction::triggered, this,
		[this]()
		{
			quick_save_state = emulator->SaveState();
			ui.actionQuick_Load->setEnabled(true);
		}
	);
	connect(ui.actionQuick_Load, &QAction::triggered, this, [this](){ emulator->LoadState(quick_save_state); });

	connect(ui.actionSave_to_File, &QAction::triggered, this,
		[this]()
		{
			const EmulatorStuff::State &save_state = emulator->SaveState();
			QFileDialog::saveFileContent(QByteArray(reinterpret_cast<const char*>(&save_state), sizeof(save_state)), "Save State.bin", this);
		}
	);
	connect(ui.actionLoad_from_File, &QAction::triggered, this,
		[this]()
		{
			QFileDialog::getOpenFileContent("Save State (*.*)",
				[this]([[maybe_unused]] const QString &file_name, const QByteArray &file_contents)
				{
					if (std::size(file_contents) != sizeof(EmulatorStuff::State))
					{
						MessageBox::critical(this, "Load State", "Unable to load save-state file.", "Size was incorrect.");
						return;
					}

					emulator->LoadState(*reinterpret_cast<const EmulatorStuff::State*>(std::data(file_contents)));
				},
				this
			);
		}
	);

	connect(ui.actionCPUs, &QAction::triggered, this,
		[&]()
		{
			emulator->debug_cpu.Open(this, *emulator);
			connect(&*emulator, &Widgets::Emulator::NewFrame, &*emulator->debug_cpu, &Dialogs::Debug::CPU::StateChanged);
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

void MainWindow::DestroyEmulator()
{
	emulator = std::nullopt;

	DoActionEnablement(false);

	setCentralWidget(central_widget);
	central_widget = nullptr;
}

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
	// Convert the ROM buffer to 16-bit.
	cartridge_rom_buffer.resize(file_contents.size() / sizeof(cc_u16l));

	const auto &GetByte = [&](const auto index) { return static_cast<cc_u16f>(file_contents[index]) & 0xFF; };

	for (qsizetype i = 0; i < std::size(cartridge_rom_buffer); ++i)
	{
		cartridge_rom_buffer[i]
			= (GetByte(i * 2 + 0) << 8)
			| (GetByte(i * 2 + 1) << 0);
	}

	ui.actionUnload_Cartridge_File->setEnabled(true);
	CreateEmulator();
}

void MainWindow::UnloadCartridgeData()
{
	DestroyEmulator();

	cartridge_rom_buffer.clear();
	cartridge_rom_buffer.squeeze();

	ui.actionUnload_Cartridge_File->setEnabled(false);

	if (!cd_buffer.isEmpty())
		CreateEmulator();
}

void MainWindow::LoadCDData(const QByteArray &file_contents, const QString &file_path)
{
	cd_buffer = file_contents;
	cd_file_path = reinterpret_cast<const char8_t*>(file_path.toStdString().c_str());

	ui.actionUnload_CD_File->setEnabled(true);
	CreateEmulator();
}

void MainWindow::UnloadCDData()
{
	DestroyEmulator();

	cd_buffer.clear();
	cd_buffer.squeeze();
	cd_file_path.clear();

	ui.actionUnload_CD_File->setEnabled(false);

	if (!cartridge_rom_buffer.isEmpty())
		CreateEmulator();
}

MainWindow::MainWindow(QWidget* const parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	setAcceptDrops(true);

	connect(ui.actionLoad_Cartridge_File, &QAction::triggered, this,
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

	connect(ui.actionLoad_CD_File, &QAction::triggered, this,
		[this]()
		{
			QFileDialog::getOpenFileContent("Mega CD Disc Software (*.bin *.cue *.iso)",
				[this]([[maybe_unused]] const QString &file_name, const QByteArray &file_contents)
				{
					LoadCDData(file_contents, file_name);
				},
				this
			);
		}
	);

	connect(ui.actionUnload_CD_File, &QAction::triggered, this, &MainWindow::UnloadCDData);

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
			connect(&*options_menu, &Dialogs::Options::rewindingOptionChanged, this,
				[this](const bool enabled)
				{
					if (emulator)
						emulator->SetRewindEnabled(enabled);
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
