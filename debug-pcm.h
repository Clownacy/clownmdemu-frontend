#ifndef DEBUG_PCM_H
#define DEBUG_PCM_H

#include "window-popup.h"

namespace DebugPCM
{
	class Registers : public WindowPopup
	{
	public:
		using WindowPopup::WindowPopup;

		void Display();
	};
}

#endif /* DEBUG_PCM_H */
