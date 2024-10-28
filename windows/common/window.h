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

	float GetSizeScale() const;
	float GetDPIScale() const;
	void SetFullscreen(bool enabled) { SDL_SetWindowFullscreen(GetSDLWindow(), enabled ? SDL_WINDOW_FULLSCREEN : 0); }
	bool GetFullscreen() const { return (SDL_GetWindowFlags(GetSDLWindow()) & SDL_WINDOW_FULLSCREEN) != 0; }
	void ToggleFullscreen() { SetFullscreen(!GetFullscreen()); }
	SDL_Window* GetSDLWindow() const { return sdl_window.get(); }
	SDL_Renderer* GetRenderer() const { return renderer.get(); }
	void SetTitleBarColour(unsigned char red, unsigned char green, unsigned char blue);
	void ShowWarningMessageBox(const char *message) const;
	void ShowErrorMessageBox(const char *message) const;
	void ShowFatalMessageBox(const char *message) const;
};

#endif /* WINDOW_H */
