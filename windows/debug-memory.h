#ifndef DEBUG_MEMORY_H
#define DEBUG_MEMORY_H

#include <cstddef>

#include "../common/core/clowncommon/clowncommon.h"

#include "common/window-popup.h"

class DebugMemory : public WindowPopup<DebugMemory>
{
private:
	using Base = WindowPopup<DebugMemory>;

	static constexpr Uint32 window_flags = 0;

	void DisplayInternal(const cc_u8l *buffer, std::size_t buffer_length);
	void DisplayInternal(const cc_u16l *buffer, std::size_t buffer_length);
	template<typename T>
	void DisplayInternal(const T &buffer)
	{
		DisplayInternal(std::data(buffer), std::size(buffer));
	}

public:
	using Base::WindowPopup;

	friend Base;
};

#endif /* DEBUG_MEMORY_H */
