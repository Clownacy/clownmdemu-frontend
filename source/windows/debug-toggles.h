#ifndef DEBUG_TOGGLES_H
#define DEBUG_TOGGLES_H

#include "common/window-popup.h"

class DebugToggles : public WindowPopup<DebugToggles>
{
private:
	using Base = WindowPopup<DebugToggles>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_TOGGLES_H */
