#ifndef DEBUG_FRONTEND_H
#define DEBUG_FRONTEND_H

#include "common/window-popup.h"

class DebugFrontend : public WindowPopup<DebugFrontend>
{
private:
	using Base = WindowPopup<DebugFrontend>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_FRONTEND_H */
