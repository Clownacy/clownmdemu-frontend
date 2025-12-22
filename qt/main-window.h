#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <filesystem>
#include <optional>

#include <QMainWindow>

#include "dialogs/about.h"
#include "dialogs/debug-cpu.h"
#include "dialogs/debug-toggles.h"
#include "dialogs/options.h"
#include "allocated-dialog.h"
#include "emulator-widget.h"
#include "options.h"
#include "ui_main-window.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

private:
	struct EmulatorStuff : public EmulatorWidget
	{
		AllocatedDialog<Dialogs::Debug::CPU> debug_cpu;

		using EmulatorWidget::EmulatorWidget;
	};

	Ui::MainWindow ui;
	Options options;
	QVector<cc_u16l> cartridge_rom_buffer;
	QByteArray cd_buffer;
	std::filesystem::path cd_file_path;
	std::optional<EmulatorStuff> emulator;
	AllocatedDialog<Dialogs::Debug::Toggles> debug_toggles;
	AllocatedDialog<Dialogs::Options> options_menu;
	AllocatedDialog<Dialogs::About> about_menu;
	QWidget *central_widget = nullptr;

	void DoActionEnablement(bool enabled);
	void CreateEmulator();
	void DestroyEmulator();
	[[nodiscard]] bool LoadCartridgeData(const QString &file_path);
	void LoadCartridgeData(const QByteArray &file_contents);
	void UnloadCartridgeData();
	void LoadCDData(const QByteArray &file_contents, const QString &file_path);
	void UnloadCDData();

public:
	MainWindow(QWidget *parent = nullptr);

	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;
};

#endif // MAINWINDOW_H
