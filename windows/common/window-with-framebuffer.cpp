#include "window-with-framebuffer.h"

#include <stdexcept>

#include "../../frontend.h"

SDL::Texture WindowWithFramebuffer::CreateFramebufferTexture(SDL::Renderer &renderer, const int framebuffer_width, const int framebuffer_height)
{
	auto texture = SDL::CreateTexture(renderer, SDL_TEXTUREACCESS_STREAMING, framebuffer_width, framebuffer_height, SDL_SCALEMODE_NEAREST);

	// Try to use SDL3's fancy new 'pixel-art' scaling mode.
	// The cast is to support older versions of SDL3 (pre-3.2.12).
	SDL_SetTextureScaleMode(texture, static_cast<SDL_ScaleMode>(2)/*SDL_SCALEMODE_PIXELART*/);

	return texture;
}
