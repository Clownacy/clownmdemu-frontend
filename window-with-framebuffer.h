#ifndef WINDOW_WITH_FRAMEBUFFER_H
#define WINDOW_WITH_FRAMEBUFFER_H

#include "sdl-wrapper.h"

#include "debug-log.h"
#include "window.h"

class WindowWithFramebuffer : public Window
{
public:
	SDL::Texture framebuffer_texture;

	WindowWithFramebuffer(DebugLog &debug_log, const char *window_title, int window_width, int window_height, int framebuffer_width, int framebuffer_height);

	SDL_Texture* GetFramebufferTexture() const { return framebuffer_texture.get(); }
};

#endif /* WINDOW_WITH_FRAMEBUFFER_H */
