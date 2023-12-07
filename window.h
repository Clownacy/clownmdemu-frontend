#ifndef WINDOW_H
#define WINDOW_H

#include "SDL.h"

#include "debug-log.h"

class Window
{
private:
	DebugLog &debug_log;
	bool fullscreen;

public:
	SDL_Window *sdl;
	SDL_Renderer *renderer;
	SDL_Texture *framebuffer_texture;

	Window(DebugLog &debug_log) : debug_log(debug_log) {}

	float GetDPIScale() const;
	bool Initialise(const char *window_title, int window_width, int window_height, int framebuffer_width, int framebuffer_height);
	void Deinitialise();
	void SetFullscreen(bool enabled)
	{
		fullscreen = enabled;
		SDL_SetWindowFullscreen(sdl, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	}
	bool GetFullscreen() const { return (SDL_GetWindowFlags(sdl) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0; }
	void ToggleFullscreen() { SetFullscreen(!GetFullscreen()); }
	void ShowWarningMessageBox(const char *message) const;
	void ShowErrorMessageBox(const char *message) const;
	void ShowFatalMessageBox(const char *message) const;
};

#endif /* WINDOW_H */
