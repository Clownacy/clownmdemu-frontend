#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <memory>
#include <optional>

#include <QMainWindow>

#include "dialogs/about.h"
#include "dialogs/debug-cpu.h"
#include "dialogs/options.h"
#include "allocated-dialog.h"
#include "emulator-widget.h"
#include "ui_main-window.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	struct EmulatorStuff
	{
		EmulatorWidget emulator;
		AllocatedDialog<Dialogs::Debug::CPU> debug_cpu;

		template<typename... Args>
		EmulatorStuff(QWidget* const parent, Args &&...args)
			: emulator(std::forward<Args>(args)..., parent)
		{}
	};

	Ui::MainWindow ui;
	Emulator::Configuration emulator_configuration;
	std::optional<EmulatorStuff> emulator_stuff;
	AllocatedDialog<Dialogs::Options> options;
	AllocatedDialog<Dialogs::About> about;
	QWidget *central_widget = nullptr;

	void DoActionEnablement(bool enabled);
	[[nodiscard]] bool LoadCartridgeData(const QString &file_path);
	void LoadCartridgeData(const QByteArray &file_contents);
	void UnloadCartridgeData();
};

#endif // MAINWINDOW_H
