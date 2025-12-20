#ifndef DIALOG_DEBUG_TOGGLES_H
#define DIALOG_DEBUG_TOGGLES_H

#include "ui_debug-toggles.h"

namespace Dialogs::Debug
{
	class Toggles : public QDialog
	{
		Q_OBJECT

	private:
		Ui::DebugToggles ui;

	public:
		explicit Toggles(QWidget *parent = nullptr);
	};
}

#endif // DIALOG_DEBUG_TOGGLES_H
