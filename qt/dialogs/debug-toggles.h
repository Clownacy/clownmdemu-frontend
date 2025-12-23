#ifndef DIALOGS_DEBUG_TOGGLES_H
#define DIALOGS_DEBUG_TOGGLES_H

#include "../options.h"

#include "ui_debug-toggles.h"

namespace Dialogs::Debug
{
	class Toggles : public QDialog
	{
		Q_OBJECT

	private:
		Ui::DebugToggles ui;

	public:
		explicit Toggles(Options &options, QWidget *parent = nullptr);
	};
}

#endif // DIALOGS_DEBUG_TOGGLES_H
