#ifndef DEBUG_M68K_H
#define DEBUG_M68K_H

#include "clownmdemu-frontend-common/clownmdemu/clown68000/interpreter/clown68000.h"

#include "window-popup.h"

namespace DebugM68k
{
	class Registers : public WindowPopup<Registers>
	{
	private:
		using Base = WindowPopup<Registers>;

		static constexpr Uint32 window_flags = 0;

		void DisplayInternal(const Clown68000_State &m68k);

	public:
		using Base::WindowPopup;

		friend Base;
	};
}

#endif /* DEBUG_M68K_H */
