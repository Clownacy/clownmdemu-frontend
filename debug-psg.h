#ifndef DEBUG_PSG_H
#define DEBUG_PSG_H

#include "window-popup.h"

namespace DebugPSG
{
	class Registers : public WindowPopup
	{
	public:
		using WindowPopup::WindowPopup;

		void Display();
	};
}

#endif /* DEBUG_PSG_H */
