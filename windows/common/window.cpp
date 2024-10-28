#include "window.h"

#include <algorithm>
#include <stdexcept>

#include "../../clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "../../frontend.h"
#include "winapi.h"

static float HandleDPIError(const float dpi_scale)
{
	// Prevent any insanity if we somehow get bad values.
	if (dpi_scale == 0.0f)
		return 1.0f;

	return dpi_scale;
}

static float GetDisplayDPIScale()
{
	const SDL_DisplayID display_index = SDL_GetPrimaryDisplay();

	if (display_index == 0)
	{
		Frontend::debug_log.Log("SDL_GetPrimaryDisplay failed with the following message - '%s'", SDL_GetError());
		return 1.0f;
	}

	return HandleDPIError(SDL_GetDisplayContentScale(display_index));
}

float Window::GetSizeScale() const
{
	const SDL_DisplayID display_index = SDL_GetDisplayForWindow(GetSDLWindow());

	if (display_index == 0)
	{
		Frontend::debug_log.Log("SDL_GetDisplayForWindow failed with the following message - '%s'", SDL_GetError());
		return 1.0f;
	}

	return HandleDPIError(SDL_GetDisplayContentScale(display_index));
}

float Window::GetDPIScale() const
{
	return HandleDPIError(SDL_GetWindowDisplayScale(GetSDLWindow()));
}

Window::Window(const char* const window_title, const int window_width, const int window_height, const bool resizeable)
{
	const float scale = GetDisplayDPIScale();

	Uint32 window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

	if (resizeable)
		window_flags |= SDL_WINDOW_RESIZABLE;

	// SDL3's documentation currently advices using this instead of 'SDL_CreateWindow'
	// and 'SDL_CreateRenderer' separately to avoid "window flicker".
	// https://wiki.libsdl.org/SDL3/SDL_CreateWindow
	SDL_Window *window;
	SDL_Renderer *renderer;
	if (!SDL_CreateWindowAndRenderer(window_title, static_cast<int>(window_width * scale), static_cast<int>(window_height * scale), window_flags, &window, &renderer))
		throw std::runtime_error(std::string("SDL_CreateWindowAndRenderer failed with the following message - '") + SDL_GetError() + "'");

	sdl_window = SDL::Window(window);
	this->renderer = SDL::Renderer(renderer);
}

void Window::SetTitleBarColour(const unsigned char red, const unsigned char green, const unsigned char blue)
{
	SetWindowTitleBarColour(sdl_window.get(), red, green, blue);
}

void Window::ShowWarningMessageBox(const char* const message) const
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", message, GetSDLWindow());
}

void Window::ShowErrorMessageBox(const char* const message) const
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message, GetSDLWindow());
}

void Window::ShowFatalMessageBox(const char* const message) const
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", message, GetSDLWindow());
}
