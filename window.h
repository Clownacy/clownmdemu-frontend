#ifndef WINDOW_H
#define WINDOW_H

#include <memory>

#include "SDL.h"

#include "debug-log.h"

class WindowInner
{
private:
	DebugLog &debug_log;
	bool fullscreen;

public:
	std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> sdl_window;
	std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer;
	std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> framebuffer_texture;

	WindowInner(DebugLog &debug_log, const char *window_title, int window_width, int window_height, int framebuffer_width, int framebuffer_height);

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
};

class Window
{
private:
	DebugLog &debug_log;
	bool fullscreen;
	WindowInner *window;

public:
	Window(DebugLog &debug_log) : debug_log(debug_log) {};

	bool Initialise(const char *window_title, int window_width, int window_height, int framebuffer_width, int framebuffer_height)
	{
		window = new WindowInner(debug_log, window_title, window_width, window_height, framebuffer_width, framebuffer_height);
		return window != nullptr;
	}
	void Deinitialise()
	{
		delete window;
	}

	float GetDPIScale() const;
	void SetFullscreen(bool enabled)
	{
		fullscreen = enabled;
		SDL_SetWindowFullscreen(GetSDLWindow(), enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
	}
	bool GetFullscreen() const { return (SDL_GetWindowFlags(GetSDLWindow()) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP)) != 0; }
	void ToggleFullscreen() { SetFullscreen(!GetFullscreen()); }
	SDL_Window* GetSDLWindow() const { return window->GetSDLWindow(); }
	SDL_Renderer* GetRenderer() const { return window->GetRenderer(); }
	SDL_Texture* GetFramebufferTexture() const { return window->GetFramebufferTexture(); }
	void ShowWarningMessageBox(const char *message) const;
	void ShowErrorMessageBox(const char *message) const;
	void ShowFatalMessageBox(const char *message) const;
};

#endif /* WINDOW_H */
