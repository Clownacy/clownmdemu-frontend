#ifndef ABOUT_H
#define ABOUT_H

#include "common/window-popup.h"

class AboutWindow : public WindowPopup<AboutWindow>
{
private:
	using Base = WindowPopup<AboutWindow>;

	static constexpr Uint32 window_flags = ImGuiWindowFlags_HorizontalScrollbar;

	void DisplayInternal();

public:
	using Base::WindowPopup;

	friend Base;
};


#endif /* ABOUT_H */
