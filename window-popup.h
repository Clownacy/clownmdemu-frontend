#ifndef WINDOW_POPUP_H
#define WINDOW_POPUP_H

#include "window-with-dear-imgui.h"

class WindowPopup : private WindowWithDearImGui
{
public:
	WindowPopup(DebugLog &debug_log, const char *window_title, int window_width, int window_height)
		: WindowWithDearImGui(debug_log, window_title, window_width, window_height)
	{
		SDL_ShowWindow(GetSDLWindow());
	}

	bool Begin();
	void End();
};

#endif /* WINDOW_POPUP_H */
