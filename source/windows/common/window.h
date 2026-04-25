#ifndef WINDOW_H
#define WINDOW_H

#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "../../sdl-wrapper.h"

class Window
{
private:
	SDL::Window sdl_window;
	SDL::Renderer renderer;

public:
	static inline std::map<std::string, std::pair<int, int>> positions;

	static float GetDPIScale(Window* const window)
	{
		return window == nullptr ? GetDisplayDPIScale() : window->GetDPIScale();
	}

	Window(const char *window_title, float window_width, float window_height, bool resizeable, const std::optional<float> &forced_scale = std::nullopt, Uint32 window_flags = 0);

	static float GetDisplayDPIScale();
	float GetSizeScale();
	float GetDPIScale();
	void SetFullscreen(bool enabled);
	bool GetFullscreen() { return (SDL_GetWindowFlags(GetSDLWindow()) & SDL_WINDOW_FULLSCREEN) != 0; }
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
