#include "about.h"

#include <QPushButton>

About::About(QWidget *parent) :
	QDialog(parent)
{
	ui.setupUi(this);

	connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &About::close);
}
