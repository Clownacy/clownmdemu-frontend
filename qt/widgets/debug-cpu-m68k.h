#ifndef WIDGETS_DEBUG_CPU_M68K_H
#define WIDGETS_DEBUG_CPU_M68K_H

#include "debug-cpu-common.h"

#include "../../common/core/clown68000/interpreter/clown68000.h"

namespace Widgets::Debug::CPU
{
	class M68k : public Common
	{
	protected:
		LabelGrid<4, 2> data_registers = LabelGrid<4, 2>("Data Registers");
		LabelGrid<4, 2> address_registers = LabelGrid<4, 2>("Address Registers");
		LabelGrid<3, 1> misc_registers = LabelGrid<3, 1>("Misc. Registers");

	public:
		M68k(const Clown68000_State &state);

		void StateChanged(const Clown68000_State &state);
	};
}

#endif // WIDGETS_DEBUG_CPU_M68K_H
