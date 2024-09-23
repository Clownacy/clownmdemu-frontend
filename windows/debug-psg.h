#ifndef DEBUG_PSG_H
#define DEBUG_PSG_H

#include "common/window-popup.h"

namespace DebugPSG
{
	class Registers : public WindowPopup<Registers>
	{
	private:
		using Base = WindowPopup<Registers>;

		static constexpr Uint32 window_flags = 0;

		void DisplayInternal();

	public:
		using Base::WindowPopup;

		friend Base;
	};
}

#endif /* DEBUG_PSG_H */
