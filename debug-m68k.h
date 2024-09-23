#ifndef DEBUG_M68K_H
#define DEBUG_M68K_H

#include "clownmdemu-frontend-common/clownmdemu/clown68000/interpreter/clown68000.h"

#include "window-popup.h"

namespace DebugM68k
{
	class Registers : public WindowPopup
	{
	public:
		using WindowPopup::WindowPopup;

		bool Display(const Clown68000_State &m68k);
	};
}

#endif /* DEBUG_M68K_H */
