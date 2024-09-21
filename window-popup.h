#ifndef WINDOW_POPUP_H
#define WINDOW_POPUP_H

#include <optional>
#include <string>

#include "window-with-dear-imgui.h"

class WindowPopup
{
private:
	std::optional<WindowWithDearImGui> window;
	std::string title;
	int width, height;
	Window *parent_window;

public:
	WindowPopup(DebugLog &debug_log, const char *window_title, int window_width, int window_height, Window *parent_window = nullptr);

	bool Begin();
	void End();
};

#endif /* WINDOW_POPUP_H */
