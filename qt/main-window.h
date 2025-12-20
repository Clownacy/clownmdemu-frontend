#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <optional>

#include <QMainWindow>

#include "dialogs/about.h"
#include "dialogs/debug-cpu.h"
#include "dialogs/options.h"
#include "allocated-dialog.h"
#include "emulator-widget.h"
#include "options.h"
#include "ui_main-window.h"

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);

	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	struct EmulatorStuff : public EmulatorWidget
	{
		AllocatedDialog<Dialogs::Debug::CPU> debug_cpu;

		using EmulatorWidget::EmulatorWidget;
	};

	Ui::MainWindow ui;
	Options options;
	std::optional<EmulatorStuff> emulator;
	AllocatedDialog<Dialogs::Options> options_menu;
	AllocatedDialog<Dialogs::About> about_menu;
	QWidget *central_widget = nullptr;

	void DoActionEnablement(bool enabled);
	[[nodiscard]] bool LoadCartridgeData(const QString &file_path);
	void LoadCartridgeData(const QByteArray &file_contents);
	void UnloadCartridgeData();
};

#endif // MAINWINDOW_H
