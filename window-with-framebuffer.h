#ifndef WINDOW_WITH_FRAMEBUFFER_H
#define WINDOW_WITH_FRAMEBUFFER_H

#include "sdl-wrapper.h"

#include "debug-log.h"
#include "window-with-dear-imgui.h"

class WindowWithFramebuffer : public WindowWithDearImGui
{
private:
	static SDL_Texture* CreateFramebufferTexture(DebugLog &debug_log, SDL_Renderer* const renderer, const int framebuffer_width, const int framebuffer_height);

public:
	SDL::Texture framebuffer_texture;

	WindowWithFramebuffer(DebugLog &debug_log, const char* const window_title, const int window_width, const int window_height, const int framebuffer_width, const int framebuffer_height, const bool resizeable)
		: WindowWithDearImGui(debug_log, window_title, window_width, window_height, resizeable)
		, framebuffer_texture(CreateFramebufferTexture(debug_log, GetRenderer(), framebuffer_width, framebuffer_height))
	{

	}

	SDL_Texture* GetFramebufferTexture() const { return framebuffer_texture.get(); }
};

#endif /* WINDOW_WITH_FRAMEBUFFER_H */
