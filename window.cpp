#include "window.h"

float Window::GetDPIScale() const
{
	// TODO: Make the DPI scale two-dimensional.
	float dpi_scale;

#ifdef _WIN32
	// Windows needs silly custom bollocks. Maybe I should just use WinAPI.
	dpi_scale = 1.0f;

	float ddpi;
	if (SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(sdl), &ddpi, nullptr, nullptr) == 0)
		dpi_scale = ddpi / 96.0f;
#else
	int window_width, renderer_width;
	SDL_GetWindowSize(sdl, &window_width, nullptr);
	SDL_GetRendererOutputSize(renderer, &renderer_width, nullptr);

	dpi_scale = static_cast<float>(renderer_width) / CC_MAX(1, window_width); // Prevent a division by 0.
#endif

	// Prevent any insanity if we somehow get bad values.
	return CC_MAX(1.0f, dpi_scale);
}

bool Window::Initialise(const char* const window_title, const int window_width, const int window_height, const int framebuffer_width, const int framebuffer_height)
{
	sdl = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, window_width, window_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN
#ifndef _WIN32
		// This currently does nothing on Windows, so we use our own custom implementation.
		// However, in case this ever does do something in the future, avoid using it on Windows.
		// We wouldn't want the two to clash.
		| SDL_WINDOW_ALLOW_HIGHDPI
#endif
	);

	if (sdl == nullptr)
	{
		debug_log.Log("SDL_CreateWindow failed with the following message - '%s'", SDL_GetError());
	}
	else
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

		// Create renderer
		renderer = SDL_CreateRenderer(sdl, -1, SDL_RENDERER_TARGETTEXTURE);

		if (renderer == nullptr)
			debug_log.Log("SDL_CreateRenderer failed with the following message - '%s'", SDL_GetError());
		else if (InitialiseFramebuffer(framebuffer_width, framebuffer_height))
			return true;

		SDL_DestroyWindow(sdl);
	}

	return false;
}

void Window::Deinitialise()
{
	DeinitialiseFramebuffer();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(sdl);
}

bool Window::InitialiseFramebuffer(const int width, const int height)
{
	// Create framebuffer texture
	// We're using ARGB8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	framebuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);

	if (framebuffer_texture == nullptr)
	{
		debug_log.Log("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Disable blending, since we don't need it
		if (SDL_SetTextureBlendMode(framebuffer_texture, SDL_BLENDMODE_NONE) < 0)
			debug_log.Log("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());

		return true;
	}

	return false;
}

void Window::DeinitialiseFramebuffer()
{
	SDL_DestroyTexture(framebuffer_texture);
}

void Window::ShowWarningMessageBox(const char* const message) const
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Warning", message, sdl);
}

void Window::ShowErrorMessageBox(const char* const message) const
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Error", message, sdl);
}

void Window::ShowFatalMessageBox(const char* const message) const
{
	SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Fatal Error", message, sdl);
}
