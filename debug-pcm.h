#ifndef DEBUG_PCM_H
#define DEBUG_PCM_H

#include "window-popup.h"

namespace DebugPCM
{
	class Registers : public WindowPopup
	{
	public:
		using WindowPopup::WindowPopup;

		bool Display();
	};
}

#endif /* DEBUG_PCM_H */
