#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <filesystem>
#include <optional>

#include <QDataStream>
#include <QMainWindow>

#include "../sdl-wrapper-inner.h"
#include "dialogs/about.h"
#include "dialogs/debug-cpu.h"
#include "dialogs/debug-toggles.h"
#include "dialogs/options.h"
#include "widgets/emulator.h"
#include "allocated-dialog.h"
#include "options.h"
#include "ui_main-window.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

private:
	struct EmulatorStuff : public Widgets::Emulator
	{
		AllocatedDialog<Dialogs::Debug::CPU> debug_cpu;

		using Widgets::Emulator::Emulator;
	};

	Ui::MainWindow ui;
	Options options;
	QVector<cc_u16l> cartridge_rom_buffer;
	std::filesystem::path cartridge_file_path;
	std::optional<QByteArray> cd_buffer;
	SDL::IOStream cd_stream;
	std::filesystem::path cd_file_path;
	std::optional<EmulatorStuff> emulator;
	std::optional<EmulatorStuff::StateBackup> quick_save_state;
	AllocatedDialog<Dialogs::Debug::Toggles> debug_toggles;
	AllocatedDialog<Dialogs::Options> options_menu;
	AllocatedDialog<Dialogs::About> about_menu;
	QWidget *central_widget = nullptr;

	void DoActionEnablement(bool enabled);
	void CreateEmulator();
	void DestroyEmulator();
	[[nodiscard]] bool LoadCartridgeData(const std::filesystem::path &file_path);
	void LoadCartridgeData(const std::filesystem::path &file_path, SDL::IOStream &&stream);
	void UnloadCartridgeData();
	[[nodiscard]] bool LoadCDData(const std::filesystem::path &file_path);
	void LoadCDData(const std::filesystem::path &file_path, SDL::IOStream &&file_stream, const QByteArray *file_contents = nullptr);
	void UnloadCDData();

public:
	MainWindow(QWidget *parent = nullptr);

	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

	void SetFullscreen(bool enabled);
};

#endif // MAINWINDOW_H
