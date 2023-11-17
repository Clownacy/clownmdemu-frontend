#ifndef WINDOW_H
#define WINDOW_H

#include "SDL.h"

#include "debug_log.h"

#define FRAMEBUFFER_WIDTH 320
#define FRAMEBUFFER_HEIGHT (240*2) // *2 because of double-resolution mode.

class Window
{
private:
	DebugLog &debug_log;

public:
	SDL_Window *sdl; // TODO: Make private
	SDL_Renderer *renderer;
	SDL_Texture *framebuffer_texture;
	Uint32 *framebuffer_texture_pixels; // Move out
	int framebuffer_texture_pitch; // Move out
	SDL_Texture *framebuffer_texture_upscaled;
	unsigned int framebuffer_texture_upscaled_width;
	unsigned int framebuffer_texture_upscaled_height;

	unsigned int current_screen_width;
	unsigned int current_screen_height;

	bool use_vsync; // Move out of this class
	bool fullscreen;
	bool integer_screen_scaling;
	bool tall_double_resolution_mode;

	Window(DebugLog &debug_log) : debug_log(debug_log) {}

	float GetNewDPIScale() const;
	bool Initialise(const char *window_title);
	void Deinitialise();
	void SetFullscreen(bool enabled);
	void ToggleFullscreen();
	bool InitialiseFramebuffer();
	void DeinitialiseFramebuffer();
	void RecreateUpscaledFramebuffer(unsigned int display_width, unsigned int display_height);
};

#endif /* WINDOW_H */
