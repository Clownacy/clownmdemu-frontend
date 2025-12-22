#ifndef WIDGETS_DEBUG_CPU_Z80_H
#define WIDGETS_DEBUG_CPU_Z80_H

#include "debug-cpu-common.h"

#include "../../common/core/clownz80/interpreter.h"

namespace Widgets::Debug::CPU
{
	class Z80 : public Common
	{
	protected:
		LabelGrid<8, 1> regular_registers = LabelGrid<8, 1>("Regular Registers");
		LabelGrid<8, 1> shadow_registers = LabelGrid<8, 1>("Shadow Registers");
		LabelGrid<6, 1> misc_registers = LabelGrid<6, 1>("Misc. Registers");

	public:
		Z80(const ClownZ80_State &state);

		void StateChanged(const ClownZ80_State &state);
	};
}

#endif // WIDGETS_DEBUG_CPU_Z80_H
