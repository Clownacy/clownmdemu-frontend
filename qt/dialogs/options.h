#ifndef DIALOGS_OPTIONS_H
#define DIALOGS_OPTIONS_H

#include "ui_options.h"

#include "../emulator.h"

namespace Dialogs
{
	class Options : public QDialog
	{
		Q_OBJECT

	private:
		Ui::Options ui;
		Emulator::Configuration &configuration;

	public:
		explicit Options(Emulator::Configuration &configuration, QWidget *parent = nullptr);
	};
}

#endif // DIALOGS_OPTIONS_H
