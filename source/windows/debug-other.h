#ifndef DEBUG_OTHER_H
#define DEBUG_OTHER_H

#include "common/window-popup.h"

class DebugOther : public WindowPopup<DebugOther>
{
private:
	using Base = WindowPopup<DebugOther>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_OTHER_H */
