#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include <cstddef>

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "window-popup.h"

class DebugMemory : public WindowPopup
{
public:
	using WindowPopup::WindowPopup;

	void Display(const cc_u8l *buffer, std::size_t buffer_length);
	void Display(const cc_u16l *buffer, std::size_t buffer_length);
};

#endif /* DEBUG_MEMORY_H */
