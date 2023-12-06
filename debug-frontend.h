#ifndef DEBUG_FRONTEND_H
#define DEBUG_FRONTEND_H

#include "audio-output.h"
#include "window.h"

class DebugFrontend
{
public:
	typedef void GetUpscaledFramebufferSize(unsigned int &width, unsigned int &height);

private:
	const AudioOutput &audio_output;
	const Window &window;
	const GetUpscaledFramebufferSize &get_upscaled_framebuffer_size;

public:
	DebugFrontend(const AudioOutput &audio_output, const Window &window, const GetUpscaledFramebufferSize &get_upscaled_framebuffer_size) : audio_output(audio_output), window(window), get_upscaled_framebuffer_size(get_upscaled_framebuffer_size){}
	void Display(bool &open);
};

#endif /* DEBUG_FRONTEND_H */
