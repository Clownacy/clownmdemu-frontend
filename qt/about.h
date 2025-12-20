#ifndef ABOUT_H
#define ABOUT_H

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

#endif // ABOUT_H
