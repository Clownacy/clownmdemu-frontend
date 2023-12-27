#include "window.h"

#include <algorithm>
#include <stdexcept>

#include "clownmdemu-frontend-common/clownmdemu/clowncommon/clowncommon.h"

float Window::GetDPIScale() const
{
	// TODO: Make the DPI scale two-dimensional.
	int window_width, renderer_width;
	SDL_GetWindowSize(GetSDLWindow(), &window_width, nullptr);
	SDL_GetRendererOutputSize(GetRenderer(), &renderer_width, nullptr);

	const float dpi_scale = static_cast<float>(renderer_width) / std::max(1, window_width); // Prevent a division by 0.

	// Prevent any insanity if we somehow get bad values.
	return std::max(1.0f, dpi_scale);
}

static SDL_Window* CreateWindow(const char* const window_title, const int window_width, const int window_height)
{
	SDL_Window* const window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);

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

static SDL_Texture* CreateFramebufferTexture(DebugLog &debug_log, SDL_Renderer* const renderer, const int framebuffer_width, const int framebuffer_height)
{
	// Create framebuffer texture
	// We're using ARGB8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	SDL_Texture* const framebuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, framebuffer_width, framebuffer_height);

	if (framebuffer_texture == nullptr)
	{
		throw std::runtime_error(std::string("SDL_CreateTexture failed with the following message - '") + SDL_GetError() + "'");
	}
	else
	{
		// Disable blending, since we don't need it
		if (SDL_SetTextureBlendMode(framebuffer_texture, SDL_BLENDMODE_NONE) < 0)
			debug_log.Log("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());
	}

	return framebuffer_texture;
}

Window::Window(DebugLog &debug_log, const char* const window_title, const int window_width, const int window_height, const int framebuffer_width, const int framebuffer_height)
	: sdl_window(CreateWindow(window_title, window_width, window_height))
	, renderer(CreateRenderer(GetSDLWindow()))
	, framebuffer_texture(CreateFramebufferTexture(debug_log, GetRenderer(), framebuffer_width, framebuffer_height))
{

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
