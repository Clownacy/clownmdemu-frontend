#ifndef DEBUG_CDC_H
#define DEBUG_CDC_H

#include "common/window-popup.h"

class DebugCDC : public WindowPopup<DebugCDC>
{
private:
	using Base = WindowPopup<DebugCDC>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_CDC_H */
