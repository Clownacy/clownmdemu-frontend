#ifndef DEBUG_FM_H
#define DEBUG_FM_H

#include "window-popup.h"

namespace DebugFM
{
	class Registers : public WindowPopup
	{
	public:
		using WindowPopup::WindowPopup;

		void Display();
	};
}

#endif /* DEBUG_FM_H */
