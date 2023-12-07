#ifndef DEBUG_FRONTEND_H
#define DEBUG_FRONTEND_H

#include <functional>

#include "audio-output.h"
#include "window.h"

class DebugFrontend
{
public:
	typedef std::function<bool(unsigned int &width, unsigned int &height)> GetUpscaledFramebufferSize;

private:
	const AudioOutput &audio_output;
	const Window &window;
	const GetUpscaledFramebufferSize get_upscaled_framebuffer_size;

public:
	unsigned int output_width, output_height;
	unsigned int upscale_width, upscale_height;

	DebugFrontend(const AudioOutput &audio_output, const Window &window, const GetUpscaledFramebufferSize &get_upscaled_framebuffer_size) : audio_output(audio_output), window(window), get_upscaled_framebuffer_size(get_upscaled_framebuffer_size){}
	void Display(bool &open);
};

#endif /* DEBUG_FRONTEND_H */
