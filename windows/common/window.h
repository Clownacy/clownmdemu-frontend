#ifndef WINDOW_H
#define WINDOW_H

#include <memory>

#include "../../sdl-wrapper.h"

class Window
{
public:
	SDL::Window sdl_window;
	SDL::Renderer renderer;

	Window(const char *window_title, int window_width, int window_height, bool resizeable);

	float GetSizeScale();
	float GetDPIScale();
	void SetFullscreen(bool enabled) { SDL_SetWindowFullscreen(GetSDLWindow(), enabled ? SDL_WINDOW_FULLSCREEN : 0); }
	bool GetFullscreen() { return (SDL_GetWindowFlags(GetSDLWindow()) & SDL_WINDOW_FULLSCREEN) != 0; }
	void ToggleFullscreen() { SetFullscreen(!GetFullscreen()); }
	SDL::Window& GetSDLWindow() { return sdl_window; }
	const SDL::Window& GetSDLWindow() const { return sdl_window; }
	SDL::Renderer& GetRenderer() { return renderer; }
	const SDL::Renderer& GetRenderer() const { return renderer; }
	void SetTitleBarColour(unsigned char red, unsigned char green, unsigned char blue);
	void ShowWarningMessageBox(const char *message);
	void ShowErrorMessageBox(const char *message);
	void ShowFatalMessageBox(const char *message);
};

#endif /* WINDOW_H */
