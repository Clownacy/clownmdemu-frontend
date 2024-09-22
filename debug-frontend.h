#ifndef DEBUG_FRONTEND_H
#define DEBUG_FRONTEND_H

#include <functional>

#include "emulator-instance.h"
#include "window.h"

class WindowPopup;

class DebugFrontend
{
public:
	using GetUpscaledFramebufferSize = std::function<bool(unsigned int &width, unsigned int &height)>;

private:
	const EmulatorInstance &emulator;
	const GetUpscaledFramebufferSize get_upscaled_framebuffer_size;

public:
	unsigned int output_width = 0, output_height = 0;
	unsigned int upscale_width = 0, upscale_height = 0;

	DebugFrontend(const EmulatorInstance &emulator, const GetUpscaledFramebufferSize &get_upscaled_framebuffer_size)
		: emulator(emulator)
		, get_upscaled_framebuffer_size(get_upscaled_framebuffer_size)
	{

	}
	void Display(WindowPopup &window);
};

#endif /* DEBUG_FRONTEND_H */
