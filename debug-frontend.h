#ifndef DEBUG_FRONTEND_H
#define DEBUG_FRONTEND_H

#include <functional>

#include "emulator-instance.h"
#include "window.h"

class DebugFrontend
{
public:
	using GetUpscaledFramebufferSize = std::function<bool(unsigned int &width, unsigned int &height)>;

private:
	const EmulatorInstance &emulator;
	Window &window;
	const GetUpscaledFramebufferSize get_upscaled_framebuffer_size;

public:
	unsigned int output_width, output_height;
	unsigned int upscale_width, upscale_height;

	DebugFrontend(const EmulatorInstance &emulator, Window &window, const GetUpscaledFramebufferSize &get_upscaled_framebuffer_size)
		: emulator(emulator)
		, window(window)
		, get_upscaled_framebuffer_size(get_upscaled_framebuffer_size)
	{

	}
	void Display(bool &open);
};

#endif /* DEBUG_FRONTEND_H */
