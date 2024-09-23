#ifndef DEBUG_LOG_VIEWER_H
#define DEBUG_LOG_VIEWER_H

#include "common/window-popup.h"

class DebugLogViewer : public WindowPopup<DebugLogViewer>
{
private:
	using Base = WindowPopup<DebugLogViewer>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_LOG_VIEWER_H */
