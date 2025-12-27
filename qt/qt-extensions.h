#ifndef QT_EXTENSIONS_H
#define QT_EXTENSIONS_H

#include <filesystem>

#include <QFileDialog>
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
			QMessageBox message_box(parent);
			message_box.setIcon(QMessageBox::Critical);
			message_box.setWindowTitle(what_failed);
			message_box.setText(how_it_failed);
			message_box.setInformativeText(why_it_failed);
			message_box.setStandardButtons(buttons);
			message_box.setDefaultButton(defaultButton);
			return static_cast<QMessageBox::StandardButton>(message_box.exec());
		}
	}

	inline std::filesystem::path toStdPath(const QString &string)
	{
		return reinterpret_cast<const char8_t*>(string.toStdString().c_str());
	}

	inline void getOpenFileContent(const QString &nameFilter, const std::function<void (const std::filesystem::path &file_path, SDL::IOStream &&file_stream, const QByteArray *file_contents)> &fileOpenCompleted, QWidget *parent = nullptr)
	{
		QFileDialog::getOpenFileContent(nameFilter,
			[fileOpenCompleted](const QString &file_name, const QByteArray &file_contents)
			{
				fileOpenCompleted(QtExtensions::toStdPath(file_name), SDL::IOStream(SDL_IOFromConstMem(file_contents.data(), file_contents.size())), &file_contents);
			},
			parent
		);
	}
}

#endif // QT_EXTENSIONS_H
