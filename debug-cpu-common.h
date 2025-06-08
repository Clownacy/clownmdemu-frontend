#ifndef DEBUG_CPU_COMMON_H
#define DEBUG_CPU_COMMON_H

#include <array>

#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QWidget>

namespace Debug
{
	namespace CPU
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
						}
					}
				}
			};

			QVBoxLayout layout;

			template<std::size_t width, std::size_t height>
			void DefaultSetup(LabelGrid<width, height> &label_grid)
			{
				layout.addWidget(&label_grid.group_box);
				for (std::size_t y = 0; y < height; ++y)
					for (std::size_t x = 0; x < width; ++x)
						label_grid.layout.addWidget(&label_grid.labels[y][x], y, x, Qt::AlignCenter);
			};

			QString RegisterToString(const int digits, const QString &label, const unsigned long value)
			{
				return QStringLiteral("%1:%2").arg(label).arg(value, digits, 0x10, '0').toUpper();
			};

			QString ByteRegisterToString(const QString &label, const unsigned long value)
			{
				return RegisterToString(2, label, value);
			};

			QString WordRegisterToString(const QString &label, const unsigned long value)
			{
				return RegisterToString(4, label, value);
			};

			QString LongRegisterToString(const QString &label, const unsigned long value)
			{
				return RegisterToString(8, label, value);
			};

		public:
			Common();
		};
	}
}

#endif // DEBUG_CPU_COMMON_H
