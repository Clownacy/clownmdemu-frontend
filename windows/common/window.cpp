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

	SDL_Window* const window = SDL_CreateWindow(window_title, window_width, window_height, window_flags);

	if (window == nullptr)
		throw std::runtime_error(std::string("SDL_CreateWindow failed with the following message - '") + SDL_GetError() + "'");

	return window;
}

static SDL_Renderer* CreateRenderer(SDL_Window* const window)
{
	SDL_Renderer* const renderer = SDL_CreateRenderer(window, nullptr);

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
