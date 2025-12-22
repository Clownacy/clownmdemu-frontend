#ifndef WIDGETS_DEBUG_CPU_COMMON_H
#define WIDGETS_DEBUG_CPU_COMMON_H

#include <array>

#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QWidget>

namespace Widgets::Debug::CPU
{
	class Common : public QWidget
	{
		Q_OBJECT

	protected:
		template<std::size_t Width, std::size_t Height>
		class LabelGrid
		{
		public:
			static constexpr std::size_t width = Width;
			static constexpr std::size_t height = Height;

			QGroupBox group_box;
			QGridLayout layout;
			std::array<std::array<QLabel, width>, height> labels;

			LabelGrid(const QString &title)
				: group_box(title)
			{
				group_box.setLayout(&layout);
				group_box.setFlat(true);

				const auto &font = QFontDatabase::systemFont(QFontDatabase::SystemFont::FixedFont);

				for (std::size_t y = 0; y < height; ++y)
				{
					for (std::size_t x = 0; x < width; ++x)
					{
						auto &label = labels[y][x];
						label.setFont(font);
						layout.addWidget(&labels[y][x], y, x, Qt::AlignCenter);
					}
				}
			}
		};

		QVBoxLayout layout;

		static QString RegisterToString(const int digits, const QString &label, const unsigned long value)
		{
			return QStringLiteral("%1:%2").arg(label).arg(value, digits, 0x10, '0').toUpper();
		};

		static QString ByteRegisterToString(const QString &label, const unsigned long value)
		{
			return RegisterToString(2, label, value);
		};

		static QString WordRegisterToString(const QString &label, const unsigned long value)
		{
			return RegisterToString(4, label, value);
		};

		static QString LongRegisterToString(const QString &label, const unsigned long value)
		{
			return RegisterToString(8, label, value);
		};

	public:
		Common()
		{
			setLayout(&layout);
		}
	};
}

#endif // WIDGETS_DEBUG_CPU_COMMON_H
