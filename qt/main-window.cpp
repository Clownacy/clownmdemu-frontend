#include "main-window.h"

#include <QDebug>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QLayout>
#include <QMimeData>

#include "qt-extensions.h"

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
}

void MainWindow::CreateEmulator()
{
	emulator.emplace(options, cartridge_rom_buffer, cartridge_file_path, cd_stream, cd_file_path, this);
	emulator->Pause(ui.actionPause->isChecked());

	if (central_widget == nullptr)
	{
		central_widget = takeCentralWidget();
		central_widget->hide();
	}
	setCentralWidget(&*emulator);

	connect(ui.actionPause, &QAction::triggered, &*emulator, &Widgets::Emulator::Pause);
	connect(ui.actionReset, &QAction::triggered, &*emulator, &Widgets::Emulator::Reset);

	connect(ui.actionQuick_Save, &QAction::triggered, this,
		[this]()
		{
			quick_save_state.emplace(*emulator);
			ui.actionQuick_Load->setEnabled(true);
		}
	);
	connect(ui.actionQuick_Load, &QAction::triggered, this, [this](){ quick_save_state->Apply(*emulator); });

	connect(ui.actionSave_to_File, &QAction::triggered, this,
		[this]()
		{
			// Allocate on the heap to prevent stack exhaustion.
			const auto &save_state = std::make_unique<EmulatorStuff::StateBackup>(*emulator);
			QFileDialog::saveFileContent(QByteArray(reinterpret_cast<const char*>(&*save_state), sizeof(*save_state)), "Save State.bin", this);
		}
	);
	connect(ui.actionLoad_from_File, &QAction::triggered, this,
		[this]()
		{
			QFileDialog::getOpenFileContent("Save State (*.*)",
				[this]([[maybe_unused]] const QString &file_name, const QByteArray &file_contents)
				{
					if (std::size(file_contents) != sizeof(EmulatorStuff::StateBackup))
					{
						QtExtensions::MessageBox::critical(this, "Load State", "Unable to load save-state file.", "Size was incorrect.");
						return;
					}

					reinterpret_cast<const EmulatorStuff::StateBackup*>(std::data(file_contents))->Apply(*emulator);
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

	// Options
	connect(&options, &Options::IntegerScreenScalingEnabledChanged, &*emulator, qOverload<>(&Widgets::Emulator::update));
	connect(&options, &Options::TallInterlaceMode2EnabledChanged, &*emulator, qOverload<>(&Widgets::Emulator::update));
	connect(&options, &Options::RewindingEnabledChanged, &*emulator, &Widgets::Emulator::SetRewindEnabled);

	connect(&options, &Options::TVStandardChanged, &*emulator, &Widgets::Emulator::SetTVStandard);
	connect(&options, &Options::RegionChanged, &*emulator, &Widgets::Emulator::SetRegion);
	connect(&options, &Options::CDAddOnEnabledChanged, &*emulator, &Widgets::Emulator::SetCDAddOnEnabled);
	connect(&options, &Options::WidescreenTilePairsChanged, &*emulator, &Widgets::Emulator::SetWidescreenTilePairs);
	connect(&options, &Options::LowPassFilterEnabledChanged, &*emulator, &Widgets::Emulator::SetLowPassFilterEnabled);
	connect(&options, &Options::LadderEffectEnabledChanged, &*emulator, &Widgets::Emulator::SetLadderEffectEnabled);
	connect(&options, &Options::SpritePlaneEnabledChanged, &*emulator, &Widgets::Emulator::SetSpritePlaneEnabled);
	connect(&options, &Options::WindowPlaneEnabledChanged, &*emulator, &Widgets::Emulator::SetWindowPlaneEnabled);
	connect(&options, &Options::ScrollPlaneAEnabledChanged, &*emulator, &Widgets::Emulator::SetScrollPlaneAEnabled);
	connect(&options, &Options::ScrollPlaneBEnabledChanged, &*emulator, &Widgets::Emulator::SetScrollPlaneBEnabled);
	connect(&options, &Options::FM1EnabledChanged, &*emulator, &Widgets::Emulator::SetFM1Enabled);
	connect(&options, &Options::FM2EnabledChanged, &*emulator, &Widgets::Emulator::SetFM2Enabled);
	connect(&options, &Options::FM3EnabledChanged, &*emulator, &Widgets::Emulator::SetFM3Enabled);
	connect(&options, &Options::FM4EnabledChanged, &*emulator, &Widgets::Emulator::SetFM4Enabled);
	connect(&options, &Options::FM5EnabledChanged, &*emulator, &Widgets::Emulator::SetFM5Enabled);
	connect(&options, &Options::FM6EnabledChanged, &*emulator, &Widgets::Emulator::SetFM6Enabled);
	connect(&options, &Options::DACEnabledChanged, &*emulator, &Widgets::Emulator::SetDACEnabled);
	connect(&options, &Options::PSG1EnabledChanged, &*emulator, &Widgets::Emulator::SetPSG1Enabled);
	connect(&options, &Options::PSG2EnabledChanged, &*emulator, &Widgets::Emulator::SetPSG2Enabled);
	connect(&options, &Options::PSG3EnabledChanged, &*emulator, &Widgets::Emulator::SetPSG3Enabled);
	connect(&options, &Options::PSGNoiseEnabledChanged, &*emulator, &Widgets::Emulator::SetPSGNoiseEnabled);
	connect(&options, &Options::PCM1EnabledChanged, &*emulator, &Widgets::Emulator::SetPCM1Enabled);
	connect(&options, &Options::PCM2EnabledChanged, &*emulator, &Widgets::Emulator::SetPCM2Enabled);
	connect(&options, &Options::PCM3EnabledChanged, &*emulator, &Widgets::Emulator::SetPCM3Enabled);
	connect(&options, &Options::PCM4EnabledChanged, &*emulator, &Widgets::Emulator::SetPCM4Enabled);
	connect(&options, &Options::PCM5EnabledChanged, &*emulator, &Widgets::Emulator::SetPCM5Enabled);
	connect(&options, &Options::PCM6EnabledChanged, &*emulator, &Widgets::Emulator::SetPCM6Enabled);
	connect(&options, &Options::PCM7EnabledChanged, &*emulator, &Widgets::Emulator::SetPCM7Enabled);
	connect(&options, &Options::PCM8EnabledChanged, &*emulator, &Widgets::Emulator::SetPCM8Enabled);

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
	central_widget->show();
	central_widget = nullptr;
}

bool MainWindow::LoadCartridgeData(const std::filesystem::path &file_path)
{
	auto stream = SDL::IOFromFile(file_path, "rb");

	if (!stream)
		return false;

	LoadCartridgeData(file_path, std::move(stream));
	return true;
}

void MainWindow::LoadCartridgeData(const std::filesystem::path &file_path, SDL::IOStream &&stream)
{
	// Convert the ROM buffer to 16-bit.
	cartridge_rom_buffer.resize(SDL_GetIOSize(stream) / sizeof(cc_u16l));

	for (auto &word : cartridge_rom_buffer)
		SDL_ReadU16BE(stream, &word);

	cartridge_file_path = file_path;

	ui.actionUnload_Cartridge_File->setEnabled(true);
	CreateEmulator();
}

void MainWindow::UnloadCartridgeData()
{
	DestroyEmulator();

	cartridge_rom_buffer.clear();
	cartridge_rom_buffer.squeeze();
	cartridge_file_path.clear();

	ui.actionUnload_Cartridge_File->setEnabled(false);

	if (!cd_file_path.empty())
		CreateEmulator();
}

bool MainWindow::LoadCDData(const std::filesystem::path &file_path)
{
	auto file_stream = SDL::IOFromFile(file_path, "rb");

	if (!file_stream)
		return false;

	LoadCDData(file_path, std::move(file_stream));
	return true;
}

void MainWindow::LoadCDData(const std::filesystem::path &file_path, SDL::IOStream &&file_stream, const QByteArray *file_contents)
{
	cd_buffer = std::nullopt;
	if (file_contents != nullptr)
		cd_buffer = *file_contents;
	cd_stream = std::move(file_stream);
	cd_file_path = file_path;

	ui.actionUnload_CD_File->setEnabled(true);
	CreateEmulator();
}

void MainWindow::UnloadCDData()
{
	DestroyEmulator();

	cd_buffer = std::nullopt;
	cd_stream.reset();
	cd_file_path.clear();

	ui.actionUnload_CD_File->setEnabled(false);

	if (!cartridge_rom_buffer.isEmpty())
		CreateEmulator();
}

MainWindow::MainWindow(QWidget* const parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);

	// Enable keyboard input.
	setFocusPolicy(Qt::StrongFocus);

	setAcceptDrops(true);

	connect(ui.actionLoad_Cartridge_File, &QAction::triggered, this,
		[this]()
		{
			QtExtensions::getOpenFileContent("Mega Drive Cartridge Software (*.bin *.md *.gen)",
				[this](const std::filesystem::path &file_path, SDL::IOStream &&file_stream, [[maybe_unused]] const QByteArray *file_contents)
				{
					LoadCartridgeData(file_path, std::move(file_stream));
				},
				this
			);
		}
	);

	connect(ui.actionUnload_Cartridge_File, &QAction::triggered, this, &MainWindow::UnloadCartridgeData);

	connect(ui.actionLoad_CD_File, &QAction::triggered, this,
		[this]()
		{
			QtExtensions::getOpenFileContent("Mega CD Disc Software (*.bin *.cue *.iso)",
				[this](const std::filesystem::path &file_path, SDL::IOStream &&file_stream, const QByteArray *file_contents)
				{
					LoadCDData(file_path, std::move(file_stream), file_contents);
				},
				this
			);
		}
	);

	connect(ui.actionUnload_CD_File, &QAction::triggered, this, &MainWindow::UnloadCDData);

	connect(ui.actionToggles, &QAction::triggered, this, [&](){ debug_toggles.Open(this, options); });

	// TODO: Full-screen the OpenGL widget only!
	connect(ui.actionFullscreen, &QAction::triggered, this, &MainWindow::SetFullscreen);
	connect(ui.actionOptions, &QAction::triggered, this, [&](){ options_menu.Open(this, options); });
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

	for (const auto &url : mime_data->urls())
	{
		if (!url.isLocalFile())
			continue;

		const auto &file_path = QtExtensions::toStdPath(url.toLocalFile());

		if (CDReader::IsDefinitelyACD(file_path))
		{
			if (!LoadCDData(file_path))
				continue;
		}
		else
		{
			if (!LoadCartridgeData(file_path))
				continue;
		}
	}

	event->acceptProposedAction();
}

void MainWindow::keyPressEvent(QKeyEvent* const event)
{
	if (!event->isAutoRepeat())
	{
		// Exit fullscreen by pressing the escape key.
		if (event->key() == Qt::Key_Escape && isFullScreen())
		{
			SetFullscreen(false);
			return;
		}
	}

	QMainWindow::keyPressEvent(event);
}

void MainWindow::SetFullscreen(const bool enabled)
{
	if (enabled)
	{
		showFullScreen();
		menuBar()->hide();
	}
	else
	{
		showNormal();
		menuBar()->show();
	}

	ui.actionFullscreen->setChecked(enabled);
}
