#ifndef QT_EXTENSIONS_H
#define QT_EXTENSIONS_H

#include <filesystem>

#include <QMessageBox>

namespace QtExtensions
{
	namespace MessageBox
	{
		inline QMessageBox::StandardButton critical(
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

	inline std::filesystem::path toStdPath(const QString &string)
	{
		return reinterpret_cast<const char8_t*>(string.toStdString().c_str());
	}
}

#endif // QT_EXTENSIONS_H
