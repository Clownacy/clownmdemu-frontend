#ifndef DEBUG_LOG_VIEWER_H
#define DEBUG_LOG_VIEWER_H

#include "../../libraries/ImGuiTextSelect/textselect.hpp"
#include "../frontend.h"
#include "common/window-popup.h"

class DebugLogViewer : public WindowPopup<DebugLogViewer>
{
private:
	using Base = WindowPopup<DebugLogViewer>;

	static constexpr Uint32 window_flags = 0;

	TextSelect text_select = {
		[](const std::size_t index) -> std::string_view
		{
			return Frontend::debug_log.lines[index];
		},
		[]() -> std::size_t
		{
			return std::size(Frontend::debug_log.lines);
		}
	};

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_LOG_VIEWER_H */
