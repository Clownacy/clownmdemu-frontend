#ifndef WINDOW_H
#define WINDOW_H

#include "SDL.h"

#include "debug_log.h"

class Window
{
private:
	DebugLog &debug_log;
	bool fullscreen;

	bool InitialiseFramebuffer(int width, int height);
	void DeinitialiseFramebuffer();

public:
	SDL_Window *sdl;
	SDL_Renderer *renderer;
	SDL_Texture *framebuffer_texture;
	SDL_Texture *framebuffer_texture_upscaled;
	unsigned int framebuffer_texture_upscaled_width;
	unsigned int framebuffer_texture_upscaled_height;

	Window(DebugLog &debug_log) : debug_log(debug_log) {}

	float GetDPIScale() const;
	bool Initialise(const char *window_title, int framebuffer_width, int framebuffer_height);
	void Deinitialise();
	void SetFullscreen(bool enabled)
	{
		fullscreen = enabled;
		SDL_SetWindowFullscreen(sdl, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	}
	bool GetFullscreen() const { return fullscreen; }
	void ToggleFullscreen() { SetFullscreen(!GetFullscreen()); }
	void RecreateUpscaledFramebuffer(unsigned int display_width, unsigned int display_height);
};

#endif /* WINDOW_H */
