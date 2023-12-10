#ifndef WINDOW_H
#define WINDOW_H

#include <memory>

#include "sdl-wrapper.h"

#include "debug-log.h"

class Window
{
private:
	bool fullscreen = false;

public:
	SDL::Window sdl_window;
	SDL::Renderer renderer;
	SDL::Texture framebuffer_texture;

	Window(DebugLog &debug_log, const char *window_title, int window_width, int window_height, int framebuffer_width, int framebuffer_height);

	float GetDPIScale() const;
	void SetFullscreen(bool enabled)
	{
		fullscreen = enabled;
		SDL_SetWindowFullscreen(GetSDLWindow(), enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	}
	bool GetFullscreen() const { return (SDL_GetWindowFlags(GetSDLWindow()) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0; }
	void ToggleFullscreen() { SetFullscreen(!GetFullscreen()); }
	SDL_Window* GetSDLWindow() const { return sdl_window.get(); }
	SDL_Renderer* GetRenderer() const { return renderer.get(); }
	SDL_Texture* GetFramebufferTexture() const { return framebuffer_texture.get(); }
	void ShowWarningMessageBox(const char *message) const;
	void ShowErrorMessageBox(const char *message) const;
	void ShowFatalMessageBox(const char *message) const;
};

#endif /* WINDOW_H */
