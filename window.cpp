#include "window.h"

float Window::GetNewDPIScale() const
{
	float dpi_scale = 1.0f;

#ifdef _WIN32
	// This doesn't work right on Linux nor macOS.
	// TODO: Find a method that doesn't suck balls.
	float ddpi;
	if (SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(sdl), &ddpi, nullptr, nullptr) == 0)
		dpi_scale = ddpi / 96.0f;
#endif

	return dpi_scale;
}

bool Window::Initialise(const char* const window_title)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		debug_log.Log("SDL_InitSubSystem(SDL_INIT_VIDEO) failed with the following message - '%s'", SDL_GetError());
	}
	else
	{
		// Create window
		sdl = SDL_CreateWindow(window_title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 320 * 2, 224 * 2, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);

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
			else if (InitialiseFramebuffer())
				return true;

			SDL_DestroyWindow(sdl);
		}

		SDL_QuitSubSystem(SDL_INIT_VIDEO);
	}

	return false;
}

void Window::Deinitialise()
{
	DeinitialiseFramebuffer();
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(sdl);
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

void Window::SetFullscreen(bool enabled)
{
	SDL_SetWindowFullscreen(sdl, enabled ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
}

void Window::ToggleFullscreen()
{
	fullscreen = !fullscreen;
	SetFullscreen(fullscreen);
}

bool Window::InitialiseFramebuffer()
{
	// Create framebuffer texture
	// We're using ARGB8888 because it's more likely to be supported natively by the GPU, avoiding the need for constant conversions
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	framebuffer_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT);

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

void Window::RecreateUpscaledFramebuffer(unsigned int display_width, unsigned int display_height)
{
	static unsigned int previous_framebuffer_size_factor = 0;

	const unsigned int framebuffer_size_factor = CC_MAX(1, CC_MIN(CC_DIVIDE_CEILING(display_width, 640), CC_DIVIDE_CEILING(display_height, 480)));

	if (framebuffer_texture_upscaled == nullptr || framebuffer_size_factor != previous_framebuffer_size_factor)
	{
		previous_framebuffer_size_factor = framebuffer_size_factor;

		framebuffer_texture_upscaled_width = 640 * framebuffer_size_factor;
		framebuffer_texture_upscaled_height = 480 * framebuffer_size_factor;

		SDL_DestroyTexture(framebuffer_texture_upscaled); // It should be safe to pass nullptr to this
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
		framebuffer_texture_upscaled = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, framebuffer_texture_upscaled_width, framebuffer_texture_upscaled_height);

		if (framebuffer_texture_upscaled == nullptr)
		{
			debug_log.Log("SDL_CreateTexture failed with the following message - '%s'", SDL_GetError());
		}
		else
		{
			// Disable blending, since we don't need it
			if (SDL_SetTextureBlendMode(framebuffer_texture_upscaled, SDL_BLENDMODE_NONE) < 0)
				debug_log.Log("SDL_SetTextureBlendMode failed with the following message - '%s'", SDL_GetError());
		}
	}
}

