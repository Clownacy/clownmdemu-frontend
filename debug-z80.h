#ifndef DEBUG_Z80_H
#define DEBUG_Z80_H

#include "window-popup.h"

namespace DebugZ80
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

#endif /* DEBUG_Z80_H */
