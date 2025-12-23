#ifndef MESSAGE_BOX_H
#define MESSAGE_BOX_H

#include <QMessageBox>

namespace MessageBox
{
	QMessageBox::StandardButton critical(
		QWidget* const parent,
		const QString &what_failed,
		const QString &how_it_failed,
		const QString &why_it_failed,
		QMessageBox::StandardButtons buttons = QMessageBox::Ok,
		QMessageBox::StandardButton defaultButton = QMessageBox::NoButton)
	{
		QMessageBox msgBox(parent);
		msgBox.setIcon(QMessageBox::Critical);
		msgBox.setWindowTitle(what_failed);
		msgBox.setText(how_it_failed);
		msgBox.setInformativeText(why_it_failed);
		msgBox.setStandardButtons(buttons);
		msgBox.setDefaultButton(defaultButton);
		return static_cast<QMessageBox::StandardButton>(msgBox.exec());
	}
}

#endif // MESSAGE_BOX_H
