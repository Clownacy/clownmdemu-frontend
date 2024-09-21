#include "window-with-framebuffer.h"

#include <stdexcept>

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

WindowWithFramebuffer::WindowWithFramebuffer(DebugLog &debug_log, const char* const window_title, const int window_width, const int window_height, const int framebuffer_width, const int framebuffer_height)
	: Window(debug_log, window_title, window_width, window_height, framebuffer_width, framebuffer_height)
	, framebuffer_texture(CreateFramebufferTexture(debug_log, GetRenderer(), framebuffer_width, framebuffer_height))
{

}
