#include "window-with-framebuffer.h"

#include <stdexcept>

#include "../../frontend.h"

SDL_Texture* WindowWithFramebuffer::CreateFramebufferTexture(SDL_Renderer* const renderer, const int framebuffer_width, const int framebuffer_height)
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
			Frontend::debug_log.Log("SDL_SetTextureBlendMode failed with the following message - '{}'", SDL_GetError());
	}

	return framebuffer_texture;
}
