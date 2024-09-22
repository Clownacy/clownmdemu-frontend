#ifndef DEBUG_FRONTEND_H
#define DEBUG_FRONTEND_H

#include "window-popup.h"

class DebugFrontend : public WindowPopup
{
public:
	using WindowPopup::WindowPopup;

	void Display();
};

#endif /* DEBUG_FRONTEND_H */
