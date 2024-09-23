#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include <cstddef>

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "window-popup.h"

class DebugMemory : public WindowPopup<DebugMemory>
{
private:
	using Base = WindowPopup<DebugMemory>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal(const cc_u8l *buffer, std::size_t buffer_length);
	void DisplayInternal(const cc_u16l *buffer, std::size_t buffer_length);

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_MEMORY_H */
