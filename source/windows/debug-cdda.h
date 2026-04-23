#ifndef DEBUG_CDDA_H
#define DEBUG_CDDA_H

#include "common/window-popup.h"

class DebugCDDA : public WindowPopup<DebugCDDA>
{
private:
	using Base = WindowPopup<DebugCDDA>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_CDDA_H */
