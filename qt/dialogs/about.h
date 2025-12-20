#ifndef DIALOGS_ABOUT_H
#define DIALOGS_ABOUT_H

#include "ui_about.h"

namespace Dialogs
{
	class About : public QDialog
	{
			Q_OBJECT

		public:
			explicit About(QWidget *parent = nullptr);

		private:
			Ui::About ui;
	};
}

#endif // DIALOGS_ABOUT_H
