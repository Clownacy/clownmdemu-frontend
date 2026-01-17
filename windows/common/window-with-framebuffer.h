#ifndef WINDOW_WITH_FRAMEBUFFER_H
#define WINDOW_WITH_FRAMEBUFFER_H

#include "../../sdl-wrapper.h"

#include "window-with-dear-imgui.h"

class WindowWithFramebuffer : public WindowWithDearImGui
{
private:
	static SDL::Texture CreateFramebufferTexture(SDL::Renderer &renderer, const int framebuffer_width, const int framebuffer_height);

public:
	SDL::Texture framebuffer_texture;

	WindowWithFramebuffer(const char* const window_title, const int window_width, const int window_height, const int framebuffer_width, const int framebuffer_height, const bool resizeable, const Uint32 window_flags = 0)
		: WindowWithDearImGui(window_title, window_width, window_height, resizeable, window_flags)
		, framebuffer_texture(CreateFramebufferTexture(GetRenderer(), framebuffer_width, framebuffer_height))
	{

	}
};

#endif /* WINDOW_WITH_FRAMEBUFFER_H */
