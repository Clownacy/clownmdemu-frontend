#ifndef WINDOW_H
#define WINDOW_H

#include <memory>

#include "../../sdl-wrapper.h"

class Window
{
private:
	SDL::Window sdl_window;
	SDL::Renderer renderer;
	bool fullscreen;

public:
	Window(const char *window_title, int window_width, int window_height, bool resizeable, Uint32 window_flags = 0);

	float GetSizeScale();
	float GetDPIScale();
	void SetFullscreen(const bool enabled)
	{
		fullscreen = enabled;

		SDL_SetWindowFullscreen(GetSDLWindow(), fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
		if (!fullscreen)
			DisableRounding();
	}
	bool GetFullscreen() { return fullscreen; }
	void ToggleFullscreen() { SetFullscreen(!GetFullscreen()); }
	SDL::Window& GetSDLWindow() { return sdl_window; }
	const SDL::Window& GetSDLWindow() const { return sdl_window; }
	SDL::Renderer& GetRenderer() { return renderer; }
	const SDL::Renderer& GetRenderer() const { return renderer; }
	bool GetVSync()
	{
		int vsync;

		if (!SDL_GetRenderVSync(renderer, &vsync))
			return false;

		return vsync != 0;
	}
	void SetVSync(const bool enabled)
	{
		if (enabled)
		{
			// Try enabling adaptive V-sync, and fall-back on regular V-sync if it fails.
			if (!SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_ADAPTIVE))
				SDL_SetRenderVSync(renderer, 1);
		}
		else
		{
			SDL_SetRenderVSync(renderer, SDL_RENDERER_VSYNC_DISABLED);
		}
	}
	void SetTitleBarColour(unsigned char red, unsigned char green, unsigned char blue);
	void DisableRounding();
	void ShowWarningMessageBox(const char *message);
	void ShowErrorMessageBox(const char *message);
	void ShowFatalMessageBox(const char *message);
};

#endif /* WINDOW_H */
