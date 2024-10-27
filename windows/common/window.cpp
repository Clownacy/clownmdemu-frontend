#include "window.h"

#include <algorithm>
#include <stdexcept>

#include "../../clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

#include "winapi.h"

float Window::GetDPIScale() const
{
	// TODO: Make the DPI scale two-dimensional.
	int window_width, renderer_width;
	SDL_GetWindowSize(GetSDLWindow(), &window_width, nullptr);
	SDL_GetCurrentRenderOutputSize(GetRenderer(), &renderer_width, nullptr);

	const float dpi_scale = static_cast<float>(renderer_width) / std::max(1, window_width); // Prevent a division by 0.

	// Prevent any insanity if we somehow get bad values.
	return std::max(1.0f, dpi_scale);
}

static SDL_Window* CreateWindow(const char* const window_title, const int window_width, const int window_height, const bool resizeable)
{
	Uint32 window_flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

	if (resizeable)
		window_flags |= SDL_WINDOW_RESIZABLE;

	SDL_Window* const window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, window_flags);

	if (window == nullptr)
		throw std::runtime_error(std::string("SDL_CreateWindow failed with the following message - '") + SDL_GetError() + "'");

	return window;
}

static SDL_Renderer* CreateRenderer(SDL_Window* const window)
{
	// Use batching even if the user forces a specific rendering backend (wtf SDL).
	//
	// Personally, I like to force SDL2 to use D3D11 instead of D3D9 by setting an environment
	// variable, but it turns out that, if I do that, then SDL2 will disable its render batching
	// for backwards compatibility reasons. Setting this hint prevents that.
	//
	// Normally if a user is setting an environment variable to force a specific rendering
	// backend, then they're expected to set another environment variable to set this hint too,
	// but I might as well just do it myself and save everyone else the hassle.
	SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");

	SDL_Renderer* const renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_TARGETTEXTURE);

	if (renderer == nullptr)
		throw std::runtime_error(std::string("SDL_CreateRenderer failed with the following message - '") + SDL_GetError() + "'");

	return renderer;
}

Window::Window(const char* const window_title, const int window_width, const int window_height, const bool resizeable)
	: sdl_window(CreateWindow(window_title, window_width, window_height, resizeable))
	, renderer(CreateRenderer(GetSDLWindow()))
{

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
