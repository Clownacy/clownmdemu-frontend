#ifndef DEBUG_Z80_H
#define DEBUG_Z80_H

#include "window-popup.h"

namespace DebugZ80
{
	class Registers : public WindowPopup
	{
	public:
		using WindowPopup::WindowPopup;

		void Display();
	};
}

#endif /* DEBUG_Z80_H */
