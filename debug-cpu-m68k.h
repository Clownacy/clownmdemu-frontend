#ifndef DEBUG_CPU_M68K_H
#define DEBUG_CPU_M68K_H

#include <array>

#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QWidget>

#include "common/core/clown68000/interpreter/clown68000.h"

namespace Debug
{
	namespace CPU
	{
		class M68k : public QWidget
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
			LabelGrid<4, 2> data_registers = LabelGrid<4, 2>("Data Registers");
			LabelGrid<4, 2> address_registers = LabelGrid<4, 2>("Address Registers");
			LabelGrid<3, 1> misc_registers = LabelGrid<3, 1>("Misc. Registers");

		public:
			M68k(const Clown68000_State &state);

			void StateChanged(const Clown68000_State &state);
		};
	}
}

#endif // DEBUG_CPU_M68K_H
