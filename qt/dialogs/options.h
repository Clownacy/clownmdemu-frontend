#ifndef DIALOGS_OPTIONS_H
#define DIALOGS_OPTIONS_H

#include "ui_options.h"

#include "../options.h"

namespace Dialogs
{
	class Options : public QDialog
	{
		Q_OBJECT

	private:
		Ui::Options ui;

	public:
		explicit Options(::Options &options, QWidget *parent = nullptr);
	};
}

#endif // DIALOGS_OPTIONS_H
