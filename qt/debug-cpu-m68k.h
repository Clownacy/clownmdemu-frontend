#ifndef DEBUG_CPU_M68K_H
#define DEBUG_CPU_M68K_H

#include "debug-cpu-common.h"

#include "common/core/clown68000/interpreter/clown68000.h"

namespace Debug
{
	namespace CPU
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
}

#endif // DEBUG_CPU_M68K_H
