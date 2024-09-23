#ifndef DEBUG_PCM_H
#define DEBUG_PCM_H

#include "common/window-popup.h"

namespace DebugPCM
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

#endif /* DEBUG_PCM_H */
