#include "window-with-framebuffer.h"

#include <stdexcept>

#include "../../frontend.h"

SDL::Texture WindowWithFramebuffer::CreateFramebufferTexture(SDL::Renderer &renderer, const int framebuffer_width, const int framebuffer_height)
{
	return SDL::CreateTexture(renderer, SDL_TEXTUREACCESS_STREAMING, framebuffer_width, framebuffer_height, SDL_SCALEMODE_NEAREST);
}
